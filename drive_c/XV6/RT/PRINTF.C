/* PRINTF.C -- formatted output for DeSmet C 3.03 (K&R port of RealXV6
 * usr/printf.c).  ANSI <stdarg.h> is replaced by the V6-style stack
 * walk: cdecl pushes arguments right-to-left at increasing addresses,
 * so (&format + 1) points at the first variadic argument and each
 * int/unsigned/pointer occupies one word (small model).
 *
 * format string: "%-5d"
 *  d - decimal        o - octal          x - hexadecimal
 *  c - char           s - string         u - unsigned decimal
 */
#include "UNIX.H"

static int ljflg, fw, pr, prflg;
static char xbuf[21], pad;
static char *xp;

#define PUTC(c) { *xp++ = c; }

static char *
itoa(x)
int x;
{
    int sf;
    char *cp;

    sf = 0;
    cp = &xbuf[20];
    *cp-- = 0;
    *cp = '0';
    if (x == 0)
        return cp;
    if (x < 0) {
        sf++;
        x = -x;
    }
    while (x > 0) {
        *cp-- = x % 10 + '0';
        x /= 10;
    }
    cp++;
    if (sf)
        *--cp = '-';
    return cp;
}

static char *
utoa(x, r)
unsigned x;
int r;
{
    char *hx;
    char *cp;

    hx = "0123456789abcdef";
    cp = &xbuf[20];
    *cp-- = 0;
    *cp = '0';
    if (x == 0)
        return cp;
    while (x > 0) {
        *cp-- = hx[x % r];
        x /= r;
    }
    return cp + 1;
}

static int
xstrlen(s)
char *s;
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}

static
putsr(cp)
char *cp;
{
    int sl, i;

    sl = xstrlen(cp);
    if (pr < sl && prflg)
        sl = pr;
    i = fw - sl;
    if (sl < fw && !ljflg) {
        while (i--)
            PUTC(pad);
    }
    while (*cp) {
        PUTC(*cp++);
        if (--pr <= 0 && prflg)
            break;
    }
    if (sl < fw && ljflg) {
        while (i--)
            PUTC(pad);
    }
}

vsprintf(buffer, format, arglist)
char *buffer, *format;
int *arglist;
{
    char *per, *fr;

    fr = format;
    xp = buffer;

    while (*fr) {
        if (*fr != '%') {
            PUTC(*fr++);
            continue;
        }

        ljflg = fw = pr = prflg = 0;
        pad = ' ';
        per = ++fr;

        if (*fr == '-') {
            ljflg++;
            fr++;
        }
        if (*fr == '0') {
            pad = '0';
            fr++;
        }
        while ('0' <= *fr && *fr <= '9') {
            fw *= 10;
            fw += *fr++ - '0';
        }
        if (*fr == '.') {
            fr++;
            prflg++;
            while ('0' <= *fr && *fr <= '9') {
                pr *= 10;
                pr += *fr++ - '0';
            }
        }
        switch (*fr) {
        case 'd':
            putsr(itoa(*arglist++));
            break;
        case 'o':
            putsr(utoa((unsigned)*arglist++, 8));
            break;
        case 'x':
            putsr(utoa((unsigned)*arglist++, 16));
            break;
        case 'c':
            xbuf[0] = *arglist++;
            xbuf[1] = 0;
            putsr(xbuf);
            break;
        case 's':
            putsr((char *)*arglist++);
            break;
        case 'u':
            putsr(utoa((unsigned)*arglist++, 10));
            break;
        default:
            fr++;
            while (per < fr)
                PUTC(*per++);
            fr--;
        }
        fr++;
    }
    PUTC(0);
    return xp - buffer - 1;
}

sprintf(buffer, format)
char *buffer, *format;
{
    return vsprintf(buffer, format, (int *)(&format + 1));
}

fprintf(fd, format)
int fd;
char *format;
{
    char str[200];
    int cnt;

    cnt = vsprintf(str, format, (int *)(&format + 1));
    write(fd, str, cnt);
    return cnt;
}

printf(format)
char *format;
{
    char str[200];
    int cnt;

    cnt = vsprintf(str, format, (int *)(&format + 1));
    write(1, str, cnt);
    return cnt;
}
