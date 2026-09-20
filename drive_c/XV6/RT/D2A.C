/* D2A.C -- convert a DeSmet C 3.03 "BIND -A" small-model MZ .exe into the
 * RealXV6 separated-I&D a.out format (VMM stack-high variant, magic 0411).
 * ONE K&R source that builds both as a DOS tool (plain BIND, DeSmet CSTDIO
 * low-level I/O) and as a RealXV6 program (CRT0 + SYSCALL + DCOMPAT +
 * PRINTF), so a.out can be produced on-target with no host tool at all.
 *
 *     d2a IN.EXE OUT.AOUT [stackhex]
 *
 * Links DCOMPAT (not UCOMPAT): it needs the DOS-flavor exit(status) so a
 * nonzero exit reaches drun/DOS ERRORLEVEL, and the 1-arg creat(name).
 *
 * Layout facts, measured and cross-checked against BIND.C:
 *   - BIND writes e_sp = alldata + 0x7F (BIND.C:797, stacklen==0 under -A)
 *     and prints that SAME alldata as the -P map "data=" field (BIND.C:894),
 *     so the total static data (init+bss) is exactly e_sp - 0x7F: no map
 *     file is needed.  (Holds only because our link passes no stack size.)
 *   - a_text = e_ss*16 (code padded to the 512-byte header page); a_data =
 *     image - a_text; a_bss = data_total - a_data.
 *   - a_entry = the CRT0.A signal-trampoline JMP at code offset 0
 *     (EB -> 2, E9 -> 3); BIND leaves CS:IP = 0:0 and zero relocations.
 *   - ENDBRK_ (end-of-BSS for sbrk) sits at DSEG offset 0x0A -- CRT0 is
 *     linked first and its data begins NULLGD,ERRNO_,R0_,R1_,R3_,ENDBRK_ --
 *     and is patched with data_total (BIND has no "end" symbol).
 *   - a_data (=datatot) is BIND's init-data size ROUNDED UP to a 512-byte
 *     page (BIND.C:679 datatot=(over_offs[0][0]+511)&~511); its final odata()
 *     flush pads the last partial page from the 4096-byte databuf sliding
 *     window, so the tail [_edata, a_data) is a byte-exact copy of the data
 *     4096 bytes earlier -- stale C storage, not real init data.  DOS _csetup
 *     zeroes all BSS so it never shows; under BIND -A the V6 kernel only
 *     zeroes past a_data, so that copy would wreck the first BSS globals
 *     (e.g. ASM88's `incfile`).  It is zeroed below (== what _csetup does).
 *
 * Reads the header (512 bytes) and text region strictly forward, then buffers
 * the whole initialized-data region so the stale -4096 tail can be detected.
 */
#include "UNIX.H"

#define A_MAGIC    0x109       /* 0411 octal */
#define DEF_STACK  0x1000
#define USTACK     0xF000L     /* kernel pins the stack top here (h/param.h) */
#define HDRLEN     512         /* BIND writes a 32-paragraph header */
#define ENDBRK_OFF 0x0A        /* DSEG offset of ENDBRK_ (CRT0 first) */
/* Buffer for the whole initialized-data region.  Covers the largest client
 * seen, at 11776 bytes (ASM88, the largest of the toolchain proper, is
 * ~5.6K).
 * It is BSS, so raising it costs no file bytes -- only 16K of the 64K data
 * segment (DOS small model / the V6 0xF000 user window).  Hard ceiling is
 * 32767: a_data is cast to int in the read/zero/write path below. */
#define MAXDATA    16384

char hdr[32];
char buf[512];
char dbuf[MAXDATA];

die(msg, arg)
char *msg, *arg;
{
    printf("d2a: %s%s\n", msg, arg);
    exit(1);
}

/* little-endian 16-bit load from a byte buffer (char is unsigned in DeSmet) */
uint ld16(p, off)
char *p;
int off;
{
    return (p[off] & 0xFF) | ((p[off + 1] & 0xFF) << 8);
}

st16(p, off, v)
char *p;
int off;
uint v;
{
    p[off] = v & 0xFF;
    p[off + 1] = (v >> 8) & 0xFF;
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
int fd, n;
{
    int chunk;

    while (n > 0) {
        chunk = n > sizeof(buf) ? sizeof(buf) : n;
        if (readn(fd, buf, chunk) != 0)
            return -1;
        n -= chunk;
    }
    return 0;
}

/* copy exactly n bytes ifd -> ofd; -1 on short read/write */
copyn(ifd, ofd, n)
int ifd, ofd;
long n;
{
    int chunk;

    while (n > 0) {
        chunk = n > (long)sizeof(buf) ? sizeof(buf) : (int)n;
        if (readn(ifd, buf, chunk) != 0)
            return -1;
        if (write(ofd, buf, chunk) != chunk)
            return -1;
        n -= chunk;
    }
    return 0;
}

/* parse an unsigned hex string (no 0x prefix) */
uint xtoi(s)
char *s;
{
    uint v;
    int c;

    v = 0;
    while ((c = *s++) != 0) {
        if (c >= '0' && c <= '9') c -= '0';
        else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
        else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
        else break;
        v = v * 16 + c;
    }
    return v;
}

main(argc, argv)
int argc;
char *argv[];
{
    int ifd, ofd, first, k, lo;
    uint e_cblp, e_cp, e_crlc, e_cparhdr, e_ss, e_sp, e_ip, e_cs;
    uint data_total, a_bss, a_stack, a_entry;
    long total, imagelen, a_text, a_data;

    if (argc != 3 && argc != 4)
        die("usage: d2a IN.EXE OUT.AOUT [stackhex]", "");
    a_stack = argc == 4 ? xtoi(argv[3]) : DEF_STACK;

    if ((ifd = open(argv[1], 0)) < 0)
        die("cannot open ", argv[1]);
    if (readn(ifd, hdr, sizeof(hdr)) != 0)
        die("short read of MZ header: ", argv[1]);

    if (!((hdr[0] == 'M' && hdr[1] == 'Z') || (hdr[0] == 'Z' && hdr[1] == 'M')))
        die("not an MZ executable: ", argv[1]);

    e_cblp    = ld16(hdr, 2);
    e_cp      = ld16(hdr, 4);
    e_crlc    = ld16(hdr, 6);
    e_cparhdr = ld16(hdr, 8);
    e_ss      = ld16(hdr, 14);
    e_sp      = ld16(hdr, 16);
    e_ip      = ld16(hdr, 20);
    e_cs      = ld16(hdr, 22);

    if (e_crlc != 0)
        die("has relocations -- not a plain BIND small-model exe", "");
    if (e_cs != 0 || e_ip != 0)
        die("CS:IP != 0:0 -- not linked with BIND -A", "");
    if (e_cparhdr * 16 != HDRLEN)
        die("unexpected header size (e_cparhdr*16 != 512)", "");

    if (e_cblp == 0)
        total = (long)e_cp * 512;
    else
        total = ((long)e_cp - 1) * 512 + e_cblp;
    imagelen = total - HDRLEN;

    a_text = (long)e_ss * 16;
    if (a_text <= 0 || a_text > imagelen)
        die("code size (from e_ss) outside the image", "");

    a_data = imagelen - a_text;
    /* a_data (=datatot) is BIND's init-data size, rounded up to a 512-byte
     * page; the last partial page is stale databuf padding zeroed below (see
     * the header note).  a_data is bounded by data_total and the 0xF000
     * window (checked below) and by the dbuf[] buffer (checked before read). */

    data_total = e_sp - 0x7F;
    if ((long)data_total < a_data)
        die("data_total < a_data -- layout assumption broken", "");
    a_bss = data_total - (uint)a_data;

    if (((long)data_total + a_stack) > USTACK)
        printf("d2a: warning: data+bss+stack exceeds the 0xF000 user window\n");

    /* skip the rest of the 512-byte header (32 bytes already read) */
    if (skipn(ifd, HDRLEN - sizeof(hdr)) != 0)
        die("short read skipping header: ", argv[1]);

    /* entry: the CRT0 trampoline JMP at code offset 0 */
    if (read(ifd, buf, 1) != 1)
        die("short read of code: ", argv[1]);
    first = buf[0] & 0xFF;
    if (first == 0xEB)
        a_entry = 2;
    else if (first == 0xE9)
        a_entry = 3;
    else
        die("code offset 0 is not a JMP -- CRT0.O must be BIND's first object", "");

    if ((ofd = creat(argv[2])) < 0)
        die("cannot create ", argv[2]);

    /* 16-byte struct exec: magic text data bss syms entry stack flag */
    st16(hdr, 0, A_MAGIC);
    st16(hdr, 2, (uint)a_text);
    st16(hdr, 4, (uint)a_data);
    st16(hdr, 6, a_bss);
    st16(hdr, 8, 0);
    st16(hdr, 10, a_entry);
    st16(hdr, 12, a_stack);
    st16(hdr, 14, 0);
    if (write(ofd, hdr, 16) != 16)
        die("write error (header): ", argv[2]);

    /* text: the byte already read, then the remainder */
    if (write(ofd, buf, 1) != 1)
        die("write error (text): ", argv[2]);
    if (copyn(ifd, ofd, a_text - 1) != 0)
        die("copy error (text): ", argv[2]);

    /* data: buffer the whole initialized-data region, patch the ENDBRK_ word
     * (data offset 0x0A), zero BIND's stale page-padding, then write it. */
    if (a_data > MAXDATA)
        die("initialized data too large for dbuf[]", "");
    if (readn(ifd, dbuf, (int)a_data) != 0)
        die("short read of data: ", argv[1]);
    if (a_data > (long)(ENDBRK_OFF + 1))
        st16(dbuf, ENDBRK_OFF, data_total);
    /* trailing suffix (within the last 512-byte page) that byte-exactly
     * copies the data 4096 earlier is stale databuf padding -- zero it. */
    k = (int)a_data;
    lo = (int)a_data - 512;
    while (k > lo && k - 1 >= 4096 && dbuf[k - 1] == dbuf[k - 1 - 4096])
        k--;
    while (k < (int)a_data)
        dbuf[k++] = 0;
    if (write(ofd, dbuf, (int)a_data) != (int)a_data)
        die("write error (data): ", argv[2]);

    close(ofd);
    close(ifd);
    printf("d2a: %s -> %s text=%u data=%u bss=%u stack=%u entry=%u\n",
           argv[1], argv[2], (uint)a_text, (uint)a_data, a_bss, a_stack, a_entry);
    return 0;
}
