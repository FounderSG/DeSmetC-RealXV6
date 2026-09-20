#include <os.h>

/*
 * Data segment base (paragraph) of a process.  Every process block keeps
 * the stored u-area in a paragraph-aligned UPREFIX-byte prefix at p_addr,
 * so the data segment starts UPREFIX bytes above it -- one rule for EXE
 * processes and the single-segment icode bootstrap alike.  The u-area sits
 * just below DS:0, out of user reach (8086 segment offsets cannot go below
 * the segment base).
 */
uint udseg(p)
struct proc *p;
{
    return p->p_addr * (PAGESIZ/16) + UPREFIX/16;
}

void savu(p)
struct proc *p;
{
    fmemcpy(p->p_addr*(PAGESIZ/16), 0, core_ds, (uint)&u, sizeof(u));
}

void retu(p)
struct proc *p;
{
    fmemcpy(core_ds, (uint)&u, p->p_addr*(PAGESIZ/16), 0, sizeof(u));
}

/*
 * Size the swap image of p and record how it splits, for swapio below.
 * The data segment is DSEG bytes for every process, but only its two ends
 * hold anything: [0, break) at the bottom and [SP, DSEG) at the top.  The
 * hole between them is never written to the swap device, so an image costs
 * what the process actually uses instead of the whole DBLK-page block --
 * which is what keeps a fixed 63K segment affordable on 872 swap sectors.
 *
 * Both runs are whole pages and the stack run ends at the top of the
 * block, so the image on disk is
 *      [ p_ndpg pages of u-area + data ][ p_nspg pages of stack ]
 * and the runs meet (no hole left) when the break has climbed into the
 * stack's pages.
 *
 * The sizes come from the stored u, so every caller must savu first.
 * Everything the process still needs is at or above the saved user SP --
 * the trap frame sits at [SP, SP+24) -- and one page of slack below it
 * covers a signal frame pushed on the way back to user mode.
 */
int swsize(p)
struct proc *p;
{
    uint pseg;
    uint dsz, sp;
    int nd, ns;

    if(p->p_size != DBLK) {         /* the icode bootstrap's one-page block */
        p->p_ndpg = p->p_size;
        p->p_nspg = 0;
        return p->p_size;
    }
    pseg = p->p_addr*(PAGESIZ/16);  /* the stored u-area, own segment */
    dsz = peekw(pseg, OFFS(struct user, u_dsize));
    if(dsz > DSEG)
        dsz = DSEG;                 /* a runaway break must not size the image */
    nd = dsz/PAGESIZ + 1;           /* +1 page carries the UPREFIX prefix */
    if(dsz % PAGESIZ > PAGESIZ - UPREFIX)
        nd++;                       /* prefix + tail don't share a page */
    sp = peekw(pseg, OFFS(struct user, u_stack[KSSIZE-2]));
    if(sp >= DSEG)
        sp = DSEG - 2;              /* garbage SP: nothing on the stack to lose */
    ns = (UPREFIX + sp)/PAGESIZ;    /* block page holding the stack in use */
    if(ns > 0)
        ns--;                       /* a page of slack below the saved SP */
    ns = DBLK - ns;
    if(nd + ns > DBLK)
        ns = DBLK - nd;             /* heap met stack: there is no hole to skip */
    p->p_ndpg = nd;
    p->p_nspg = ns;
    return nd + ns;
}

/*
 * Read or write the swap image of p: the two runs swsize recorded, laid
 * out back to back on the device.  blkno is the swap sector, coreaddr the
 * page address of the block in core.
 */
int swapio(p, blkno, coreaddr, rdflg)
struct proc *p;
int blkno;
int coreaddr;
int rdflg;
{
    if(swap(blkno, coreaddr, p->p_ndpg, rdflg))
        return 1;
    if(p->p_nspg == 0)
        return 0;
    return swap(blkno + p->p_ndpg*(PAGESIZ/512),
                coreaddr + DBLK - p->p_nspg, p->p_nspg, rdflg);
}

void spl0()
{
    core_spl = 0;
    enable();
}

void spl1()
{
    core_spl = 1;
    disable();
}

void spl5()
{
    core_spl = 5;
    disable();
}

void spl6()
{
    core_spl = 6;
    disable();
}

void spl7()
{
    core_spl = 7;
    disable();
}

#define user_space_seg  udseg(u.u_procp)

int fubyte(addr)
int addr;
{
    return peekb(user_space_seg, (uint)addr) & 0377;
}

int fuword(addr)
int addr;
{
    return peekw(user_space_seg, (uint)addr);
}

int subyte(addr, ch)
int addr;
char ch;
{
    pokeb(user_space_seg, (uint)addr, ch);
    return 0;
}

int suword(addr, value)
int addr;
int value;
{
    pokew(user_space_seg, (uint)addr, value);
    return 0;
}

/*
 * I-space (code segment) byte access, used by exec to load the code
 * segment of a separated I&D program.  The code segment base is p_taddr.
 */
#define user_ispace_seg (u.u_procp->p_taddr*(PAGESIZ/16))

int fuibyte(addr)
int addr;
{
    return peekb(user_ispace_seg, (uint)addr) & 0377;
}

int suibyte(addr, ch)
int addr;
char ch;
{
    pokeb(user_ispace_seg, (uint)addr, ch);
    return 0;
}

#define PAGE_SEG(page) ((page)*(PAGESIZ/16))

void copyseg(src, dst)
uint src;
uint dst;
{
    fmemcpy(PAGE_SEG(dst), 0, PAGE_SEG(src), 0, PAGESIZ);
}

void clearseg(dst)
uint dst;
{
    fmemset(PAGE_SEG(dst), 0, 0, PAGESIZ);
}

/*
 * Block moves between the kernel and the current process's data segment.
 * V6's versions can fail (nofault catches a bad user address and returns -1);
 * here the whole data segment is mapped and a real-mode segment offset cannot
 * leave it, so they only ever return 0.
 */
int copyout(srcAddr, dstAddr, iSize)
uint srcAddr;
uint dstAddr;
int iSize;
{
    fmemcpy(udseg(u.u_procp), dstAddr, core_ds, srcAddr, iSize);
    return 0;
}

int copyin(srcAddr, dstAddr, iSize)
uint srcAddr;
uint dstAddr;
int iSize;
{
    fmemcpy(core_ds, dstAddr, udseg(u.u_procp), srcAddr, iSize);
    return 0;
}

typedef union {
    long i32;
    struct { int lo; int hi; } i16;
} unix_int32;

void dpadd(x, y)
int x[2];
int y;
{
    unix_int32 a;
    a.i16.lo = x[1];
    a.i16.hi = x[0];
    a.i32 += (uint)y;
    x[1] = a.i16.lo;
    x[0] = a.i16.hi;
}

int dpcmp(xh, xl, yh, yl)
int xh;
int xl;
int yh;
int yl;
{
    long diff;
    unix_int32 x, y;
    x.i16.hi = xh;
    x.i16.lo = xl;
    y.i16.hi = yh;
    y.i16.lo = yl;
    diff = x.i32 - y.i32;
    if(diff>512) return 512;
    else if(diff<-512) return -512;
    else return (int)diff;
}

int ldiv(x, y)
int x;
int y;
{ 
    return (x/y);
}

int lrem(x, y)
int x;
int y;
{
    return x%y; 
}

int lshift(num, bits)
int num[2];
int bits;
{
    unix_int32 a;
    a.i16.lo = num[1];
    a.i16.hi = num[0];
    if(bits>=0) a.i32 <<= bits;
    else a.i32 >>= (-bits);
    return (int)a.i32;
}

void outport(port, val)
unsigned port;
unsigned val;
{
#asm
	MOV	DX,WORD [BP+4]
	MOV	AX,WORD [BP+6]
	OUT	DX,AX
#endasm
}

void outportb(port, val)
unsigned port;
unsigned char val;
{
#asm
	MOV	DX,WORD [BP+4]
	MOV	AL,BYTE [BP+6]
	OUT	DX,AL
#endasm
}

unsigned inport(port)
unsigned port;
{
#asm
	MOV	DX,WORD [BP+4]
	IN	AX,DX
#endasm
}

unsigned char inportb(port)
unsigned port;
{
#asm
	MOV	DX,WORD [BP+4]
	IN	AL,DX
	XOR	AH,AH
#endasm
}

void idle()
{
#asm
	STI
	HLT
	CLI
#endasm
}

/*
 * Console output: the screen through the BIOS, plus a copy on COM1 for a
 * headless run to capture.  The serial write is polled, so the tee needs no
 * interrupt and costs nothing on a machine without a UART (LSR reads 0xFF).
 */
void cnputc(c)
char c;
{
    bios_putc(c);
#ifdef KL_SERIAL_TEE
    uart_putc(c);
#endif
}

void putchar(c)
char c;
{
#ifdef KL_BACKEND_UART
    uart_putc(c);
#else
    cnputc(c);
#endif
}

void setvect(vectnumber, vectfunc)
int vectnumber;
uint vectfunc;
{
    uint vaddr;

    vaddr = vectnumber * 4;             /* real-mode vector table at 0:0 */
    pokew(0x0000, vaddr, vectfunc);
    pokew(0x0000, vaddr + 2, core_cs);  /* an ISR entry is CS:IP, so this
                                         * is the one place that wants the
                                         * CODE segment, not core_ds */
}

#define  TICK_T0_8254_CWR             0x43       /* 8254 PIT Control Word Register address.            */
#define  TICK_T0_8254_CTR0            0x40       /* 8254 PIT Timer 0 Register address.                 */
#define  TICK_T0_8254_CTR1            0x41       /* 8254 PIT Timer 1 Register address.                 */
#define  TICK_T0_8254_CTR2            0x42       /* 8254 PIT Timer 2 Register address.                 */

#define  TICK_T0_8254_CTR0_MODE3      0x36       /* 8254 PIT Binary Mode 3 for Counter 0 control word. */
#define  TICK_T0_8254_CTR2_MODE0      0xB0       /* 8254 PIT Binary Mode 0 for Counter 2 control word. */
#define  TICK_T0_8254_CTR2_LATCH      0x80       /* 8254 PIT Latch command control word                */

void PC_SetTickRate()
{
    uint count;                                           /* count = (2386360L / freq + 1) >> 1            */
    count = 19886;                                        /* 60Hz                                          */
    outportb(TICK_T0_8254_CWR,  TICK_T0_8254_CTR0_MODE3); /* Load the 8254 with desired frequency          */
    outportb(TICK_T0_8254_CTR0, count & 0xFF);            /* Low  byte                                     */
    outportb(TICK_T0_8254_CTR0, (count >> 8) & 0xFF);     /* High byte                                     */
}

/* external helper function defined in asm code */
extern void use_resume_stack();
extern void do_resume();
struct proc *resume_proc;
int *resume_ctx;
void resume(p, ctx)
struct proc *p;
label_t ctx;
{
    resume_proc = p;
    resume_ctx = ctx;
    use_resume_stack();

    if(resume_proc != u.u_procp)
    {
        /*
         * Do not savu a process whose block is not in core: a zombie's
         * p_addr is its swap-side u save, and a process that swapped
         * ITSELF out (swgrow, xalloc's no-core valve) already carries
         * its u in the swapped image -- p_addr is a swap address either
         * way, not a page number.
         */
        if(u.u_procp->p_stat != SZOMB && (u.u_procp->p_flag & SLOAD))
            savu(u.u_procp);
        retu(resume_proc);
    }
    do_resume(resume_ctx);
}

/*
 * Re-base the saved register frame of process p to its current core
 * location.  Called after the process image is moved (fork copy, swap in);
 * relocation is just this segment rewrite -- the image itself is position
 * independent.  The stored u-area is the UPREFIX prefix at p_addr; read the
 * saved user SP from it and re-base the interrupt frame found there.  For a
 * single-segment process (p_tsize == 0, the icode bootstrap) cs == ds; for
 * a separated I&D process cs points at the (resident) code segment.
 */
void estabur(p)
struct proc *p;
{
    uint pseg, csp;
    uint dbase, cbase;

    dbase = udseg(p);
    cbase = p->p_tsize ? p->p_taddr * (PAGESIZ / 16) : dbase;
    pseg = p->p_addr * (PAGESIZ / 16);  /* the stored u-area */
    pokew(pseg, OFFS(struct user, u_stack[KSSIZE - 1]), dbase);  /* SS */
    csp = peekw(pseg, OFFS(struct user, u_stack[KSSIZE - 2]));
    pokew(dbase, csp + OFFS(struct ctx, cs), cbase);
    pokew(dbase, csp + OFFS(struct ctx, ds), dbase);
    pokew(dbase, csp + OFFS(struct ctx, es), dbase);
}

void isr_savuar(ds, es, dx, cx, bx, ax, di, si, bp, ip, cs, flags)
int ds;
int es;
int dx;
int cx;
int bx;
int ax;
int di;
int si;
int bp;
int ip;
int cs;
int flags;
{
    (void)es; (void)ds; (void)si; (void)di; (void)bp;
    (void)ip; (void)cs; (void)flags;

    u.u_ar0[R0] = ax;
    u.u_ar0[R1] = bx;
    u.u_ar0[R2] = cx;
    u.u_ar0[R3] = dx;
}

void isr_router(irq, mode)
int irq;
int mode;
{
    switch(irq)
    {
        case 0: clock(mode); break;
        case 1: ideintr(); break;
        case 2: kbdintr(); break;
        case 3: uartintr(); break;
    }
}

void check_runrun()
{
loop:
    spl7();
    if(runrun == 0)
    {
        return;
    }
    spl0();
    swtch();
    goto loop;
}

void trap0(ds, es, dx, cx, bx, ax, di, si, bp, ip, cs, flags, arg0, arg1, arg2)
int ds;
int es;
int dx;
int cx;
int bx;
int ax;
int di;
int si;
int bp;
int ip;
int cs;
int flags;
int arg0;
int arg1;
int arg2;
{
    (void)es; (void)ds; (void)si; (void)di; (void)bp;
    (void)ip; (void)cs; (void)flags;

    u.u_ar0[R0] = ax;
    u.u_ar0[R1] = bx;
    u.u_ar0[R2] = cx;
    u.u_ar0[R3] = dx;
    u.u_arg[0] = arg0;
    u.u_arg[1] = arg1;
    u.u_arg[2] = arg2;
    u.u_dirp = u.u_arg[0];
    u.u_error = 0;
}

void trap_epilogue()
{
    uint sseg, csp;

    sseg = u.u_stack[KSSIZE - 1];
    csp  = u.u_stack[KSSIZE - 2];
    pokew(sseg, csp + OFFS(struct ctx, ax), u.u_ar0[R0]);
    pokew(sseg, csp + OFFS(struct ctx, bx), u.u_ar0[R1]);
    pokew(sseg, csp + OFFS(struct ctx, cx), u.u_ar0[R2]);
    pokew(sseg, csp + OFFS(struct ctx, dx), u.u_ar0[R3]);
}

/*
 * Vector a caught signal: duplicate the interrupt frame the trap entry left
 * on the user stack, point the copy at the signal trampoline (the first
 * instruction of the code segment, SIGTRAMP_EXE) with the handler address in
 * SI, and rearrange the original frame into the return the trampoline
 * takes back into the interrupted code.
 */
void sendsig(p)
int p;
{
    uint sseg, usp, old;
    int oax, oip, oflag;

    u.u_stack[KSSIZE - 2] -= 24;  /* duplicate interrupt stack frame */
    sseg = u.u_stack[KSSIZE - 1];
    usp  = u.u_stack[KSSIZE - 2];
    old  = usp + 24;              /* the frame just copied down */
    fmemcpy(sseg, usp, sseg, old, 24);
    pokew(sseg, usp + OFFS(struct ctx, ip), SIGTRAMP_EXE);
    pokew(sseg, usp + OFFS(struct ctx, si), p);
    /* ip, cs, flag = ax, flag, return address.  Read all three first:
     * the rotation would otherwise overwrite a source it still needs. */
    oax   = peekw(sseg, old + OFFS(struct ctx, ax));
    oip   = peekw(sseg, old + OFFS(struct ctx, ip));
    oflag = peekw(sseg, old + OFFS(struct ctx, flag));
    pokew(sseg, old + OFFS(struct ctx, cs), oflag);
    pokew(sseg, old + OFFS(struct ctx, flag), oip);
    pokew(sseg, old + OFFS(struct ctx, ip), oax);
}

#define PC_CLOCK_INTR   8
#define PC_KBD_INTR     9
#define PC_UART_INTR    12
#define PC_IDE_INTR     0x76
#define PC_UNIX_INTR    0x81

void pc_init()
{
    /*
     * Under the Watcom .COM build CS == DS == SS, so a single core_cs
     * served both roles and was taken from the address of a data object.
     * DeSmet's small model puts code and data in different segments, so
     * the two are read separately: core_cs is the real CS (it goes into
     * interrupt vectors) and core_ds the data segment every other offset
     * is paired with.  They coincide again if the image is ever relinked
     * single-segment.
     */
    core_cs = getcs();
    core_ds = getds();

#ifdef KL_USES_UART
    uart_init();
#endif
#ifdef KL_BACKEND_UART
    setvect(PC_UART_INTR, (uint)uart_isr);
#endif
#ifdef KL_BACKEND_KBD
    kbd_init();
    setvect(PC_KBD_INTR, (uint)kbd_isr);
#endif
    setvect(PC_IDE_INTR, (uint)ide_isr);
    outportb(0x1f6, 0xe0 | (0<<4));  /* select disk 0 */

    setvect(PC_CLOCK_INTR, (uint)clock_isr);
    PC_SetTickRate();

    setvect(PC_UNIX_INTR, (uint)trap_isr);
}
