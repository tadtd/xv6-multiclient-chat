struct stat;
struct rtcdate;
struct sockaddr;
struct pollfd;

// Poll event flags (same as kernel/socket.h)
#define POLLIN      0x001   // Data available to read
#define POLLOUT     0x004   // Writing now will not block
#define POLLERR     0x008   // Error condition
#define POLLHUP     0x010   // Hung up (connection closed)
#define POLLNVAL    0x020   // Invalid file descriptor

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int ntas();
int nfree();
int socket(int, int, int);
int connect(int, const struct sockaddr*, int);
int bind(int, const struct sockaddr*, int);
int listen(int, int);
int accept(int, struct sockaddr*, int*);
int gethostbyname(const char*, struct sockaddr*);
int inetaddress(const char*, struct sockaddr*);
uint timenow();
int net_poll(struct pollfd*, int, int);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
uint16 htons(uint16 n);
