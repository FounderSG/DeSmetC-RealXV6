/* CTIME.C -- time conversion for DeSmet C 3.03 (K&R port of RealXV6
 * usr/ctime.c, itself derived from V6).
 *
 * The epoch is 0000 Jan 1 1970 GMT; the argument time is two ints
 * (high, low) of seconds since then.  localtime(t) returns a pointer
 * to an int[9]: sec min hour mday mon year-1900... see usr/ctime.c for
 * the full commentary; this port only drops the ANSI prototypes and
 * rewrites &"str"[i] as "str" + i.
 */
#include "UNIX.H"

char *asctime();
char *ct_numb();
int *localtime();
int *gmtime();

char cbuf[26];
int dmsize[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int timezone = 5*60*60;
char *tzname[] = {
    "EST",
    "EDT",
};
int daylight = 1;   /* Allow daylight conversion */
/*
 * The following table is used for 1974 and 1975 and
 * gives the day number of the first day after the Sunday of the
 * change.
 */
struct dayt {
    int daylb;
    int dayle;
} daytab[] = {
    {5,  333},   /* 1974: Jan 6 - last Sun. in Nov */
    {58, 303},   /* 1975: Last Sun. in Feb - last Sun in Oct */
};

#define SEC     0
#define MIN     1
#define HOUR    2
#define MDAY    3
#define MON     4
#define YEAR    5
#define WDAY    6
#define YDAY    7
#define ISDAY   8

dpadd(a, b)
int *a, b;
{
    uint lo, hi, abs_b;

    lo = (uint)a[1];
    hi = (uint)a[0];

    if (b >= 0) {
        lo += (uint)b;
        if (lo < (uint)b)
            hi++;               /* Overflow */
    } else {
        abs_b = (uint)(-b);
        if (lo < abs_b)
            hi--;               /* Underflow */
        lo -= abs_b;
    }

    a[1] = (int)lo;
    a[0] = (int)hi;
}

uint
ldiv(ah, al, bl, rem)
uint ah, al, bl;
uint *rem;
{
    int i;
    uint quotient, remainder;

    quotient = 0;
    remainder = 0;
    for (i = 31; i >= 0; i--) {
        remainder = (remainder << 1) |
            ((i >= 16 ? ah : al) >> (i % 16) & 1);

        if (remainder >= bl) {
            remainder -= bl;
            quotient |= (1 << (i % 16));
        }
    }

    *rem = remainder;
    return quotient;
}

char *
ctime(at)
int *at;
{
    return asctime(localtime(at));
}

int *
localtime(tim)
int tim[];
{
    register int *t, *ct, dayno;
    int daylbegin, daylend;
    int copyt[2];

    t = copyt;
    t[0] = tim[0];
    t[1] = tim[1];
    /* dpadd(t, -timezone); */
    ct = gmtime(t);
    dayno = ct[YDAY];
    daylbegin = 119;    /* last Sun in Apr */
    daylend = 303;      /* Last Sun in Oct */
    if (ct[YEAR] == 74 || ct[YEAR] == 75) {
        daylbegin = daytab[ct[YEAR]-74].daylb;
        daylend = daytab[ct[YEAR]-74].dayle;
    }
    daylbegin = sunday(ct, daylbegin);
    daylend = sunday(ct, daylend);
    if (daylight &&
        (dayno > daylbegin || (dayno == daylbegin && ct[HOUR] >= 2)) &&
        (dayno < daylend || (dayno == daylend && ct[HOUR] < 1))) {
        dpadd(t, 1*60*60);
        ct = gmtime(t);
        ct[ISDAY]++;
    }
    return ct;
}

/*
 * The argument is a 0-origin day number.
 * The value is the day number of the first
 * Sunday on or after the day.
 */
sunday(at, ad)
int *at, ad;
{
    register int *t, d;

    t = at;
    d = ad;
    if (d >= 58)
        d += dysize(t[YEAR]) - 365;
    return d - (d - t[YDAY] + t[WDAY] + 700) % 7;
}

int *
gmtime(tim)
int tim[];
{
    int d0, d1;
    register int *tp;
    static int xtime[9];

    /*
     * break initial number into
     * multiples of 8 hours.
     * (28800 = 60*60*8)
     */

    d0 = ldiv((uint)tim[0], (uint)tim[1], 28800, (uint *)&d1);
    tp = &xtime[0];

    /*
     * generate hours:minutes:seconds
     */

    *tp++ = d1 % 60;
    d1 /= 60;
    *tp++ = d1 % 60;
    d1 /= 60;
    d1 += (d0 % 3) * 8;
    d0 /= 3;
    *tp++ = d1;

    /*
     * d0 is the day number.
     * generate day of the week.
     */

    xtime[WDAY] = (d0 + 4) % 7;

    /*
     * year number
     */
    for (d1 = 70; d0 >= dysize(d1); d1++)
        d0 -= dysize(d1);
    xtime[YEAR] = d1;
    xtime[YDAY] = d0;

    /*
     * generate month
     */

    if (dysize(d1) == 366)
        dmsize[1] = 29;
    for (d1 = 0; d0 >= dmsize[d1]; d1++)
        d0 -= dmsize[d1];
    dmsize[1] = 28;
    *tp++ = d0 + 1;
    *tp++ = d1;
    xtime[ISDAY] = 0;
    return xtime;
}

char *
asctime(t)
int *t;
{
    register char *cp, *ncp;
    register int *tp;

    cp = cbuf;
    for (ncp = "Day Mon 00 00:00:00 1900\n"; (*cp++ = *ncp++) != 0; )
        ;
    ncp = "SunMonTueWedThuFriSat" + 3*t[6];
    cp = cbuf;
    *cp++ = *ncp++;
    *cp++ = *ncp++;
    *cp++ = *ncp++;
    cp++;
    tp = &t[4];
    ncp = "JanFebMarAprMayJunJulAugSepOctNovDec" + (*tp)*3;
    *cp++ = *ncp++;
    *cp++ = *ncp++;
    *cp++ = *ncp++;
    cp = ct_numb(cp, *--tp);
    cp = ct_numb(cp, *--tp + 100);
    cp = ct_numb(cp, *--tp + 100);
    cp = ct_numb(cp, *--tp + 100);
    cp += 2;
    cp = ct_numb(cp, t[YEAR]);
    return cbuf;
}

dysize(y)
int y;
{
    if ((y % 4) == 0)
        return 366;
    return 365;
}

char *
ct_numb(acp, n)
char *acp;
int n;
{
    register char *cp;

    cp = acp;
    cp++;
    if (n >= 10)
        *cp++ = (n/10) % 10 + '0';
    else
        *cp++ = ' ';
    *cp++ = n % 10 + '0';
    return cp;
}
