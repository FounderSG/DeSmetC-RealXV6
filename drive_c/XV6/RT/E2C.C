/* E2C.C -- convert a DeSmet BIND .EXE into a DOS .COM (CS == DS == SS).
 * ONE K&R source that builds both as a DOS tool (plain BIND, DeSmet CSTDIO
 * low-level I/O) and as a RealXV6 program (CRT0 + SYSCALL + DCOMPAT +
 * PRINTF), so the kernel .COM can be produced on-target with no host tool at
 * all -- the same arrangement D2A.C uses for a.out.
 *
 *     e2c IN.EXE OUT.COM [MAP|-] [holehex] [cpadhex]
 *
 * Defaults: hole = 8100H (DPAD's 33024 bytes), cpad = 100H (CPAD's 256).
 *
 * DeSmet's BIND has no single-segment output: it emits a two-segment MZ EXE
 * whose code lives at CS:0 and whose data lives at DS:0, with DS = CS +
 * codeparagraphs.  There are no relocation entries, so nothing in the image
 * can be moved after the fact -- every data reference is a DS-relative offset
 * burned in at link time, and every function pointer is a CS-relative one.
 *
 * The trick this implements gets the two segments to coincide by making the
 * LINKER lay them out disjointly inside one 64K segment, using two spacer
 * objects that must come first in the BIND list:
 *
 *     CPAD.A   256 bytes of CSEG   -- real code is linked at 100H and up
 *     DPAD.C   33024 bytes of DSEG -- real data is linked at 8100H and up
 *
 * A .COM is loaded at offset 100H of its segment with CS = DS = SS, so:
 *
 *     file offset F  ->  segment offset F + 100H
 *
 *     real code  wants segment 100H       -> file offset 0
 *     real data  wants segment 8100H      -> file offset 8000H
 *
 * so the .COM is simply
 *
 *     [ real code ][ zero fill up to 8000H ][ real initialized data ]
 *
 * and this tool's whole job is to drop the two spacers back out and place
 * the two pieces at those offsets.  Nothing is relocated, which is why the
 * absence of relocation entries is not a problem here.
 *
 * The one constraint is that the real code must fit below the data spacer:
 * realcode (= codelen - cpad) must be no more than hole - 100H, the .COM file
 * offset the real data lands at.  BSS follows the initialized data and is not
 * in the file; M86.A's STARTX clears it, which is what the MAP argument is
 * for (see readmap below).
 *
 * Streams the .EXE with no seek (neither build has a portable one) and reads
 * it TWICE: the first pass validates the header and the data spacer, the
 * second writes the .COM.  Reading 64K a second time costs nothing, and it
 * means a bad input never leaves a half-written .COM behind -- the same
 * property the Python gets by slicing the whole file in memory, which a
 * 64K-data-segment target cannot afford.
 */
#include "UNIX.H"

#define DEF_HOLE 0x8100L        /* DPAD.C: 16512 * 2 = 32K + 100H */
#define DEF_CPAD 0x100L         /* CPAD.A: one .COM origin's worth */
#define ORG      0x100L         /* where DOS loads a .COM */
#define SPARE    0x7FL          /* BIND writes e_sp = static data + 7FH */
#define HDRMIN   32L            /* the part of the MZ header we read */

char hdr[32];
char buf[512];
char zbuf[512];                 /* BSS: the zero fill between code and data */

/* --- BIND -P map scan (see readmap) ------------------------------------ */
char tok[40];
int  mfd, mlen, mpos;
char mbuf[512];
long map_at, map_tot;
int  have_at, have_tot;

die(msg, arg)
char *msg, *arg;
{
    printf("e2c: %s%s\n", msg, arg);
    exit(1);
}

dienum(msg, v)
char *msg;
uint v;
{
    printf("e2c: %s%u\n", msg, v);
    exit(1);
}

/* little-endian 16-bit load, widened to long so values above 7FFFH (e_sp on
 * any real kernel link) never come back negative */
long ldw(p, off)
char *p;
int off;
{
    return (long)(p[off] & 0xFF) + (long)(p[off + 1] & 0xFF) * 256L;
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

    while (n > 0) {
        chunk = n > (long)sizeof(buf) ? sizeof(buf) : (int)n;
        if (readn(fd, buf, chunk) != 0)
            return -1;
        n -= chunk;
    }
    return 0;
}

/* Verify that n bytes from fd really are the DPAD spacer.  DPAD.C declares
 * its chunks as `= {1}`, so the region is 01H at each chunk start and 00H
 * everywhere else.  Any other byte means real program data landed inside it
 * -- i.e. the link order is wrong, which is the mistake this scheme is most
 * likely to fail on, and it would otherwise produce a .COM that is silently
 * garbage.  0 = spacer, -1 = short read, -2 = not a spacer.
 */
checkpad(fd, n)
int fd;
long n;
{
    int chunk, i, first;

    first = 1;
    while (n > 0) {
        chunk = n > (long)sizeof(buf) ? sizeof(buf) : (int)n;
        if (readn(fd, buf, chunk) != 0)
            return -1;
        if (first && (buf[0] & 0xFF) != 1)
            return -2;
        first = 0;
        for (i = 0; i < chunk; i++)
            if ((buf[i] & 0xFF) > 1)
                return -2;
        n -= chunk;
    }
    return 0;
}

/* Copy exactly n bytes ifd -> ofd.  When pat >= 0 the two bytes at copy-
 * relative offsets pat and pat+1 are replaced by the 16-bit value v: that is
 * the BSSEND_ patch, applied in flight so no build-size-dependent buffer is
 * needed.  -1 on short read/write.
 */
copyn(ifd, ofd, n, pat, v)
int ifd, ofd;
long n, pat, v;
{
    int chunk;
    long pos;

    pos = 0L;
    while (n > 0) {
        chunk = n > (long)sizeof(buf) ? sizeof(buf) : (int)n;
        if (readn(ifd, buf, chunk) != 0)
            return -1;
        if (pat >= 0L) {
            if (pat >= pos && pat < pos + chunk)
                buf[(int)(pat - pos)] = (int)(v & 0xFFL);
            if (pat + 1L >= pos && pat + 1L < pos + chunk)
                buf[(int)(pat + 1L - pos)] = (int)((v / 256L) & 0xFFL);
        }
        if (write(ofd, buf, chunk) != chunk)
            return -1;
        pos += chunk;
        n -= chunk;
    }
    return 0;
}

/* write n zero bytes to ofd; -1 on short write */
fillz(ofd, n)
int ofd;
long n;
{
    int chunk;

    while (n > 0) {
        chunk = n > (long)sizeof(zbuf) ? sizeof(zbuf) : (int)n;
        if (write(ofd, zbuf, chunk) != chunk)
            return -1;
        n -= chunk;
    }
    return 0;
}

/* parse an unsigned hex string (no 0x prefix), stopping at the first
 * non-digit -- so "CB66H" yields CB66H */
long xtol(s)
char *s;
{
    long v;
    int c;

    v = 0L;
    while ((c = *s++) != 0) {
        if (c >= '0' && c <= '9') c -= '0';
        else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
        else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
        else break;
        v = v * 16L + c;
    }
    return v;
}

/* 1 if s starts with p */
pfx(s, p)
char *s, *p;
{
    while (*p)
        if (*s++ != *p++)
            return 0;
    return 1;
}

seq(a, b)
char *a, *b;
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

mgetc()
{
    if (mpos >= mlen) {
        mlen = read(mfd, mbuf, sizeof(mbuf));
        mpos = 0;
        if (mlen <= 0)
            return -1;
    }
    return mbuf[mpos++] & 0xFF;
}

/* next whitespace-delimited token of the map into tok[]; 0 at end of file */
mtoken()
{
    int c, i;

    do {
        c = mgetc();
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\n');
    if (c < 0)
        return 0;
    i = 0;
    while (c > 0 && c != ' ' && c != '\t' && c != '\r' && c != '\n') {
        if (i < (int)sizeof(tok) - 1)
            tok[i++] = c;
        c = mgetc();
    }
    tok[i] = 0;
    return 1;
}

/* Scan a BIND -P map for what the BSS patch needs.  BSS is not in the image
 * and BIND publishes no symbol for its extent, so STARTX has to be told where
 * to stop: M86.A keeps an initialized word BSSEND_ for exactly that, and the
 * map carries both its address ("DS:8100 BSSEND_") and the value to store in
 * it -- the trailer's "data=CB66H", which is initialized data plus BSS.  The
 * start needs no patch: STARTX derives it from OFFSET ENDDATA_+2, and ZEND.A
 * is linked last, so that is exact at link time.
 *
 * The map is one long list of "SEG:offset NAME" pairs, so a token scan that
 * remembers the previous DS: offset finds the symbol wherever it wraps.
 */
readmap(name)
char *name;
{
    long pend;

    if ((mfd = open(name, 0)) < 0)
        die("cannot open ", name);
    mlen = mpos = 0;
    pend = -1L;
    while (mtoken()) {
        if (pend >= 0L && !have_at && seq(tok, "BSSEND_")) {
            map_at = pend;
            have_at = 1;
        }
        if (pfx(tok, "DS:"))
            pend = xtol(tok + 3);
        else
            pend = -1L;
        if (!have_tot && pfx(tok, "data=")) {
            map_tot = xtol(tok + 5);
            have_tot = 1;
        }
    }
    close(mfd);
}

main(argc, argv)
int argc;
char *argv[];
{
    int ifd, ofd, rc;
    char *mapname;
    long hole, cpad, cblp, cp, hdrlen, total, imagelen;
    long codelen, datalen, realcode, realdata, datoff;
    long e_sp, derived, bssend, bssat, patoff;

    if (argc < 3 || argc > 6)
        die("usage: e2c IN.EXE OUT.COM [MAP|-] [holehex] [cpadhex]", "");
    mapname = (char *)0;
    if (argc >= 4 && !seq(argv[3], "-"))
        mapname = argv[3];
    hole = argc >= 5 ? xtol(argv[4]) : DEF_HOLE;
    cpad = argc >= 6 ? xtol(argv[5]) : DEF_CPAD;
    if (hole <= ORG || cpad < 0L)
        die("nonsense hole/cpad sizes", "");

    /* ---- pass 1: validate the header and the data spacer ---- */
    if ((ifd = open(argv[1], 0)) < 0)
        die("cannot open ", argv[1]);
    if (readn(ifd, hdr, sizeof(hdr)) != 0)
        die("short read of MZ header: ", argv[1]);
    if (!((hdr[0] == 'M' && hdr[1] == 'Z') || (hdr[0] == 'Z' && hdr[1] == 'M')))
        die("not an MZ executable: ", argv[1]);
    if (ldw(hdr, 6) != 0L)
        die("image has relocation entries; this scheme assumes none", "");

    cblp     = ldw(hdr, 2);
    cp       = ldw(hdr, 4);
    hdrlen   = ldw(hdr, 8) * 16L;
    codelen  = ldw(hdr, 14) * 16L;      /* BIND sets SS = codesize/16 */
    e_sp     = ldw(hdr, 16);

    if (hdrlen < HDRMIN)
        die("bogus MZ header size", "");
    total    = cblp ? (cp - 1L) * 512L + cblp : cp * 512L;
    imagelen = total - hdrlen;
    if (codelen <= 0L || codelen > imagelen)
        die("code size (from e_ss) outside the image", "");
    datalen  = imagelen - codelen;

    if (codelen < cpad)
        dienum("code segment smaller than the code spacer: ", (uint)codelen);
    if (datalen < hole)
        dienum("data smaller than the data spacer -- is DPAD first?  data=",
               (uint)datalen);
    realcode = codelen - cpad;
    realdata = datalen - hole;
    datoff   = hole - ORG;
    if (realcode > datoff)
        dienum("real code does not fit below the data spacer; enlarge the hole.  code=",
               (uint)realcode);

    if (skipn(ifd, hdrlen - HDRMIN) != 0 || skipn(ifd, codelen) != 0)
        die("short read of the code segment: ", argv[1]);
    rc = checkpad(ifd, hole);
    close(ifd);
    if (rc == -1)
        die("short read of the data spacer: ", argv[1]);
    if (rc == -2)
        die("the first data bytes are not the DPAD spacer -- link order is wrong (DPAD must be the first object contributing DSEG)", "");

    /* ---- what to patch into BSSEND_ (only with a map, as in the Python) ---- */
    patoff = -1L;
    bssend = bssat = 0L;
    derived = e_sp - SPARE;             /* BIND -A: e_sp = alldata + 7FH */
    if (mapname) {
        readmap(mapname);
        if (!have_tot)
            die("no 'data=' total in ", mapname);
        if (!have_at)
            die("BSSEND_ not in the map -- is M86.A linked?  ", mapname);
        bssend = map_tot;
        bssat  = map_at;
        /* Two independent statements of the same number: BIND writes the
         * total static data into e_sp as well as into the map trailer.  They
         * can only disagree if a link option this scheme does not model (a
         * stack size) is in play, so say so and keep trusting the map. */
        if (derived != bssend)
            printf("e2c: warning: map data=%xh but e_sp-7fh=%xh\n",
                   (uint)bssend, (uint)derived);
        patoff = bssat - hole;
        if (patoff < 0L || patoff + 2L > realdata)
            dienum("BSSEND_ is outside the real initialized data: DS:",
                   (uint)bssat);
    }

    /* ---- pass 2: write the .COM ---- */
    if ((ifd = open(argv[1], 0)) < 0)
        die("cannot reopen ", argv[1]);
    if ((ofd = creat(argv[2])) < 0)
        die("cannot create ", argv[2]);
    if (skipn(ifd, hdrlen + cpad) != 0)
        die("short read of the code spacer: ", argv[1]);
    if (copyn(ifd, ofd, realcode, -1L, 0L) != 0)
        die("copy error (code): ", argv[2]);
    if (fillz(ofd, datoff - realcode) != 0)
        die("write error (fill): ", argv[2]);
    if (skipn(ifd, hole) != 0)
        die("short read of the data spacer: ", argv[1]);
    if (copyn(ifd, ofd, realdata, patoff, bssend) != 0)
        die("copy error (data): ", argv[2]);
    close(ofd);
    close(ifd);

    printf("e2c: %s -> %s\n", argv[1], argv[2]);
    printf("  code segment in EXE : %u bytes (spacer %u + real %u)\n",
           (uint)codelen, (uint)cpad, (uint)realcode);
    printf("  real code  to .COM offset 0000..%04x  (segment %04x..)\n",
           (uint)realcode, (uint)ORG);
    printf("  real data  to .COM offset %04x..%04x  (segment %04x..)\n",
           (uint)datoff, (uint)(datoff + realdata), (uint)hole);
    printf("  .COM size           : %u bytes\n", (uint)(datoff + realdata));
    if (mapname)
        printf("  BSS cleared up to   : DS:%04x patched at DS:%04x\n",
               (uint)bssend, (uint)bssat);
    return 0;
}
