/* SYSCALL.C -- RealXV6 system call wrappers for DeSmet C 3.03.
 * K&R port of RealXV6 usr/syscall.c.  Differences from the Watcom copy:
 *   - r0/r1/r3/errno are defined in CRT0.A (asm), declared in UNIX.H;
 *   - syscall() itself is SYSCALL_ in CRT0.A (cdecl: fn -> DX, r0 -> AX,
 *     remaining args read from the user stack by the kernel);
 *   - the end-of-BSS value sbrk() needs comes from the CRT0.A word
 *     ENDBRK_, patched by d2a with the linked image's data total.
 */
#include "UNIX.H"

#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_wait        7
#define SYS_creat       8
#define SYS_link        9
#define SYS_unlink     10
#define SYS_exec       11
#define SYS_chdir      12
#define SYS_time       13
#define SYS_mknod      14
#define SYS_chmod      15
#define SYS_chown      16
#define SYS_break      17
#define SYS_stat       18
#define SYS_seek       19
#define SYS_getpid     20
#define SYS_mount      21
#define SYS_umount     22
#define SYS_setuid     23
#define SYS_getuid     24
#define SYS_stime      25
#define SYS_ptrace     26

#define SYS_fstat      28
#define SYS_stty       31
#define SYS_gtty       32

#define SYS_nice       34
#define SYS_sleep      35
#define SYS_sync       36
#define SYS_kill       37
#define SYS_switch     38
#define SYS_psinfo     39

#define SYS_dup        41
#define SYS_pipe       42
#define SYS_times      43
#define SYS_prof       44
#define SYS_setgid     46
#define SYS_getgid     47
#define SYS_sig        48

/* exit/exit1/creat live in UCOMPAT.C (V6 flavor, user programs) or
 * DCOMPAT.C (DOS flavor, DeSmet tool ports) -- the two worlds disagree
 * on their signatures, so each link picks one compat module. */

fork()
{
    return syscall(SYS_fork, -1);
}

/* open() lives in the compat modules, not here: the DeSmet tool ports
 * (DCOMPAT) get a case-insensitive open (DOS/FAT folds case, V6 does not,
 * and the tool sources #include one header under both "nodes.h" and
 * "NODES.H"); user programs (UCOMPAT) get a plain verbatim open.  Both
 * cannot share one definition without a multiply-defined symbol, so open
 * follows the same split as exit/creat/wait/pipe. */

read(fd, buf, len)
int fd;
char *buf;
int len;
{
    return syscall(SYS_read, fd, buf, len);
}

write(fd, buf, len)
int fd;
char *buf;
int len;
{
    return syscall(SYS_write, fd, buf, len);
}

close(fd)
int fd;
{
    syscall(SYS_close, fd);
    return r3;
}

/* wait()/waits() live in UCOMPAT.C: ASM88's assembler (ASM3.C) defines
 * its own wait() -- it emits the x87 WAIT instruction byte -- which would
 * be multiply-defined against this syscall wrapper in the tool link (the
 * same reason pipe() is kept out, see below).  User programs link UCOMPAT
 * and get the process-wait wrappers there. */

link(filelink, filenew)
char *filelink, *filenew;
{
    syscall(SYS_link, -1, filelink, filenew);
    return r3;
}

unlink(filename)
char *filename;
{
    syscall(SYS_unlink, -1, filename);
    return r3;
}

exec(file, argv)
char *file;
char *argv[];
{
    return syscall(SYS_exec, -1, file, argv);
}

chdir(dirname)
char *dirname;
{
    syscall(SYS_chdir, -1, dirname);
    return r3;
}

time(tim)
int tim[];
{
    syscall(SYS_time, -1);
    tim[0] = r0;
    tim[1] = r1;
    return 1;
}

mknod(filename, mode, dev)
char *filename;
uint mode;
int dev;
{
    syscall(SYS_mknod, -1, filename, mode, dev);
    return r3;
}

chmod(filename, mode)
char *filename;
uint mode;
{
    syscall(SYS_chmod, -1, filename, mode);
    return r3;
}

chown(filename, uid, gid)
char *filename;
int uid, gid;
{
    int owner;

    owner = (gid << 8) | uid;
    syscall(SYS_chown, -1, filename, owner);
    return r3;
}

brk(addr)
char *addr;
{
    syscall(SYS_break, -1, addr);
    return r3;
}

/* endbrk (end of BSS = initial break) is declared in UNIX.H and
 * patched into CRT0.A's data by d2a. */
char *
sbrk(incr)
int incr;
{
    static char *cur;
    char *old;

    if (cur == 0)
        cur = (char *)endbrk;   /* first call: break starts at end of BSS */
    old = cur;
    if (brk(cur + incr) != 0)
        return (char *)-1;
    cur += incr;
    return old;
}

stat(filename, buf)
char *filename;
char *buf;
{
    syscall(SYS_stat, -1, filename, buf);
    return r3;
}

seek(fd, offset, flag)
int fd, offset, flag;
{
    return syscall(SYS_seek, fd, offset, flag);
}

getpid()
{
    return syscall(SYS_getpid, -1);
}

mount(pDevFile, pMountDir, flag)
char *pDevFile, *pMountDir;
int flag;
{
    syscall(SYS_mount, -1, pDevFile, pMountDir, flag);
    return r3;
}

umount(pDevFile)
char *pDevFile;
{
    syscall(SYS_umount, -1, pDevFile);
    return r3;
}

setuid(uid)
int uid;
{
    syscall(SYS_setuid, uid);
    return r3;
}

getuid()
{
    return syscall(SYS_getuid, -1);
}

fstat(fd, buf)
int fd;
char *buf;
{
    syscall(SYS_fstat, fd, buf);
    return r3;
}

stty(fd, buf)
int fd;
char *buf;
{
    syscall(SYS_stty, fd, buf);
    return r3;
}

gtty(fd, buf)
int fd;
char *buf;
{
    syscall(SYS_gtty, fd, buf);
    return r3;
}

nice(value)
int value;
{
    syscall(SYS_nice, value);
    return r3;
}

sleep(nTicks)
int nTicks;
{
    syscall(SYS_sleep, nTicks);
    return r3;
}

sync()
{
    syscall(SYS_sync, -1);
    return r3;
}

kill(pid, signalNo)
int pid, signalNo;
{
    syscall(SYS_kill, pid, signalNo);
    return r3;
}

psinfo(index, buf)
int index;
char *buf;
{
    return syscall(SYS_psinfo, index, buf);
}

dup(fd)
int fd;
{
    return syscall(SYS_dup, fd);
}

/* pipe() lives in UCOMPAT.C: GEN6.C has a global array named `pipe`,
 * so the wrapper must stay out of the modules the tool ports link. */

setgid(gid)
int gid;
{
    syscall(SYS_setgid, gid);
    return r3;
}

getgid()
{
    return syscall(SYS_getgid, -1);
}

char *
signal(signalNo, handler)
int signalNo;
int (*handler)();
{
    return (char *)syscall(SYS_sig, -1, signalNo, handler);
}
