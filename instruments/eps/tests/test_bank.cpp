// Banks (type 23): a new bank built and read back, a copy kept, the effect behind the header.
#include "../pd/engine/eps_bank.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

int main() {
    eps::Bank b;
    b.name = "MY BANK";
    b.track[0].valid = true;
    b.track[0].disk = "DISK1";
    b.track[0].path = {3, 1};
    b.track[0].volume = 100;
    b.track[2].valid = true;
    b.track[2].copy = true;
    b.track[2].copyOf = 0;
    b.track[5].valid = true;
    b.track[5].device = 1;
    b.track[5].path = {2};
    b.effect.assign(0x400, 0x11);
    const std::vector<uint8_t> d = eps::buildBank(b);
    CHECK(d.size() == eps::kBankHeader + 0x400);
    CHECK(d[0] == 0x60 && d[1] == 0 && d[2] == 0 && d[3] == 0);   // 1536 as an effect length
    CHECK(d[4] == 0xA8 && d[5] == 0x20);                            // EPS-16 PLUS
    CHECK(d[0x20] == 0x25);                                         // tracks 1, 3, 6
    eps::Bank r;
    std::string err;
    CHECK(eps::parseBank(d, r, err));
    CHECK(r.name == "MY BANK");
    CHECK(r.track[0].valid && !r.track[0].copy && r.track[0].disk == "DISK1");
    CHECK(r.track[0].path.size() == 6 && r.track[0].path[0] == 3 && r.track[0].path[1] == 1);
    CHECK(r.track[0].volume == 100);
    CHECK(r.track[2].valid && r.track[2].copy && r.track[2].copyOf == 0);
    CHECK(r.track[5].device == 1 && r.track[5].path[0] == 2);
    CHECK(!r.track[1].valid);
    CHECK(r.effect.size() == 0x400);
    CHECK(eps::buildBank(r) == d);                                  // read and written again: the same
    std::printf("test_bank: ok\n");
    return 0;
}
