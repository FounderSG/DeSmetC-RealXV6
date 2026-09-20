/*
 * Everything in this file is a routine implementing a system call.
 */

#include <os.h>

void getswit()
{
    u.u_ar0[R0] = getps();
}

void gtime()
{
    u.u_ar0[R0] = time[0];
    u.u_ar0[R1] = time[1];
}

void stime()
{
    if(suser()) {
        time[0] = u.u_ar0[R0];
        time[1] = u.u_ar0[R1];
        wakeup(tout);
    }
}

void setuid()
{
    char uid;

    uid = u.u_ar0[R0] & 0xff;
    if(u.u_ruid == uid || suser()) {
        u.u_uid = uid;
        u.u_procp->p_uid = uid;
        u.u_ruid = uid;
    }
}

void getuid()
{
    u.u_ar0[R0] = (u.u_uid << 8) | u.u_ruid;
}

void setgid()
{
    char gid;

    gid = u.u_ar0[R0] & 0xff;
    if(u.u_rgid == gid || suser()) {
        u.u_gid = gid;
        u.u_rgid = gid;
    }
}

void getgid()
{
    u.u_ar0[R0] = (u.u_gid<<8) + u.u_rgid;
}

void getpid()
{
    u.u_ar0[R0] = u.u_procp->p_pid;
}

void sync()
{
    update();
}

void nice()
{
    int n;

    n = u.u_ar0[R0];
    if(n > 20)
        n = 20;
    /* V6 clamps only the top and lets the truncation into a char bound the
     * bottom.  p_nice is an int here, so bound it explicitly: without this a
     * superuser's nice(-30000) would reach setpri, which clamps no low end
     * either, and pin p_pri below PSWP for good.  -20 keeps a raised process
     * at p >= 80, still under every kernel sleep priority. */
    if(n < -20)
        n = -20;
    if(n < 0 && !suser())
        n = 0;
    u.u_procp->p_nice = n;
}

/*
 * Unlink system call.
 * panic: unlink -- "cannot happen"
 */
void unlink()
{
    struct inode *ip, *pp;

    pp = namei(&uchar, 2);
    if(pp == NULL)
        return;
    prele(pp);
    ip = iget(pp->i_dev, u.u_dent.u_ino);
    if(ip == NULL)
        panic("unlink -- iget");
    if((ip->i_mode&IFMT)==IFDIR && !suser())
        goto out;
    u.u_offset[1] -= DIRSIZ+2;
    u.u_base = (char *)&u.u_dent;
    u.u_count = DIRSIZ+2;
    u.u_dent.u_ino = 0;
    writei(pp);
    ip->i_nlink--;
    ip->i_flag |= IUPD;

out:
    iput(pp);
    iput(ip);
}

void chdir()
{
    struct inode *ip;

    ip = namei(&uchar, 0);
    if(ip == NULL)
        return;
    if((ip->i_mode&IFMT) != IFDIR) {
        u.u_error = ENOTDIR;
    bad:
        iput(ip);
        return;
    }
    if(access(ip, IEXEC))
        goto bad;
    iput(u.u_cdir);
    u.u_cdir = ip;
    prele(ip);
}

void chmod()
{
    struct inode *ip;

    if ((ip = owner()) == NULL)
        return;
    ip->i_mode &= ~07777;
    if (u.u_uid)
        u.u_arg[1] &= ~ISVTX;
    ip->i_mode |= u.u_arg[1]&07777;
    ip->i_flag |= IUPD;
    iput(ip);
}

void chown()
{
    struct inode *ip;

    if (!suser() || (ip = owner()) == NULL)
        return;
    ip->i_uid = u.u_arg[1] & 0xff;
    ip->i_gid = u.u_arg[1] >> 8;
    ip->i_flag |= IUPD;
    iput(ip);
}

/*
 * Change modified date of file:
 * time to r0-r1; sys smdate; file
 * This call has been withdrawn because it messes up
 * incremental dumps (pseudo-old files aren't dumped).
 * It works though and you can uncomment it if you like.

smdate()
{
    register struct inode *ip;
    register int *tp;
    int tbuf[2];

    if ((ip = owner()) == NULL)
        return;
    ip->i_flag =| IUPD;
    tp = &tbuf[2];
    *--tp = u.u_ar0[R1];
    *--tp = u.u_ar0[R0];
    iupdat(ip, tp);
    ip->i_flag =& ~IUPD;
    iput(ip);
}
*/

void ssig()
{
    int a;

    a = u.u_arg[0];
    if(a<=0 || a>=NSIG || a ==SIGKIL) {
        u.u_error = EINVAL;
        return;
    }
    u.u_ar0[R0] = u.u_signal[a];
    u.u_signal[a] = u.u_arg[1];
    if(u.u_procp->p_sig == a)
        u.u_procp->p_sig = 0;
}

void kill()
{
    register struct proc *p, *q;
    int a;
    int f;

    f = 0;
    a = u.u_ar0[R0];
    q = u.u_procp;
    for(p = &proc[0]; p < &proc[NPROC]; p++) {
        if(p == q)
            continue;
        if(a != 0 && p->p_pid != a)
            continue;
        if(a == 0 && (p->p_ttyp != q->p_ttyp || p <= &proc[1]))
            continue;
        if(u.u_uid != 0 && u.u_uid != p->p_uid)
            continue;
        f++;
        psignal(p, u.u_arg[0]);
    }
    if(f == 0)
        u.u_error = ESRCH;
}

void times()
{
    int *p;

    for(p = &u.u_utime; p  < &u.u_utime+6;) {
        suword(u.u_arg[0], *p++);
        u.u_arg[0] += 2;
    }
}

#ifdef PDP11
profil()
{
    u.u_prof[0] = u.u_arg[0] & ~1;  /* base of sample buf */
    u.u_prof[1] = u.u_arg[1];   /* size of same */
    u.u_prof[2] = u.u_arg[2];   /* pc offset */
    u.u_prof[3] = (u.u_arg[3]>>1) & 077777; /* pc scale */
}
#endif

/*
 * psinfo system call -- everything ps needs about proc[idx].  ps never
 * reads /dev/mem or /dev/kmem and never needs to know the physical memory
 * layout, nor any kernel header: it carries its own copy of the struct
 * below, extended with the 512-byte frame image that follows it in the
 * caller's buffer, so a new field belongs ahead of that image in both.
 * Filling the fields one at a time keeps struct proc private to the
 * kernel.  Every scalar is int: p_pri is signed, and a char field would
 * carry that sign only by the compiler's choice -- DeSmet's plain char is
 * unsigned, Watcom's is signed only under -j.  That difference is what
 * made the old raw copy unshareable: this kernel's struct proc is 29
 * bytes where upstream's is 28, p_pri and p_nice being int here.
 *
 * R0 = index into proc[]; u_arg[0] = destination buffer, laid out as
 *   [ struct psbuf ][ 512-byte stack top ]
 * R0 returns 0; an index past the end of the proc table is an EINVAL
 * error, which is how a caller finds the end.
 *
 * stkbase reports the user address the image came from, or 0 when no
 * frame was captured: a free slot, a process carrying p_tsize==0, or an
 * unreadable swap block.  p_tsize==0 means a zombie -- exit clears it
 * along with the text and leaves p_addr pointing at a one-block image of
 * u, which holds no argument frame -- or proc 0, which never exec'd (exec
 * rejects an EXE with no text, so every other live process has one).  ps
 * consults stkbase before parsing, so a stale image left in the caller's
 * buffer by an earlier call is never mistaken for this process's
 * arguments.
 *
 * The stack top is the data-segment range [DSEG-512, DSEG), the same for
 * every process, which sits at block offset UPREFIX + that (the u-area
 * prefix comes first).  In core the copy is a non-blocking memcpy,
 * and the kernel is non-preemptive, so it cannot race the swapper.  For
 * a swapped-out process the stack sectors are read from the swap device,
 * which sleeps; the process may be swapped back in, or the slot reused,
 * meanwhile.  Snapshot {p_pid,p_addr,SLOAD} across the reads and retry if
 * anything moved; a retry re-reads the slot from the top, so it also
 * copes with the slot now holding a different process, or none at all.
 */
struct psbuf
{
    int     p_stat;         /* 0 marks a free slot */
    int     p_flag;
    int     p_pri;          /* priority, negative is high */
    int     p_uid;
    int     p_pid;
    int     p_ppid;
    int     p_addr;
    int     p_wchan;
    int     stkbase;        /* user address the frame came from, 0 = none */
};

void psinfo()
{
    struct proc *p;
    struct buf *bp;
    struct psbuf pb;
    int idx, oaddr, opid;
    uint udst, sdst, off;

    idx = u.u_ar0[R0];
    udst = (uint)u.u_arg[0];
    if(idx < 0 || idx >= NPROC) {
        u.u_error = EINVAL;
        return;
    }
    p = &proc[idx];
    sdst = udst + sizeof(pb);
    off = UPREFIX + DSEG - 512;         /* block offset of the stack window */
    pb.stkbase = 0;                     /* no frame captured yet */

loop:
    if(p->p_stat == 0 || p->p_tsize == 0)
        goto out;                       /* free slot, or no EXE frame */
    oaddr = p->p_addr;
    opid  = p->p_pid;
    if(p->p_flag & SLOAD) {
        fmemcpy(udseg(u.u_procp), sdst,
                (uint)oaddr*(PAGESIZ/16), off, 512);
    } else {
        /* the stack run ends the swap image, so the window is its last
         * sector -- the hole in between was never written (swsize) */
        bp = bread(swapdev,
                   oaddr + (p->p_ndpg + p->p_nspg)*(PAGESIZ/512) - 1);
        if(p->p_pid != opid || (p->p_flag&SLOAD) || p->p_addr != oaddr) {
            brelse(bp);                 /* swapped in while we slept; reread */
            goto loop;
        }
        if(bp->b_flags & B_ERROR) {
            brelse(bp);
            goto out;                   /* unreadable: stkbase stays 0 */
        }
        copyout((uint)bp->b_addr, sdst, 512);
        brelse(bp);
    }
    pb.stkbase = DSEG - 512;

    /*
     * Read the scalars only now that the frame has settled.  bread sleeps,
     * and a retry can find the slot holding a different process, so a
     * snapshot taken before the read could describe one process while the
     * frame beside it came from another.  Nothing below sleeps, so the two
     * halves always describe the same instant.
     */
out:
    pb.p_stat = p->p_stat;
    pb.p_flag = p->p_flag & 0377;
    pb.p_pri = p->p_pri;
    pb.p_uid = p->p_uid & 0377;
    pb.p_pid = p->p_pid;
    pb.p_ppid = p->p_ppid;
    pb.p_addr = p->p_addr;
    pb.p_wchan = p->p_wchan;
    copyout((uint)&pb, udst, sizeof(pb));
    u.u_ar0[R0] = 0;
}

/*
 * reboot -- write the buffer cache out to the disk, then reset the machine.
 *
 * A user program cannot do this for itself, and that is the whole reason
 * this call exists.  sync(2) only *starts* the writes: update() ends in
 * bflush(), which marks each delayed-write buffer B_ASYNC, hands it to
 * rkstrategy and returns while the transfers are still queued on rktab --
 * the IDE path finishes them one at a time from ideintr.  Reset there and
 * every block that had not reached the platter is lost, which is exactly
 * the corruption a clean shutdown is supposed to prevent.
 *
 * So the flush and the reset have to happen without going back to user
 * mode in between.  update() first; then wait for the writes it started.
 *
 * What to wait ON is the whole subtlety.  Not B_BUSY: a buffer is busy from
 * notavail() until whoever took it gives it back, and two of them are never
 * given back at all -- iinit() holds the root superblock in mount[0].m_bufp
 * and smount() holds one per mounted filesystem, each from getblk(NODEV, 0),
 * for as long as the filesystem is mounted.  Waiting for "no buffer is busy"
 * hangs on the first of those and never reaches the reset.
 *
 * B_ASYNC is the right flag.  bflush() sets it on exactly the buffers it
 * hands to the driver without waiting, and brelse() clears it when iodone()
 * gives one back -- so B_ASYNC is "in flight", which is what we mean.  A
 * superblock buffer never carries it.  Reads and synchronous writes need no
 * waiting either: whoever issued them is already in iowait().  brelse()
 * wakes us through B_WANTED, the handshake getblk uses.
 *
 * EBUSY when an update() is already running: updlock would make ours a
 * no-op (update() returns at once) and we would reset on a cache we never
 * flushed.  The caller retries; /bin/reboot does.
 *
 * Nothing dirties a buffer between the wait and the reset here -- reboot
 * runs from the shell with init and sh both asleep in wait().  Killing
 * every other process first, the way V6 sites did it, is what would close
 * that window in general.
 */
void reboot()
{
    register struct buf *bp;

    if(!suser())
        return;
    if(updlock) {
        u.u_error = EBUSY;
        return;
    }
    printf("reboot: flushing\r\n");
    update();
    for(bp = &buf[0]; bp < &buf[NBUF]; bp++) {
again:
        spl6();
        if(bp->b_flags & B_ASYNC) {
            bp->b_flags |= B_WANTED;
            sleep(bp, PRIBIO);
            spl0();
            goto again;
        }
        spl0();
    }
    /*
     * Pulse the 8042 reset line.  That is a real CPU reset, so the BIOS
     * runs POST and re-reads the boot sector: the machine comes up on
     * whatever is on the floppy NOW.  INT 19H would re-enter the bootstrap
     * with this kernel's state still live -- the PIT still dividing, the
     * timer vector still pointing into memory the incoming kernel is about
     * to load over -- and the first tick after the jump would land in it.
     */
    printf("reboot: disks quiet\r\n");
    spl7();
    outportb(0x64, 0xfe);
    for(;;)
        ;
}
