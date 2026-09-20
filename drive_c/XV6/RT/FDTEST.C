/* FDTEST.C -- exercise the RealXV6 floppy block device (dmr/FD.C).
 *
 *     fdtest DEV NBLK [FILE]
 *
 * Three checks, in an order that matters:
 *
 *   1. FILE, if given, is compared against DEV from block 0.  Run straight
 *      after `cp FILE DEV`, this is the end-to-end proof that a plain copy
 *      reached the medium.  It has to come first: check 3 overwrites
 *      block 0.
 *   2. Block NBLK-1 reads and block NBLK does not.  A block special has no
 *      size, so the end of the medium is the driver's to report; a driver
 *      that ran off the end would show up here and nowhere else.
 *
 *      The refusal is read out of errno, NOT out of the byte count.  V6's
 *      readi (ken/rdwri.c) never looks at B_ERROR: it iomoves the buffer
 *      and only then notices u_error, so a refused block-special read
 *      still reports 512 bytes moved.  errno is what CRT0.A latches from
 *      r3, and it is the only place the driver's answer survives.
 *   3. Write a pattern to the first, middle and last block, push them out
 *      of the buffer cache by reading past it, then read the three back.
 *      Without the eviction the cache would answer and the medium would
 *      never be touched -- which is the whole thing under test.
 *
 * Prints FDTEST OK when every check passed; the exit status is the number
 * of failures.  Deliberately small: no long arithmetic, no stdio beyond
 * printf, so it links against the same runtime as the rest of dsrc.
 */
#include "UNIX.H"

#define BSIZE   512
#define EVICT   20      /* distinct blocks to read to clear a 15-buffer cache */

char buf[BSIZE];
char ref[BSIZE];
int nerr;

fail(msg, blk)
char *msg;
int blk;
{
    printf("FDTEST FAIL: %s at block %d\n", msg, blk);
    nerr++;
}

same(a, b, n)
char *a;
char *b;
int n;
{
    int i;

    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

/* seek flag 3 is V6's absolute BLOCK seek (offset x 512), which is how a
 * 16-bit argument still reaches every block of the medium. */
/* errno is cleared after the seek, so it always belongs to the transfer
 * and never to the positioning. */
rdblk(fd, blk, b)
int fd;
int blk;
char *b;
{
    if (seek(fd, blk, 3) < 0)
        return -1;
    errno = 0;
    return read(fd, b, BSIZE);
}

wrblk(fd, blk, b)
int fd;
int blk;
char *b;
{
    if (seek(fd, blk, 3) < 0)
        return -1;
    errno = 0;
    return write(fd, b, BSIZE);
}

fillpat(b, blk)
char *b;
int blk;
{
    int i;

    for (i = 0; i < BSIZE; i++)
        b[i] = blk + i;
}

main(argc, argv)
int argc;
char *argv[];
{
    int fd, ffd, nblk, i, n, blk, bad;
    int blks[3];

    if (argc < 3) {
        printf("usage: fdtest DEV NBLK [FILE]\n");
        return 1;
    }
    nblk = atoi(argv[2]);
    if (nblk < EVICT + 3) {
        printf("FDTEST FAIL: nonsense block count %d\n", nblk);
        return 1;
    }
    if ((fd = open(argv[1], 2)) < 0) {
        printf("FDTEST FAIL: cannot open %s (%d)\n", argv[1], fd);
        return 1;
    }

    /* ---- 1: what cp left on the medium ---- */
    if (argc > 3) {
        if ((ffd = open(argv[3], 0)) < 0) {
            printf("FDTEST FAIL: cannot open %s\n", argv[3]);
            nerr++;
        } else {
            blk = 0;
            bad = 0;
            for (;;) {
                n = read(ffd, ref, BSIZE);
                if (n <= 0)
                    break;
                if (rdblk(fd, blk, buf) != BSIZE) {
                    fail("short device read", blk);
                    bad++;
                    break;
                }
                if (!same(buf, ref, n)) {
                    fail("copy differs", blk);
                    bad++;
                    break;
                }
                blk++;
            }
            close(ffd);
            if (!bad)
                printf("fdtest: %s reads back from %s: %d block(s)\n",
                    argv[3], argv[1], blk);
        }
    }

    /* ---- 2: the end of the medium ---- */
    if (rdblk(fd, nblk - 1, buf) != BSIZE || errno != 0)
        fail("last block unreadable", nblk - 1);
    rdblk(fd, nblk, buf);
    if (errno == 0)
        fail("read past the end was not refused", nblk);

    /* ---- 3: write, evict, read back ---- */
    blks[0] = 0;
    blks[1] = nblk / 2;
    blks[2] = nblk - 1;
    for (i = 0; i < 3; i++) {
        fillpat(buf, blks[i]);
        if (wrblk(fd, blks[i], buf) != BSIZE || errno != 0)
            fail("short write", blks[i]);
    }
    /* blocks 1..EVICT are none of the three above, so this evicts them
     * without re-reading what was just written. */
    for (i = 1; i <= EVICT; i++)
        rdblk(fd, i, buf);
    for (i = 0; i < 3; i++) {
        fillpat(ref, blks[i]);
        if (rdblk(fd, blks[i], buf) != BSIZE || errno != 0)
            fail("short read back", blks[i]);
        else if (!same(buf, ref, BSIZE))
            fail("round trip", blks[i]);
    }
    close(fd);

    if (nerr == 0)
        printf("FDTEST OK\n");
    return nerr;
}
