#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ksocket.h"

#define NUM 10

int main() {
    int sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0) {
        printf("Error creating KTP socket\n");
        printf("%s", getCustomErrorMessage(global_err_var));
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
    char *messages[NUM] = {
        "Message 1: Hello from User1",
        "Message 2: How are you?",
        "Message 3: Goodbye!",
        "Message 4: Hope you're doing well.",
        "Message 5: Just checking in.",
        "Message 6: What's up?",
        "Message 7: Call me when you can.",
        "Message 8: I'll be waiting.",
        "Message 9: See you at the meeting.",
        "Message 10: Keep in touch."
    };
    
    
    application_print(sock);
    
    for (int i = 0; i < NUM; i++) {
        int sent = k_sendto(sock, messages[i], strlen(messages[i]) + 1, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (sent < 0) {
            printf("Error sending message %d\n", i + 1);
            printf("Error:\t%s\n", getCustomErrorMessage(global_err_var));
        } else {
            printf("Sent: %s\n", messages[i]);
            // application_print(sock);

        }
        // Wait a bit to allow the ACK and transmission logic to complete

        if(i == NUM-1){
            char c;
            break;
        }
    }
    
    sleep(3);
    for (int i = 0; i < NUM; i++) {
        int sent = k_sendto(sock, messages[i], strlen(messages[i]) + 1, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (sent < 0) {
            printf("Error sending message %d\n", i + 1);
            printf("Error:\t%s\n", getCustomErrorMessage(global_err_var));
        } else {
            printf("Sent: %s\n", messages[i]);
            // application_print(sock);

        }
        // Wait a bit to allow the ACK and transmission logic to complete

        if(i == NUM-1){
            char c;
            scanf(" %c", &c);
        }
    }
    
    k_close(sock);
    return 0;
}
