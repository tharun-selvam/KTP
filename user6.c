#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"


int main() {
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0) {
        printf("User6: Error creating KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Bind: local IP 127.0.0.1:10000, remote IP 127.0.0.1:9000
    if (k_bind(sock, "127.0.0.1", 10000, "127.0.0.1", 9000) < 0) {
        printf("User6: Error binding KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    char buffer[MSSG_SIZE + 1];
    while (1) {
        int recvd = k_recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (recvd < 0) {
            printf("User6: Error receiving message: %s\n", getCustomErrorMessage(global_err_var));
        } else {
            printf("User6: Received: %s\n", buffer);
        }
    }
    
    k_close(sock);
    return 0;
}
