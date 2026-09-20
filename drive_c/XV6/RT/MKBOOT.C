/* MKBOOT.C -- assemble the RealXV6 boot floppy from a boot sector and a
 * kernel .COM.  ONE K&R source that builds both as a DOS tool
 * (plain BIND, DeSmet CSTDIO low-level I/O) and as a RealXV6 program (DCRT0 +
 * SYSCALL + DCOMPAT + PRINTF), so the bootable floppy can be produced
 * on-target with no host tool at all -- the same arrangement D2A.C uses for
 * a.out and E2C.C for the .COM.
 *
 *     mkboot BOOT.COM KERNEL.COM OUT.IMG [nsect [maxsect]]
 *     mkboot -s BOOTSEC.EXE BOOT.COM
 *
 * Defaults: nsect = 720 (a 360K floppy), maxsect = 119 (what the boot sector
 * reads).  This is the last link of the chain: c88/gen/asm88 compile the
 * kernel, dbind links it, e2c folds the .exe into the .COM, and mkboot lays
 * that .COM onto the medium the machine boots from.
 *
 * -s makes the boot sector itself, from what BIND leaves after assembling
 * BOOTSEC.A: drop the 512-byte MZ header, zero-fill the code out to 510
 * bytes and stamp the 55 AA signature.  The stamping is here rather than in
 * the assembly because ASM88 has no ORG directive -- wasm placed the
 * signature with "org 02FEh / dw 0AA55h", which has no ASM88 spelling --
 * and because this is the program that already knows what a boot sector has
 * to look like.  The build this sector was ported from stamped it the same
 * way.  A boot sector has no data segment, so everything after the header is
 * code; it must fit in 510 bytes, which is checked.
 *
 * THE LAYOUT
 * ----------
 * There is nothing to it:
 *
 *     [ boot sector, 512 B ][ kernel .COM ][ zero fill to 368640 ]
 *
 * 360K floppy = 2 heads * 40 tracks * 9 sectors * 512 = 368640 bytes.  There
 * is no filesystem on it: the boot sector finds the kernel by LBA, not by
 * name, so the kernel's placement at sector 1 IS the file format.
 *
 * WHAT THE BOOT SECTOR DOES, and the two checks that follow from it
 * ----------------------------------------------------------------
 * BOOTSEC.A reads sectors 1..119 one at a time with
 * INT 13h AH=2 into ES:BX = 1000h:0100h upward, then sets SS = 1000h,
 * SP = FFFEh and far-jumps to 1000h:0100h.  That is exactly the .COM
 * convention E2C.C targets -- CS == DS == SS, image at offset 100H -- which
 * is why a DeSmet-built kernel is loadable by the stock boot sector with no
 * change to either side.  Hence:
 *
 *   - the kernel must fit in the sectors the loader reads: it occupies
 *     sectors 1..n, so n must be <= maxsect (119).  A kernel one sector over
 *     boots with its tail missing, which shows up as a wild jump long after
 *     the boot sector has finished -- there is no diagnostic to be had at
 *     run time, so the check has to be here.
 *   - the boot sector must be exactly 512 bytes and end in 55 AA.  Stamping
 *     it here would be the easy thing to do, and is wrong: a file that needs
 *     the signature added is not the boot sector, so this refuses rather than
 *     turn "wrong input" into "an image that fails in the BIOS".  -s is where
 *     a sector legitimately acquires its signature.
 *
 * WHY IT VERIFIES WHAT IT WROTE
 * -----------------------------
 * On the target this writes 720 blocks into a V6 filesystem with about 978
 * free, so "out of space" is a real outcome and not a theoretical one.  Every
 * write is checked, and then the whole image is read back and compared
 * against the two inputs and against zero -- 360K of reads costs a couple of
 * seconds under emulation and is the only way to know the medium holds what
 * this program thinks it wrote.  Inputs are streamed in 512-byte blocks and
 * read twice rather than buffered: a 64K data segment cannot hold a 360K
 * image, and neither build has a portable seek.
 */
#include "UNIX.H"

#define SECSIZE  512
#define HEADS    2                      /* 360K floppy geometry, the same    */
#define TRACKS   40                     /* SECPTRK / NHEADS the LBA-to-CHS   */
#define SECTORS  9                      /* arithmetic in BOOTSEC.A uses      */
#define DEF_NSECT   (HEADS * TRACKS * SECTORS)  /* 720 sectors = 368640 B */
#define DEF_MAXSECT 119                 /* BOOTSEC.A's STOP: reads SI < 120 */
#define KERNSECT 1                      /* the kernel starts at LBA 1 */
#define SIGOFF   510                    /* -s: where 55 AA goes */
#define HDRMIN   32L                    /* -s: the part of the MZ header read */

char boot[SECSIZE];                     /* the boot sector, held for pass 2 */
char hdr[32];                           /* -s: the MZ header of BOOTSEC.EXE */
char buf[SECSIZE];
char vbuf[SECSIZE];                     /* the read-back, compared with buf */
char zbuf[SECSIZE];                     /* BSS: the zero fill */
char numb[16];

die(msg, arg)
char *msg, *arg;
{
    printf("mkboot: %s%s\n", msg, arg);
    exit(1);
}

/* decimal text for a long; PRINTF.C has %d %o %x %c %s %u and no %ld, and
 * every size here (368640) is past what %u can say */
char *
dec(v)
long v;
{
    char d[12];
    int n, i;
    long q;

    n = 0;
    if (v == 0L)
        d[n++] = '0';
    while (v > 0L) {
        q = v / 10L;
        d[n++] = (int)(v - q * 10L) + '0';
        v = q;
    }
    i = 0;
    while (n > 0)
        numb[i++] = d[--n];
    numb[i] = 0;
    return numb;
}

dienum(msg, v)
char *msg;
long v;
{
    printf("mkboot: %s%s\n", msg, dec(v));
    exit(1);
}

/* parse an unsigned decimal string; -1 on a non-digit */
long dtol(s)
char *s;
{
    long v;
    int c;

    v = 0L;
    if (*s == 0)
        return -1L;
    while ((c = *s++) != 0) {
        if (c < '0' || c > '9')
            return -1L;
        v = v * 10L + (c - '0');
    }
    return v;
}

/* read exactly n bytes into p; 0 on success, -1 on short read/eof */
readn(fd, p, n)
int fd;
char *p;
int n;
{
    int got;

    while (n > 0) {
        got = read(fd, p, n);
        if (got <= 0)
            return -1;
        p += got;
        n -= got;
    }
    return 0;
}

/* discard exactly n bytes from fd; -1 on short read */
skipn(fd, n)
int fd;
long n;
{
    int chunk;

    while (n > 0L) {
        chunk = n > (long)sizeof(buf) ? sizeof(buf) : (int)n;
        if (readn(fd, buf, chunk) != 0)
            return -1;
        n -= (long)chunk;
    }
    return 0;
}

/* total length of the file on fd, by streaming it to the end */
long flen(fd)
int fd;
{
    long n;
    int got;

    n = 0L;
    while ((got = read(fd, buf, sizeof(buf))) > 0)
        n += (long)got;
    if (got < 0)
        return -1L;
    return n;
}

/* copy the whole of ifd to ofd, returning what was copied; -1 on error */
long copyall(ifd, ofd)
int ifd, ofd;
{
    long n;
    int got;

    n = 0L;
    while ((got = read(ifd, buf, sizeof(buf))) > 0) {
        if (write(ofd, buf, got) != got)
            return -1L;
        n += (long)got;
    }
    if (got < 0)
        return -1L;
    return n;
}

/* copy exactly n bytes ifd -> ofd; -1 on short read/write */
copyn(ifd, ofd, n)
int ifd, ofd;
long n;
{
    int chunk;

    while (n > 0L) {
        chunk = n > (long)sizeof(buf) ? sizeof(buf) : (int)n;
        if (readn(ifd, buf, chunk) != 0)
            return -1;
        if (write(ofd, buf, chunk) != chunk)
            return -1;
        n -= (long)chunk;
    }
    return 0;
}

/* write n zero bytes to ofd; -1 on short write */
fillz(ofd, n)
int ofd;
long n;
{
    int chunk;

    while (n > 0L) {
        chunk = n > (long)sizeof(zbuf) ? sizeof(zbuf) : (int)n;
        if (write(ofd, zbuf, chunk) != chunk)
            return -1;
        n -= (long)chunk;
    }
    return 0;
}

/* compare n bytes of fd against p (p == 0 means "must be zero");
 * 0 = same, -1 = short read, -2 = mismatch */
vcmp(fd, p, n)
int fd;
char *p;
long n;
{
    int chunk, i;

    while (n > 0L) {
        chunk = n > (long)sizeof(vbuf) ? sizeof(vbuf) : (int)n;
        if (readn(fd, vbuf, chunk) != 0)
            return -1;
        for (i = 0; i < chunk; i++)
            if (vbuf[i] != (p ? p[i] : 0))
                return -2;
        if (p)
            p += chunk;
        n -= (long)chunk;
    }
    return 0;
}

/* compare n bytes of afd against bfd, block by block */
vcmpf(afd, bfd, n)
int afd, bfd;
long n;
{
    int chunk, i;

    while (n > 0L) {
        chunk = n > (long)sizeof(vbuf) ? sizeof(vbuf) : (int)n;
        if (readn(afd, vbuf, chunk) != 0 || readn(bfd, buf, chunk) != 0)
            return -1;
        for (i = 0; i < chunk; i++)
            if (vbuf[i] != buf[i])
                return -2;
        n -= (long)chunk;
    }
    return 0;
}

/* little-endian 16-bit load out of the MZ header, widened so a size above
 * 7FFFH never comes back negative */
long ldw(p, off)
char *p;
int off;
{
    return (long)(p[off] & 0xFF) + (long)(p[off + 1] & 0xFF) * 256L;
}

seq(a, b)
char *a, *b;
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* -s: BIND .EXE -> a 512-byte boot sector.  See the header comment. */
mksector(inname, outname)
char *inname, *outname;
{
    int ifd, ofd, rc;
    long hdrlen, total, imagelen, body;

    if ((ifd = open(inname, 0)) < 0)
        die("cannot open ", inname);
    if (readn(ifd, hdr, HDRMIN) != 0)
        die("short read of MZ header: ", inname);
    if (!((hdr[0] == 'M' && hdr[1] == 'Z') || (hdr[0] == 'Z' && hdr[1] == 'M')))
        die("not an MZ executable: ", inname);
    if (ldw(hdr, 6) != 0L)
        die("image has relocation entries; a boot sector cannot be relocated",
            "");
    hdrlen = ldw(hdr, 8) * 16L;
    if (hdrlen < HDRMIN)
        die("bogus MZ header size in ", inname);
    total = flen(ifd) + HDRMIN;         /* ... what is left, plus what we read */
    close(ifd);
    imagelen = total - hdrlen;
    if (imagelen <= 0L)
        die("no image after the MZ header in ", inname);

    /* The SIZE FIELDS ARE NOT USABLE HERE.  For a link this small BIND
     * writes e_cblp = 0 with e_cp = 2 -- "two full pages" -- for a file of
     * 626 bytes, so the usual (cp-1)*512+cblp gives 1024 and an image 398
     * bytes longer than the file.  (E2C.C can trust them because a kernel
     * link really is page-aligned.)  The file's own length is exact, and
     * where BIND pads to a page instead, the padding is zero -- so rather
     * than trust either number, take the first 510 bytes and REQUIRE that
     * whatever follows is zero.  That is the check "the code fits in a boot
     * sector" really wants, and it holds under both behaviours. */
    body = imagelen > (long)SIGOFF ? (long)SIGOFF : imagelen;

    /* Pass 1: whatever sits past 510 must be padding.  Checked in its own
     * pass, before the output exists, so a too-big sector cannot leave half
     * a .COM behind -- the property E2C.C reads its .exe twice for. */
    if (imagelen > body) {
        if ((ifd = open(inname, 0)) < 0)
            die("cannot reopen ", inname);
        if (skipn(ifd, hdrlen + body) != 0)
            die("short read of ", inname);
        rc = vcmp(ifd, (char *)0, imagelen - body);
        close(ifd);
        if (rc != 0)
            dienum("boot sector code does not fit in 510 bytes; image is ",
                   imagelen);
    }

    /* Pass 2: write the sector. */
    if ((ifd = open(inname, 0)) < 0)
        die("cannot reopen ", inname);
    if (skipn(ifd, hdrlen) != 0)
        die("short read of the MZ header: ", inname);
    if ((ofd = creat(outname)) < 0)
        die("cannot create ", outname);
    if (copyn(ifd, ofd, body) != 0)
        die("copy error: ", outname);
    if (fillz(ofd, (long)SIGOFF - body) != 0)
        die("write error (pad): ", outname);
    buf[0] = 0x55;
    buf[1] = 0xAA;
    if (write(ofd, buf, 2) != 2)
        die("write error (signature): ", outname);
    close(ofd);
    close(ifd);

    /* Pass 3: read it back, as the floppy build does.  The sector is what
     * the BIOS runs, so "it was written" is worth proving even for 512. */
    if ((ofd = open(outname, 0)) < 0)
        die("cannot reopen ", outname);
    if ((ifd = open(inname, 0)) < 0)
        die("cannot reopen ", inname);
    if (skipn(ifd, hdrlen) != 0)
        die("short read on ", inname);
    rc = vcmpf(ofd, ifd, body);
    if (rc == 0)
        rc = vcmp(ofd, (char *)0, (long)SIGOFF - body);
    if (rc == 0 && (readn(ofd, vbuf, 2) != 0
                    || (vbuf[0] & 0xFF) != 0x55 || (vbuf[1] & 0xFF) != 0xAA))
        rc = -2;
    if (rc == 0 && read(ofd, buf, 1) != 0)
        rc = -2;
    close(ifd);
    close(ofd);
    if (rc == -1)
        die("sector is short: ", outname);
    if (rc == -2)
        die("sector does not read back as written: ", outname);

    printf("mkboot: %s -> %s\n", inname, outname);
    printf("  code        : %s bytes", dec(body));
    printf(" of %s\n", dec((long)SIGOFF));
    printf("  signature   : 55 AA at offset %s\n", dec((long)SIGOFF));
    printf("mkboot: OK\n");
    return 0;
}

main(argc, argv)
int argc;
char *argv[];
{
    int bfd, kfd, ofd, rc;
    long nsect, maxsect, total, klen, ksect, fill;

    if (argc == 4 && seq(argv[1], "-s"))
        return mksector(argv[2], argv[3]);
    if (argc < 4 || argc > 6)
        die("usage: mkboot BOOT.COM KERNEL.COM OUT.IMG [nsect [maxsect]]\n       mkboot -s BOOTSEC.EXE BOOT.COM", "");
    nsect   = argc >= 5 ? dtol(argv[4]) : (long)DEF_NSECT;
    maxsect = argc >= 6 ? dtol(argv[5]) : (long)DEF_MAXSECT;
    if (nsect < 2L || maxsect < 1L)
        die("nonsense sector counts", "");
    total = nsect * (long)SECSIZE;

    /* ---- pass 1: the boot sector, whole and signed ---- */
    if ((bfd = open(argv[1], 0)) < 0)
        die("cannot open ", argv[1]);
    if (readn(bfd, boot, SECSIZE) != 0)
        die("boot sector is shorter than 512 bytes: ", argv[1]);
    if (read(bfd, buf, 1) != 0)
        die("boot sector is longer than 512 bytes: ", argv[1]);
    close(bfd);
    if ((boot[510] & 0xFF) != 0x55 || (boot[511] & 0xFF) != 0xAA)
        die("boot sector lacks the 55 AA signature: ", argv[1]);

    /* ---- pass 1: how much of the floppy the kernel wants ---- */
    if ((kfd = open(argv[2], 0)) < 0)
        die("cannot open ", argv[2]);
    klen = flen(kfd);
    close(kfd);
    if (klen < 0L)
        die("read error on ", argv[2]);
    if (klen == 0L)
        die("kernel image is empty: ", argv[2]);
    ksect = (klen + (long)SECSIZE - 1L) / (long)SECSIZE;
    if ((long)KERNSECT + ksect > nsect)
        dienum("kernel does not fit on the floppy; sectors needed: ", ksect);
    if (ksect > maxsect)
        dienum("kernel needs more sectors than the boot sector reads; needed: ",
               ksect);

    /* ---- pass 2: lay the floppy down ---- */
    if ((ofd = creat(argv[3])) < 0)
        die("cannot create ", argv[3]);
    if ((kfd = open(argv[2], 0)) < 0)
        die("cannot reopen ", argv[2]);
    if (write(ofd, boot, SECSIZE) != SECSIZE)
        die("write error (boot sector): ", argv[3]);
    if (copyall(kfd, ofd) != klen)
        die("write error (kernel): ", argv[3]);
    close(kfd);
    fill = total - (long)SECSIZE - klen;
    if (fillz(ofd, fill) != 0)
        die("write error (zero fill) -- out of space?  ", argv[3]);
    close(ofd);

    /* ---- pass 3: read it back and compare against the inputs ---- */
    if ((ofd = open(argv[3], 0)) < 0)
        die("cannot reopen ", argv[3]);
    if ((kfd = open(argv[2], 0)) < 0)
        die("cannot reopen ", argv[2]);
    rc = vcmp(ofd, boot, (long)SECSIZE);
    if (rc == 0)
        rc = vcmpf(ofd, kfd, klen);
    if (rc == 0)
        rc = vcmp(ofd, (char *)0, fill);
    if (rc == 0 && read(ofd, buf, 1) != 0)
        rc = -2;
    close(kfd);
    close(ofd);
    if (rc == -1)
        dienum("image is short of the full ", total);
    if (rc == -2)
        die("image does not read back as written: ", argv[3]);

    printf("mkboot: %s + %s -> %s\n", argv[1], argv[2], argv[3]);
    printf("  floppy      : %s bytes", dec(total));
    printf(" (%s sectors)\n", dec(nsect));
    printf("  boot sector : 512 bytes at sector 0 (55 AA ok)\n");
    printf("  kernel      : %s bytes", dec(klen));
    printf(", sectors %s", dec((long)KERNSECT));
    printf("..%s", dec((long)KERNSECT + ksect - 1L));
    printf(" (the boot sector reads 1..%s)\n", dec(maxsect));
    printf("  zero fill   : %s bytes\n", dec(fill));
    printf("  verify      : re-read and compared -- OK\n");
    printf("mkboot: OK\n");
    return 0;
}
