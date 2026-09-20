#include <os.h>

/*
 * Call the system-entry routine f (out of the
 * sysent table). This is a subroutine for trap, and
 * not in-line, because if a signal occurs
 * during processing, an (abnormal) return is simulated from
 * the last caller to savu(qsav); if this took place
 * inside of trap, it wouldn't have a chance to clean up.
 *
 * If this occurs, the return takes place without
 * clearing u_intflg; if it's still set, trap
 * marks an error which means that a system
 * call (like read on a typewriter) got interrupted
 * by a signal.
 */
void trap1(f)
void (*f)();
{
    u.u_intflg = 1;
    if (save(u.u_qsav)) {
        return;
    }
    (*f)();
    u.u_intflg = 0;
}

/*
 * nonexistent system call-- set fatal error code.
 */
void nosys()
{
    u.u_error = 100;
}

/*
 * Ignored system call
 */
void nullsys()
{
}

void trap()
{
    register struct sysent *callp;
    callp = &sysent[u.u_ar0[R3] & 077];

    u.u_dirp = u.u_arg[0];
    trap1(callp->call);
    if(u.u_intflg)
        u.u_error = EINTR;   

    if(u.u_error) {
        /* -(int) matters: u_error is a char, and C88 evaluates a whole-byte
         * expression at byte width before widening it, so plain -u.u_error
         * is 0EAH for EINVAL and reaches C as +234 -- DeSmet's char being
         * unsigned.  Every failing call would then return 256-errno, and no
         * user program's "< 0" or "== -1" test would ever fire.  Probed;
         * "0 - u.u_error" does not help either, only the cast does. */
        u.u_ar0[R0] = -(int)u.u_error;
    }
    u.u_ar0[R3] = u.u_error;
    trap_epilogue();

    if(issig())
        psig();
    setpri(u.u_procp);
}
