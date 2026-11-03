#include <assert.h>
#include "button_pipeline.h"
#include "button_state.h"

int main(void)
{
    struct a04_button_pipeline p;
    unsigned both = A04_BUTTON_SOURCE_KEYPAD | A04_BUTTON_SOURCE_PMIC;
    a04_button_pipeline_init(&p);
    /* A held key is sampled but cannot be injected before state is known. */
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == -1);
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == -1);
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == -1);
    a04_button_pipeline_sync(&p, 0);
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == A04_HOME);
    /* A failed notification is retried; an acknowledgement advances state. */
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == A04_HOME);
    a04_button_pipeline_ack(&p, A04_HOME);
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == -1);
    /* An unavailable source neither synthesizes a release nor sends stale data. */
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xffff, A04_BUTTON_SOURCE_PMIC) == -1);
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xffff, both) == -1);
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xffff, both) == -1);
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xffff, both) == A04_HOME);
    a04_button_pipeline_ack(&p, A04_HOME);
    a04_button_pipeline_invalidate(&p);
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == -1);
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == -1);
    assert(a04_button_pipeline_sample(&p, 0xfffd, 0xffff, both) == -1);
    /* The PMIC path remains independent when keypad sampling is unavailable. */
    a04_button_pipeline_sync(&p, 0);
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xfffd, A04_BUTTON_SOURCE_PMIC) == -1);
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xfffd, A04_BUTTON_SOURCE_PMIC) == -1);
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xfffd, A04_BUTTON_SOURCE_PMIC) == A04_POWER);
    a04_button_pipeline_ack(&p, A04_POWER);
    assert(a04_button_pipeline_sample(&p, 0xffff, 0xfffd, A04_BUTTON_SOURCE_PMIC) == -1);
    return 0;
}
