#ifndef _PC_H
#define _PC_H       1

#define NULL        0

typedef unsigned uint;

/*
 * DeSmet C 3.03 has no far/near pointers, no #pragma and no unsigned long,
 * so the machine interface the Watcom build expressed with far pointers is
 * expressed here as explicit (segment, offset) pairs.  Every access outside
 * the kernel's own data segment goes through these helpers, which live in
 * M86.A because only assembly can load a segment register.
 *
 *      peekb/peekw     read a byte/word at seg:off
 *      pokeb/pokew     write a byte/word at seg:off
 *      fmemcpy         copy n bytes dseg:doff <- sseg:soff
 *      fmemset         fill n bytes at seg:off
 *
 * MK_FP/FP_SEG/FP_OFF are gone: a "far pointer" no longer exists as a value,
 * so a call site that used to build one now passes the two halves along.
 *
 * OFFS is the offset of a member within a struct, used to reach a field of a
 * structure that lives in another segment (a stored u-area, a trap frame).
 */
#define OFFS(type, mem) ((uint)&(((type *)0)->mem))

int  peekb();
int  peekw();
void pokeb();
void pokew();
void fmemcpy();
void fmemset();

/*
 * The kernel's own segment registers.  Under the Watcom .COM build CS, DS
 * and SS were one segment; in DeSmet's small model code and data are
 * separate, so the two are read individually (see pc_init).
 */
uint getcs();
uint getds();

/*
 * Interrupt enable/disable.  These were { _asm cli } macros under Watcom;
 * DeSmet's inline assembly is the #asm/#endasm block form, which cannot be
 * produced by a macro expansion, so they are ordinary calls into M86.A.
 */
void disable();
void enable();

/*
 * Console backend.  KL_BACKEND_KBD is the PC console: PS/2 keyboard in, BIOS
 * teletype out.  KL_SERIAL_TEE additionally copies every console byte to COM1
 * (polled, output only), so a headless run can capture the console on the
 * serial line while the screen stays the interactive console.
 */
#define xKL_BACKEND_UART
#define KL_BACKEND_KBD
#define KL_SERIAL_TEE

/*
 * The UART is brought up when it is either the console backend or the
 * serial tee.  3.03's #if takes neither a defined() operator nor ||, so
 * that disjunction is precomputed here as a plain flag to #ifdef on.
 */
#ifdef KL_BACKEND_UART
#define KL_USES_UART
#endif
#ifdef KL_SERIAL_TEE
#ifndef KL_USES_UART
#define KL_USES_UART
#endif
#endif

#endif
