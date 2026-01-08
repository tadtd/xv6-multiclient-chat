/*
 * Event-Driven Multiclient Chat Server for xv6
 * 
 * This server uses a single-threaded event loop with the net_poll system call
 * to handle multiple concurrent client connections efficiently without
 * spawning separate processes for each client.
 */

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/spinlock.h"
#include "kernel/socket.h"
#include "user/user.h"

#define MAX_CLIENTS     14      // Maximum concurrent clients (NSOCK - 2 for server sockets)
#define BUF_SIZE        512     // Message buffer size
#define SERVER_HOST     "0.0.0.0"
#define SERVER_PORT     80      // Default chat server port

// Client state tracking
struct client {
    int fd;                     // Socket file descriptor (-1 if unused)
    int active;                 // Is this client slot active?
    char name[32];              // Client nickname
    uint32 addr;                // Client IP address
    uint16 port;                // Client port
};

// Global state
static struct client clients[MAX_CLIENTS];
static int server_sock = -1;
static int num_clients = 0;

// Initialize client array
void init_clients(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].active = 0;
        clients[i].name[0] = '\0';
    }
}

// Find an empty client slot
int find_empty_slot(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            return i;
        }
    }
    return -1;
}

// Find client by fd
int find_client_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

// Remove a client
void remove_client(int slot) {
    if (slot < 0 || slot >= MAX_CLIENTS || !clients[slot].active) {
        return;
    }
    
    printf("chatserver: client '%s' disconnected (slot %d, fd %d)\n", 
           clients[slot].name[0] ? clients[slot].name : "unknown", 
           slot, clients[slot].fd);
    
    close(clients[slot].fd);
    clients[slot].fd = -1;
    clients[slot].active = 0;
    clients[slot].name[0] = '\0';
    num_clients--;
}

// Broadcast a message to all connected clients except the sender
void broadcast_message(const char *msg, int len, int sender_slot) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && i != sender_slot) {
            int sent = write(clients[i].fd, msg, len);
            if (sent < 0) {
                printf("chatserver: failed to send to client %d\n", i);
            }
        }
    }
}

// Handle a new incoming connection
void handle_new_connection(void) {
    struct sockaddr client_addr;
    int addr_len;
    
    int client_fd = accept(server_sock, &client_addr, &addr_len);
    if (client_fd < 0) {
        printf("chatserver: accept failed\n");
        return;
    }
    
    int slot = find_empty_slot();
    if (slot < 0) {
        printf("chatserver: server full, rejecting connection\n");
        char *msg = "Server is full. Please try again later.\n";
        write(client_fd, msg, strlen(msg));
        close(client_fd);
        return;
    }
    
    // Initialize client slot
    clients[slot].fd = client_fd;
    clients[slot].active = 1;
    clients[slot].addr = client_addr.sin_addr;
    clients[slot].port = client_addr.sin_port;
    
    // Generate default name
    char name_buf[32];
    // Simple name generation using slot number
    strcpy(name_buf, "user");
    // Convert slot to string and append
    char slot_str[8];
    int temp = slot;
    int j = 0;
    do {
        slot_str[j++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    slot_str[j] = '\0';
    // Reverse the string
    for (int k = 0; k < j / 2; k++) {
        char c = slot_str[k];
        slot_str[k] = slot_str[j - 1 - k];
        slot_str[j - 1 - k] = c;
    }
    // Append to name
    int name_len = strlen(name_buf);
    for (int k = 0; k <= j; k++) {
        name_buf[name_len + k] = slot_str[k];
    }
    strcpy(clients[slot].name, name_buf);
    
    num_clients++;
    
    printf("chatserver: new client connected (slot %d, fd %d)\n", slot, client_fd);
    
    // Send welcome message
    char welcome[128];
    strcpy(welcome, "Welcome to xv6 Chat Server! Your name is: ");
    int wlen = strlen(welcome);
    for (int k = 0; clients[slot].name[k]; k++) {
        welcome[wlen++] = clients[slot].name[k];
    }
    welcome[wlen++] = '\n';
    welcome[wlen] = '\0';
    write(client_fd, welcome, wlen);
    
    // Broadcast join message
    char join_msg[128];
    strcpy(join_msg, "*** ");
    int jlen = 4;
    for (int k = 0; clients[slot].name[k]; k++) {
        join_msg[jlen++] = clients[slot].name[k];
    }
    char *joined = " has joined the chat ***\n";
    for (int k = 0; joined[k]; k++) {
        join_msg[jlen++] = joined[k];
    }
    join_msg[jlen] = '\0';
    broadcast_message(join_msg, jlen, slot);
}

// Handle incoming data from a client
void handle_client_data(int slot) {
    char buf[BUF_SIZE];
    int n = read(clients[slot].fd, buf, BUF_SIZE - 1);
    
    if (n <= 0) {
        // Client disconnected or error
        char leave_msg[128];
        strcpy(leave_msg, "*** ");
        int llen = 4;
        for (int k = 0; clients[slot].name[k]; k++) {
            leave_msg[llen++] = clients[slot].name[k];
        }
        char *left = " has left the chat ***\n";
        for (int k = 0; left[k]; k++) {
            leave_msg[llen++] = left[k];
        }
        leave_msg[llen] = '\0';
        
        int slot_save = slot;
        remove_client(slot);
        broadcast_message(leave_msg, llen, slot_save);
        return;
    }
    
    buf[n] = '\0';
    
    // Check for /name command to change nickname
    if (n > 6 && buf[0] == '/' && buf[1] == 'n' && buf[2] == 'a' && 
        buf[3] == 'm' && buf[4] == 'e' && buf[5] == ' ') {
        char old_name[32];
        strcpy(old_name, clients[slot].name);
        
        // Extract new name (skip "/name " and remove trailing newline)
        int new_len = 0;
        for (int i = 6; i < n && buf[i] != '\n' && buf[i] != '\r' && new_len < 31; i++) {
            clients[slot].name[new_len++] = buf[i];
        }
        clients[slot].name[new_len] = '\0';
        
        // Broadcast name change
        char name_msg[128];
        strcpy(name_msg, "*** ");
        int mlen = 4;
        for (int k = 0; old_name[k]; k++) {
            name_msg[mlen++] = old_name[k];
        }
        char *changed = " is now known as ";
        for (int k = 0; changed[k]; k++) {
            name_msg[mlen++] = changed[k];
        }
        for (int k = 0; clients[slot].name[k]; k++) {
            name_msg[mlen++] = clients[slot].name[k];
        }
        name_msg[mlen++] = ' ';
        name_msg[mlen++] = '*';
        name_msg[mlen++] = '*';
        name_msg[mlen++] = '*';
        name_msg[mlen++] = '\n';
        name_msg[mlen] = '\0';
        broadcast_message(name_msg, mlen, -1);  // Send to everyone including sender
        return;
    }
    
    // Check for /list command to list connected users
    if (n >= 5 && buf[0] == '/' && buf[1] == 'l' && buf[2] == 'i' && 
        buf[3] == 's' && buf[4] == 't') {
        char list_msg[512];
        strcpy(list_msg, "Connected users:\n");
        int llen = strlen(list_msg);
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                list_msg[llen++] = ' ';
                list_msg[llen++] = '-';
                list_msg[llen++] = ' ';
                for (int k = 0; clients[i].name[k]; k++) {
                    list_msg[llen++] = clients[i].name[k];
                }
                if (i == slot) {
                    char *you = " (you)";
                    for (int k = 0; you[k]; k++) {
                        list_msg[llen++] = you[k];
                    }
                }
                list_msg[llen++] = '\n';
            }
        }
        list_msg[llen] = '\0';
        write(clients[slot].fd, list_msg, llen);
        return;
    }
    
    // Regular message - broadcast to all clients
    char broadcast[BUF_SIZE + 64];
    broadcast[0] = '[';
    int blen = 1;
    for (int k = 0; clients[slot].name[k]; k++) {
        broadcast[blen++] = clients[slot].name[k];
    }
    broadcast[blen++] = ']';
    broadcast[blen++] = ' ';
    
    // Copy message, handling newline
    for (int i = 0; i < n; i++) {
        broadcast[blen++] = buf[i];
    }
    // Ensure message ends with newline
    if (n > 0 && buf[n-1] != '\n') {
        broadcast[blen++] = '\n';
    }
    broadcast[blen] = '\0';
    
    printf("chatserver: %s", broadcast);
    broadcast_message(broadcast, blen, slot);
}

// Build the poll fd array
int build_poll_array(struct pollfd *fds) {
    int count = 0;
    
    // Add server socket (for new connections)
    fds[count].fd = server_sock;
    fds[count].events = POLLIN;
    fds[count].revents = 0;
    count++;
    
    // Add all active client sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            fds[count].fd = clients[i].fd;
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }
    }
    
    return count;
}

int main(int argc, char *argv[]) {
    printf("===========================================\n");
    printf("   xv6 Event-Driven Multiclient Chat Server\n");
    printf("===========================================\n");
    
    // Initialize client tracking
    init_clients();
    
    // Set up server address
    struct sockaddr serv_addr = {
        .sa_family = AF_INET,
        .sin_port = SERVER_PORT,
    };
    inetaddress(SERVER_HOST, &serv_addr);
    
    printf("chatserver: binding to %s:%d\n", SERVER_HOST, SERVER_PORT);
    
    // Create server socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        printf("chatserver: failed to create socket\n");
        exit(1);
    }
    
    // Bind to address
    if (bind(server_sock, &serv_addr, sizeof(serv_addr)) < 0) {
        printf("chatserver: failed to bind\n");
        close(server_sock);
        exit(1);
    }
    
    // Start listening
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        printf("chatserver: failed to listen\n");
        close(server_sock);
        exit(1);
    }
    
    printf("chatserver: listening for connections...\n");
    printf("chatserver: commands - /name <newname>, /list\n");
    printf("-------------------------------------------\n");
    
    // Main event loop
    struct pollfd fds[MAX_CLIENTS + 1];
    
    while (1) {
        // Build poll array with current sockets
        int nfds = build_poll_array(fds);
        
        // Poll for activity (block indefinitely with timeout = -1)
        int ready = net_poll(fds, nfds, -1);
        
        if (ready < 0) {
            printf("chatserver: poll error\n");
            continue;
        }
        
        if (ready == 0) {
            // Timeout (shouldn't happen with -1 timeout)
            continue;
        }
        
        // Process events
        int idx = 0;
        
        // Check server socket first (new connections)
        if (fds[idx].revents & POLLIN) {
            handle_new_connection();
        }
        if (fds[idx].revents & (POLLERR | POLLHUP)) {
            printf("chatserver: server socket error\n");
            break;
        }
        idx++;
        
        // Check client sockets
        for (int i = 0; i < MAX_CLIENTS && idx < nfds; i++) {
            if (!clients[i].active) continue;
            
            // Find this client's entry in fds array
            int found = 0;
            for (int j = idx; j < nfds; j++) {
                if (fds[j].fd == clients[i].fd) {
                    if (fds[j].revents & POLLIN) {
                        handle_client_data(i);
                    }
                    if (fds[j].revents & (POLLERR | POLLHUP)) {
                        // Client disconnected
                        char leave_msg[128];
                        strcpy(leave_msg, "*** ");
                        int llen = 4;
                        for (int k = 0; clients[i].name[k]; k++) {
                            leave_msg[llen++] = clients[i].name[k];
                        }
                        char *left = " has left the chat ***\n";
                        for (int k = 0; left[k]; k++) {
                            leave_msg[llen++] = left[k];
                        }
                        leave_msg[llen] = '\0';
                        remove_client(i);
                        broadcast_message(leave_msg, llen, -1);
                    }
                    found = 1;
                    break;
                }
            }
            if (found) idx++;
        }
    }
    
    // Cleanup
    printf("chatserver: shutting down...\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            close(clients[i].fd);
        }
    }
    close(server_sock);
    
    exit(0);
}
