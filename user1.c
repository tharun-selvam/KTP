#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"

int main() {
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0) {
        printf("Error creating KTP socket\n");
        exit(1);
    }
    
    // Bind the KTP socket: local IP 127.0.0.1:5000, remote IP 127.0.0.1:6000
    if (k_bind(sock, "127.0.0.1", 5000, "127.0.0.1", 6000) < 0) {
        printf("Error binding KTP socket\n");
        exit(1);
    }
    
    // Prepare the destination address structure
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(6000);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);
    
    // Three messages to send
    char *messages[3] = {
        "Message 1: Hello from User1",
        "Message 2: How are you?",
        "Message 3: Goodbye!"
    };
    
    for (int i = 0; i < 3; i++) {
        int sent = k_sendto(sock, messages[i], strlen(messages[i]) + 1, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (sent < 0) {
            printf("Error sending message %d\n", i + 1);
        } else {
            printf("Sent: %s\n", messages[i]);
        }

        application_print(sock);
        // Wait a bit to allow the ACK and transmission logic to complete
        sleep(2);
    }
    
    k_close(sock);
    return 0;
}
