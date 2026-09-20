#include <os.h>

struct user u;
struct proc proc[NPROC];
struct text text[NTEXT];
struct inode *rootdir;

int core_cs;
int core_ds;
int core_spl;

int mpid;
char runin;
char runout;
char runrun;
int curpri;
int rootdev = 0;
int swapdev = 0;
int swplo = 4000;
int nswap = 872;
int updlock = 0;

int execnt;
int lbolt;
int time[2];
int tout[2];
int nchrdev;

struct mount mount[NMOUNT];
struct inode inode[NINODE];
char canonb[CANBSIZ];
int coremap[CMAPSIZ];
int swapmap[SMAPSIZ];
struct callo callout[NCALL];

extern struct devtab rktab;
extern struct devtab fdtab;

/*
 * binit counts nblkdev by walking this while d_open is set, so a new row
 * belongs before the terminator and needs a d_open even when it has nothing
 * to do -- openi turns major >= nblkdev into ENXIO.
 */
struct bdevsw bdevsw[] = {
    { nulldev, nulldev, rkstrategy, &rktab },   /* 0 = rk, the IDE disk */
    { fdopen, nulldev, fdstrategy, &fdtab },    /* 1 = fd, the BIOS floppy */
    { NULL, NULL, NULL, NULL},
};

struct cdevsw cdevsw[] = {
    { klopen, klclose, klread, klwrite, klsgtty },
    { nulldev, nulldev, mmread, mmwrite, mmsgtty },
    { nulldev, nulldev, rkread, rkwrite, (int (*)())nulldev },
    { NULL, NULL, NULL, NULL, NULL }
};

/*
 * Icode is the octal bootstrap
 * program executed in user mode
 * to bring up the system.
 *
 * Disassembly:
 * 0100        B8 12 01         MOV AX, 0112h  ; argv
 * 0103        50               PUSH AX
 * 0104        B8 0D 01         MOV AX, 010Dh  ; prog
 * 0107        50               PUSH AX
 * 0108        BA 0B 00         MOV DX, 000Bh  ; sys_exec
 * 010B        CD 81            INT 81h        ; syscall exec(prog, argv)
 * 010D  prog: 69 6E 69 74 00   DB 'init\0'    ; program to exec
 * 0112  argv: 0D 01            DW av          ; argv[] array
 * 0114        00 00            DW 0           ; NULL
 */
char icode[] = {
    0xB8, 0x12, 0x01, 0x50, 0xB8, 0x0D, 0x01, 0x50,
    0xBA, 0x0B, 0x00, 0xCD, 0x81, 0x69, 0x6E, 0x69,
    0x74, 0x00, 0x0D, 0x01, 0x00, 0x00
};

void main()
{
    pc_init();

    mfree(coremap, 128, USPACE);
    mfree(swapmap, nswap, swplo);

    /*
     * set up system process
     *
     * proc 0's one-page block is the last page of the kernel's 64KB
     * arena: the stored u-area lives at the block front (p_addr), the
     * same rule as every other process (see udseg in dmr/pc.c).
     */
    if(sizeof(struct user) != UPREFIX)
        panic("user size");
    proc[0].p_addr = core_cs/(PAGESIZ/16) + USIZE - 1;
    proc[0].p_size = 1;
    proc[0].p_stat = SRUN;
    proc[0].p_flag |= SLOAD|SSYS;
    u.u_procp = &proc[0];

    cinit();
    binit();
    iinit();

    rootdir = iget(rootdev, ROOTINO);
    rootdir->i_flag &= ~ILOCK;
    u.u_cdir = iget(rootdev, ROOTINO);
    u.u_cdir->i_flag &= ~ILOCK;

    printf("Unix Ready.\r\n");

    /*
     * make init process
     * enter scheduling loop
     * with system process
     */

    if(newproc()) {
        copyout((uint)icode, 0x100, sizeof(icode));
        move_to_user_mode(udseg(u.u_procp), PAGESIZ - UPREFIX);
        /*
         * Return goes to loc. 0 of user init
         * code just copied out.
         */
        return;
    }
    sched();
}
