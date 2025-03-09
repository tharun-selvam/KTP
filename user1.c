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
        printf("User1: Error creating KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Bind: local IP 127.0.0.1:5000, remote IP 127.0.0.1:6000
    if (k_bind(sock, "127.0.0.1", 5000, "127.0.0.1", 6000) < 0) {
        printf("User1: Error binding KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Set destination address for sending messages to user2
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(6000);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);
    
    int counter = 1;
    char message[256];
    while (1) {
        snprintf(message, sizeof(message), "User1 Message %d", counter);
        int sent = k_sendto(sock, message, strlen(message) + 1, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (sent < 0) {
            printf("User1: Error sending message: %s\n", getCustomErrorMessage(global_err_var));
        } else {
            printf("User1: Sent: %s\n", message);
        }
        counter++;

        if(counter%3 == 0)
            sleep(5); // send one message per second
    }
    
    k_close(sock);
    return 0;
}
