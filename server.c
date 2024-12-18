#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/select.h> // Required for fd_set and select

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define PORT 8080

typedef struct {
    char username[50];
    int room_id;
    int socket_fd; // Use int instead of SOCKET
    int authenticated;
} Client;

typedef struct {
    char name[50];
    int client_fds[MAX_CLIENTS];
    int client_count;
} ChatRoom;

atomic_int shutting_down = 0; // Global flag for shutdown
ChatRoom chat_rooms[MAX_CLIENTS];
int chat_room_count = 0;

Client clients[MAX_CLIENTS];
int client_count = 0;

int server_fd;
fd_set master_set, read_fds;
int fd_max;

// Helper Functions
void cleanup_and_exit();
void handle_signal(int sig);
void accept_new_connection();
void handle_client_message(int fd, char *message);

// Signal handler for clean shutdown
void handle_signal(int sig) {
    printf("\nServer shutting down...\n");
    atomic_store(&shutting_down, 1);
    cleanup_and_exit();
    exit(0);
}

// Cleanup resources on shutdown
void cleanup_and_exit() {
    for (int i = 0; i < client_count; i++) {
        close(clients[i].socket_fd); // Use close instead of closesocket
        FD_CLR(clients[i].socket_fd, &master_set);
    }
    close(server_fd); // Use close instead of closesocket
    printf("Resources cleaned up.\n");
}

// Setup server socket
int setup_server_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}

// Accept new client connection
void accept_new_connection() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (new_fd < 0) {
        perror("Accept failed");
        return;
    }

    if (client_count >= MAX_CLIENTS) {
        printf("Too many clients connected. Rejecting connection.\n");
        close(new_fd);
        return;
    }

    FD_SET(new_fd, &master_set);
    if (new_fd > fd_max) {
        fd_max = new_fd;
    }

    clients[client_count].socket_fd = new_fd;
    clients[client_count].room_id = -1;
    clients[client_count].authenticated = 0;
    client_count++;

    printf("New client connected: FD %d\n", new_fd);
}
int authenticate_user(const char *username, const char *password) {
    // Example logic for username/password validation
    return strcmp(username, "admin") == 0 && strcmp(password, "password") == 0;
}

int find_or_create_chat_room(const char *name) {
    for (int i = 0; i < chat_room_count; i++) {
        if (strcmp(chat_rooms[i].name, name) == 0) {
            return i; // Room found
        }
    }

    if (chat_room_count < MAX_CLIENTS) {
        strcpy(chat_rooms[chat_room_count].name, name);
        chat_rooms[chat_room_count].client_count = 0;
        return chat_room_count++; // Room created
    }

    return -1; // Room creation failed (limit reached)
}

void broadcast_to_room(int room_id, const char *message, int exclude_fd) {
    if (room_id < 0 || room_id >= chat_room_count) {
        return; // Invalid room ID
    }

    ChatRoom *room = &chat_rooms[room_id];
    for (int i = 0; i < room->client_count; i++) {
        int client_fd = room->client_fds[i];
        if (client_fd != exclude_fd) {
            if (send(client_fd, message, strlen(message), 0) == -1) {
                perror("Broadcast send failed");
            }
        }
    }
}

void remove_from_room(int room_id, int fd) {
    if (room_id < 0 || room_id >= chat_room_count) {
        return; // Invalid room ID
    }

    ChatRoom *room = &chat_rooms[room_id];
    for (int i = 0; i < room->client_count; i++) {
        if (room->client_fds[i] == fd) {
            for (int j = i; j < room->client_count - 1; j++) {
                room->client_fds[j] = room->client_fds[j + 1];
            }
            room->client_count--;
            break;
        }
    }
}

// Handle messages from clients
void handle_client_message(int fd, char *message) {
    char command[BUFFER_SIZE], arg1[BUFFER_SIZE], arg2[BUFFER_SIZE];
    int num_args = sscanf(message, "%s %s %s", command, arg1, arg2);

    // Trim newline characters from the message
    char *newline = strchr(message, '\n');
    if (newline) *newline = '\0';
    newline = strchr(message, '\r');
    if (newline) *newline = '\0';

    // Check the received command
    if (strcmp(command, "login") == 0 && num_args == 3) {
        // Login functionality
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket_fd == fd) {
                if (authenticate_user(arg1, arg2)) {
                    strcpy(clients[i].username, arg1);
                    clients[i].authenticated = 1;
                    send(fd, "Login successful\n", 17, 0);
                } else {
                    send(fd, "Login failed\n", 13, 0);
                }
                return;
            }
        }
    } else if (strcmp(command, "create") == 0 && num_args == 2) {
        // Create chat room functionality
        int room_id = find_or_create_chat_room(arg1);
        if (room_id >= 0) {
            send(fd, "Room created\n", 13, 0);
        } else {
            send(fd, "Room creation failed\n", 22, 0);
        }
    } else if (strcmp(command, "enter") == 0 && num_args == 2) {
        // Enter chat room functionality
        int room_id = find_or_create_chat_room(arg1);
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket_fd == fd && clients[i].authenticated) {
                if (clients[i].room_id != -1) {
                    remove_from_room(clients[i].room_id, fd);
                }
                clients[i].room_id = room_id;
                chat_rooms[room_id].client_fds[chat_rooms[room_id].client_count++] = fd;
                send(fd, "Entered room\n", 13, 0);
                broadcast_to_room(room_id, "A new user has joined the room\n", fd);
                return;
            }
        }
    } else {
        // Handle generic messages after the client has entered a room
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket_fd == fd) {
                if (!clients[i].authenticated) {
                    send(fd, "You must log in to send messages\n", 33, 0);
                    return;
                }

                if (clients[i].room_id != -1) { // Check if the client is in a room
                    int room_id = clients[i].room_id;

                    // Avoid broadcasting empty messages
                    if (strlen(message) == 0) {
                        send(fd, "Cannot send empty messages\n", 27, 0);
                        return;
                    }

                    char broadcast_message[BUFFER_SIZE];
                    snprintf(broadcast_message, BUFFER_SIZE, "%s: %s", clients[i].username, message);

                    // Debug: Log the broadcast message
                    printf("Broadcasting message to room %d: %s\n", room_id, broadcast_message);

                    broadcast_to_room(room_id, broadcast_message, fd);
                    return;
                } else {
                    send(fd, "You must join a room to send messages\n", 37, 0);
                    return;
                }
            }
        }

        // If no specific handler matches
        send(fd, "Unknown command\n", 17, 0);
    }
}

int main() {
    signal(SIGINT, handle_signal);

    FD_ZERO(&master_set);
    FD_ZERO(&read_fds);

    server_fd = setup_server_socket();
    FD_SET(server_fd, &master_set);
    fd_max = server_fd;

    printf("Server running on port %d\n", PORT);

    while (!atomic_load(&shutting_down)) {
        read_fds = master_set;

        if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (atomic_load(&shutting_down)) break;
            perror("Select error");
            cleanup_and_exit();
        }

        for (int i = 0; i <= fd_max; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
                    accept_new_connection();
                } else {
                    char buffer[BUFFER_SIZE];
                    memset(buffer, 0, BUFFER_SIZE);
                    int bytes_read = recv(i, buffer, BUFFER_SIZE - 1, 0);
                    if (bytes_read <= 0) {
                        close(i);
                        FD_CLR(i, &master_set);
                    } else {
                        buffer[bytes_read] = '\0';
                        handle_client_message(i, buffer);
                    }
                }
            }
        }
    }

    cleanup_and_exit();
    return 0;
}
