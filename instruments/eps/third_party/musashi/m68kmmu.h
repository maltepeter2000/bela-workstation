/* eps~: no PMMU on the 68000 (replaces Musashi's m68kmmu.h). */
void m68881_mmu_ops(void) { m68ki_exception_1111(); }
uint pmmu_translate_addr(uint addr_in) { return addr_in; }
