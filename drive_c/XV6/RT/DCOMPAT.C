/* DCOMPAT.C -- DOS/DeSmet-flavor exit and creat, for running the
 * DeSmet toolchain (BIND.C, LIB88.C, ...) on RealXV6 with unmodified
 * sources.  DeSmet's exit(status) passes an exit code and its
 * creat(name) has no mode argument; user programs link UCOMPAT.C
 * instead (see there).
 */
#include "UNIX.H"

#define SYS_exit   1
#define SYS_open   5
#define SYS_creat  8

exit(code)
int code;
{
    syscall(SYS_exit, code);
}

/* Copy src to dst, folding to UPPER (up != 0) or lower (up == 0) case. */
static
foldcase(dst, src, up)
char *dst, *src;
int up;
{
    int c;

    while ((c = *src++) != 0) {
        if (up) {
            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';
        } else {
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
        }
        *dst++ = c;
    }
    *dst = 0;
}

/* DOS/FAT is case-insensitive; RealXV6 is not.  The DeSmet toolchain
 * sources #include one header under mixed spellings ("nodes.h" in some
 * passes, "NODES.H" in others) and open() would miss the second on V6.
 * Emulate DOS: try the name as written, then all-UPPER, then all-lower.
 * Verbatim-first means an open that already resolves is never redirected,
 * so a tool's output stays byte-identical to the DOS build; the fold can
 * only rescue an open that would otherwise have failed. */
open(name, mode)
char *name;
int mode;
{
    int fd;
    char alt[256];

    if ((fd = syscall(SYS_open, -1, name, mode)) >= 0)
        return fd;
    foldcase(alt, name, 1);
    if ((fd = syscall(SYS_open, -1, alt, mode)) >= 0)
        return fd;
    foldcase(alt, name, 0);
    return syscall(SYS_open, -1, alt, mode);
}

/* DOS creat() returns a READ/WRITE handle; the V6 creat syscall opens
 * write-only.  GEN6's internal assembler creats CTEMP3 (the DSEG spill),
 * then seeks it to 0 and reads it back in asm_endit -- on a write-only fd
 * that read returns EBADF (-9), which GEN6's `== -1` guard does not catch,
 * so it emits the stale buffer as the DSEG and corrupts any object whose
 * data spills past one 2048-byte block.  Reopen read/write to match DOS. */
creat(filename)
char *filename;
{
    int fd;

    fd = syscall(SYS_creat, -1, filename, 0666);
    if (fd < 0)
        return fd;
    close(fd);
    return open(filename, 2);
}
