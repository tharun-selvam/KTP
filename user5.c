#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"

int main() {
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0) {
        printf("User5: Error creating KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Bind: local IP 127.0.0.1:9000, remote IP 127.0.0.1:10000
    if (k_bind(sock, "127.0.0.1", 9000, "127.0.0.1", 10000) < 0) {
        printf("User5: Error binding KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Set destination address for sending messages to user6
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(10000);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);
    
    int counter = 1;
    char message[256];
    while (1) {
        snprintf(message, sizeof(message), "User5 Message %d", counter);
        int sent = k_sendto(sock, message, strlen(message) + 1, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (sent < 0) {
            printf("User5: Error sending message: %s\n", getCustomErrorMessage(global_err_var));
        } else {
            printf("User5: Sent: %s\n", message);
        }
        counter++;
        sleep(1);
    }
    
    k_close(sock);
    return 0;
}
