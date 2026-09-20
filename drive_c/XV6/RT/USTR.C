/* USTR.C -- the string functions the DeSmet tool ports actually use
 * (strcpy/strcat/strlen from the tools, memmove from V6DOS's _move),
 * copied from ULIB.C.  Tool links use this instead of ULIB because
 * ULIB's initialized data (sys_errlist etc.) pushes the tools' EXE
 * data image past 4096 bytes, where BIND's page buffering leaves
 * stale bytes under the uninitialized variables (see V6DOS.C notes).
 * User programs keep linking the full ULIB.
 */
#include "UNIX.H"

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
