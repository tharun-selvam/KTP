#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"

#define NUM 30

int main() {
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0) {
        printf("Error creating KTP socket\n%s", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Bind the KTP socket: local IP 127.0.0.1:5000, remote IP 127.0.0.1:6000
    if (k_bind(sock, "127.0.0.1", 5000, "127.0.0.1", 6000) < 0) {
        printf("Error binding KTP socket: %s\n", getCustomErrorMessage(global_err_var));
        exit(1);
    }
    
    // Prepare the destination address structure
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(6000);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);
    
    // Array of 16 messages
    char *messages[NUM];
    for (int i = 0; i < NUM; i++) {
        char temp[100];
        sprintf(temp, "Message %d: Extra message %d", i + 1, i + 1);
        messages[i] = strdup(temp);
    }
    
    // Print the initial state of the send buffer.
    application_print(sock);
    
    // Send each message one by one
    for (int i = 0; i < NUM; i++) {
        int sent = k_sendto(sock, messages[i], strlen(messages[i]) + 1, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        int attempt = 1;
        // If k_sendto returns -1, wait until the send buffer clears and retry.
        while (sent < 0) {
            printf("Error sending message %d: %s. Attempt %d: Waiting for send buffer to clear...\n",
                   i + 1, getCustomErrorMessage(global_err_var), attempt);
            sleep(1);
            attempt++;
            sent = k_sendto(sock, messages[i], strlen(messages[i]) + 1, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        }
        printf("Sent message %d: %s\n", i + 1, messages[i]);
    }
    
    // Wait a bit to allow ACK and transmission logic to complete.
    sleep(3);
    
    char c;
    printf("Press any key to exit...\n");
    scanf(" %c", &c);
    
    k_close(sock);
    return 0;
}
