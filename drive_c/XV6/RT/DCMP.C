/* DCMP.C -- byte-compare two files (RealXV6, DeSmet C 3.03).
 * Prints IDENTICAL, or the first differing offset and total count of
 * differing bytes.  Used to verify that the DeSmet tools ported to
 * RealXV6 produce byte-identical output to their DOS originals.
 */
#include "UNIX.H"

char b1[512], b2[512];

main(argc, argv)
int argc;
char *argv[];
{
    int f1, f2, n1, n2, i, n;
    unsigned off, firstdif, ndif;

    if (argc != 3) {
        printf("usage: dcmp file1 file2\n");
        return 1;
    }
    if ((f1 = open(argv[1], 0)) < 0) {
        printf("dcmp: cannot open %s\n", argv[1]);
        return 1;
    }
    if ((f2 = open(argv[2], 0)) < 0) {
        printf("dcmp: cannot open %s\n", argv[2]);
        return 1;
    }
    off = 0;
    ndif = 0;
    firstdif = 0;
    for (;;) {
        n1 = read(f1, b1, sizeof(b1));
        n2 = read(f2, b2, sizeof(b2));
        if (n1 <= 0 && n2 <= 0)
            break;
        if (n1 != n2) {
            printf("DIFFER: sizes (%u vs %u at offset %u)\n",
                off + (n1 > 0 ? n1 : 0), off + (n2 > 0 ? n2 : 0), off);
            return 1;
        }
        n = n1;
        for (i = 0; i < n; i++) {
            if (b1[i] != b2[i]) {
                if (ndif == 0)
                    firstdif = off + i;
                ndif++;
            }
        }
        off += n;
    }
    if (ndif) {
        printf("DIFFER: %u byte(s), first at offset %u, size %u\n",
            ndif, firstdif, off);
        return 1;
    }
    printf("IDENTICAL (%u bytes)\n", off);
    return 0;
}
