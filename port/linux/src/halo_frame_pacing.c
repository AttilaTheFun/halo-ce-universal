#include "halo_frame_pacing.h"

void halo_frame_pacing_reset(struct halo_frame_pacing *pacing)
{
    if (!pacing) return;
    pacing->deadline_ns = 0;
    pacing->previous_now_ns = 0;
    pacing->frame_limit = 0;
    pacing->fraction = 0;
}

uint64_t halo_frame_pacing_deadline(struct halo_frame_pacing *pacing,
    uint64_t now_ns, long frame_limit, int interpolation_enabled)
{
    uint64_t period, deadline;
    uint32_t limit, fraction;
    if (!pacing) return 0;
    if (!interpolation_enabled ||
        (frame_limit != 30 && frame_limit != 60 && frame_limit != 120))
    {
        halo_frame_pacing_reset(pacing);
        return 0;
    }
    limit = (uint32_t)frame_limit;
    if (pacing->frame_limit != limit || now_ns < pacing->previous_now_ns)
    {
        pacing->deadline_ns = now_ns;
        /* Ceil cumulative fractional periods instead of rounding every
         * frame; exactly limit periods span one second. */
        pacing->fraction = limit - 1;
    }
    pacing->frame_limit = limit;
    pacing->previous_now_ns = now_ns;
    period = UINT64_C(1000000000) / limit;
    fraction = pacing->fraction + (uint32_t)(UINT64_C(1000000000) % limit);
    if (fraction >= limit)
    {
        period++;
        fraction -= limit;
    }
    if (pacing->deadline_ns > UINT64_MAX - period)
    {
        halo_frame_pacing_reset(pacing);
        return 0;
    }
    deadline = pacing->deadline_ns + period;
    if (deadline <= now_ns)
    {
        if (now_ns - deadline < period)
        {
            /* Preserve short lateness so a following fast frame can recover
             * the absolute cadence without accumulating fractional drift. */
            pacing->deadline_ns = deadline;
            pacing->fraction = fraction;
        }
        else
        {
            /* A whole missed period or stall starts fresh; do not walk
             * through old deadlines or accumulate catch-up frames. */
            pacing->deadline_ns = now_ns;
            pacing->fraction = limit - 1;
        }
        return 0;
    }
    pacing->deadline_ns = deadline;
    pacing->fraction = fraction;
    return deadline;
}
