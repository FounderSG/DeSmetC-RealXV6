/*
 * tunable variables
 */

#define NBUF    15      /* size of buffer cache */
#define NINODE  100     /* number of in core inodes */
#define NFILE   100     /* number of in core file structures */
#define NMOUNT  5       /* number of mountable file systems */
#define NEXEC   3       /* number of simultaneous exec's */
#define MAXMEM  (64*32) /* max core per process - first # is Kw */
#define SSIZE   20      /* initial stack size (*64 bytes) */
#define SINCR   20      /* increment of stack (*64 bytes) */
#define NOFILE  15      /* max open files per process */
#define CANBSIZ 256     /* max size of typewriter line */
#define CMAPSIZ 100     /* size of core allocation area */
#define SMAPSIZ 100     /* size of swap allocation area */
#define NCALL   20      /* max simultaneous time callouts */
#define NPROC   50      /* max number of processes */
#define NTEXT   40      /* max number of pure texts */
#define NCLIST  100     /* max total clist size */
#define HZ      60      /* Ticks/second of the clock */

/*
 * priorities
 * probably should not be
 * altered too much
 */

#define PSWP    -100
#define PINOD   -90
#define PRIBIO  -50
#define PPIPE   1
#define PWAIT   40
#define PSLEP   90
#define PUSER   100

/*
 * signals
 * dont change
 */

#define NSIG    20
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

/*
 * fundamental constants
 * cannot be changed
 */

#define USIZE   16      /* page count of the kernel's 64KB arena; proc 0's
                           one-page block is its last page */
#define UPREFIX 0x400   /* bytes from p_addr to the user data segment (DS):
                           a paragraph-aligned prefix holding the stored
                           u-area (== sizeof(struct user)) at the front of
                           every process block.  It sits just below DS:0,
                           out of user reach -- 8086 segment offsets cannot
                           go below the segment base */
/*
 * 0xFC00 is 64512, past the range of a K&R int, and C88 3.03 types an
 * integer constant that does not fit as a plain (signed) int -- so bare
 * 0xFC00 is -1024 here, and "(long)DSEG" sign-extended to -1024, making
 * exec's "a_data + a_bss > DSEG" test true for every program: every exec
 * failed with ENOMEM.  ANSI would have given the constant unsigned type,
 * which is what the Watcom build got.  Say unsigned explicitly.
 */
#define DSEG    ((unsigned)0xFC00)
                        /* bytes of user data segment.  Fixed for every EXE
                           process: data and bss at the bottom, the stack at
                           the top, the heap growing up into the hole between
                           them.  63K, so UPREFIX + DSEG is exactly DBLK
                           pages and a process block rounds without waste */
#define DBLK    16      /* pages in a process data block: (UPREFIX+DSEG)/PAGESIZ */
#define PAGESIZ 4096    /* page size */
#define USPACE  0x20    /* start of user space page */
#define KSSIZE  365     /* size of kernel stack 365 words */
#define NULL    0
#define NODEV   (-1)
#define ROOTINO 1       /* i number of all roots */
#define DIRSIZ  14      /* max characters per directory */

/*
 * structure to access an
 * integer in bytes
 */
struct hilo_t
{
    char    lobyte;
    char    hibyte;
};

/*
 * structure to access an integer
 */
struct integ_t
{
    int integ;
};

/*
 * Certain processor registers
 */
#define PS  0177776
#define KL  0177560
#define SW  0177570
