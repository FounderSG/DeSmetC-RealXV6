#include <os.h>

/*
 * exec system call.
 * Because of the fact that an I/O buffer is used
 * to store the caller's arguments during exec,
 * and more buffers are needed to read in the text file,
 * deadly embraces waiting for free buffers are possible.
 * Therefore the number of processes simultaneously
 * running in exec has to be limited to NEXEC.
 */
#define EXPRI   -1

void exec()
{
    int ap, na, nc;
    int ts, ds;
    int reuse;
    uint da, ustktop;
    long l;
    struct text *oldtp, *newtp;
    struct exec hdr;
    struct buf *bp;
    struct inode *ip;
    register int c;
    register char *cp;
    register int *wp;

    /*
     * pick up file names
     * and check various modes
     * for execute permission
     */

    ip = namei(&uchar, 0);
    if(ip == NULL)
        return;
    while(execnt >= NEXEC)
        sleep(&execnt, EXPRI);
    execnt++;
    bp = getblk(NODEV, 0);
    if(access(ip, IEXEC) || (ip->i_mode&IFMT)!=0)
        goto bad;

    /*
     * pack up arguments into
     * allocated disk buffer
     */
    cp = bp->b_addr;
    na = 0;
    nc = 0;
    while(ap = fuword(u.u_arg[1])) {
        na++;
        if(ap == -1)
            goto bad;
        u.u_arg[1] += 2;
        for(;;) {
            c = fubyte(ap++);
            if(c == -1)
                goto bad;
            *cp++ = c;
            nc++;
            if(nc > 510) {
                u.u_error = E2BIG;
                goto bad;
            }
            if(c == 0)
                break;
        }
    }
    if((nc&1) != 0) {
        *cp++ = 0;
        nc++;
    }

    /*
     * Read the executable header (16 bytes).  A separated I&D (EXE) image
     * has hdr.a_magic == A_MAGIC; anything else is not an executable.
     */
    u.u_base = (char *)&hdr;
    u.u_count = sizeof(hdr);
    u.u_offset[1] = 0;
    u.u_offset[0] = 0;
    u.u_segflg = 1;
    readi(ip);
    u.u_segflg = 0;
    if(u.u_error)
        goto bad;
    if(hdr.a_magic != A_MAGIC) {
        u.u_error = ENOEXEC;
        goto bad;
    }

    /*
     * Separated I&D.  The data block is
     *   [ UPREFIX-byte u-area prefix ][ data ][ bss ][ heap .. stack ]
     * and is DBLK pages for every process: the data segment is DSEG bytes
     * whatever the program's own sizes are, so the heap and the stack grow
     * toward each other through the unused hole and neither has to move.
     * The code segment is a shared block managed by the text table
     * (ken/text.c).  Nothing is freed until both the block and the text
     * attach are settled, so a failure leaves the image intact.
     */
    if((ip->i_flag&ITEXT)==0 && ip->i_count!=1) {
        u.u_error = ETXTBSY;          /* file is open for writing (V6) */
        goto bad;
    }
    if(hdr.a_text == 0) {
        u.u_error = ENOEXEC;          /* an EXE with no text cannot run; also
                                       * keeps p_tsize!=0 <=> p_textp!=NULL */
        goto bad;
    }
    l = (long)hdr.a_data + hdr.a_bss;
    if(l > (long)DSEG) {
        u.u_error = ENOMEM;
        goto bad;
    }

    /*
     * Every EXE block is the same size, so a process that already has one
     * reuses it in place -- no allocation, no ENOMEM, no 2*DBLK peak.  Only
     * the single-segment icode bootstrap (a one-page block) allocates.
     */
    reuse = u.u_procp->p_size == DBLK;
    if(!reuse) {
        da = malloc(coremap, DBLK);   /* data block (u-area prefix + data) */
        if(da == NULL)
            da = swgrow(DBLK);        /* V6: swap self out; sched evicts
                                       * sleepers and swaps us back in with
                                       * room for old image + da, so a failed
                                       * exec still falls back to the intact
                                       * old image */
    }
    oldtp = u.u_procp->p_textp;       /* xalloc retargets p_textp/p_taddr */
    xalloc(ip, hdr.a_text);           /* attach or build the shared text */
    if(u.u_error) {
        if(!reuse)
            mfree(coremap, DBLK, da); /* nothing attached; old image intact */
        goto bad;
    }

    /*
     * Commit.  The text is attached and the block is settled, so the old
     * image goes now; the live u-area and the running kernel stack are the
     * kernel global u, not part of the process block, so it goes outright.
     * A reused block is the old image -- it is overwritten in place, and
     * xalloc may have moved it (its no-core valve swaps the process out and
     * back), so take p_addr afresh rather than the value from before.
     * Release the reference to the old text: V6 exec calls xfree() BEFORE
     * xalloc(); it is deferred to the commit point here so a failed exec
     * keeps the old image.  xfree works on p_textp, which xalloc already
     * retargeted, so swap the old pointer back for the call.  (Same-binary
     * exec: both references land on one entry and the counts still balance.)
     */
    if(reuse)
        da = u.u_procp->p_addr;
    else
        mfree(coremap, u.u_procp->p_size, u.u_procp->p_addr);
    newtp = u.u_procp->p_textp;
    u.u_procp->p_textp = oldtp;
    xfree();
    u.u_procp->p_textp = newtp;
    u.u_procp->p_taddr = newtp->x_caddr;
    u.u_procp->p_tsize = newtp->x_size;
    u.u_procp->p_addr = da;
    u.u_procp->p_size = DBLK;
    u.u_dsize = (uint)l;              /* the break: end of data+bss */

    for(c=0; c<DBLK; c++)             /* clear u-area prefix + the data segment */
        clearseg(da+c);
    /* the code block was cleared and loaded by xalloc; clearing it here
     * would wipe a segment other processes share */

    l = (long)sizeof(hdr) + hdr.a_text;
    u.u_base = (char *)0;             /* data -> data segment at DS:0 */
    u.u_offset[0] = (int)(l >> 16);
    u.u_offset[1] = (int)l;
    u.u_count = hdr.a_data;
    readi(ip);

    /*
     * initialize stack segment: the user stack tops the data segment
     * (SP grows down from DSEG); lay argc/argv/strings at the top, the
     * initial interrupt frame just below them.
     */
    ustktop = DSEG - 2;               /* top word left unused, so the SP
                                       * holder sits at a fixed offset in
                                       * the frame image psinfo/ps read */
    cp = bp->b_addr;
    /* argc, argv, [arg0, arg1, .., 0] [strings] */
    ap = (ustktop-2) - nc - na*2 - 6;
    suword(ustktop-2, ap);       /* user sp */
    ts = ap - 24;
    suword(ap, na);              /* argc */
    suword(ap + 2, ap + 4);      /* argv */
    ap += 4;
    c = (ustktop-2) - nc;
    while(na--) {
        suword(ap, c);
        ap += 2;
        do
            subyte(c++, *cp);
        while(*cp++);
    }
    suword(ap, 0);                /* argv[argc] = NULL; exec's own arg walk
                                   * (and any program passing main's argv on)
                                   * relies on the vector being terminated */

    ds = udseg(u.u_procp);        /* data segment base (UPREFIX above p_addr) */
    suword(ts + 0, ds);           /* ds */
    suword(ts + 2, ds);           /* es */
    suword(ts + 18, hdr.a_entry); /* ip */
    suword(ts + 20, u.u_procp->p_taddr*(PAGESIZ/16));  /* cs */
    suword(ts + 22, 0x200);       /* flag */
    u.u_stack[KSSIZE - 1] = ds;   /* ss (data base changed: reset live SS) */
    u.u_stack[KSSIZE - 2] = ts;   /* sp */

    /*
     * set SUID/SGID protections, if no tracing
     */

    if ((u.u_procp->p_flag&STRC)==0) {
        if(ip->i_mode&ISUID)
            if(u.u_uid != 0) {
                u.u_uid = ip->i_uid;
                u.u_procp->p_uid = ip->i_uid;
            }
        if(ip->i_mode&ISGID)
            u.u_gid = ip->i_gid;
    }

    /*
     * clear sigs, regs and return
     */
    for(wp = &u.u_signal[0]; wp < &u.u_signal[NSIG]; wp++)
        if(*wp != 1)
            *wp = 0;

bad:
    iput(ip);
    brelse(bp);
    if(execnt >= NEXEC)
        wakeup(&execnt);
    execnt--;
}

/*
 * exit system call:
 * pass back caller's r0
 */
void rexit()
{
    u.u_arg[0] = u.u_ar0[R0] << 8;
    exit();
}

/*
 * Release resources.
 * Save u. area for parent to look at.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
void exit()
{
    int *w, a;
    struct proc *p, *q;
    struct buf *bp;

    u.u_procp->p_flag &= ~STRC;
    for(w = &u.u_signal[0]; w < &u.u_signal[NSIG];)
        *w++ = 1;
    for(w = &u.u_ofile[0]; w < &u.u_ofile[NOFILE]; w++)
        if(a = *w) {
            *w = NULL;
            closef((struct file *)a);
        }
    iput(u.u_cdir);
    xfree();
    a = malloc(swapmap, 1);
    if(a == NULL)
        panic("out of swap");
    bp = getblk(swapdev, a);
    bcopy(&u, bp->b_addr, 256);
    bwrite(bp);
    q = u.u_procp;
    mfree(coremap, q->p_size, q->p_addr);
    q->p_tsize = 0;             /* the text went with xfree above */
    q->p_taddr = 0;
    q->p_addr = a;
    q->p_stat = SZOMB;

loop:
    for(p = &proc[0]; p < &proc[NPROC]; p++)
    if(q->p_ppid == p->p_pid) {
        wakeup(&proc[1]);
        wakeup(p);
        for(p = &proc[0]; p < &proc[NPROC]; p++)
        if(q->p_pid == p->p_ppid) {
            p->p_ppid  = 1;
            if (p->p_stat == SSTOP)
                setrun(p);
        }
        swtch();
        /* no return */
    }
    q->p_ppid = 1;
    goto loop;
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 * Look also for stopped (traced) children,
 * and pass back status from them.
 */
void wait()
{
    int f;
    struct proc *p;
    struct buf *bp;
    struct user *pu;

    f = 0;

loop:
    for(p = &proc[0]; p < &proc[NPROC]; p++)
    if(p->p_ppid == u.u_procp->p_pid) {
        f++;
        if(p->p_stat == SZOMB) {
            u.u_ar0[R0] = p->p_pid;
            bp = bread(swapdev, f=p->p_addr);
            mfree(swapmap, 1, f);
            p->p_stat = NULL;
            p->p_pid = 0;
            p->p_ppid = 0;
            p->p_sig = 0;
            p->p_ttyp = 0;
            p->p_flag = 0;
            pu = bp->b_addr;
            u.u_cstime[0] += pu->u_cstime[0];
            dpadd(u.u_cstime, pu->u_cstime[1]);
            dpadd(u.u_cstime, pu->u_stime);
            u.u_cutime[0] += pu->u_cutime[0];
            dpadd(u.u_cutime, pu->u_cutime[1]);
            dpadd(u.u_cutime, pu->u_utime);
            u.u_ar0[R1] = pu->u_arg[0];
            brelse(bp);
            return;
        }
        if(p->p_stat == SSTOP) {
            if((p->p_flag&SWTED) == 0) {
                p->p_flag |= SWTED;
                u.u_ar0[R0] = p->p_pid;
                u.u_ar0[R1] = (p->p_sig<<8) | 0177;
                return;
            }
            p->p_flag &= ~(STRC|SWTED);
            setrun(p);
        }
    }
    if(f) {
        sleep(u.u_procp, PWAIT);
        goto loop;
    }
    u.u_error = ECHILD;
}

/*
 * fork system call.
 */
void fork()
{
    register struct proc *p1, *p2;

    p1 = u.u_procp;
    for(p2 = &proc[0]; p2 < &proc[NPROC]; p2++)
        if(p2->p_stat == NULL)
            goto found;
    u.u_error = EAGAIN;
    return;

found:
    if(newproc()) {
        u.u_ar0[R0] = 0;
        u.u_ar0[R1] = p1->p_pid;
        u.u_cstime[0] = 0;
        u.u_cstime[1] = 0;
        u.u_stime = 0;
        u.u_cutime[0] = 0;
        u.u_cutime[1] = 0;
        u.u_utime = 0;
        return;
    }
    u.u_ar0[R0] = p2->p_pid;
}

/*
 * break system call.
 *  -- bad planning: "break" is a dirty word in C.
 *
 * The whole DSEG-byte data segment is allocated at exec time, so the break
 * is only a bookkeeping mark: moving it hands the caller memory it already
 * owns.  Nothing is checked -- the heap and the stack grow toward each
 * other through the same hole and there is no MMU to catch them meeting,
 * exactly as stack overflow already goes undetected.
 */
void sbreak()
{
    u.u_dsize = (uint)u.u_arg[0];
}
