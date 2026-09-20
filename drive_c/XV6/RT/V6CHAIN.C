/* V6CHAIN.C -- _chain() for RealXV6 (DeSmet C 3.03, K&R).
 * Replaces CHAIN.A, the DOS in-process program loader the DeSmet
 * passes use to hand off to the next pass (C88 -> GEN -> ASM88).
 * The DOS loader searched the PATH for NAME.EXE and jumped into it;
 * here we split the command string into an argv and exec() it, trying
 * the current directory first and then /bin, with the program name
 * lowercased for the V6 filesystem.  Like the DOS original, _chain()
 * never returns to the caller: code after a _chain() call only runs
 * when chaining was skipped, so on failure we print and exit(2).
 */
#include "UNIX.H"

#define MAXCARGS 16

_chain(cmd)
char *cmd;
{
    char *argv[MAXCARGS];
    char name[16], path[24];
    char *p;
    int argc, i, c;

    argc = 0;
    p = cmd;
    while (*p) {
        while (*p == ' ')
            p++;
        if (*p == 0)
            break;
        if (argc < MAXCARGS - 1)
            argv[argc++] = p;
        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = 0;
    }
    argv[argc] = 0;
    if (argc) {
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
        exec(name, argv);
        strcpy(path, "/bin/");
        strcat(path, name);
        exec(path, argv);
        puts("\ncannot chain ");
        puts(name);
        puts("\n");
    }
    exit(2);
}
