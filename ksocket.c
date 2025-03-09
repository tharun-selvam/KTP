#include <stdlib.h>
#include "ksocket.h"
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

// QUEUE Functions
void initCircularArray(struct data_buffer *ca) {
    ca->head = 0;
    ca->tail = 0;
    ca->count = 0;

}

int isFull(struct data_buffer *ca) {
    return ca->count == BUFFER_SIZE;
}

int isEmpty(struct data_buffer *ca) {
    return ca->count == 0;
}

int enqueue(struct data_buffer *ca, char* mssg) {
    if (isFull(ca)) {
        return 0;
    }
    printf("Before enque\n\t\thead: %d\n\t\ttail: %d\n\t\tsize: %d\n", ca->head, ca->tail, ca->count);
    
    strncpy(ca->_buf[ca->tail], mssg, MSSG_SIZE);
    ca->_buf[ca->tail][MSSG_SIZE] = '\0';

    ca->tail = (ca->tail + 1) % BUFFER_SIZE;
    ca->count++;
    printf("After enque\n\t\thead: %d\n\t\ttail: %d\n\t\tsize: %d\n", ca->head, ca->tail, ca->count);

    return 1;
}

int dequeue(struct data_buffer *ca, char *mssg) {
    if (isEmpty(ca)) {
        return 0; 
    }
    if (mssg != NULL) {  // Allow NULL for just removing entry
        strcpy(mssg, ca->_buf[ca->head]);
    }
    ca->head = (ca->head + 1) % BUFFER_SIZE;
    ca->count--;
    return 1;
}

int sizeOfCircularArray(struct data_buffer *ca){
    return ca->count;
}

int getKMessages(struct data_buffer* ca, int k, char ***message_buf) {
    /* 
     * This function retrieves the topmost k messages from the circular buffer 'ca'
     * without removing them (i.e. it does not dequeue the messages).
     * Returns number of messages loaded. -1 if error
     */
    
    // Allocate memory for k pointers to message strings.
    *message_buf = (char **)malloc(sizeof(char *) * k);
    if (!(*message_buf)) {
        setCustomError(ERR_ALLOCATING);
        return -1;
    }
    
    // Determine the actual number of messages available (cannot exceed the current count).
    int num_messages = (k < ca->count) ? k : ca->count;
    
    // Start from the head of the circular buffer.
    int index = ca->head;
    for (int i = 0; i < num_messages; i++) {
        // Allocate memory for each message string (plus one byte for the null terminator).
        (*message_buf)[i] = (char *)malloc(sizeof(char) * (MSSG_SIZE + 1));
        if (!(*message_buf)[i]) {
            // In case of allocation failure, free previously allocated memory.
            for (int j = 0; j < i; j++) {
                free((*message_buf)[j]);
            }
            free((*message_buf));
            return -1;
        }
        // Copy the message from the circular buffer to the allocated space.
        strncpy((*message_buf)[i], ca->_buf[index], MSSG_SIZE);
        // Ensure null termination.
        (*message_buf)[i][MSSG_SIZE] = '\0';
        
        // Move to the next message in the circular buffer, wrapping around if necessary.
        index = (index + 1) % BUFFER_SIZE;
    }

    return num_messages;
}


void print_buff(struct data_buffer *ca){
    if(isEmpty(ca)){
        printf("Empty buffer\n");
    }
    else{
        int curr = ca->head;
        int i=0;
        while(curr != ca->tail){

            printf("Mssg %d:\n%s\n", i, ca->_buf[curr]);

            i++;
            curr = (curr + 1)%BUFFER_SIZE;
        }
    }
}

void application_print(int ktp_socket){
 
    // access SHM
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();

    init_semaphore(sem);
    sem_wait(sem);
    print_buff(&ktp_arr[ktp_socket].send_buf);
    sem_post(sem);

    shmdt(ktp_arr);
}


// error handling
void setCustomError(CustomErrorCode error) {
    global_err_var = error;
}

CustomErrorCode getCustomError(void) {
    return global_err_var;
}

const char* getCustomErrorMessage(CustomErrorCode error) {
    switch (error) {

        case CUSTOM_ERROR_NONE:
            return "No error";

        // k_socket()
        case ENOSPACE:
            return "No space in SHM to create a new KTP socket";

        case EAFNOSUPPORT_:
            return "The specified address family is not supported";

        case ESOCKTYPE:
            return "The specifed socket type is not supported";

        case ENOPROTO:
            return "The specifed protocol type is not supported";

        case ESOCKCREAT:
            return "UDP socket creation failed";

        // k_bind
        case EBIND:
            return "Error binding the UDP socket";
        
        // k_sendto
        case ENOTBOUND:
            return "Destination IP/Port not matching with Bounded IP/Port";

        // k_recvfrom
        case ENOMESSAGE:
            return "No message in recv buffer";

        case RECV_BUFF_FULL:
            return "Enque in recv_buff not possible due to full buffer";

        case ERR_ALLOCATING:
            return "Memory could not be allocated.";

        default:
            return "Unknown error";
    }
}

/* Helper Functions */
struct ktp_sockaddr* open_ktp_arr(){
    int shmkey = ftok("/", 'A');
    int shmid = shmget(shmkey, MAX_CONC_SOSCKETS * sizeof(struct ktp_sockaddr), 0777);
    struct ktp_sockaddr* ktp_arr= (struct ktp_sockaddr*)shmat(shmid, 0, 0);
    if (ktp_arr == (void*)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }

    return ktp_arr;
}

int* open_udp_arr(){
    int shmkey = ftok("/", 'B');
    int shmid = shmget(shmkey, MAX_CONC_SOSCKETS * sizeof(int), 0777);
    int* udp_arr = (int*)shmat(shmid, 0, 0);
    if (udp_arr == (void*)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }
    return udp_arr;
}

char* pkt_create(struct ktp_header header, char* mssg){
    size_t mssglen = strlen(mssg);
    size_t total_packet_size = sizeof(header) + strlen(mssg) + 1;
    char *packet = (char *)malloc(total_packet_size);
    memcpy(packet, &header, sizeof(header));
    memcpy(packet + sizeof(header), mssg, mssglen);

    printf("Packet inside: |%s|\n", packet + sizeof(header));

    return packet;
}

void extract_pkt(char* packet, struct ktp_header *header, char* mssg){

    memcpy(header, packet, sizeof(struct ktp_header));
    memcpy(mssg, packet + sizeof(struct ktp_header), MSSG_SIZE);
    mssg[MSSG_SIZE] = '\0';
}

void print_header(struct ktp_header *header){
    printf("--- Header\n");
    printf("\tIs Ack: %d\n", header->is_ack);
    printf("\tAck Num: %d\n", header->ack_num);
    printf("\tSeq Num: %d\n", header->seq_num);
    printf("\trwnd size: %d\n", header->rwnd_size);

    return;
}


// ktp network functions
int k_socket(int domain, int type, int protocol){

    if(type != SOCK_KTP){
        setCustomError(ESOCKTYPE);
        return -1;
    }

    if(domain != AF_INET){
        setCustomError(EAFNOSUPPORT_);
        return -1;
    }

    if(protocol != 0){
        setCustomError(ENOPROTO);
    }
    
    // access SHM
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();
    int * udp_arr = open_udp_arr();

    // check if space available for new socket
    int flag = 0;
    int i = 0;
    for(i=0; i<MAX_CONC_SOSCKETS; i++){
        
        printf("%d ", ktp_arr[i].process_id);

        if(ktp_arr[i].process_id < 0){
            // free socket available
            flag = 1;

            printf("here\n");

            // set semaphore
            init_semaphore(sem);
            sem_wait(sem);

            // set process id
            ktp_arr[i].process_id = getpid();

            // setting udp socket num in the SHM
            ktp_arr[i].udp_fd = udp_arr[i];
            
            // setting bind_status
            ktp_arr[i].bind_status = UNBINDED;
            
            // init send and recv buffers
            initCircularArray(&ktp_arr[i].send_buf);
            initCircularArray(&ktp_arr[i].recv_buf);
            
            // set other fields
            // ktp_arr[i].next_expected_seq = 1;
            ktp_arr[i].nospace_flag = 0;
            
            // init swnd
            ktp_arr[i].swnd.base = 0;
            ktp_arr[i].swnd.next_seq_num = 1;
            ktp_arr[i].swnd.window_size = WINDOW_SIZE;
            ktp_arr[i].swnd.available_rwnd = WINDOW_SIZE;
            memset(&ktp_arr[i].swnd.sent_seq_nums, 1, sizeof(ktp_arr[i].swnd.sent_seq_nums));
            
            // init rwnd
            ktp_arr[i].rwnd.last_ack_sent = -1;
            ktp_arr[i].rwnd.base = 1;
            
            memset(&ktp_arr[i].last_send_time, 0, sizeof(struct timeval));
            memset(&ktp_arr[i].swnd.send_times, 0, sizeof(ktp_arr[i].swnd.send_times));

            ktp_arr[i].rwnd.free_space = WINDOW_SIZE;
            
            // initialise the expected seq numbers data structure
            ktp_arr[i].rwnd.window_size = WINDOW_SIZE;
            ktp_arr[i].rwnd.next_expected_seq = 1;
            memset(&ktp_arr[i].rwnd.seq_nums_map, 0, sizeof(ktp_arr[i].rwnd.seq_nums_map));

            if(ktp_arr[i].udp_fd < 0){
                setCustomError(ESOCKCREAT);
            }

            // signal semaphore
            sem_post(sem);
            sem_close(sem);

            break;
        }

    }
    if(!flag){
        setCustomError(ENOSPACE);
    }

    shmdt(ktp_arr);
    shmdt(udp_arr);

    if(!flag) return -1;
    return i;
}

int k_bind(int sock_fd, char *src_ip, int src_port, char *des_ip, int des_port){

    // access SHM
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();
    
    // acces KTP socket
    struct ktp_sockaddr *sock = &ktp_arr[sock_fd];
    
    // set ip and port to KTP socket
    strcpy(sock->src_ip, src_ip);
    sock->src_port = src_port;

    strcpy(sock->des_ip, des_ip);
    sock->des_port = des_port;
    
    // bind_status is set to AWAIT_BIND
    sock->bind_status = AWAIT_BIND;
    printf("Process %d asked to bind\n", getpid());

    shmdt(ktp_arr);
    return 0;
}

int k_sendto(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len){
    
    // get wanted destination IP as char* (as given in dest_addr)
    char des_ip[INET_ADDRSTRLEN];
    struct in_addr ip_addr = ((struct sockaddr_in*)dest_addr)->sin_addr;
    inet_ntop(AF_INET, &ip_addr, des_ip, INET_ADDRSTRLEN);

    // get the wanted destination Port (as given in dest_addr)
    int des_port = ntohs(((struct sockaddr_in*)dest_addr)->sin_port);

    // access SHM
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();

    // open semaphore
    init_semaphore(sem);
    sem_wait(sem);

    // acces KTP socket
    struct ktp_sockaddr *sock = &ktp_arr[socket];

    // get bounded IP as char * (as given in ktp_arr)
    if(strcmp(sock->des_ip, des_ip) || sock->des_port != des_port){
        setCustomError(ENOTBOUND);
        shmdt(ktp_arr);

        sem_post(sem);
        sem_close(sem);
        return -1;
    }


    // Add to send buffer with sequence number
    if(enqueue(&sock->send_buf, (char *)buffer) == 0) {
        shmdt(ktp_arr);
        setCustomError(ENOSPACE);

        sem_post(sem);
        sem_close(sem);
        return -1;
    }


    shmdt(ktp_arr);
    sem_post(sem);
    sem_close(sem);
    
    return strlen(((char*)buffer));
}

void initialise_shm_ele(struct ktp_sockaddr* ele){
    ele->udp_fd = -1;

    printf("here1\n");
    ele->process_id = -1;
    ele->udp_fd = -1;
    
    ele->des_port = -1;
    ele->des_ip[0] = '\0';

    ele->src_port = -1;
    ele->src_ip[0] = '\0';

    initCircularArray(&ele->send_buf);
    initCircularArray(&ele->recv_buf);
    

    // ele->next_expected_seq = -1;
    ele->bind_status = UNBINDED;
}

int k_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len){

    // SHM access
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();

    init_semaphore(sem);
    sem_wait(sem);

    int flag = 1;
    while(isEmpty(&ktp_arr[socket].recv_buf)){
        if(flag){
            flag = 0;
            printf("Waiting for a message in the receive buffer\n");
        }
    };

    if(dequeue(&ktp_arr[socket].recv_buf, (char *)buffer) == 0){

        setCustomError(ENOMESSAGE);
        shmdt(ktp_arr);

        sem_post(sem);
        return -1;
    }

    shmdt(ktp_arr);
    sem_post(sem);
    sem_close(sem);
    
    return strlen(buffer);
}

int k_close(int socket){
    // SHM access
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();


    initialise_shm_ele(&ktp_arr[socket]);

    shmdt(ktp_arr);
    return 0;
}

void init_semaphore(sem_t *sem){
    sem = sem_open(SEM_NAME, 0);
}


// rwnd functions
int update_rwnd(struct rwnd * rwnd, int recvd_pkt_num, char *mssg, struct ktp_sockaddr* sock){
    // Updates the rwnd when new pkts are received
    // Makes sure that only pkts within the window range is received
    // Out of order pkts are stashed in the stash_buffer
    // Returns updated window-size or -1 if err

    int lower_limit = rwnd->next_expected_seq;
    int upper_limit = lower_limit + rwnd->window_size - 1;

    if(upper_limit > MAX_SEQ_NUM){
        if(recvd_pkt_num >= 1 && recvd_pkt_num <= upper_limit % MAX_SEQ_NUM){
            recvd_pkt_num += MAX_SEQ_NUM;
        }
    }

    // pkt not in window range
    if(recvd_pkt_num < lower_limit || recvd_pkt_num > upper_limit){
        // send last ackd packet again with the same rwnd window size
        return -1;
    }

    // nexte_expected pkt has arrived
    if(recvd_pkt_num == lower_limit){
        // update window size appropriately


        // enqueue to recv_buf
        if ( enqueue(&sock->recv_buf, mssg) < 0){
            
            setCustomError(RECV_BUFF_FULL);
            return -1;

        }

        // unnecessary because lower_limit will always be in range [1, MAX_SEQ_NUM]
        if(recvd_pkt_num > MAX_SEQ_NUM) recvd_pkt_num = recvd_pkt_num % MAX_SEQ_NUM;

        // update window size
        rwnd->seq_nums_map[recvd_pkt_num] = 0;
        
        // iterate from lower_mit+1 and transfer from stash_buffer to enqueue buffer 
        int tmp = lower_limit % MAX_SEQ_NUM + 1;
        while(rwnd->seq_nums_map[tmp] == 1 && tmp <= (upper_limit % MAX_SEQ_NUM)){
            
            // move the mssg from stash buffer to main recv_buf 
            enqueue(&sock->recv_buf, rwnd->stash_buffer[tmp]);
            rwnd->stash_buffer[tmp][0] = '\0';

            rwnd->seq_nums_map[tmp] = 0;
            rwnd->free_space++;
            tmp = tmp % MAX_SEQ_NUM;
            tmp = tmp + 1;
        }

        rwnd->next_expected_seq = tmp;
        // rwnd->last_ack_sent = (tmp - 1) % MAX_SEQ_NUM;
        rwnd->last_ack_sent = (tmp == 1) ? MAX_SEQ_NUM : (tmp - 1);

    }
    else if (rwnd->seq_nums_map[recvd_pkt_num % MAX_SEQ_NUM] == 0){
        // out of order but within window pkt has arrived
    
        // update seq_nums_map and stash it in the buffer

        rwnd->seq_nums_map[recvd_pkt_num % MAX_SEQ_NUM] = 1;
        strncpy(rwnd->stash_buffer[recvd_pkt_num % MAX_SEQ_NUM], mssg, MSSG_SIZE);
        rwnd->stash_buffer[recvd_pkt_num % MAX_SEQ_NUM][MSSG_SIZE] = '\0';
        rwnd->free_space--;
    }

    return rwnd->free_space;
}

/*
    Add a parameter in rwnd to indicated how many more messages it can receive.
    Right now window_size param is kinda ambiguous

*/

void print_swnd(struct swnd *sw, struct data_buffer *buf) {
    if (!sw) {
        printf("Sender window pointer is NULL.\n");
        return;
    }
    
    printf("----- Sender Window State -----\n");
    printf("Base: %u\n", sw->base);
    printf("Next Sequence Number: %u\n", sw->next_seq_num);
    printf("Window Size: %u\n", sw->window_size);
    
    printf("\nSent Sequence Numbers:\n");
    // Loop through the entire array (MAX_SEQ_NUM+1 elements)
    for (int i = 0; i < MAX_SEQ_NUM + 1; i++) {
        printf("%3u ", sw->sent_seq_nums[i]);
        // Print a newline every 16 values for better readability
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");
    
    printf("\nSend Times (in seconds and microseconds):\n");
    // Since WINDOW_SIZE is the size of the send_times array, loop through each element.
    for (int i = 0; i < WINDOW_SIZE; i++) {
        printf("Packet %d: %ld sec, %ld usec\n", 
               i, 
               (long)sw->send_times[i].tv_sec, 
               (long)sw->send_times[i].tv_usec);
    }
    
    printf("\nAvailable Receiver Window: %d\n", sw->available_rwnd);
    print_buff(buf);
    printf("--------------------------------\n");
}