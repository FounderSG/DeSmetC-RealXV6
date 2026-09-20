/*
 * Random set of variables
 * used by more than one
 * routine.
 */
extern char canonb[CANBSIZ];    /* buffer for erase and kill (#@) */
extern int coremap[CMAPSIZ];    /* space for core allocation */
extern int swapmap[SMAPSIZ];    /* space for swap allocation */
extern struct inode *rootdir;   /* pointer to inode of root directory */
extern int execnt;              /* number of processes in exec */
extern int lbolt;               /* time of day in 60th not in time */
extern int time[2];             /* time in sec from 1970 */
extern int tout[2];             /* time of day of next sleep */
/*
 * The callout structure is for
 * a routine arranging
 * to be called by the clock interrupt
 * (clock.c) with a specified argument,
 * in a specified amount of time.
 * Used, for example, to time tab
 * delays on teletypes.
 */
struct callo
{
    int c_time;     /* incremental time */
    int c_arg;      /* argument to routine */
    int (*c_func)(); /* routine */
};
extern struct callo callout[NCALL];
/*
 * Mount structure.
 * One allocated on every mount.
 * Used to find the super block.
 */
struct mount
{
    int m_dev;              /* device mounted */
    struct buf *m_bufp;     /* pointer to superblock */
    struct inode *m_inodp;  /* pointer to mounted on inode */
};
extern struct mount mount[NMOUNT];

extern int  mpid;       /* generic for unique process id's */
extern char runin;      /* scheduling flag */
extern char runout;     /* scheduling flag */
extern char runrun;     /* scheduling flag */
extern int  curpri;     /* more scheduling (signed: see p_pri in proc.h) */
extern int  rootdev;    /* dev of root see conf.c */
extern int  swapdev;    /* dev of swap see conf.c */
extern int  swplo;      /* block number of swap space */
extern int  nswap;      /* size of swap space */
extern int  updlock;    /* lock for sync */
extern int  rablock;    /* block to be read ahead */

/*
 * structure of the system entry table (sysent.c)
 */
struct sysent
{
    int count;          /* argument count */
    void (*call)();  /* name of handler */
};
extern struct sysent sysent[64];

extern int core_cs;     /* kernel CODE segment, for interrupt vectors */
extern int core_ds;     /* kernel DATA segment, paired with data offsets */
extern int core_spl;    /* system priority level */

/* trap.c */
void nosys();
void nullsys();

/* subr.c */
int bmap();
int passc();
int cpass();
void nodev();
int nulldev();
void bcopy();

/* prf.c */
void printf();
void panic();
void prdev();
void deverror();

/* alloc.c */
void iinit();
struct buf *alloc();
void free();
int badblock();
struct inode *ialloc();
void ifree();
struct filsys *getfs();
void update();

/* nami.c */
struct inode *namei();
int uchar();
int schar();

/* pipe.c */
void pipe();
void readp();
void writep();
void plock();
void prele();

/* bio.c */
struct buf* bread();
struct buf* breada();
void bwrite();
void bdwrite();
void bawrite();
void brelse();
struct buf* incore();
struct buf* getblk();
void iowait();
void notavail();
void iodone();
void clrbuf();
void binit();
void mapfree();
void mapalloc();
int swap();
void bflush();
void physio();
void geterror();

/* iget.c */
struct inode* iget();
void iput();
void iupdat();
void itrunc();
struct inode* maknode();
void wdir();

/* rdwri.c */
void readi();
void writei();
int max();
int min();
void iomove();

/* fio.c */
struct file *getf();
void closef();
void closei();
void openi();
int access();
struct inode *owner();
int suser();
int ufalloc();
struct file *falloc();

/* sys1.c */
void exec();
void rexit();
void exit();
void wait();
void fork();
void sbreak();

/* sys2.c */
void read();
void write();
void rdwr();
void open();
void creat();
void open1();
void close();
void seek();
void link();
void mknod();
void sslep();

/* sys3.c */
void fstat();
void stat();
void stat1();
void dup();
void smount();
void sumount();
int getmdev();

/* sys4.c */
void getswit();
void gtime();
void stime();
void setuid();
void getuid();
void setgid();
void getgid();
void getpid();
void sync();
void nice();
void unlink();
void chdir();
void chmod();
void chown();
void ssig();
void kill();
void times();
void psinfo();
void reboot();

/* slp.c */
void swtch();
void setrun();
void setpri();
void sched();
void wakeup();
void sleep();
int save();
uint swgrow();
int newproc();

/* sig.c */
void signal();
void psignal();
int issig();
void psig();
int core();

/* malloc.c */
uint malloc();
void mfree();
void coremap_init();

/* text.c */
void xswap();
void xalloc();
void xfree();
void xccdec();

/* clock.c */
void clock();
void timeout();

/* tty.c */
void gtty();
void stty();
void wflushtty();
void cinit();
void flushtty();
void ttstart();
void ttread();
void ttwrite();
int ttystty();
void ttyinput();
int getc();
int putc();

/* kl.c */
int klopen();
int klclose();
int klread();
int klwrite();
int klsgtty();
void kltxintr();
void klrxintr();

/* mem.c */
int mmread();
int mmwrite();
int mmsgtty();

/* rk.c */
void rkstrategy();
void rkintr();
int rkread();
int rkwrite();

/* fd.c */
int fdtry();
int fdprobe();
int fdsize();
int fdopen();
void fdstrategy();

/* m86.a */
void memcpy();
void memset();
int getps();
void setps();
int bios_getc();
void bios_putc();
int bios_diskio();
int bios_diskparm();
void bios_diskreset();
void move_to_user_mode();
void clock_isr();
void uart_isr();
void kbd_isr();
void ide_isr();
void trap_isr();

/* pc.c */
uint udseg();
void savu();
void retu();
void resume();
void estabur();
int swsize();
int swapio();
void trap0();
void trap_epilogue();
void sendsig();
void isr_savuar();
void isr_router();
void check_runrun();
void spl0();
void spl1();
void spl5();
void spl6();
void spl7();
int fubyte();
int fuword();
int subyte();
int suword();
int fuibyte();
int suibyte();
void copyseg();
void clearseg();
int copyout();
int copyin();
void dpadd();
int dpcmp();
int ldiv();
int lrem();
int lshift ();
void outport();
void outportb();
unsigned inport();
unsigned char inportb();
void idle();
void putchar();
void cnputc();
void pc_init();

/* kbd.c */
void kbd_init();
int kbd_getc();
void kbdintr();

/* ide.c */
void ideio();
void ideintr();

/* uart.c */
void uart_init();
void uartintr();
void uart_putc();
int uart_getc();
