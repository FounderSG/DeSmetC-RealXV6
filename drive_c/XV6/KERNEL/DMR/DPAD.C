/*
 * DPAD.C -- data-segment spacer, part 1 of 2 (see DPAD2.C).  Together they
 * must be the first objects in the BIND list that contribute to DSEG, so
 * they own DS:0000 and push all real kernel data to DS:8100H and above.
 * With the code linked at 100H..8100H (see CPAD.A) code and data occupy
 * disjoint offsets in ONE segment, which is what lets the kernel run with
 * CS == DS == SS the way the Watcom .COM build did.
 *
 * The "= {1}" matters: C88 emits a fully initialized DB run for a partly
 * initialized array, so this really occupies DSEG address space.  Without
 * an initializer it would be RB, and BIND collects all uninitialized space
 * AFTER all initialized data regardless of link order -- the spacer would
 * land at the end and push nothing.
 *
 * TWO 16512-byte halves, and they must live in TWO SOURCE FILES:
 *
 *   - two arrays, because a single C88 array initializer silently collapses
 *     to ONE byte somewhere above 32768, with no diagnostic;
 *   - two files, because GEN's per-segment size check is an unsigned-overflow
 *     test written on signed ints -- `offs[seg]+offsett < offs[seg]`, with
 *     `int offs[5],offsett` (GEN6.C) -- so it trips at 32768 rather than 65536
 *     and rejects any one source emitting 33024 bytes into a segment with
 *     "pass 2 error segment over 64K".  The shipped 1987 GEN.EXE does not do
 *     this (the GPL source is not the shipping source, same story as its
 *     OPT=0 optimizer), so the DOS build never hit it -- but the REBUILT GEN
 *     does, and that is the one RealXV6's /bin/gen is linked from.  Splitting
 *     the file keeps each GEN run at 16512 and costs the image nothing: BIND
 *     concatenates the two objects in link order, so the bytes are the same.
 *
 * 16512 * 2 = 33024 = 32K + 100H.  e2c strips it back out.
 */
char dhole1[16512] = {1};
