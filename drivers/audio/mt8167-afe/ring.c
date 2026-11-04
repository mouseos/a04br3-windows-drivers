#include "ring.h"
int mt8167_ring_spans(uint32_t size, uint32_t offset, uint32_t bytes,
                      struct mt8167_ring_span spans[2])
{
    struct mt8167_ring_span a, b;
    uint32_t available;
    if (!spans || !size || !bytes || offset >= size || bytes > size ||
        ((size | offset | bytes) & 3u)) return 0;
    available = size - offset;
    a.offset = offset;
    a.length = bytes < available ? bytes : available;
    b.offset = 0;
    b.length = bytes - a.length;
    spans[0] = a;
    spans[1] = b;
    return 1;
}
