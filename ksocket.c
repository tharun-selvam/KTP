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
    memset(ca->seq_nums, 0, WINDOW_SIZE * sizeof(uint8_t));
}

int isFull(struct data_buffer *ca) {
    return ca->count == WINDOW_SIZE;
}

int isEmpty(struct data_buffer *ca) {
    return ca->count == 0;
}

int enqueue(struct data_buffer *ca, char* mssg, uint8_t seq_num) {
    if (isFull(ca)) {
        return 0;
    }
    strcpy(ca->_buf[ca->tail], mssg);
    ca->seq_nums[ca->tail] = seq_num;  // Store sequence number
    ca->tail = (ca->tail + 1) % WINDOW_SIZE;
    ca->count++;
    return 1;
}

int dequeue(struct data_buffer *ca, char *mssg) {
    if (isEmpty(ca)) {
        return 0; 
    }
    if (mssg != NULL) {  // Allow NULL for just removing entry
        strcpy(mssg, ca->_buf[ca->head]);
    }
    ca->head = (ca->head + 1) % WINDOW_SIZE;
    ca->count--;
    return 1;
}

uint8_t get_seq_num(struct data_buffer *ca, int index) {
    if (index >= ca->count) return 0;
    int actual_index = (ca->head + index) % WINDOW_SIZE;
    return ca->seq_nums[actual_index];
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
            curr = (curr + 1)%WINDOW_SIZE;
        }
    }
}

void application_print(int ktp_socket){
 
    // access SHM
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();

    print_buff(&ktp_arr[ktp_socket].send_buf);

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

        default:
            return "Unknown error";
    }
}

/* Helper Functions */
struct ktp_sockaddr* open_ktp_arr(){
    int shmkey = ftok("/", 'A');
    int shmid = shmget(shmkey, MAX_CONC_SOSCKETS * sizeof(struct ktp_sockaddr), 0777);
    struct ktp_sockaddr* ktp_arr= (struct ktp_sockaddr*)shmat(shmid, 0, 0);

    return ktp_arr;
}

char* pkt_create(struct ktp_header header, char* mssg){
    size_t mssglen = strlen(mssg);
    size_t total_packet_size = sizeof(header) + strlen(mssg);
    char *packet = (char *)malloc(total_packet_size);
    memcpy(packet, &header, sizeof(header));
    memcpy(packet + sizeof(header), mssg, mssglen);

    return packet;
}

void extract_pkt(char* packet, struct ktp_header *header, char* mssg){

    memcpy(header, packet, sizeof(struct ktp_header));
    memcpy(mssg, packet + sizeof(struct ktp_header), MSSG_SIZE);
    mssg[MSSG_SIZE] = '\0';
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

    // check if space available for new socket
    int flag = 0;
    int i = 0;
    for(i=0; i<MAX_CONC_SOSCKETS; i++){

        if(ktp_arr[i].udp_fd == -1){
            // free socket available
            flag = 1;

            // set process id
            ktp_arr[i].process_id = getpid();

            // setting udp socket num in the SHM
            ktp_arr[i].udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

            // setting seq_num
            ktp_arr[i].last_ack_sent = -1;

            // init send and recv buffers
            initCircularArray(&ktp_arr[i].send_buf);
            initCircularArray(&ktp_arr[i].recv_buf);

            // set other fields
            // ktp_arr[i].next_expected_seq = 1;
            ktp_arr[i].nospace_flag = 0;
            ktp_arr[i].last_ack_sent = -1;

            // init swnd
            ktp_arr[i].swnd.base = 0;
            ktp_arr[i].swnd.next_seq_num = 1;
            ktp_arr[i].swnd.window_size = WINDOW_SIZE;
            memset(&ktp_arr[i].swnd.sent_seq_nums, 0, sizeof(ktp_arr[i].swnd.sent_seq_nums));
            
            // init rwnd
            ktp_arr[i].rwnd.base = 1;
            ktp_arr[i].rwnd.window_size = WINDOW_SIZE;
            ktp_arr[i].rwnd.next_expected_seq = 1;
            memset(&ktp_arr[i].rwnd.received_seq_nums, 0, sizeof(ktp_arr[i].rwnd.received_seq_nums));

            memset(&ktp_arr[i].last_send_time, 0, sizeof(struct timeval));
            memset(&ktp_arr[i].swnd.send_times, 0, sizeof(ktp_arr[i].swnd.send_times));

            if(ktp_arr[i].udp_fd < 0){
                setCustomError(ESOCKCREAT);
            }

            break;
        }

    }
    if(!flag){
        setCustomError(ENOSPACE);
    }

    shmdt(ktp_arr);

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
    
    // call UDP bind()
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(src_port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock->udp_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))){
        shmdt(ktp_arr);
        setCustomError(EBIND);
        return -1;
    }


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

    // acces KTP socket
    struct ktp_sockaddr *sock = &ktp_arr[socket];

    // get bounded IP as char * (as given in ktp_arr)
    if(strcmp(sock->des_ip, des_ip) || sock->des_port != des_port){
        setCustomError(ENOTBOUND);
        shmdt(ktp_arr);
        return -1;
    }

    // Create packet with sequence number
    struct ktp_header header;
    header.seq_num = sock->swnd.next_seq_num++;  // Increment sequence number
    header.is_ack = 0;
    header.rwnd_size = WINDOW_SIZE - sock->recv_buf.count;

    char *packet = pkt_create(header, (char *)buffer);

    // Add to send buffer with sequence number
    if(enqueue(&sock->send_buf, (char *)buffer, header.seq_num) == 0) {
        free(packet);
        shmdt(ktp_arr);
        setCustomError(ENOSPACE);
        return -1;
    }


    shmdt(ktp_arr);
    
    return strlen(((char*)buffer));
}

void initialise_shm_ele(struct ktp_sockaddr* ele){
    ele->udp_fd = -1;
    ele->last_ack_sent = -1;

    ele->process_id = -1;
    ele->udp_fd = -1;
    
    ele->des_port = -1;
    ele->des_ip[0] = '\0';

    ele->src_port = -1;
    ele->src_ip[0] = '\0';

    initCircularArray(&ele->send_buf);
    initCircularArray(&ele->recv_buf);
    
    ele->last_ack_sent = -1;

    // ele->next_expected_seq = -1;
}

int k_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len){

    // SHM access
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();

    if(dequeue(&ktp_arr[socket].recv_buf, (char *)buffer) == 0){

        setCustomError(ENOMESSAGE);
        shmdt(ktp_arr);

        return -1;
    }

    shmdt(ktp_arr);
    
    return strlen(buffer);
}

int k_close(int socket){
    // SHM access
    struct ktp_sockaddr* ktp_arr = open_ktp_arr();

    close(ktp_arr[socket].udp_fd);

    initialise_shm_ele(&ktp_arr[socket]);

    shmdt(ktp_arr);
    return 0;
}