// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define MAX_CLIENTS 256
#define BUFFER_SIZE 1024
#define NAME_SIZE 32
#define SERVER_PORT 5208

typedef struct {
    char name[NAME_SIZE];
    int socket;
    bool in_use;
} Client;

// Global variables
Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;

// Function declarations
void *handle_client(void *arg);
void send_message(const char *message, int sender_sock);
void send_private_message(const char *message, const char *recipient, int sender_sock);
void remove_client(int sock);
int find_client_by_name(const char *name);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pthread_t thread_id;

    // Initialize client array
    memset(clients, 0, sizeof(clients));

    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_sock, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Chat server started on port %d\n", SERVER_PORT);

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_sock == -1) {
            perror("Accept failed");
            continue;
        }

        printf("New connection from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));

        // Create thread for new client
        if (pthread_create(&thread_id, NULL, handle_client, (void*)(intptr_t)client_sock) != 0) {
            perror("Thread creation failed");
            close(client_sock);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_sock);
    return 0;
}

void *handle_client(void *arg) {
    int sock = (intptr_t)arg;
    char buffer[BUFFER_SIZE];
    char name[NAME_SIZE];
    int read_len;
    bool is_first_message = true;

    while ((read_len = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_len] = '\0';

        if (is_first_message) {
            // First message should be "#new client:name"
            if (strncmp(buffer, "#new client:", 11) == 0) {
                strncpy(name, buffer + 11, NAME_SIZE - 1);
                name[NAME_SIZE - 1] = '\0';

                pthread_mutex_lock(&clients_mutex);
                
                // Check if name already exists
                if (find_client_by_name(name) != -1) {
                    char error_msg[] = "Name already exists. Please choose another name.";
                    send(sock, error_msg, strlen(error_msg), 0);
                    pthread_mutex_unlock(&clients_mutex);
                    close(sock);
                    return NULL;
                }

                // Add new client to array
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (!clients[i].in_use) {
                        clients[i].socket = sock;
                        strncpy(clients[i].name, name, NAME_SIZE - 1);
                        clients[i].in_use = true;
                        client_count++;
                        break;
                    }
                }
                
                pthread_mutex_unlock(&clients_mutex);
                
                // Announce new client
                char welcome[BUFFER_SIZE];
                snprintf(welcome, BUFFER_SIZE, "Client %s has joined the chat", name);
                send_message(welcome, sock);
                
                is_first_message = false;
                continue;
            }
        }

        // The message format is already correct from the client
        // Just forward it to send_message which will handle the parsing
        send_message(buffer, sock);
    }

    // Client disconnected
    pthread_mutex_lock(&clients_mutex);
    remove_client(sock);
    pthread_mutex_unlock(&clients_mutex);
    
    close(sock);
    return NULL;
}



void send_message(const char *message, int sender_sock) {
    pthread_mutex_lock(&clients_mutex);
    
    // Find sender's name
    char sender_name[NAME_SIZE] = "";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].socket == sender_sock) {
            strcpy(sender_name, clients[i].name);
            break;
        }
    }
    
    // Check if it's a private message
    if (message[0] == '@') {  // Message starts with @
        char temp_message[BUFFER_SIZE];
        strcpy(temp_message, message);
        
        char *recipient = strtok(temp_message + 1, " ");  // Skip @ and get recipient name
        if (recipient) {
            char *private_msg = strchr(message, ' ');
            if (private_msg) {
                private_msg++; // Skip the space
                
                // Find recipient's socket
                int recipient_sock = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].in_use && strcmp(clients[i].name, recipient) == 0) {
                        recipient_sock = clients[i].socket;
                        break;
                    }
                }
                
                if (recipient_sock != -1) {
                    // Format private message
                    char private_full_msg[BUFFER_SIZE];
                    snprintf(private_full_msg, BUFFER_SIZE, "[%s][Private] %s", sender_name, private_msg);
                    
                    // Send to recipient
                    send(recipient_sock, private_full_msg, strlen(private_full_msg), 0);
                    // Send copy to sender
                    send(sender_sock, private_full_msg, strlen(private_full_msg), 0);
                } else {
                    char error_msg[BUFFER_SIZE];
                    snprintf(error_msg, BUFFER_SIZE, "User %s not found", recipient);
                    send(sender_sock, error_msg, strlen(error_msg), 0);
                }
            }
        }
    } else {
        // Format full message with sender name for broadcast
        char full_message[BUFFER_SIZE];
        snprintf(full_message, BUFFER_SIZE, "[%s] %s", sender_name, message);
        
        // Broadcast message to all clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].in_use) {
                send(clients[i].socket, full_message, strlen(full_message), 0);
            }
        }
    }
    
    pthread_mutex_unlock(&clients_mutex);
}
void send_private_message(const char *message, const char *recipient, int sender_sock) {
    pthread_mutex_lock(&clients_mutex);
    
    int recipient_sock = -1;
    const char *sender_name = "";
    
    // Find sender's name
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].socket == sender_sock) {
            sender_name = clients[i].name;
            break;
        }
    }
    
    // Find recipient's socket
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && strcmp(clients[i].name, recipient) == 0) {
            recipient_sock = clients[i].socket;
            break;
        }
    }
    
    if (recipient_sock != -1) {
        char full_message[BUFFER_SIZE];
        snprintf(full_message, BUFFER_SIZE, "[%s][Private] %s", sender_name, message);
        
        // Send to recipient
        send(recipient_sock, full_message, strlen(full_message), 0);
        // Send copy to sender
        send(sender_sock, full_message, strlen(full_message), 0);
    } else {
        char error_msg[BUFFER_SIZE];
        snprintf(error_msg, BUFFER_SIZE, "User %s not found", recipient);
        send(sender_sock, error_msg, strlen(error_msg), 0);
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int sock) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].socket == sock) {
            char leave_msg[BUFFER_SIZE];
            snprintf(leave_msg, BUFFER_SIZE, "Client %s has left the chat", clients[i].name);
            
            clients[i].in_use = false;
            client_count--;
            
            // Announce departure to remaining clients
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].in_use) {
                    send(clients[j].socket, leave_msg, strlen(leave_msg), 0);
                }
            }
            break;
        }
    }
}

int find_client_by_name(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && strcmp(clients[i].name, name) == 0) {
            return clients[i].socket;
        }
    }
    return -1;
}