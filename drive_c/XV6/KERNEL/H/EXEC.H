/*
 * Header of a separated I&D ("EXE") executable.
 *
 * Kept close to the V6 a.out 8-word header.  An EXE has a distinct code
 * segment and data segment: the kernel runs it with CS = code base and
 * DS = SS = ES = data base.  Each base is a real-mode paragraph address;
 * the code bakes in no segment value, so it is position independent and
 * relocation (or swap-in to a different physical address) is just a
 * rewrite of the segment registers.
 *
 * On disk the file is laid out as:
 *      [ 16-byte struct exec ][ a_text bytes of code ][ a_data bytes of data ]
 * Neither BSS nor the stack is stored.  The data segment is DSEG bytes for
 * every process, laid out low to high as:
 *      [ initialized data (a_data) ][ bss (a_bss) ][ heap -> ... <- stack ]
 * The kernel loads a_data at DS:0, zeroes the rest, and sets SP = DSEG -- so
 * the stack is at the HIGH end and grows down while the break (a_data+a_bss
 * at exec time) grows up.  Both run into the same hole; there is no MMU, so
 * nothing detects a collision and stack overflow is silent.
 */
struct exec
{
    int  a_magic;   /* magic number, A_MAGIC for separated I&D */
    uint a_text;    /* size of code segment in bytes */
    uint a_data;    /* size of initialized data in bytes */
    uint a_bss;     /* size of bss (uninitialized data) in bytes */
    int  a_syms;    /* size of symbol table (unused, 0) */
    uint a_entry;   /* entry point: offset into the code segment */
    uint a_stack;   /* unused: the stack is not sized at link time */
    int  a_flag;    /* unused */
};

#define A_MAGIC 0411    /* separated instruction & data (V6 0411) */

/*
 * Offset, within a process's code segment, of the "jmp _callsig" signal
 * trampoline that psig() vectors a caught signal through.  The EXE startup
 * makes it the first instruction of the code segment.
 */
#define SIGTRAMP_EXE 0x0000
