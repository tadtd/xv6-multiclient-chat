# Non-Blocking Socket Support for xv6 Multiclient Chat

This branch adds comprehensive non-blocking socket support to the xv6 operating system, enabling efficient event-driven I/O for network applications.

## Features Added

### 1. **fcntl() System Call**
A new system call for managing file descriptor flags:
- `F_GETFL` - Get file status flags
- `F_SETFL` - Set file status flags
- `O_NONBLOCK` - Non-blocking I/O flag

### 2. **Non-Blocking Socket Operations**
All socket operations now support non-blocking mode:
- **read()** - Returns `-1` with `EAGAIN` when no data available
- **write()** - Returns `-1` with `EAGAIN` when buffer full, or partial write count
- **accept()** - Returns `-1` with `EAGAIN` when no pending connections

### 3. **Event-Driven Programming Support**
Combined with the existing `net_poll()` system call, applications can now:
- Monitor multiple file descriptors efficiently
- Avoid blocking on I/O operations
- Build high-performance event-driven servers

## API Usage

### Setting Non-Blocking Mode

```c
#include "kernel/types.h"
#include "kernel/socket.h"
#include "user/user.h"

int sockfd = socket(AF_INET, SOCK_STREAM, 0);

// Get current flags
int flags = fcntl(sockfd, F_GETFL, 0);

// Set non-blocking mode
fcntl(sockfd, F_SETFL, O_NONBLOCK);
```

### Event Loop with net_poll()

```c
while (running) {
    struct pollfd fds[2];
    
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    
    fds[1].fd = client_fd;
    fds[1].events = POLLIN | POLLOUT;
    fds[1].revents = 0;
    
    int ready = net_poll(fds, 2, 1000);  // 1 second timeout
    
    if (ready > 0) {
        // Check which FDs are ready
        if (fds[0].revents & POLLIN) {
            // Accept new connection (non-blocking)
            int new_fd = accept(sockfd, &addr, &addrlen);
            if (new_fd < 0) {
                // EAGAIN means no pending connections
            }
        }
        
        if (fds[1].revents & POLLIN) {
            // Read available data (non-blocking)
            int n = read(client_fd, buf, sizeof(buf));
            if (n < 0) {
                // EAGAIN means no data available
            }
        }
    }
}
```

### Error Handling

```c
int n = read(sockfd, buf, sizeof(buf));
if (n < 0) {
    // Check errno via proc->error_no
    // In real usage, errno would be checked
    printf("Read would block (EAGAIN)\n");
} else if (n == 0) {
    printf("Connection closed\n");
} else {
    printf("Read %d bytes\n", n);
}
```

## Demo Programs

### chat_server - Event-Driven Non-Blocking Chat Server

The main chat server demonstrates all non-blocking features in production use:

```bash
$ chat_server
```

**Features demonstrated:**
- Server socket in non-blocking mode
- Client sockets in non-blocking mode  
- Event-driven I/O with net_poll()
- Efficient handling of multiple concurrent clients
- Non-blocking accept, read, and write operations

The server uses:
1. **fcntl()** to set all sockets non-blocking
2. **net_poll()** for efficient event notification
3. **Non-blocking I/O** - never blocks on any operation

This is a production-quality implementation showing the power of combining event-driven architecture with non-blocking I/O.

## Implementation Details

### Kernel Changes

#### File Structure (`kernel/file.h`)
```c
struct file {
    // ... existing fields ...
    char nonblocking;  // Non-blocking mode flag
};
```

#### Process Structure (`kernel/proc.h`)
```c
struct proc {
    // ... existing fields ...
    int error_no;  // Error number (e.g., EAGAIN)
};
```

#### Socket Functions (`kernel/socket.c`)
- `sockread()` - Updated signature: `int sockread(struct socket*, uint64, int, char nonblocking)`
- `sockwrite()` - Updated signature: `int sockwrite(struct socket*, uint64, int, char nonblocking)`
- `sockaccept()` - Updated signature: `int sockaccept(int, struct sockaddr*, int*, char nonblocking)`

All functions check the `nonblocking` flag and return `EAGAIN` appropriately.

#### New System Call
- **System Call Number**: 33
- **Implementation**: `kernel/sysfile.c::sys_fcntl()`
- **User Wrapper**: `user/usys.pl`, `user/user.h`

### Constants Defined

#### `kernel/fcntl.h` and `user/user.h`
```c
#define O_NONBLOCK  0x800
#define F_GETFL     3
#define F_SETFL     4
#define EAGAIN      11
#define EWOULDBLOCK EAGAIN
```

## Testing

### Start the Chat Server in xv6
```bash
$ chat_server
```

### Connect Python Clients
```bash
python3 chat_client.py
```

### Expected Behavior

The chat server now uses **non-blocking + event-driven** architecture:

1. Server socket is non-blocking - accept() returns immediately
2. Client sockets are non-blocking - read/write return immediately
3. net_poll() efficiently waits for I/O events
4. Server handles multiple clients without blocking

## Advantages of Non-Blocking I/O

1. **Scalability**: Single process can handle multiple connections
2. **Responsiveness**: Applications remain responsive during I/O
3. **Efficiency**: No thread/process overhead per connection
4. **Control**: Fine-grained control over I/O operations

## Integration with Existing Code

The chat server (`chatserver.c`) already uses event-driven patterns with `net_poll()`. With non-blocking support:

1. Server socket can be set non-blocking for accept()
2. Client sockets can be set non-blocking for read/write
3. All I/O operations return immediately
4. net_poll() efficien_server.c`) now demonstrates the full power of non-blocking + event-driven I/O:

```c
// Set server socket non-blocking
fcntl(server_sock, F_SETFL, O_NONBLOCK);

// Set all client sockets non-blocking
fcntl(client_fd, F_SETFL, O_NONBLOCK);

// Event loop with net_poll()
while (1) {
    int ready = net_poll(fds, nfds, -1);
    
    // Non-blocking accept
    if (fds[0].revents & POLLIN) {
        int client_fd = accept(server_sock, ...);
        // Returns immediately, even if no connection
    }
    
    // Non-blocking read
    if (fds[i].revents & POLLIN) {
        int n = read(client_fd, buf, sizeof(buf));
        // Returns immediately with data or EAGAIN
    }
}
```
- [ ] Support for edge-triggered polling (EPOLLET)
- [ ] More comprehensive errno support
- [ ] Non-blocking DNS resolution
- [ ] Support for SO_REUSEADDR and other socket options

## Files Modified

### Kernel
- `kernel/fcntl.h` - Added O_NONBLOCK, F_GETFL, F_SETFL, EAGAIN
- `kernel/file.h` - Added nonblocking field to struct file
- `kernel/proc.h` - Added error_no field to struct proc
- `kernel/syscall.h` - Added SYS_fcntl
- `kernel/syscall.c` - Added fcntl system call
- `kernel/sysfile.c` - Implemented sys_fcntl(), updated sys_accept()
- `kernel/socket.c` - Updated sockread(), sockwrite(), sockaccept()
- `kernel/file.c` - Pass nonblocking flag to socket functions
- `kernel/defs.h` - Updated function signatures

### User Space
- `user/user.h` - Added fcntl() declaration and O_NONBLOCK constants
- `user/usys.pl` - Added fcntl entry
- `user/chat_server.c` - Updated to use non-blocking sockets
- `Makefile` - Updated build configuration

### Documentation
- `NON_BLOCKING.md` - This file

## License

Same as xv6-riscv (MIT License)

## Authors

- Original xv6: MIT PDOS
- Networking support: Previous contributors
- Non-blocking support: This implementation
