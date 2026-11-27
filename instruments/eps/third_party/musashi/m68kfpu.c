/* eps~: the 68000 needs no FPU. This file replaces Musashi's m68kfpu.c (and with it the
   SoftFloat dependency); an F-line opcode is the 68000's line-1111 exception. */
void m68040_fpu_op0(void) { m68ki_exception_1111(); }
void m68040_fpu_op1(void) { m68ki_exception_1111(); }
