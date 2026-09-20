#include <os.h>

/*
 * Floppy disk driver, through the BIOS.
 *
 * Unlike rk/ide this driver is synchronous: the transfer happens inside
 * fdstrategy and iodone runs before it returns, so there is no request queue
 * and no interrupt entry.  Vector 0EH is still the BIOS's own -- pc_init
 * never claims it and the kernel never touches the PIC mask IRQ6 lives in
 * (the one mask write, uart.c, is under the disabled KL_BACKEND_UART), so
 * INT 13H completes here the way it does under DOS.  The cost is that the
 * kernel stalls for the length of a transfer, which on a single-user V6 is
 * the same wall clock the drive spends seeking either way.
 *
 * Programming the FDC and the 8237 directly, the way ide.c programs the ATA
 * ports, would need a motor/recalibrate/seek/sense state machine, timeout()
 * callouts and a bounce buffer for the 64K DMA boundary -- in a code segment
 * with under 1.8K left.  The BIOS already owns all of it.
 */

#define NFD     2       /* units: /dev/fd0, /dev/fd1 */
#define FDRETRY 3       /* tries per sector, with a reset between */

/* 360K, the format RT/MKBOOT.C lays out and the only geometry the boot
 * sector's LBACHS knows.  Where the probe below starts, and where it stays
 * if the drive will not answer. */
#define FDSEC   9
#define FDTRK   40
#define FDHEAD  2
#define FDTRK80 80      /* every format above 360K has 80 tracks */

#define FDREAD  2       /* INT 13H AH */
#define FDWRITE 3

/*
 * getblk panics on a null d_tab and binit heads this major's buffer list in
 * it, so the devtab is not optional even though d_actf/d_active never move.
 */
struct devtab fdtab;

/* int, not char: DeSmet's char is unsigned, so a -1 sentinel would read back
 * as 255 and both of the tests below would be dead code. */
static int fdstate[NFD];        /* 0 not probed, 1 usable, -1 no such drive */
static char fdknown[NFD];       /* geometry has been announced once */
static int fdnsec[NFD];
static int fdntrk[NFD];
static int fdnhead[NFD];
static char fdbuf[512];         /* scratch sector for the geometry probe */

/*
 * One sector into the scratch buffer, for probing only.  A failed read
 * latches an error in the controller, so clear it before the next try.
 */
int fdtry(unit, cyl, head, sec)
int unit;
int cyl;
int head;
int sec;
{
    int r;

    r = bios_diskio(FDREAD, unit, cyl, head, sec, 1, core_ds, (uint)fdbuf);
    if(r != 0)
        bios_diskreset(unit);
    return r;
}

/*
 * Geometry, probed off the MEDIUM rather than the drive.
 *
 * INT 13H AH=08H looks like the answer and is not: it reports the drive
 * type.  qemu presents this project's 360K image as a 360K disk in a 1.2M
 * drive -- which is what a real PC did too -- and AH=08H then says 15
 * sectors and 80 tracks for a medium that has 9 and 40.  Every CHS after
 * the first track would be computed against a disk that is not in there.
 *
 * So ask the disk.  The highest sector number that reads on track 0 is its
 * sector count, and reads of head 1 and of track 79 say whether it has
 * them.  That separates all four standard formats with four reads:
 *
 *       9 sec  40 trk  2 hd    720 blocks   360K
 *       9 sec  80 trk  2 hd   1440 blocks   720K
 *      15 sec  80 trk  2 hd   2400 blocks   1.2M
 *      18 sec  80 trk  2 hd   2880 blocks   1.44M
 *
 * AH=08H is still worth one call, for the drive COUNT in DL: it is the only
 * way to tell "no such drive" from "empty drive", and unit 1 on a one-drive
 * machine should be ENXIO rather than a unit whose every transfer fails.
 *
 * Run from fdopen, so a disk swapped between two opens is re-measured; the
 * result is announced only the first time, since a cp per boot would
 * otherwise put a line on the console for nothing.
 * Returns 1 if the unit is usable, -1 if there is no such drive.
 */
int fdprobe(unit)
int unit;
{
    int res[2];
    int i;
    static int trysec[] = { 18, 15, 9, 0 };

    if(fdstate[unit] < 0)
        return -1;              /* a drive does not appear later */

    fdnsec[unit] = FDSEC;
    fdntrk[unit] = FDTRK;
    fdnhead[unit] = FDHEAD;
    fdstate[unit] = 1;

    if(bios_diskparm(unit, res) >= 0 && unit >= (res[1] & 0377)) {
        fdstate[unit] = -1;
        return -1;
    }

    /* The first access after a disk change always reports one; spend it
     * here rather than on the caller's first real block. */
    bios_diskreset(unit);
    fdtry(unit, 0, 0, 1);

    for(i = 0; trysec[i] != 0; i++)
        if(fdtry(unit, 0, 0, trysec[i]) == 0) {
            fdnsec[unit] = trysec[i];
            break;
        }
    if(fdtry(unit, 0, 1, 1) != 0)
        fdnhead[unit] = 1;
    if(fdtry(unit, FDTRK80 - 1, 0, 1) == 0)
        fdntrk[unit] = FDTRK80;

    if(fdknown[unit] == 0) {
        fdknown[unit] = 1;
        printf("fd%l: %l sec %l trk %l hd\n", unit,
            fdnsec[unit], fdntrk[unit], fdnhead[unit]);
    }
    return 1;
}

/*
 * Blocks on the unit.  Only meaningful after fdprobe; every path here runs
 * through fdopen first, since openi is the only way to reach a block special.
 */
int fdsize(dev)
int dev;
{
    register int unit;

    unit = minor(dev);
    return fdnsec[unit] * fdntrk[unit] * fdnhead[unit];
}

int fdopen(dev, rw)
int dev;
int rw;
{
    if(minor(dev) >= NFD) {
        u.u_error = ENXIO;
        return 0;
    }
    if(fdprobe(minor(dev)) < 0)
        u.u_error = ENXIO;
    return 0;
}

void fdstrategy(abp)
struct buf *abp;
{
    register struct buf *bp;
    int unit, blk, cyl, head, sec, cmd, r, try;

    bp = abp;
    unit = minor(bp->b_dev);
    if(unit >= NFD)
        goto bad;
    /*
     * B_PHYS should never arrive: the floppy is neither swapdev nor backed
     * by a raw character device.  It is refused rather than mapped because
     * physio's absolute segment:offset buffer can straddle a 64K DMA page,
     * which a buffer-cache buffer -- always inside the kernel's own segment,
     * itself one whole 64K page under the .COM layout -- cannot.
     */
    if(bp->b_flags&B_PHYS)
        goto bad;
    /* Not fdprobe(): the probe belongs to open, and re-measuring the disk
     * per block would cost four BIOS reads for every one asked for.  openi
     * is the only way to reach a block special, so fdopen has always run. */
    if(fdstate[unit] <= 0)
        goto bad;

    /* writei/readi do not bounds-check a block special (rdwri.c), so the
     * end of the medium is the driver's to report.  Note that readi then
     * iomoves the buffer before it notices u_error, so a caller sees the
     * refusal in errno and NOT in the byte count -- see RT/FDTEST.C. */
    blk = bp->b_blkno;
    if(blk < 0 || blk >= fdsize(bp->b_dev))
        goto bad;

    /* One V6 block is one 512-byte sector, so the block number IS the LBA. */
    sec = blk % fdnsec[unit] + 1;
    blk = blk / fdnsec[unit];
    head = blk % fdnhead[unit];
    cyl = blk / fdnhead[unit];

    cmd = FDWRITE;
    if(bp->b_flags&B_READ)
        cmd = FDREAD;

    /* b_addr is an offset into the kernel's data segment, as in rk.c's
     * devstart, so the segment half of the BIOS buffer address is core_ds. */
    r = 0;
    for(try = 0; try < FDRETRY; try++) {
        r = bios_diskio(cmd, unit, cyl, head, sec, 1,
                        core_ds, (uint)bp->b_addr);
        if(r == 0)
            break;
        /* AH=00H also clears a latched "media changed" (06H), which is what
         * the first access after a disk swap always reports. */
        bios_diskreset(unit);
    }
    if(r != 0) {
        deverror(bp, r, 0);
        bp->b_flags |= B_ERROR;
    }
    iodone(bp);
    return;

bad:
    bp->b_flags |= B_ERROR;
    iodone(bp);
}
