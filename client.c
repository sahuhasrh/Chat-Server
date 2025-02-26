// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define NAME_SIZE 32
#define SERVER_PORT 5208
#define SERVER_IP "127.0.0.1"

void *send_message(void *arg);
void *receive_message(void *arg);

char name[NAME_SIZE];
char message[BUFFER_SIZE];

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server_addr;
    pthread_t send_thread, receive_thread;

    if (argc != 2) {
        printf("Usage: %s <name>\n", argv[0]);
        exit(1);
    }

    // Set client name
    snprintf(name, sizeof(name), "%s", argv[1]);

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send initial name message
    char name_msg[NAME_SIZE + 12];
    snprintf(name_msg, sizeof(name_msg), "#new client:%s", name);
    send(sock, name_msg, strlen(name_msg), 0);

    printf("Connected to chat server\n");
    printf("Commands:\n");
    printf("- Type 'quit' to exit\n");
    printf("- Type '@username message' to send private message\n");
    printf("- Type your message and press enter to send to everyone\n\n");

    // Create threads for sending and receiving messages
    pthread_create(&send_thread, NULL, send_message, (void*)(intptr_t)sock);
    pthread_create(&receive_thread, NULL, receive_message, (void*)(intptr_t)sock);

    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(sock);
    return 0;
}

void *send_message(void *arg) {
    int sock = (intptr_t)arg;
    char message[BUFFER_SIZE];

    while (1) {
        fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = 0; // Remove newline

        if (strcmp(message, "quit") == 0) {
            close(sock);
            exit(0);
        }

        send(sock, message, strlen(message), 0);
    }
    return NULL;
}

void *receive_message(void *arg) {
    int sock = (intptr_t)arg;
    char message[BUFFER_SIZE];
    int str_len;

    while (1) {
        str_len = recv(sock, message, BUFFER_SIZE - 1, 0);
        if (str_len <= 0) {
            printf("Disconnected from server\n");
            exit(1);
        }
        message[str_len] = '\0';
        printf("%s\n", message);
    }
    return NULL;
}