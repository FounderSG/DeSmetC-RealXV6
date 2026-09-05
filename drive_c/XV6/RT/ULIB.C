/* ULIB.C -- user library for DeSmet C 3.03 (K&R port of RealXV6
 * usr/ulib.c).  Differences from the Watcom copy:
 *   - errno is defined in CRT0.A, not here;
 *   - the character differences in strcmp/strncmp/memcmp are computed
 *     through (int) casts: DeSmet does not promote char-only
 *     expressions to int, so a plain *p - *q would wrap at 8 bits.
 */
#include "UNIX.H"

/*
  Memory allocator by Kernighan and Ritchie,
  The C programming Language, 2nd ed.  Section 8.7.
*/

typedef long Align;

union header {
    struct {
        union header *ptr;
        uint size;
    } s;
    Align x;
};

typedef union header Header;

static Header base;
static Header *freep;
static char heap[4096 * sizeof(Header)];

free(ap)
char *ap;
{
    Header *bp, *p;

    bp = (Header *)ap - 1;
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    if (bp + bp->s.size == p->s.ptr) {
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    } else
        bp->s.ptr = p->s.ptr;
    if (p + p->s.size == bp) {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    } else
        p->s.ptr = bp;
    freep = p;
}

static Header *
morecore()
{
    Header *hp;

    hp = (Header *)heap;
    hp->s.size = sizeof(heap) / sizeof(Header);
    free((char *)(hp + 1));
    return freep;
}

char *
malloc(nbytes)
uint nbytes;
{
    Header *p, *prevp;
    uint nunits;

    nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;
    if ((prevp = freep) == 0) {
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }
    for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) {
        if (p->s.size >= nunits) {
            if (p->s.size == nunits)
                prevp->s.ptr = p->s.ptr;
            else {
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (char *)(p + 1);
        }
        if (p == freep)
            if ((p = morecore()) == 0)
                return 0;
    }
}

char *
strcpy(s, t)
char *s, *t;
{
    char *os;

    os = s;
    while ((*s++ = *t++) != 0)
        ;
    return os;
}

strcmp(p, q)
char *p, *q;
{
    while (*p && *p == *q)
        p++, q++;
    return (int)*p - (int)*q;
}

unsigned
strlen(s)
char *s;
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}

char *
strcat(s1, s2)
char *s1, *s2;
{
    char *os1;

    os1 = s1;
    while (*s1++)
        ;
    --s1;
    while (*s1++ = *s2++)
        ;
    return os1;
}

strncmp(p, q, n)
char *p, *q;
uint n;
{
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (int)*p - (int)*q;
}

char *
strncpy(s, t, n)
char *s, *t;
int n;
{
    char *os;

    os = s;
    while (n-- > 0 && (*s++ = *t++) != 0)
        ;
    while (n-- > 0)
        *s++ = 0;
    return os;
}

/* Like strncpy but guaranteed to NUL-terminate. */
char *
safestrcpy(s, t, n)
char *s, *t;
int n;
{
    char *os;

    os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0)
        ;
    *s = 0;
    return os;
}

char *
strchr(s, c)
char *s, c;
{
    for (; *s; s++)
        if (*s == c)
            return s;
    return 0;
}

char *
gets(buf, max)
char *buf;
int max;
{
    int i, cc;
    char c;

    for (i = 0; i + 1 < max; ) {
        cc = read(0, &c, 1);
        if (cc < 1)
            break;
        buf[i++] = c;
        if (c == '\n' || c == '\r')
            break;
    }
    buf[i] = '\0';
    return buf;
}

atoi(s)
char *s;
{
    int n;

    n = 0;
    while ('0' <= *s && *s <= '9')
        n = n * 10 + *s++ - '0';
    return n;
}

char *
memset(dst, c, n)
char *dst;
int c;
uint n;
{
    char *p;

    p = dst;
    while (n--)
        *p++ = c;
    return dst;
}

memcmp(v1, v2, n)
char *v1, *v2;
uint n;
{
    while (n-- > 0) {
        if (*v1 != *v2)
            return (int)*v1 - (int)*v2;
        v1++, v2++;
    }
    return 0;
}

char *
memmove(dst, src, n)
char *dst, *src;
uint n;
{
    char *s, *d;

    s = src;
    d = dst;
    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n-- > 0)
            *--d = *--s;
    } else
        while (n-- > 0)
            *d++ = *s++;

    return dst;
}

char *
memcpy(dst, src, n)
char *dst, *src;
uint n;
{
    return memmove(dst, src, n);
}

char *sys_errlist[] = {
    "Error 0",
    "Not super-user",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Arg list too long",
    "Exec format error",
    "Bad file number",
    "No children",
    "No more processes",
    "Not enough core",
    "Permission denied",
    "Error 14",
    "Block device required",
    "Mount device busy",
    "File exists",
    "Cross-device link",
    "No such device",
    "Not a directory",
    "Is a directory",
    "Invalid argument",
    "File table overflow",
    "Too many open files",
    "Not a typewriter",
    "Text file busy",
    "File too large",
    "No space left on device",
    "Illegal seek",
    "Read-only file system",
    "Too many links",
    "Broken Pipe",
};
int sys_nerr = sizeof(sys_errlist) / sizeof(sys_errlist[0]);

perror(s)
char *s;
{
    register char *c;
    register int n;

    c = "Unknown error";
    if (errno < sys_nerr)
        c = sys_errlist[errno];
    n = strlen(s);
    if (n) {
        write(2, s, n);
        write(2, ": ", 2);
    }
    write(2, c, strlen(c));
    write(2, "\n", 1);
}

mkdir(d)
char *d;
{
    char pname[128], dname[128];
    register int i, slash;

    slash = 0;
    pname[0] = '\0';
    for (i = 0; d[i]; ++i)
        if (d[i] == '/')
            slash = i + 1;
    if (slash)
        strncpy(pname, d, slash);
    strcpy(pname + slash, ".");
    if ((mknod(d, 040777, 0)) < 0) {
        perror("mknod");
        return -1;
    }
    chown(d, getuid(), getgid());
    strcpy(dname, d);
    strcat(dname, "/.");
    if ((link(d, dname)) < 0) {
        perror("link .");
        unlink(d);
        return -1;
    }
    strcat(dname, ".");
    if ((link(pname, dname)) < 0) {
        perror("link ..");
        dname[strlen(dname)] = '\0';
        unlink(dname);
        unlink(d);
        return -1;
    }

    return 0;
}
