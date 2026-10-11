/* Render-only deadline planning. No clocks, sleeps, graphics or game state. */
#ifndef HALO_FRAME_PACING_H
#define HALO_FRAME_PACING_H
#include <stdint.h>

struct halo_frame_pacing
{
    uint64_t deadline_ns, previous_now_ns;
    uint32_t frame_limit, fraction;
};

/* Zero initialization is sufficient. Reset after a clock/sleep failure. */
void halo_frame_pacing_reset(struct halo_frame_pacing *pacing);

/* Return an absolute CLOCK_MONOTONIC deadline in nanoseconds, or zero when
 * no sleep is needed. Call exactly once per completed render frame, before
 * pumping input for the next frame. The caller owns reading the clock and
 * sleeping; an EINTR retry must reuse the returned deadline, not call this
 * planner again. clock_nanosleep returns the error number directly.
 *
 * Only 30, 60 and 120 are supported caps. Zero/unknown values and disabled
 * interpolation reset the planner and never add a wait to the original
 * engine throttle. A new cap or backwards clock starts a fresh period.
 * Lateness below one period retains the absolute phase, so a following
 * fast frame can recover cadence. A whole period of lateness starts fresh
 * at now; loading stalls cannot accumulate catch-up frames or sleep debt.
 * Integer fractional periods preserve the requested average cadence. */
uint64_t halo_frame_pacing_deadline(struct halo_frame_pacing *pacing,
    uint64_t now_ns, long frame_limit, int interpolation_enabled);

#endif
