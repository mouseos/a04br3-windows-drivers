#include <assert.h>
#include <stdint.h>
#include "ring.h"
int main(void) {
    struct mt8167_ring_span s[2] = {{7, 8}, {9, 10}};
    assert(mt8167_ring_spans(1024, 1000, 32, s));
    assert(s[0].offset == 1000 && s[0].length == 24);
    assert(s[1].offset == 0 && s[1].length == 8);
    assert(mt8167_ring_spans(1024, 0, 1024, s));
    assert(s[0].length == 1024 && s[1].length == 0);
    assert(!mt8167_ring_spans(1024, 1024, 4, s));
    assert(!mt8167_ring_spans(1024, 0, 1028, s));
    assert(!mt8167_ring_spans(1024, 2, 4, s));
    assert(!mt8167_ring_spans(1024, 0, 0, s));
    assert(!mt8167_ring_spans(0, 0, 4, s));
    assert(!mt8167_ring_spans(1024, 0, 4, 0));
    assert(s[0].length == 1024 && s[1].length == 0);
    assert(mt8167_ring_spans(0xfffffffcu, 0xfffffff8u, 8, s));
    assert(s[0].length == 4 && s[1].length == 4);
    return 0;
}
