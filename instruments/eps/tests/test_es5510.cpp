// ES5510 core: a pass-through program and a one-frame delay through the delay RAM.
#include "../pd/engine/es5510.h"
#include <cstdio>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static void put(eps::ES5510 &e, int at, uint64_t ins, int gpr = 0) {
    for (int k = 0; k < 3; ++k) e.hostWrite(k, uint8_t(gpr >> (16 - 8 * k)));
    for (int k = 0; k < 6; ++k) e.hostWrite(3 + k, uint8_t(ins >> (40 - 8 * k)));
    e.hostWrite(0xe0, uint8_t(at));
}
// D C B A op opsel skip acc ram
static uint64_t ins(int d, int c, int b, int a, int op, int sel = 0, int ram = 0) {
    return uint64_t(d) << 40 | uint64_t(c) << 32 | uint64_t(b) << 24 | uint64_t(a) << 16 | uint64_t(op) << 12 |
           uint64_t(sel) << 8 | uint64_t(ram) << 3;
}

int main() {
    eps::ES5510 e;
    // MOV SER1L <- SER0L, END
    put(e, 0, ins(0xff, 0xff, 0xeb, 0xed, 9));
    put(e, 1, ins(0xff, 0xff, 0xff, 0xff, 15));
    e.setSerial(0, 1234);
    e.runFrame();
    CHECK(e.serial(2) == 1234);
    e.setSerial(0, -500);
    e.runFrame();
    CHECK(e.serial(2) == -500);

    // a delay line of 4 words: write SER0L, read it back after the line has turned once
    eps::ES5510 d;
    put(d, 0, ins(0xff, 0xff, 0xeb, 0x00, 9, 5, 1));   // MOV g0 <- SER0L, also to DOL; RAM write (delay)
    put(d, 1, ins(0xff, 0xff, 0xff, 0xff, 9, 0, 0));   // RAM read (delay) of offset 0
    put(d, 2, ins(0xff, 0xff, 0xf4, 0xed, 9));         // MOV SER1L <- DIL
    put(d, 3, ins(0xff, 0xff, 0xff, 0xff, 15));
    put(d, 0xf4, 0, 0x0000ff);   // MEMSIZ: 8 bits below the word address
    put(d, 0xf5, 0, 0x0003ff);   // DLENGTH: 4 words
    int first = -1;
    for (int f = 0; f < 12; ++f) {
        d.setSerial(0, f == 2 ? 3000 : 0);
        d.runFrame();
        if (first < 0 && d.serial(2) == 3000) first = f;
    }
    printf("  delay line: the impulse of frame 2 comes back at frame %d\n", first);
    CHECK(first > 2);
    printf("test_es5510: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
