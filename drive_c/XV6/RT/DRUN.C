/* DRUN.C -- a minimal script runner ("the .bat for RealXV6"), built
 * with DeSmet C 3.03 (K&R).  RealXV6's sh has no scripting, so a
 * multi-step build (c88 X; dbind ...; ...) otherwise needs the host to
 * drive each line over the console.  DRUN sequences the same steps from
 * a single on-target program: it reads a script file (argv[1]) whose
 * every nonblank, non-'#' line is one command, echoes the line (like
 * DOS `echo on`), splits it into an argv, and fork/exec/waits the
 * command -- trying the current directory first and then /bin with the
 * program name lowercased (mirrors V6CHAIN.C, the exec-based _chain).
 * A command that fails to exec, dies by a signal, or exits nonzero
 * stops the run: the analogue of the DeSmet BUILD.BAT's repeated
 * `if errorlevel 1 goto fatal`.  Because DRUN owns its line buffer it
 * is free of sh's 100-char / MAXARGS-10 limits.
 *
 * Two things beyond fork/exec, both for the same reason -- a build that
 * has to run ON the target cannot ask the host console for help:
 *
 *   cd DIR        a builtin, necessarily: a chdir(2) in the child would
 *                 die with the child.  Lets one script cross the /src
 *                 directories instead of needing a console line per
 *                 directory to drive it.
 *   cmd <IN >OUT  redirection, done in the child between fork and exec.
 *                 ed(1) takes its commands on stdin and nowhere else, so
 *                 without '<' no script can edit a source file, and the
 *                 target cannot change its own system.
 *
 * The two forms are `<file` and `< file`; the token never reaches argv.
 *
 * Usage:  drun SCRIPT
 *
 * Output uses printf (one raw write per call), never puts: a user
 * program's buffered DOS puts is never flushed on a V6 exit.
 */
#include "UNIX.H"

#define MAXCARGS 16
/* The whole script is read into one buffer, so this is a hard ceiling on a
 * script's size; main() refuses one that does not fit.  Not larger than this:
 * the buffer is BSS, drun forks once per line, and a V6 fork copies the whole
 * data segment.  The longest script here, mkusr, is 10852 bytes. */
#define BUFSIZE  16384

char filebuf[BUFSIZE];

/* Point fd at name, in the child, just before exec.  V6 hands out the
 * lowest free descriptor, so closing 0 or 1 first puts the new file
 * exactly there -- checked rather than assumed, because if the open fails
 * the descriptor simply stays closed and the command would run on with no
 * stdin at all, reading EOF and "succeeding". */
static
redirect(name, fd, out)
char *name;
int fd;
int out;
{
    int got;

    close(fd);
    if (out)
        got = creat(name, 0644);
    else
        got = open(name, 0);
    if (got != fd) {
        /* fd 1 may be the thing that just failed; say so on fd 2. */
        fprintf(2, "drun: cannot redirect %s\n", name);
        return 0;
    }
    return 1;
}

/* Split cmd in place into an argv (space/tab separated), lowercase-copy
 * the program name, and run it via fork/exec/wait.  Returns the raw V6
 * wait status word: 0 means success, any nonzero value is a failure
 * (high byte = exit code, low byte = terminating signal).  A blank line
 * runs nothing and returns 0. */
static
runline(cmd)
char *cmd;
{
    char *argv[MAXCARGS];
    char name[16], path[24];
    char *p, *infile, *outfile;
    int argc, i, c, pid, status;

    argc = 0;
    infile = outfile = 0;
    p = cmd;
    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == 0)
            break;
        if (*p == '<' || *p == '>') {
            c = *p++;
            while (*p == ' ' || *p == '\t')
                p++;
            if (c == '<')
                infile = p;
            else
                outfile = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = 0;
            continue;
        }
        if (argc < MAXCARGS - 1)
            argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = 0;
    }
    argv[argc] = 0;
    if (argc == 0)
        return 0;

    p = argv[0];
    i = 0;
    while (*p && i < 15) {
        c = *p++;
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        name[i++] = c;
    }
    name[i] = 0;
    argv[0] = name;

    /* cd changes THIS process: done after the fork it would go away with
     * the child.  chdir() returns r3, which CRT0's SYSCALL_ leaves 0 on
     * success and -1 on failure -- already runline's contract. */
    if (strcmp(name, "cd") == 0) {
        if (argc != 2) {
            printf("drun: usage: cd DIR\n");
            return 1;
        }
        if (chdir(argv[1]) != 0) {
            printf("drun: cannot cd %s\n", argv[1]);
            return 1;
        }
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        printf("drun: fork failed\n");
        return 1;
    }
    if (pid == 0) {
        if (infile && !redirect(infile, 0, 0))
            exit1(126);
        if (outfile && !redirect(outfile, 1, 1))
            exit1(126);
        exec(name, argv);
        strcpy(path, "/bin/");
        strcat(path, name);
        exec(path, argv);
        printf("drun: cannot exec %s\n", name);
        exit1(127);
    }
    status = 0;
    if (waits(&status) < 0) {
        printf("drun: wait failed\n");
        return 1;
    }
    return status;
}

main(argc, argv)
int argc;
char *argv[];
{
    int fd, n, status, ignore;
    char *line, *p, *e;
    char over;

    if (argc != 2) {
        printf("usage: drun script\n");
        exit1(1);
    }
    if ((fd = open(argv[1], 0)) < 0) {
        printf("drun: cannot open %s\n", argv[1]);
        exit1(1);
    }
    n = read(fd, filebuf, BUFSIZE - 1);
    if (n < 0) {
        printf("drun: read error on %s\n", argv[1]);
        exit1(1);
    }
    /* One byte past a full buffer: if it arrives, the script did not fit.
     * Stop here -- what was read ends at an arbitrary offset, so the last
     * line is probably half a command, and the rest is gone. */
    if (n == BUFSIZE - 1 && read(fd, &over, 1) > 0) {
        printf("drun: %s is bigger than %d bytes\n", argv[1], BUFSIZE - 1);
        exit1(1);
    }
    close(fd);
    filebuf[n] = 0;

    p = filebuf;
    while (*p) {
        line = p;
        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
            *p++ = 0;
        e = line;
        while (*e)
            e++;
        if (e > line && *(e - 1) == '\r')
            *(e - 1) = 0;
        while (*line == ' ' || *line == '\t')
            line++;
        if (*line == 0 || *line == '#')
            continue;
        /* A leading '-' means "ignore this step's status" (like make's `-`
         * prefix): C88 returns exit code 1 on a mere warning, and the vintage
         * K&R compiler sources (ASM3's global-struct-member idiom) warn but
         * compile fine.  dcmp/dbind/d2a stay strict so real failures and
         * fixpoint DIFFERs still abort. */
        ignore = 0;
        if (*line == '-') {
            ignore = 1;
            line++;
            while (*line == ' ' || *line == '\t')
                line++;
        }
        printf("+ %s\n", line);
        status = runline(line);
        if (status != 0 && !ignore) {
            printf("*** drun: step failed (status 0x%x)\n", status);
            exit1(1);
        }
    }
    printf("=== DRUN DONE ===\n");
    exit();
}
