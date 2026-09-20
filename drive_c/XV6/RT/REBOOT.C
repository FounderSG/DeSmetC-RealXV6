/* REBOOT.C -- take the machine down and bring it back up on whatever is on
 * the boot floppy now.  This is how RealXV6 boots a kernel it built itself:
 * mkkern compiles one, mkimg lays it on a 360K image, that image is copied
 * to /dev/fd0, and this resets the machine so the BIOS reads it back.
 *
 * The flushing is the part that matters, and it is not done here.  sync(2)
 * only starts the writes -- update() hands the delayed-write buffers to the
 * disk queue B_ASYNC and returns with the transfers still in flight -- so a
 * program that called sync() and then reset the machine would leave behind
 * exactly the corruption it meant to avoid.  reboot(2) does the flush and
 * the reset in one call, without returning to user mode in between; see
 * KEN/SYS4.C.
 *
 * The sync() below is therefore not the guarantee.  It is there to get the
 * bulk of the cache onto the disk while the system is still ordinary, so
 * that the uninterruptible part of reboot(2) has little left to do.
 *
 * reboot(2) returns only on failure: EPERM if not root, EBUSY if another
 * update() holds updlock -- which would make reboot's own update() a no-op
 * and reset on a cache nobody flushed.  EBUSY is worth retrying, and the
 * retry is the reason this is a program rather than one line of shell.
 */
#include "UNIX.H"

#define TRIES   15              /* ~15 seconds of a busy updater */

/* reboot(2) is this kernel's addition -- slot 49 of sysent, the one the V6
 * halt sites left free -- so its wrapper is here, with its only caller,
 * rather than in SYSCALL.C.  That file is compiled into every program,
 * including ones built against a kernel whose slot 49 is still nosys; it
 * is the same split exit/creat make into UCOMPAT.C and DCOMPAT.C. */
#define SYS_reboot      49

reboot()
{
    return syscall(SYS_reboot, -1);
}

/* Wait for the clock to tick over one second.  There is no sleep(3) in this
 * runtime and none is worth adding for one caller; time(2) is enough. */
nap()
{
    int now[2], then[2];

    time(then);
    do
        time(now);
    while(now[1] == then[1] && now[0] == then[0]);
}

main(argc, argv)
int argc;
char *argv[];
{
    int i;

    sync();
    printf("rebooting\n");
    for(i = 0; i < TRIES; i++) {
        reboot();               /* returns only if it could not */
        printf("reboot: disks busy\n");
        nap();
    }
    printf("reboot: giving up -- the disks never went quiet\n");
    return 1;
}
