/* UCOMPAT.C -- V6/Watcom-flavor exit and creat for RealXV6 user
 * programs (matches usr/syscall.c signatures).  DeSmet tool ports link
 * DCOMPAT.C instead: the DOS world's exit takes a status and creat has
 * no mode argument, so the two flavors cannot share one module.
 */
#include "UNIX.H"

#define SYS_exit   1
#define SYS_open   5
#define SYS_wait   7
#define SYS_creat  8
#define SYS_pipe  42

exit()
{
    syscall(SYS_exit, 0);
}

/* Plain verbatim open (see SYSCALL.C): user programs address files by
 * their exact V6 names, so no case folding -- the DOS-compat fold is the
 * DCOMPAT flavor only. */
open(name, mode)
char *name;
int mode;
{
    return syscall(SYS_open, -1, name, mode);
}

/* Here rather than SYSCALL.C: ASM88's assembler (ASM3.C) has its own
 * wait() that emits the x87 WAIT byte, so this syscall wrapper must stay
 * out of the modules the tool ports link (same reason as pipe below). */
wait()
{
    syscall(SYS_wait, -1);
    return r0 < 0 ? -1 : r0;
}

waits(status)
int *status;
{
    syscall(SYS_wait, -1);
    if (status != 0)
        *status = r1;
    return r0 < 0 ? -1 : r0;
}

exit1(code)
int code;
{
    syscall(SYS_exit, code);
}

creat(filename, mode)
char *filename;
int mode;
{
    return syscall(SYS_creat, -1, filename, mode);
}

/* Here rather than SYSCALL.C: GEN6.C has a global array named `pipe`,
 * and the tool ports (which link DCOMPAT, not this module) must not
 * see a conflicting definition. */
pipe(fd)
int fd[];
{
    syscall(SYS_pipe, -1);
    if (r3 == 0) {
        fd[0] = r0;
        fd[1] = r1;
    }
    return r3;
}
