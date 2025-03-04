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



int shmid, shmkey;
int shmid_udp_sock, shmkey_udp_sock;

struct ktp_sockaddr *ktp_arr;

int *udp_sock_fds;

#define P 0.2  // Packet drop probability

int dropMessage(float p) {
    float random = (float)rand() / RAND_MAX;
    return random < p;
}

void send_ack(int sock_fd, struct ktp_sockaddr *sock, uint8_t ack_num) {
    struct ktp_header ack_header;
    ack_header.seq_num = 0;  // Not used for ACK
    ack_header.ack_num = ack_num;
    ack_header.is_ack = 1;
    ack_header.rwnd_size = WINDOW_SIZE - sock->recv_buf.count;  // Available space

    // Create ACK packet
    char *ack_packet = pkt_create(ack_header, "");
    
    // Prepare destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(sock->des_port);
    inet_pton(AF_INET, sock->des_ip, &dest_addr.sin_addr);

    // Send ACK
    sendto(sock_fd, ack_packet, sizeof(struct ktp_header), 0, 
           (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    // Update last ACK sent
    sock->last_ack_sent = ack_num;
    
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
            if (ktp_arr[i].udp_fd < 0)
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
            for (int i = 0; i < MAX_CONC_SOSCKETS; i++) {
                if (ktp_arr[i].udp_fd < 0 || !FD_ISSET(ktp_arr[i].udp_fd, &read_fds))
                    continue;
                
                char packet[PACKET_SIZE];
                struct sockaddr_in sender_addr;
                socklen_t addr_len = sizeof(sender_addr);
                
                ssize_t received = recvfrom(ktp_arr[i].udp_fd, packet, sizeof(packet), 0,
                                            (struct sockaddr*)&sender_addr, &addr_len);
                if (received <= 0)
                    continue;
                
                // Simulate packet loss
                if (dropMessage(P)) {
                    continue; // Drop packet
                }
                
                // Extract header and message
                struct ktp_header pkt_header;
                char message[MSSG_SIZE + 1];
                extract_pkt(packet, &pkt_header, message);
                
                if (pkt_header.is_ack) {
                    // ACK message handling
                    // Check if the ACK is for the outstanding packet.
                    // We assume swnd.base holds the sequence number of the packet currently in flight.
                    if (pkt_header.ack_num == ktp_arr[i].swnd.base) {
                        // Valid in-order ACK received:
                        // Clear the outstanding packet (set swnd.base to 0) and update next sequence.
                        ktp_arr[i].swnd.base = 0;
                        // Update next sequence number: if ack is 255 then next becomes 1.
                        ktp_arr[i].swnd.next_seq_num = (pkt_header.ack_num % 255) + 1;
                        ktp_arr[i].last_ack_sent = pkt_header.ack_num;
                    } else {
                        // Out-of-order or duplicate ACK: ignore.
                    }
                } else {
                    // Data message handling
                    // Accept the packet only if its sequence number matches the expected sequence.
                    if (pkt_header.seq_num == ktp_arr[i].rwnd.next_expected_seq) {
                        // Enqueue the message in the receiver buffer.
                        if (enqueue(&ktp_arr[i].recv_buf, message, pkt_header.seq_num)) {
                            // Send ACK for this packet.
                            send_ack(ktp_arr[i].udp_fd, &ktp_arr[i], pkt_header.seq_num);
                            // Update expected sequence number (wrap from 255 to 1)
                            ktp_arr[i].rwnd.next_expected_seq = (pkt_header.seq_num % 255) + 1;
                        }
                    } else {
                        // Out-of-order data packet: drop it.
                        // send an ACK for the last in-order packet,
                        uint8_t last_in_order;
                        if (ktp_arr[i].rwnd.next_expected_seq == 1)
                            last_in_order = 255;
                        else
                            last_in_order = ktp_arr[i].rwnd.next_expected_seq - 1;
                        send_ack(ktp_arr[i].udp_fd, &ktp_arr[i], last_in_order);
                    }
                }
            }
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
            
            // Case 1: Outstanding packet exists. Check for timeout.
            if (sock->swnd.base != 0) {
                double elapsed = (current_time.tv_sec - sock->last_send_time.tv_sec) +
                                 (current_time.tv_usec - sock->last_send_time.tv_usec) / 1000000.0;
                if (elapsed >= T) {
                    // Retransmit the outstanding message.
                    // Retrieve the pending message from the front of the send buffer.
                    // We assume the message is stored at index send_buf.head.
                    char *message = sock->send_buf._buf[sock->send_buf.head];
                    
                    // Build the packet using the outstanding packet's sequence number.
                    struct ktp_header header;
                    header.seq_num = sock->swnd.base;
                    header.is_ack = 0;
                    header.rwnd_size = WINDOW_SIZE - sock->recv_buf.count;
                    
                    char *pkt = pkt_create(header, message);
                    
                    // Prepare destination address.
                    struct sockaddr_in dest_addr;
                    memset(&dest_addr, 0, sizeof(dest_addr));
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(sock->des_port);
                    inet_pton(AF_INET, sock->des_ip, &dest_addr.sin_addr);
                    
                    // Retransmit the packet.
                    sendto(sock->udp_fd, pkt, PACKET_SIZE, 0,
                           (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                    
                    // Update send timestamp.
                    gettimeofday(&sock->last_send_time, NULL);
                    
                    free(pkt);
                }
            }
            // Case 2: No outstanding packet. Check if there is a pending message.
            else {
                if (!isEmpty(&sock->send_buf)) {
                    // Peek at the first pending message in the send buffer.
                    char *message = sock->send_buf._buf[sock->send_buf.head];

                    
                    // Build packet using the current next sequence number.
                    struct ktp_header header;
                    header.seq_num = sock->swnd.next_seq_num;
                    header.is_ack = 0;
                    header.rwnd_size = WINDOW_SIZE - sock->recv_buf.count;
                    
                    char *pkt = pkt_create(header, message);
                    
                    // Prepare destination address.
                    struct sockaddr_in dest_addr;
                    memset(&dest_addr, 0, sizeof(dest_addr));
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(sock->des_port);
                    inet_pton(AF_INET, sock->des_ip, &dest_addr.sin_addr);
                    
                    printf("here\n");
                    // Send the packet.
                    if(sendto(sock->udp_fd, pkt, PACKET_SIZE, 0,
                           (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0){

                        printf("%d %s\n", ktp_arr[i].udp_fd, strerror(errno));

                    }
                    
                    // Mark this packet as outstanding.
                    sock->swnd.base = header.seq_num;
                    // Update next sequence number with wrap-around (1-255).
                    sock->swnd.next_seq_num = (header.seq_num % 255) + 1;
                    
                    // Record the send time.
                    gettimeofday(&sock->last_send_time, NULL);
                    
                    free(pkt);
                }
            }
        }
    }
    return NULL;
}


void custom_exit(int status){

    for(int i=0; i<MAX_CONC_SOSCKETS; i++){
    
        close(udp_sock_fds[i]);

    }

    shmdt(ktp_arr);
    shmctl(shmid, IPC_RMID, NULL);

    shmdt(udp_sock_fds);
    shmctl(shmid_udp_sock, IPC_RMID, NULL);
    
    exit(status);
}

void intialise_array(){
    for(int i=0; i<MAX_CONC_SOSCKETS; i++){
        
        initialise_shm_ele(&ktp_arr[i]);

    }
    return;
}

void sigint_handler(int signum) {
    printf("\nSIGINT received. Cleaning up and terminating.\n");
    custom_exit(0);
}

void set_udp_sock_fds(){
    for(int i=0; i<MAX_CONC_SOSCKETS; i++){
        udp_sock_fds[i] = socket(AF_INET, SOCK_DGRAM, 0);
    }
}


int main(){

    signal(SIGINT, sigint_handler);
    pthread_t send_thread, recv_thread;

    // Shared memory of KTP ARRAY of KTP SOCKET STRUCTURES
    shmkey = ftok("/", 'A');
    shmid = shmget(shmkey, MAX_CONC_SOSCKETS * sizeof(struct ktp_sockaddr), 0777|IPC_CREAT);
    ktp_arr = (struct ktp_sockaddr*) shmat(shmid, NULL, 0);
    
    // Shared Memory for the UDP socket file descriptors
    shmkey_udp_sock = ftok("/", 'B');
    shmid_udp_sock = shmget(shmkey, MAX_CONC_SOSCKETS * sizeof(int), 0777|IPC_CREAT);
    udp_sock_fds = (int *) shmat(shmid_udp_sock, NULL, 0);

    
    if (ktp_arr == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }

    intialise_array();
    
    if(pthread_create(&recv_thread, NULL, R, NULL)){
        printf("Error creating thread R\n");
        custom_exit(1);
    }
    
    if(pthread_create(&send_thread, NULL, S, NULL)){
        printf("Error creating thread S\n");
        custom_exit(1);
    }

    // main thread checks 

    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    custom_exit(0);
}
