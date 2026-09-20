/*
 * One structure allocated per active
 * process. It contains all data needed
 * about the process while the
 * process may be swapped out.
 * Other per process data (user.h)
 * is swapped with the process.
 */
struct  proc
{
    char    p_stat;
    char    p_flag;
    int     p_pri;      /* priority, negative is high -- MUST be signed:
                         * DeSmet char is UNSIGNED, and PRIBIO/PSWP are
                         * negative, so a char here makes swtch blind to
                         * every process that slept at a kernel priority */
    char    p_sig;      /* signal number sent to this process */
    char    p_uid;      /* user id, used to direct tty signals */
    char    p_time;     /* resident time for scheduling */
    char    p_cpu;      /* cpu usage for scheduling */
    int     p_nice;     /* nice for scheduling -- MUST be signed, same reason
                         * as p_pri: nice() lets the superuser set it negative
                         * and setpri adds it to a priority */
    struct tty *p_ttyp; /* controlling tty */
    int     p_pid;      /* unique process id */
    int     p_ppid;     /* process id of parent */
    int     p_addr;     /* address of swappable image */
    int     p_size;     /* size of swappable image (*64 bytes) */
    int     p_taddr;    /* code segment base page; 0 = single-segment (icode bootstrap) */
    int     p_tsize;    /* code segment size in pages; 0 = single-segment */
    char    p_ndpg;     /* swap image: pages of u-area + data at the block front */
    char    p_nspg;     /* swap image: pages of stack at the block top (see swsize) */
    int     p_wchan;    /* event process is awaiting */
    struct text *p_textp;   /* pointer to text structure */
};
extern struct proc proc[NPROC];

/* stat codes */
#define SSLEEP  1       /* sleeping on high priority */
#define SWAIT   2       /* sleeping on low priority */
#define SRUN    3       /* running */
#define SIDL    4       /* intermediate state in process creation */
#define SZOMB   5       /* intermediate state in process termination */
#define SSTOP   6       /* process being traced */

/* flag codes */
#define SLOAD   01      /* in core */
#define SSYS    02      /* scheduling process */
#define SLOCK   04      /* process cannot be swapped */
#define SSWAP   010     /* process is being swapped out */
#define STRC    020     /* process is being traced */
#define SWTED   040     /* another tracing flag */
