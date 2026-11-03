#include <assert.h>
#include "button_state.h"

int main(void)
{
    uint8_t sent = 0;
    assert(a04_buttons_decode(0xffff, 0xffff) == 0);
    assert(a04_buttons_decode(0xfffc, 0xfff9) == 0x0f);
    assert(a04_buttons_decode(0xfffe, 0xffff) == (1u << A04_VOLUME_UP));
    assert(a04_buttons_decode(0xffff, 0xfffd) == (1u << A04_POWER));
    assert(a04_buttons_next(1u << A04_HOME, sent) == A04_HOME);
    /* A failed injection leaves sent unchanged; a retry must repeat it. */
    assert(a04_buttons_next(1u << A04_HOME, sent) == A04_HOME);
    sent = a04_buttons_ack(sent, A04_HOME);
    assert(a04_buttons_next(1u << A04_HOME, sent) == -1);
    assert(a04_buttons_next(0, sent) == A04_HOME);
    sent = a04_buttons_ack(sent, A04_HOME);
    assert(sent == 0 && a04_buttons_ack(sent, 99) == 0);
    return 0;
}
