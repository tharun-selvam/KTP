#ifndef KSOCKET_H
#define KSOCKET_H

#include<sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <sys/time.h>
#include <semaphore.h>


#define WINDOW_SIZE 1
#define BUFFER_SIZE 10
#define MSSG_SIZE 512
#define SOCK_KTP 12345
#define HEADER_SIZE 4
#define PACKET_SIZE (MSSG_SIZE+HEADER_SIZE)
#define MAX_SEQ_NUM 255

/* Macros for bind_status used in ktp_arr */
#define UNBINDED -1
#define AWAIT_BIND -2
#define BINDED -3

#define MAX_CONC_SOSCKETS 10

#define T 5

typedef enum {
    CUSTOM_ERROR_NONE = 0,
    
    // socket errors
    ENOSPACE,
    EAFNOSUPPORT_,
    ESOCKTYPE,
    ENOPROTO,
    ESOCKCREAT,

    // k_bind
    EBIND,

    // k_sendto
    ENOTBOUND,

    // k_recvfrom
    ENOMESSAGE,

    // recv_buff full
    RECV_BUFF_FULL,

} CustomErrorCode;

struct ktp_sockaddr;


static CustomErrorCode global_err_var;
const char* getCustomErrorMessage(CustomErrorCode error);
void setCustomError(CustomErrorCode error);
CustomErrorCode getCustomError(void);

struct data_buffer {
    char _buf[BUFFER_SIZE][MSSG_SIZE+1];
    int head;
    int tail;
    int count;
};

// QUEUE Functions
void initCircularArray(struct data_buffer *ca);
int isFull(struct data_buffer *ca);
int isEmpty(struct data_buffer *ca);
int enqueue(struct data_buffer *ca, char* mssg);
int dequeue(struct data_buffer *ca, char *mssg);
void print_buff(struct data_buffer *ca);
void application_print(int ktp_socket);

struct swnd {
    uint8_t base;           // For our design: 0 indicates no outstanding packet.
    uint8_t next_seq_num;   // Next sequence number to use.
    uint8_t window_size;    // This is less relevant now (still defined as WINDOW_SIZE).
    uint8_t sent_seq_nums[WINDOW_SIZE]; // Unused in our one-packet model.
    struct timeval send_times[WINDOW_SIZE]; // Unused in our one-packet model.
};

struct rwnd {
    uint8_t base;           // Base of receiving window
    uint8_t received_seq_nums[WINDOW_SIZE]; // Array of received sequence numbers
    uint8_t next_expected_seq;
    uint8_t window_size;    // window size is a fixed value and does not chanhge
    int seq_nums_map[MAX_SEQ_NUM+1];   // Array containing info of whether a pkt with the said sequence number has arrived
    char stash_buffer[MAX_SEQ_NUM+1][MSSG_SIZE+1];    // Array that stashes out of order pkts

    int free_space;         // 0-WINDOW_SIZE. Initial value is WINDOW_SIZE. 
    uint8_t last_ack_sent;
};

int update_rwnd(struct rwnd * rwnd, int recvd_pkt_num, char *mssg, struct ktp_sockaddr* sock);

struct ktp_sockaddr{
    int process_id;
    int udp_fd;
    
    int des_port;
    char des_ip[INET_ADDRSTRLEN];

    int src_port;
    char src_ip[INET_ADDRSTRLEN];

    struct data_buffer send_buf;
    struct data_buffer recv_buf;

    struct swnd swnd;
    struct rwnd rwnd;

    struct timeval last_send_time;
    int nospace_flag;

    int bind_status;
};

#pragma pack(1)
struct ktp_header {
    uint8_t seq_num;        // Sequence number
    uint8_t ack_num;        // Acknowledgment number
    uint8_t is_ack;         // Flag to indicate if packet is an ACK
    uint8_t rwnd_size;      // Receiver's window size
};
#pragma pack()

// Helper Functions
void initialise_shm_ele(struct ktp_sockaddr* ele);
struct ktp_sockaddr* open_ktp_arr();
int* open_udp_arr();
char* pkt_create(struct ktp_header header, char* mssg);
void extract_pkt(char* packet, struct ktp_header *header, char* mssg);
void print_header(struct ktp_header *header);

// Semaphore Functions
#define SEM_NAME "/mysemaphore"
sem_t *sem;
void init_semaphore(sem_t *sem);

// K_SOCKET Functions
int k_socket(int domain, int type, int protocol);
int k_bind(int sock_fd, char *src_ip, int src_port, char *des_ip, int des_port);
int k_sendto(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
int k_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address,
    socklen_t *restrict address_len);
int k_close(int socket);

#endif