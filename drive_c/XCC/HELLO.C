/* HELLO.C -- the smallest complete RealXV6 program, as a worked example of
 * what the cross compiler in C:\XCC actually compiles.
 *
 *     xcc hello                 rem  HELLO.C to HELLO.AO, a RealXV6 a.out
 *
 * CROSS ends standing in C:\XCC, so that is one command from a finished
 * build; from anywhere else, cd \XCC first.
 *
 * Every line below is deliberate, and each one is a place where the DOS
 * toolchain would have let you write something the cross compiler will not:
 *
 *   #include "UNIX.H"   The quoted form, not the angle-bracket one.  UNIX.H
 *                       is found through C88's -i (XCC passes -iC:\XCC\),
 *                       because DSINC covers only <angle brackets>.  And
 *                       there is no STDIO.H to include instead: XCC.BAT
 *                       points DSINC at the target's headers alone, so a
 *                       RealXV6 program cannot quietly pick up a DOS one.
 *
 *   main(argc, argv)    K&R.  DeSmet C 3.03 predates ANSI: no prototypes,
 *                       no void, no types inside the parameter list.  The
 *                       parameters are declared between the ) and the {.
 *
 *   printf(...)         Resolved from the V6 PRINTF.O, which formats into a
 *                       200-byte buffer and write()s it to fd 1.  It is not
 *                       CSTDIO's printf -- BIND opens CSTDIO.S on every link
 *                       but takes nothing from it, because the V6 runtime
 *                       resolves first.  %d %u %o %x %c %s only: no floats,
 *                       no long, no %ld.
 *
 *   write(1, ...)       The syscall wrapper straight from SYSCALL.O, one
 *                       layer under printf.  Both work; this is what the
 *                       layer below looks like.
 *
 *   return 0            CRT0 turns main's return value into the exit status.
 *                       Do not write exit(0): V6's exit() takes NO argument
 *                       (UCOMPAT.C).  exit1(n) is the one that takes a code.
 */
#include "UNIX.H"

main(argc, argv)
int argc;
char *argv[];
{
    int i;

    printf("hello from RealXV6, compiled by DeSmet C 3.03\n");
    printf("argc = %d\n", argc);
    for (i = 0; i < argc; i++)
        printf("  argv[%d] = %s\n", i, argv[i]);
    write(1, "goodbye\n", 8);
    return 0;
}
