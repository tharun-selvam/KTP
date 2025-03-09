#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"
#include <signal.h>

int sock;
void sigHandler(int signal){
    if(signal == SIGINT){
        k_close(sock);
        exit(0);
    }   
}

int main() {

    signal(SIGINT, sigHandler);

    sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0) {
        printf("User2: Error creating KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Bind: local IP 127.0.0.1:6000, remote IP 127.0.0.1:5000
    if (k_bind(sock, "127.0.0.1", 6000, "127.0.0.1", 5000) < 0) {
        printf("User2: Error binding KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    char buffer[MSSG_SIZE + 1];
    while (1) {
        int recvd = k_recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (recvd < 0) {
            printf("User2: Error receiving message: %s\n", getCustomErrorMessage(global_err_var));
        } else {
            printf("User2: Received: %s\n", buffer);
        }
    }
    
    k_close(sock);
    return 0;
}
