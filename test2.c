#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"

#define EOF_MARKER "###EOF###"

// Usage: ./test2 <IP_2> <Port_2> <IP_1> <Port_1> <out_filename>
int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <IP_2> <Port_2> <IP_1> <Port_1> <out_filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *local_ip = argv[1];
    int local_port = atoi(argv[2]);
    char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);
    char *out_filename = argv[5];

    printf("Starting receiver with local %s:%d and remote %s:%d\n",
           local_ip, local_port, remote_ip, remote_port);

    // 1. Create a KTP socket
    int sock_fd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock_fd < 0) {
        fprintf(stderr, "Error creating KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(EXIT_FAILURE);
    }

    // 2. Bind the socket to (IP_2, Port_2) with remote (IP_1, Port_1)
    if (k_bind(sock_fd, local_ip, local_port, remote_ip, remote_port) < 0) {
        fprintf(stderr, "Error binding KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(EXIT_FAILURE);
    }
    
    // 3. Open the output file for writing
    FILE *fp = fopen(out_filename, "w");
    if (!fp) {
        perror("fopen");
        k_close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("Opened output file '%s' for writing.\n", out_filename);

    // 4. Continuously receive 512-byte chunks until EOF marker is found
    char buffer[MSSG_SIZE + 1]; // +1 for null terminator
    int chunk_count = 0;
    
    while (1) {
        ssize_t recvd = k_recvfrom(sock_fd, buffer, MSSG_SIZE, 0, NULL, NULL);
        if (recvd < 0) {
            fprintf(stderr, "Error receiving data: %s\n", getCustomErrorMessage(global_err_var));
            break;
        }
        buffer[recvd] = '\0';  // Ensure null termination
        
        chunk_count++;
        printf("Chunk %d: Received %zd bytes.\n", chunk_count, recvd);
        printf("Chunk %d content:\n|%s|\n\n", chunk_count, buffer);

        // Check if this chunk is the EOF marker
        if (strcmp(buffer, EOF_MARKER) == 0) {
            printf("Received EOF marker. Transfer complete.\n");
            break;
        }

        // Write the chunk to the output file
        size_t written = fwrite(buffer, 1, recvd, fp);
        if (written != (size_t)recvd) {
            fprintf(stderr, "Error writing to file.\n");
            break;
        }
    }

    fclose(fp);
    k_close(sock_fd);

    printf("File received and saved as '%s' in %d chunks.\n", out_filename, chunk_count);
    return 0;
}
