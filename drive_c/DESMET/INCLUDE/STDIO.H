/*	STDIO.H		Include file for C88 input/output.	*/

/*	a 'FILE' is simply an integer is this implimentation	*/

typedef int  FILE;

/*	Standard input, standard output and standard error.	*/

#define stdin	(FILE *)0
#define stdout	(FILE *)1
#define	stderr	(FILE *)2
#define stdprn	(FILE *)4

#ifdef LARGE_CASE

/*	With LARGE case (-B option), */
/*	Use lower case stdin, stdout etc with fopen(), fread(), getc() etc. */
/*	Use upper case STDIN, STDOUT etc with open(), read() etc.	*/

#define STDIN	0
#define STDOUT	1
#define	STDERR	2
#define STDPRN	4

#endif

#define	NULL	(char *)0
#define TRUE	1
#define FALSE	0
#define EOF		(-1)
#define ERR		(-1)
