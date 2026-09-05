/* UNIX.H -- RealXV6 user-space API for DeSmet C 3.03.
 * K&R port of RealXV6 usr/unix.h: no prototypes, no void, no void
 * pointers, no const, no #pragma.  DeSmet notes: char is already
 * unsigned (so uchar is a plain char typedef); "unsigned char" and
 * "unsigned long" are not valid DeSmet type combinations.
 */

typedef unsigned uint;
typedef char uchar;

/* Byte-for-byte the layout the kernel writes (verified: DeSmet packs
 * structs with no padding; sizeof == 36 / 16, offsets match V6). */
struct stat {
    int  s_dev;         /* device number    */
    int  s_inumber;     /* i-node number    */
    int  s_flags;       /* see below        */
    char s_nlinks;      /* number of links  */
    char s_uid;         /* owner user id    */
    char s_gid;         /* owner group id   */
    char s_size0;       /* file size high   */
    int  s_size1;       /* file size low    */
    int  s_addr[8];     /* block addr       */
    int  s_actime[2];   /* accessed time    */
    int  s_modtime[2];  /* modified time    */
};

#define T_DIR  1        /* Directory */
#define T_FILE 2        /* File */
#define T_DEV  3        /* Device */

#define DIRSIZ 14

struct dirent {
    uint inum;
    char name[DIRSIZ];
};

/* signals */
#define SIGHUP  1       /* hangup */
#define SIGINT  2       /* interrupt (rubout) */
#define SIGQIT  3       /* quit (FS) */
#define SIGINS  4       /* illegal instruction */
#define SIGTRC  5       /* trace or breakpoint */
#define SIGIOT  6       /* iot */
#define SIGEMT  7       /* emt */
#define SIGFPT  8       /* floating exception */
#define SIGKIL  9       /* kill */
#define SIGBUS  10      /* bus error */
#define SIGSEG  11      /* segmentation violation */
#define SIGSYS  12      /* sys */
#define SIGPIPE 13      /* end of pipe */

#define SIG_IGN 1       /* handler value, passed through to the kernel */

/* u_error codes */
#define EFAULT  -106
#define EPERM   -1
#define ENOENT  -2
#define ESRCH   -3
#define EINTR   -4
#define EIO     -5
#define ENXIO   -6
#define E2BIG   -7
#define ENOEXEC -8
#define EBADF   -9
#define ECHILD  -10
#define EAGAIN  -11
#define ENOMEM  -12
#define EACCES  -13
#define ENOTBLK -15
#define EBUSY   -16
#define EEXIST  -17
#define EXDEV   -18
#define ENODEV  -19
#define ENOTDIR -20
#define EISDIR  -21
#define EINVAL  -22
#define ENFILE  -23
#define EMFILE  -24
#define ENOTTY  -25
#define ETXTBSY -26
#define EFBIG   -27
#define ENOSPC  -28
#define ESPIPE  -29
#define EROFS   -30
#define EMLINK  -31
#define EPIPE   -32

/* file flags */
#define FREAD   01
#define FWRITE  02
#define FPIPE   04

/* syscall result registers + errno, defined in CRT0.A */
extern int errno;
extern int r0, r1, r3;
/* end of BSS = initial program break; CRT0.A word patched by d2a
 * (used by sbrk and the V6DOS arena) */
extern unsigned endbrk;

/* functions returning non-int (K&R: int-returning need no declaration) */
char *sbrk();
char *signal();
char *malloc();
char *ctime();
char *strcpy();
char *strcat();
char *strncpy();
char *safestrcpy();
char *strchr();
char *gets();
char *memset();
char *memmove();
char *memcpy();

#define stdin  0
#define stdout 1
#define stderr 2
