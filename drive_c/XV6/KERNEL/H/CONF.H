/*
 * Used to dissect integer device code
 * into major (driver designation) and
 * minor (driver parameter) parts.
 */
struct dev_t
{
    char    d_minor;
    char    d_major;
};

#define minor(x) ((x)&0xff) /* device minor */

/*
 * Device major.  V6 writes this as (x)>>8 and relies on the PDP-11's
 * arithmetic right shift: NODEV is -1, so major(NODEV) is also negative and
 * fails getblk's "major(dev) >= nblkdev" bounds test, letting control reach
 * the dev < 0 branch that hands back the free list.
 *
 * DeSmet C 3.03 shifts right LOGICALLY even for signed operands (measured:
 * int -1 >> 8 yields 255, long -1L >> 8 yields 16777215).  major(NODEV) then
 * came out 255, tripped the bounds test, and panicked "blkdev" on the very
 * first getblk(NODEV, 0).  Restore the sign explicitly.
 *
 * One other right shift in the kernel has the same problem and the same fix:
 * seek's blk*512 high word in ken/sys2.c.  Every remaining one operates on a
 * non-negative value or is masked afterwards.
 */
#define major(x) ((x) < 0 ? -1 : ((x)>>8) & 0377)
#define makedev(ma,mi) (((ma)<<8)+((mi)&0xff))

/*
 * Declaration of block device
 * switch. Each entry (row) is
 * the only link between the
 * main unix code and the driver.
 * The initialization of the
 * device switches is in the
 * file conf.c.
 */
struct bdevsw
{
    int (*d_open)();
    int (*d_close)();
    void (*d_strategy)();
    struct devtab *d_tab;
};
extern struct bdevsw bdevsw[];

/*
 * Nblkdev is the number of entries
 * (rows) in the block switch. It is
 * set in binit/bio.c by making
 * a pass over the switch.
 * Used in bounds checking on major
 * device numbers.
 */
extern int nblkdev;

/*
 * Character device switch
 */
struct cdevsw
{
    int (*d_open)();
    int (*d_close)();
    int (*d_read)();
    int (*d_write)();
    int (*d_sgtty)();
};
extern struct cdevsw cdevsw[];

/*
 * Number of character switch entries.
 * Set by cinit/tty.c
 */
extern int nchrdev;
