#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"

int main() {
    printf("PID: %d\n", getpid());
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0) {
        printf("Error creating KTP socket\n");
        exit(1);
    }
    
    // Bind the KTP socket: local IP 127.0.0.1:6000, remote IP 127.0.0.1:5000
    if (k_bind(sock, "127.0.0.1", 6000, "127.0.0.1", 5000) < 0) {
        printf("Error binding KTP socket\n");
        exit(1);
    }
    
    char buffer[MSSG_SIZE + 1];
    
    // Receive three messages
    for (int i = 0; i < 3; i++) {
        int recvd = k_recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (recvd < 0) {
            printf("Error receiving message %d\n%s\n", i + 1, getCustomErrorMessage(global_err_var));

        } else {
            printf("Received: %s\n", buffer);
        }
    }
    
    k_close(sock);
    return 0;
}
