/*	FCMP.C	byte-compare two files under DOS (DeSmet C 3.03).
 *
 *	usage:	FCMP file1 file2 flagfile
 *
 *	Prints IDENTICAL, or the first differing offset and the number of
 *	differing bytes.  This exists because the stage-3 fixpoint check has
 *	to run INSIDE DOS: DOSBox-X's shell has neither FC nor COMP, and
 *	nothing on the host is allowed to take part in the build.
 *
 *	On a difference it also creates flagfile (an empty file), which is
 *	how CROSS.BAT learns the verdict: a batch that deletes the flag
 *	first and tests "if exist" afterwards does not have to depend on
 *	the exit status surviving a chain of CALLed bats.
 *
 *	Raw open()/read() rather than fopen()/getc(): these are executables,
 *	and DOS text mode would eat a CR or stop at a Ctrl-Z.  Offsets are
 *	unsigned, so the report saturates past 64K -- the tools compared
 *	here are all well under that.
 */

#define	BUFSZ	512

char	b1[BUFSZ], b2[BUFSZ];

main(argc, argv)
int argc;
char *argv[];
{
	int f1, f2, fl, n1, n2, n, i;
	unsigned off, firstdif, ndif;

	if (argc != 4) {
		printf("usage: FCMP file1 file2 flagfile\n");
		exit(2);
	}
	if ((f1 = open(argv[1], 0)) == -1) {
		printf("FCMP: cannot open %s\n", argv[1]);
		goto differ;
	}
	if ((f2 = open(argv[2], 0)) == -1) {
		printf("FCMP: cannot open %s\n", argv[2]);
		goto differ;
	}

	off = 0;
	ndif = 0;
	firstdif = 0;
	for (;;) {
		n1 = read(f1, b1, BUFSZ);
		n2 = read(f2, b2, BUFSZ);
		if (n1 <= 0 && n2 <= 0)
			break;
		if (n1 != n2) {
			printf("FCMP: DIFFER sizes, at offset %u\n", off);
			goto differ;
		}
		n = n1;
		for (i = 0; i < n; i++)
			if (b1[i] != b2[i]) {
				if (ndif == 0)
					firstdif = off + i;
				ndif++;
			}
		off = off + n;
	}
	close(f1);
	close(f2);

	if (ndif != 0) {
		printf("FCMP: DIFFER %u byte(s), first at %u, of %u\n",
			ndif, firstdif, off);
		goto differ;
	}
	printf("FCMP: IDENTICAL %s (%u bytes)\n", argv[1], off);
	exit(0);

differ:
	/*	leave the flag behind for the caller's "if exist" test	*/
	if ((fl = creat(argv[3])) != -1)
		close(fl);
	exit(1);
}
