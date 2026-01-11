# Non-Blocking Socket API Quick Reference

## Constants

```c
// File flags
#define O_NONBLOCK  0x800    // Non-blocking I/O flag

// fcntl commands
#define F_GETFL     3        // Get file status flags
#define F_SETFL     4        // Set file status flags

// Error codes
#define EAGAIN      11       // Resource temporarily unavailable
#define EWOULDBLOCK EAGAIN   // Same as EAGAIN
```

## System Calls

### fcntl() - File Control

```c
int fcntl(int fd, int cmd, ...);
```

**Get flags:**
```c
int flags = fcntl(sockfd, F_GETFL, 0);
```

**Set non-blocking:**
```c
fcntl(sockfd, F_SETFL, O_NONBLOCK);
```

**Clear non-blocking:**
```c
int flags = fcntl(sockfd, F_GETFL, 0);
fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);
```

### net_poll() - I/O Multiplexing

```c
int net_poll(struct pollfd *fds, int nfds, int timeout);
```

**Example:**
```c
struct pollfd fds[2];
fds[0].fd = sockfd;
fds[0].events = POLLIN | POLLOUT;
fds[0].revents = 0;

int ready = net_poll(fds, 1, 1000);  // 1 second timeout
if (ready > 0) {
    if (fds[0].revents & POLLIN) {
        // Can read without blocking
    }
    if (fds[0].revents & POLLOUT) {
        // Can write without blocking
    }
}
```

## Non-Blocking I/O Patterns

### Pattern 1: Simple Event Loop

```c
// Set non-blocking
fcntl(sockfd, F_SETFL, O_NONBLOCK);

while (running) {
    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    
    if (net_poll(fds, 1, 1000) > 0) {
        char buf[512];
        int n = read(sockfd, buf, sizeof(buf));
        if (n > 0) {
            // Process data
        } else if (n == 0) {
            // EOF
            break;
        }
        // n < 0 and EAGAIN - just continue
    }
}
```

### Pattern 2: Multiple Sockets

```c
struct pollfd fds[MAX_CLIENTS];
int nfds = 0;

// Add server socket
fds[nfds].fd = server_fd;
fds[nfds].events = POLLIN;
nfds++;

// Add client sockets
for (int i = 0; i < num_clients; i++) {
    fds[nfds].fd = client_fds[i];
    fds[nfds].events = POLLIN;
    nfds++;
}

// Poll all sockets
int ready = net_poll(fds, nfds, -1);

// Check server socket
if (fds[0].revents & POLLIN) {
    int client_fd = accept(server_fd, &addr, &len);
    if (client_fd >= 0) {
        // New connection
    }
}

// Check client sockets
for (int i = 1; i < nfds; i++) {
    if (fds[i].revents & POLLIN) {
        // Client has data
    }
}
```

### Pattern 3: Bidirectional Communication

```c
struct pollfd fds[2];

while (1) {
    // Monitor stdin and socket
    fds[0].fd = 0;  // stdin
    fds[0].events = POLLIN;
    
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;
    
    int ready = net_poll(fds, 2, -1);
    
    if (fds[0].revents & POLLIN) {
        // User input available
        char buf[512];
        int n = read(0, buf, sizeof(buf));
        if (n > 0) {
            write(sockfd, buf, n);
        }
    }
    
    if (fds[1].revents & POLLIN) {
        // Server data available
        char buf[512];
        int n = read(sockfd, buf, sizeof(buf));
        if (n > 0) {
            write(1, buf, n);  // stdout
        }
    }
}
```

## Return Values

### read()
- `n > 0`: Bytes read
- `n == 0`: EOF (connection closed)
- `n < 0`: Error (check for EAGAIN in non-blocking)

### write()
- `n > 0`: Bytes written (may be partial)
- `n < 0`: Error (check for EAGAIN in non-blocking)

### accept()
- `fd >= 0`: New connection file descriptor
- `fd < 0`: Error (check for EAGAIN in non-blocking)

### net_poll()
- `n > 0`: Number of ready file descriptors
- `n == 0`: Timeout
- `n < 0`: Error

## Poll Events

```c
#define POLLIN   0x001  // Data available to read
#define POLLOUT  0x004  // Writing won't block
#define POLLERR  0x008  // Error condition
#define POLLHUP  0x010  // Hung up
#define POLLNVAL 0x020  // Invalid FD
```

## Error Handling

```c
int n = read(sockfd, buf, sizeof(buf));
if (n < 0) {
    // In production, would check errno
    // For now, assume EAGAIN
    // Try again later
} else if (n == 0) {
    // Connection closed
    close(sockfd);
} else {
    // Process n bytes
}
```

## Common Mistakes

❌ **Don't:**
```c
// Busy loop - wastes CPU
while (1) {
    int n = read(sockfd, buf, sizeof(buf));
    if (n > 0) process(buf, n);
}
```

✅ **Do:**
```c
// Use poll to wait efficiently
while (1) {
    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    
    if (net_poll(fds, 1, -1) > 0) {
        int n = read(sockfd, buf, sizeof(buf));
        if (n > 0) process(buf, n);
    }
}
```

## Complete Example (from chat_server.c)

```c
#include "kernel/types.h"
#include "kernel/socket.h"
#include "user/user.h"

int main() {
    // Create server socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr addr = { /* ... */ };
    bind(server_sock, &addr, sizeof(addr));
    listen(server_sock, MAX_CLIENTS);
    
    // Set non-blocking
    fcntl(server_sock, F_SETFL, O_NONBLOCK);
    
    // Event loop
    struct pollfd fds[MAX_CLIENTS + 1];
    
    while (1) {
        // Setup poll array
        fds[0].fd = server_sock;
        fds[0].events = POLLIN;
        // ... add client sockets
        
        int ready = net_poll(fds, nfds, -1);
        
        if (ready > 0) {
            // Non-blocking accept
            if (fds[0].revents & POLLIN) {
                int client_fd = accept(server_sock, &addr, &len);
                if (client_fd >= 0) {
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);
                    // Add to client list
                }
            }
            
            // Non-blocking read from clients
            for (int i = 1; i < nfds; i++) {
                if (fds[i].revents & POLLIN) {
                    char buf[512];
                    int n = read(fds[i].fd, buf, sizeof(buf));
                    if (n > 0) {
                        // Process message
                    } else if (n == 0) {
                        // Client disconnected
                        close(fds[i].fd);
                    }
                    // n < 0: EAGAIN - ignore
                }
            }
        }
    }
    
    close(server_sock);
    exit(0);
}
```

## See Also

- NON_BLOCKING.md - Full documentation
- IMPLEMENTATION_SUMMARY.md - Technical details
- user/chat_server.c - Production example
