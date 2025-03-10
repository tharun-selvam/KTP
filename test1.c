#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"

#define EOF_MARKER "###EOF###"

// Usage: ./test1 <IP_1> <Port_1> <IP_2> <Port_2> <filename>
int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <IP_1> <Port_1> <IP_2> <Port_2> <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *local_ip = argv[1];
    int local_port = atoi(argv[2]);
    char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);
    char *filename = argv[5];

    printf("Starting sender with local %s:%d and remote %s:%d\n",
           local_ip, local_port, remote_ip, remote_port);

    // 1. Create a KTP socket
    int sock_fd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock_fd < 0) {
        fprintf(stderr, "Error creating KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(EXIT_FAILURE);
    }

    // 2. Bind the socket to (IP_1, Port_1) with remote (IP_2, Port_2)
    if (k_bind(sock_fd, local_ip, local_port, remote_ip, remote_port) < 0) {
        fprintf(stderr, "Error binding KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(EXIT_FAILURE);
    }
    
    // 3. Prepare the destination address for k_sendto
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(remote_port);
    inet_pton(AF_INET, remote_ip, &dest_addr.sin_addr);

    // 4. Open the file to be sent
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    printf("Opened file '%s' for reading.\n", filename);

    // 5. Read and send the file in 512-byte chunks
    char buffer[MSSG_SIZE];  // MSSG_SIZE = 512
    size_t bytes_read = 0;
    int chunk_count = 0;
    
    while ((bytes_read = fread(buffer, 1, MSSG_SIZE, fp)) > 0) {
        chunk_count++;
        printf("Chunk %d: Read %zu bytes from file.\n", chunk_count, bytes_read);
        
        // If fewer than 512 bytes were read, pad with zeros.
        if (bytes_read < MSSG_SIZE) {
            memset(buffer + bytes_read, 0, MSSG_SIZE - bytes_read);
            printf("Chunk %d: Padded with %d zero bytes.\n", chunk_count, (int)(MSSG_SIZE - bytes_read));
        }
        
        // Attempt to send exactly MSSG_SIZE bytes.
        ssize_t sent = k_sendto(sock_fd, buffer, MSSG_SIZE, 0,
                                (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        int attempt = 1;
        while (sent < 0) {
            printf("Chunk %d: Send buffer full. Attempt %d: Waiting for buffer to clear...\n", chunk_count, attempt);
            sleep(1);
            attempt++;
            sent = k_sendto(sock_fd, buffer, MSSG_SIZE, 0,
                            (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
    
        printf("Chunk %d: Message sent successfully (%zu bytes).\n", chunk_count, (size_t)sent);
        printf("Chunk %d content:\n|%s|\n\n", chunk_count, buffer);
    }
    
    // 6. Send an EOF marker after file transfer is complete
    printf("Sending EOF marker: %s\n", EOF_MARKER);
    k_sendto(sock_fd, EOF_MARKER, strlen(EOF_MARKER) + 1, 0,
            (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    printf("File '%s' sent successfully in %d chunks.\n", filename, chunk_count);

    fclose(fp);

    // Wait for user input before closing (for debugging)
    printf("Press any key to close the socket and exit...\n");
    char c;
    scanf(" %c", &c);

    // 7. Close the KTP socket
    k_close(sock_fd);

    return 0;
}
