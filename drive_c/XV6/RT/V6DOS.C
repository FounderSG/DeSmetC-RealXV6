/* V6DOS.C -- DeSmet CSTDIO-compatible layer over RealXV6 syscalls, so
 * the unmodified DeSmet toolchain sources (BIND.C, LIB88.C, ...) link
 * and run on RealXV6 without pulling any DOS module from CSTDIO.S.
 *
 * DeSmet stdio model: FILE is just the file descriptor (stdin 0,
 * stdout 1, stderr 2 -- identical to V6 fds), so getc/putc map
 * straight onto read/write.  Unbuffered for now: correct first, fast
 * later (BIND/LIB88 do their bulk I/O through read/write anyway).
 *
 * Also provides the csetup-era memory primitives the tools' arena
 * idiom needs: "inext = _memory(); memlast = _showsp() - 512;".  On
 * DOS everything between BSS and SP is addressable; on RealXV6 pages
 * beyond the break are unmapped, so _memory() first grows the break
 * as close to the stack as the kernel allows.
 */
#include "UNIX.H"

getc(fd)
int fd;
{
    char c;

    if (read(fd, &c, 1) != 1)
        return -1;
    return c;               /* char is unsigned: 0..255, never -1 */
}

putc(c, fd)
int c, fd;
{
    char b;

    b = c;
    if (write(fd, &b, 1) != 1)
        return -1;
    return c;
}

putchar(c)
int c;
{
    return putc(c, 1);
}

/* DeSmet puts does not append a newline (callers embed \n). */
puts(s)
char *s;
{
    return write(1, s, strlen(s));
}

fputs(s, fd)
char *s;
int fd;
{
    return write(fd, s, strlen(s));
}

fflush(fd)
int fd;
{
    return 0;               /* unbuffered */
}

toupper(c)
int c;
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

/* DeSmet fopen: FILE is the fd; returns 0 on failure.  Modes r/w/a
 * (text = binary on V6).  Provided so tool links pull nothing from
 * CSTDIO's DOS fopen module chain: its DSEG data lands on the same
 * addresses BIND gives small uninitialized variables (GEN's option
 * flags read string bytes -- see the compiler-port notes).  creat()
 * here is the 1-arg DOS flavor from DCOMPAT.C, which every tool
 * link includes. */
fopen(name, mode)
char *name, *mode;
{
    int fd, c;

    c = *mode;
    if (c >= 'A' && c <= 'Z')
        c += 'a' - 'A';
    if (c == 'r')
        fd = open(name, 0);
    else if (c == 'a') {
        fd = open(name, 1);
        if (fd >= 0)
            seek(fd, 0, 2);
        else
            fd = creat(name);
    } else
        fd = creat(name);
    if (fd < 0)
        return 0;
    return fd;
}

/* CSTDIO's math helpers (_DIV4 and friends) call rerrno on runtime
 * faults (divide by zero, float errors); the DOS module prints
 * "*Divide by Zero*"-style messages and exits.  Defining it here keeps
 * BIND from pulling that module: its DSEG strings land on the same
 * addresses BIND gives small uninitialized variables.  Fatal, like
 * the original. */
rerrno()
{
    puts("\nruntime error (rerrno)\n");
    exit(2);
}

/* long lseek(fd, offset, mode) -- V6 seek() takes 16-bit offsets, so
 * an absolute long seek is done as block seek (flag 3, x512) plus a
 * relative byte seek (flag 1).  The tools only use mode 0 with
 * non-negative offsets; modes 1/2 are passed through and limited to
 * 16-bit offsets.  Returns the new location for mode 0 (or 0 for
 * modes 1/2), -1L on error.
 */
long
lseek(fd, offset, mode)
int fd;
long offset;
int mode;
{
    if (mode == 0) {
        seek(fd, (int)(offset / 512), 3);
        if (r3)
            return -1L;
        seek(fd, (int)(offset % 512), 1);
        if (r3)
            return -1L;
        return offset;
    }
    seek(fd, (int)offset, mode);
    if (r3)
        return -1L;
    return 0L;
}

/* csetup-era memory primitives ------------------------------------- */

static char *membase;

char *
_memory()
{
    unsigned target;
    unsigned _showsp();

    if (membase == 0) {
        membase = (char *)endbrk;
        /* map the DOS-style flat arena: grow the break toward the
         * stack until the kernel refuses (stack pages are pinned at
         * the top of the window; leave it headroom) */
        target = (_showsp() - 2048) & ~511;
        while (target > endbrk) {
            if (brk(target) == 0)
                break;
            target -= 512;
        }
    }
    return membase;
}

_setmem(dst, n, ch)
char *dst;
unsigned n;
int ch;
{
    while (n--)
        *dst++ = ch;
}

_move(n, src, dst)
unsigned n;
char *src, *dst;
{
    memmove(dst, src, n);
}
