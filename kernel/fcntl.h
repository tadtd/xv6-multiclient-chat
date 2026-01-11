#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400
#define O_NONBLOCK 0x800

// fcntl commands
#define F_GETFL   3
#define F_SETFL   4

// Error codes for non-blocking operations
#define EAGAIN    11
#define EWOULDBLOCK EAGAIN
