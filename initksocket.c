#include "ksocket.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

// Shared Memory
int shmid, shmkey;
int shmid_udp_sock, shmkey_udp_sock;

struct ktp_sockaddr *ktp_arr;
int *udp_sock_fds;

// Semaphores
sem_t *sem;

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

#define P1 0.2  // Packet drop probability

int dropMessage(float p) {
    float random = (float)rand() / RAND_MAX;
    printf("--- --- --- Packet droped --- --- ---\n");
    return random < p;
}

void send_ack(int sock_fd, struct ktp_sockaddr *sock, int rwnd_size) {
    /*
        Note: ACK message contains the last acked seq num and the amount of free messages 
        the recv window can further receive
    */

    uint8_t ack_num = sock->rwnd.last_ack_sent;

    struct ktp_header ack_header;
    ack_header.seq_num = 0;  // Not used for ACK
    ack_header.ack_num = ack_num;
    ack_header.is_ack = 1;
    ack_header.rwnd_size = rwnd_size;  // Available space

    // empty message of size 512
    char mssg[MSSG_SIZE+1] = "This is an ACK mssg";

    // Create ACK packet
    char *ack_packet = pkt_create(ack_header, mssg);

    struct ktp_header header_chk;
    char mssg_chk[MSSG_SIZE+1];
    extract_pkt(ack_packet, &header_chk, mssg_chk);
    
    printf("Checking ACK:\n\t\tMessage:|%s|\n", mssg_chk);

    printf("Ack message of size %lu prepared\n", sizeof(ack_packet));
    
    // Prepare destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(sock->des_port);
    inet_pton(AF_INET, sock->des_ip, &dest_addr.sin_addr);

    // Send ACK
    sendto(sock_fd, ack_packet, PACKET_SIZE, 0, 
           (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    printf("--- Ack successfully sent \n");
    
    free(ack_packet);
}

void *R(void* arg) {
    // Initialize random seed for dropMessage
    srand(time(NULL));
    struct timeval timeout;
    
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int maxfd = -1;
 
        // Set up select() for all active UDP sockets
        for (int i = 0; i < MAX_CONC_SOSCKETS; i++) {
            
            if (ktp_arr[i].process_id < 0)
                continue;
            FD_SET(ktp_arr[i].udp_fd, &read_fds);
            if (ktp_arr[i].udp_fd > maxfd)
                maxfd = ktp_arr[i].udp_fd;

        }
        
        if (maxfd < 0)
            continue;
        
        // Use a timeout of 1 second
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ready > 0) {
            printf("--- select received something!\n");
            
            for (int i = 0; i < MAX_CONC_SOSCKETS; i++) {
                
                if(ktp_arr[i].process_id < 0) continue;
                if (ktp_arr[i].udp_fd < 0 || !FD_ISSET(ktp_arr[i].udp_fd, &read_fds))
                continue;
                
                printf("------ select received something on %d!\n", ktp_arr[i].process_id);
                
                // Receive the packet
                char packet[PACKET_SIZE];
                struct sockaddr_in sender_addr;
                socklen_t addr_len = sizeof(sender_addr);
                
                ssize_t received = recvfrom(ktp_arr[i].udp_fd, packet, sizeof(packet), 0,
                (struct sockaddr*)&sender_addr, &addr_len);
                printf("Packet, Received: %d |%s|\n", (int)received, packet);
                if (received <= 0)
                {
                    printf("PID: %d Nothing received\n", ktp_arr[i].process_id);
                    continue;
                }
                
                // Simulate packet loss
                // if (dropMessage(P1)) {
                //     continue; // Drop packet
                // }
                
                // Extract header and message
                struct ktp_header pkt_header;
                char message[MSSG_SIZE + 1];
                extract_pkt(packet, &pkt_header, message);

                printf("Message recvd: %s\n", message);
                print_header(&pkt_header);
                
                if (pkt_header.is_ack) {
                    // ACK message handling
                    // Check if the ACK is for the outstanding packet.
                    // We assume swnd.base holds the sequence number of the packet currently in flight.
                    if (pkt_header.ack_num == ktp_arr[i].swnd.next_seq_num) {
                        
                        printf("hereAck\n");
                        sem_wait(sem);
                        // Valid in-order ACK received:
                        // Update next sequence number: if ack is MAX_SEQ_NUM then next becomes 1.
                        ktp_arr[i].swnd.next_seq_num = (pkt_header.ack_num % MAX_SEQ_NUM) + 1;
                        
                        ktp_arr[i].rwnd.last_ack_sent = pkt_header.ack_num;

                        // update available rwnd size
                        ktp_arr[i].swnd.available_rwnd = pkt_header.rwnd_size;

                        // remove the message from send buffer
                        dequeue(&ktp_arr[i].send_buf, NULL);

                        // update window size
                        ktp_arr[i].swnd.window_size = min(sizeOfCircularArray(&ktp_arr[i].send_buf), ktp_arr[i].swnd.available_rwnd);

                        // update send_times
                        ktp_arr[i].swnd.send_times[pkt_header.ack_num].tv_sec = 0;
                        ktp_arr[i].swnd.send_times[pkt_header.ack_num].tv_usec = 0;

                        // update outstanding pkt
                        ktp_arr[i].swnd.sent_seq_nums[pkt_header.ack_num] = 1;

                        sem_post(sem);
                    } else{
                        // Compute the lower (base) and upper limit of valid outstanding sequence numbers.
                        uint8_t base = ktp_arr[i].swnd.next_seq_num;
                        uint8_t window_size = ktp_arr[i].swnd.window_size;
                        uint8_t upper_limit = (((base - 1) + (window_size - 1)) % MAX_SEQ_NUM) + 1;

                        // Now, check if the received ACK is within the window.
                        // Two cases are needed to account for wrap-around.
                        int flag = 0;
                        if (base <= upper_limit) {
                            // No wrap-around: valid ACK must be > base and <= upper_limit.
                            if (pkt_header.ack_num > base && pkt_header.ack_num <= upper_limit) {
                                flag = 1;
                            }
                        } else {
                            // Wrap-around case: valid ACK is either greater than base or less than or equal to upper_limit.
                            if (pkt_header.ack_num > base || pkt_header.ack_num <= upper_limit) {
                                // Process duplicate or out-of-order ACK here.
                                flag = 1;
                            }
                        }
                        
                        if(flag == 1){
                            // Out of order ACK signifies that all packets upto that number were recvd

                            struct ktp_sockaddr* sock = &ktp_arr[i];
                            
                            int tmp = sock->swnd.next_seq_num;
                            sem_wait(sem);
                            while(1){
                                sock->swnd.send_times[tmp].tv_sec = 0;
                                sock->swnd.send_times[tmp].tv_usec = 0;
                                sock->swnd.sent_seq_nums[tmp] = 0;
                                dequeue(&ktp_arr[i].send_buf, NULL);
                                
                                if(tmp == pkt_header.ack_num){
                                    sock->swnd.next_seq_num = (tmp )% MAX_SEQ_NUM + 1;
                                    break;
                                }

                                tmp = (tmp )% MAX_SEQ_NUM + 1;
                            }
                            sem_post(sem);

                            ktp_arr[i].swnd.window_size = min(sizeOfCircularArray(&ktp_arr[i].send_buf), ktp_arr[i].swnd.available_rwnd);
                            ktp_arr[i].swnd.available_rwnd = pkt_header.rwnd_size;

                            ktp_arr[i].rwnd.last_ack_sent = pkt_header.ack_num;
                        }
                        
                    }
                    
                    print_swnd(&ktp_arr[i].swnd, &ktp_arr[i].send_buf);
                    
                } else {
                    // Data message handling
                    // Accept the packet only if its sequence number matches the expected sequence.
                    // if (pkt_header.seq_num == ktp_arr[i].rwnd.next_expected_seq) {
                    //     // Enqueue the message in the receiver buffer.
                    //     printf("hereNewMssgRecv\n");
                    //     sem_wait(sem);
                    //     if (enqueue(&ktp_arr[i].recv_buf, message)) {
                    //         // Send ACK for this packet.
                    //         send_ack(ktp_arr[i].udp_fd, &ktp_arr[i], pkt_header.seq_num);
                    //         // Update expected sequence number (wrap from MAX_SEQ_NUM to 1)
                    //         ktp_arr[i].rwnd.next_expected_seq = (pkt_header.seq_num % MAX_SEQ_NUM) + 1;
                    //     }
                    //     sem_post(sem);
                    // } else {
                    //     // Out-of-order data packet: drop it.
                    //     // send an ACK for the last in-order packet,
                    //     printf("hereAck\n");
                    //     sem_wait(sem);
                    //     uint8_t last_in_order;
                    //     if (ktp_arr[i].rwnd.next_expected_seq == 1)
                    //         last_in_order = MAX_SEQ_NUM;
                    //     else
                    //         last_in_order = ktp_arr[i].rwnd.next_expected_seq - 1;
                    //     send_ack(ktp_arr[i].udp_fd, &ktp_arr[i], last_in_order);
                    //     sem_post(sem);
                    // }
                    sem_wait(sem);
                    int free_space = update_rwnd(&ktp_arr[i].rwnd, pkt_header.seq_num, message, &ktp_arr[i]);
                    sem_post(sem);

                    if(free_space != -1)
                        send_ack(ktp_arr[i].udp_fd, &ktp_arr[i], free_space);
                    else{
                        
                        printf("Error receiving message. %s\n", getCustomErrorMessage(global_err_var));

                        // resend the last sent ACK packet
                        send_ack(ktp_arr[i].udp_fd, &ktp_arr[i], free_space);
                    }

                }
            }
        }else{
            printf("select did not receive anything!\n");
        }
        // On timeout, nothing specific to do as sender is waiting for an ACK.
    }
    return NULL;
}


// Thread S: Handles timeouts and retransmissions for one-packet-at-a-time.
void *S(void *arg) {
    // T is defined in ksocket.h, e.g. #define T 5
    while (1) {
        
        sleep(T / 2);
        
        // For each active KTP socket
        for (int i = 0; i < MAX_CONC_SOSCKETS; i++) {
            if (ktp_arr[i].udp_fd < 0)
                continue;
            
            struct ktp_sockaddr *sock = &ktp_arr[i];
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            
            // Retransmission for outstanding packets in the valid window range.
            if(1){
                struct ktp_sockaddr *sock = &ktp_arr[i];
                struct timeval current_time;
                gettimeofday(&current_time, NULL);

                // Compute the upper limit (circular, 1-indexed):
                // Range: from sock->swnd.next_seq_num to upper_limit = ((sock->swnd.next_seq_num - 1 + sock->swnd.window_size) % MAX_SEQ_NUM) + 1.
                // Iterate over the window entries.
                for (int j = 0; j < sock->swnd.window_size; j++) {
                    int seq_num = ((sock->swnd.next_seq_num - 1 + j) % MAX_SEQ_NUM) + 1;
                    // Check if this packet is outstanding (0 indicates outstanding).
                    if (sock->swnd.sent_seq_nums[seq_num] == 0) {
                        double elapsed = (current_time.tv_sec - sock->swnd.send_times[seq_num].tv_sec) +
                                        (current_time.tv_usec - sock->swnd.send_times[seq_num].tv_usec) / 1000000.0;
                        if (elapsed >= T) {
                            // Retrieve the message corresponding to this outstanding packet.
                            // We assume that the outstanding packets are stored in the send buffer in order.
                            // Use getKMessages to retrieve up to window_size messages.
                            char **k_messages = NULL;
                            int num_msgs = getKMessages(&sock->send_buf, sock->swnd.window_size, &k_messages);
                            // Map the j-th outstanding message to seq_num.
                            if (j < num_msgs) {
                                struct ktp_header header;
                                header.seq_num = seq_num;
                                header.is_ack = 0;
                                header.rwnd_size = sock->swnd.available_rwnd;  // current available space

                                char *pkt = pkt_create(header, k_messages[j]);

                                // Prepare destination address.
                                struct sockaddr_in dest_addr;
                                memset(&dest_addr, 0, sizeof(dest_addr));
                                dest_addr.sin_family = AF_INET;
                                dest_addr.sin_port = htons(sock->des_port);
                                inet_pton(AF_INET, sock->des_ip, &dest_addr.sin_addr);

                                if (sendto(sock->udp_fd, pkt, PACKET_SIZE, 0,
                                        (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
                                    printf("Retransmission error for seq %d: %s\n", seq_num, strerror(errno));
                                } else {
                                    printf("Retransmitted packet with seq %d after %.2f seconds elapsed\n", seq_num, elapsed);
                                }
                                free(pkt);
                            }
                            // Free the messages retrieved by getKMessages.
                            if (k_messages != NULL) {
                                for (int m = 0; m < num_msgs; m++) {
                                    free(k_messages[m]);
                                }
                                free(k_messages);
                            }
                        }
                    }
                }
            }

            // Case 2: No outstanding packet. Check if there is a pending message.
            if(1) {
                // if (!isEmpty(&sock->send_buf) && sock->swnd.window_size > 0) {
                //     // Print the buffer
                //     print_buff(&sock->send_buf);

                //     // Peek at the first pending message in the send buffer.
                //     char *message = sock->send_buf._buf[sock->send_buf.head];
                    
                //     // int x = sock->send_buf.head;
                //     // if(x == 2)
                //     //     message = sock->send_buf._buf[sock->send_buf.head-1];
                    
                //     // Build packet using the current next sequence number.
                //     struct ktp_header header;
                //     header.seq_num = sock->swnd.next_seq_num;
                //     header.is_ack = 0;
                //     header.rwnd_size = WINDOW_SIZE - sock->recv_buf.count;
                    
                //     char *pkt = pkt_create(header, message);
                    
                //     // Prepare destination address.
                //     struct sockaddr_in dest_addr;
                //     memset(&dest_addr, 0, sizeof(dest_addr));
                //     dest_addr.sin_family = AF_INET;
                //     printf("Des. Port: %d Des. IP: %s\n", sock->des_port, sock->des_ip);
                //     dest_addr.sin_port = htons(sock->des_port);
                //     inet_pton(AF_INET, sock->des_ip, &dest_addr.sin_addr);
                    
                //     printf("here1\n");
                //     printf("Message sent: |%s|\n", message);
                //     // Send the packet.
                //     if(sendto(sock->udp_fd, pkt, PACKET_SIZE, 0,
                //            (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0 ){

                //         printf("%d %s\n", ktp_arr[i].udp_fd, strerror(errno));

                //     }
                //     printf("Packet of size %d sent\n", sizeof(pkt));

                //     // Update window size
                //     sock->swnd.window_size--;

                //     // Mark this packet as outstanding.
                //     sock->swnd.base = header.seq_num;
                //     // Update next sequence number with wrap-around (1-MAX_SEQ_NUM).
                //     sock->swnd.next_seq_num = (header.seq_num % MAX_SEQ_NUM) + 1;
                    
                //     // Record the send time.
                    // gettimeofday(&sock->last_send_time, NULL);
                    
                //     free(pkt);
                // }


                struct ktp_sockaddr * sock = &ktp_arr[i];
                
                // extract 'limit' number of messages to send from send_buf
                sock->swnd.window_size = min(sizeOfCircularArray(&sock->send_buf), sock->swnd.available_rwnd);
                char **k_messages = NULL;
                getKMessages(&sock->send_buf, sock->swnd.window_size, &k_messages);
                
                // prepare destination address.
                struct sockaddr_in dest_addr;
                memset(&dest_addr, 0, sizeof(dest_addr));
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(sock->des_port);
                inet_pton(AF_INET, sock->des_ip, &dest_addr.sin_addr);
                
                if(sock->swnd.window_size > 0)
                printf("Sending %d messages to Des. Port: %d Des. IP: %s\n", sock->swnd.window_size, sock->des_port, sock->des_ip);
                
                for(int i=0; i<sock->swnd.window_size; i++){
                    // Build packet using the current next sequence number.
                    struct ktp_header header;
                    header.seq_num = ((sock->swnd.next_seq_num - 1 + i) % MAX_SEQ_NUM) + 1;
                    header.is_ack = 0;
                    header.rwnd_size = -1; // irrelavent

                    if (sock->swnd.sent_seq_nums[header.seq_num] == 0) continue;
                    
                    char *pkt = pkt_create(header, k_messages[i]);

                    // Send the packet.
                    if(sendto(sock->udp_fd, pkt, PACKET_SIZE, 0,
                        (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0 ){

                        printf("%d %s\n", ktp_arr[i].udp_fd, strerror(errno));

                    }else {
                        printf("\tSent seq_num: %d\n", header.seq_num);
                    }

                    // update swnd to keep track of outstanding pkts
                    sock->swnd.sent_seq_nums[header.seq_num] = 0;

                    gettimeofday(&sock->swnd.send_times[header.seq_num], NULL);

                }

                printf("\n\n");
            }
        }
    }
    return NULL;
}


void custom_exit(int status){

    for(int i=0; i<MAX_CONC_SOSCKETS; i++){
        
        printf("%d ", i);
        close(udp_sock_fds[i]);

    }

    shmdt(ktp_arr);
    shmctl(shmid, IPC_RMID, NULL);

    shmdt(udp_sock_fds);
    shmctl(shmid_udp_sock, IPC_RMID, NULL);

    sem_close(sem);
    
    exit(status);
}

void intialise_array(){
    for(int i=0; i<MAX_CONC_SOSCKETS; i++){
        
        printf("here2\n");
        sem_wait(sem);
        printf("here3\n");
        initialise_shm_ele(&ktp_arr[i]);
        sem_post(sem);

    }
    return;
}

void sigint_handler(int signum) {
    printf("\nSIGINT received. Cleaning up and terminating.\n");
    custom_exit(0);
}

void set_udp_sock_fds(){
    for(int i=0; i<MAX_CONC_SOSCKETS; i++){

        printf("hereUdp\n");
        sem_wait(sem);
        udp_sock_fds[i] = socket(AF_INET, SOCK_DGRAM, 0);
        sem_post(sem);
    }
}


int main(){

    // create semaphore to access the shared mem of different processese
    printf("here\n");
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);

    signal(SIGINT, sigint_handler);
    pthread_t send_thread, recv_thread;

    // Shared memory for KTP array (KTP socket structures)
    shmkey = ftok("/", 'A');
    if (shmkey == -1) {
        perror("ftok failed for KTP array");
        exit(EXIT_FAILURE);
    }

    shmid = shmget(shmkey, MAX_CONC_SOSCKETS * sizeof(struct ktp_sockaddr), 0777 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed for KTP array");
        exit(EXIT_FAILURE);
    }

    ktp_arr = (struct ktp_sockaddr *)shmat(shmid, NULL, 0);
    if (ktp_arr == (void *)-1) {
        perror("shmat failed for KTP array");
        exit(EXIT_FAILURE);
    }

    printf("Shared memory for KTP array allocated and attached successfully.\n");

    // Shared Memory for UDP socket file descriptors
    shmkey_udp_sock = ftok("/", 'B');
    if (shmkey_udp_sock == -1) {
        perror("ftok failed for UDP socket descriptors");
        exit(EXIT_FAILURE);
    }

    shmid_udp_sock = shmget(shmkey_udp_sock, MAX_CONC_SOSCKETS * sizeof(int), 0777 | IPC_CREAT);
    if (shmid_udp_sock == -1) {
        perror("shmget failed for UDP socket descriptors");
        exit(EXIT_FAILURE);
    }

    udp_sock_fds = (int *)shmat(shmid_udp_sock, NULL, 0);
    if (udp_sock_fds == (void *)-1) {
        perror("shmat failed for UDP socket descriptors");
        exit(EXIT_FAILURE);
    }

    printf("Shared memory for UDP socket descriptors allocated and attached successfully.\n");

    
    if (ktp_arr == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }

    // initialise KTP array
    intialise_array();

    // initialise UDP sockets array
    set_udp_sock_fds();
    
    if(pthread_create(&recv_thread, NULL, R, NULL)){
        printf("Error creating thread R\n");
        custom_exit(1);
    }
    
    if(pthread_create(&send_thread, NULL, S, NULL)){
        printf("Error creating thread S\n");
        custom_exit(1);
    }

    // main funciton now checks for outstanding k_bind calls
    // and binds them
    // also includes garbage collector code to call k_close on processes that have died
    // without calling it
    while(1){

        for(int i=0; i<MAX_CONC_SOSCKETS; i++){
            
            // check if the process has called for an outstanding bind
            struct ktp_sockaddr* sock = &ktp_arr[i];
            
            
            if(ktp_arr[i].process_id> 0 && ktp_arr[i].bind_status == AWAIT_BIND){
                
                struct sockaddr_in servaddr;
                memset(&servaddr, 0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = htons(sock->src_port);
                servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
                
                if(bind(sock->udp_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))){
                    setCustomError(EBIND);
                    printf("Error binding %d\n", sock->process_id);
                    printf("bind error: %s\n", strerror(errno));
                    printf("Binding with family=%d, port=%d, addr=%s, socket=%d\n",
                        servaddr.sin_family,
                        ntohs(servaddr.sin_port),
                        inet_ntoa(servaddr.sin_addr),
                        sock->udp_fd);
                        
                    }
                else{
                    printf("Binding with family=%d, port=%d, addr=%s, socket=%d\n",
                        servaddr.sin_family,
                        ntohs(servaddr.sin_port),
                        inet_ntoa(servaddr.sin_addr),
                        sock->udp_fd);
                    sem_wait(sem);
                    if(sock->process_id)
                    ktp_arr[i].bind_status = BINDED;
                    sem_post(sem);
                    printf("Process %d binded\n", ktp_arr[i].process_id);
                }
            }

            // check if the process has died without calling k_close
            if (kill(ktp_arr[i].process_id, 0) == -1 && errno == ESRCH) {
                sem_wait(sem);
                k_close(i);
                sem_post(sem);
            }

            // close sockets that have called k_close
            if(sock->close_status == AWAIT_CLOSE){
                close(udp_sock_fds[i]);
                sem_wait(sem);
                udp_sock_fds[i] = socket(AF_INET, SOCK_DGRAM, 0);
                sock->close_status = OPEN;
                sem_post(sem);
            }

        }

    }


    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    custom_exit(0);
}
