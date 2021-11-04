#include "utils.h"
#include "debug.h"

#include <stdint.h>

static int64_t
difftimespec_ns(
    struct timespec const * const after, struct timespec const * const before)
{
    return ((int64_t)after->tv_sec - (int64_t)before->tv_sec) * (int64_t)1000000000
         + ((int64_t)after->tv_nsec - (int64_t)before->tv_nsec);
}

static int64_t
difftimespec_msec(
    struct timespec const * const after, struct timespec const * const before)
{
    static unsigned const msec_per_nsec = 1000000;

    return difftimespec_ns(after, before) / msec_per_nsec;
}

bool
get_timestamp(struct timespec * const ts)
{
    return clock_gettime(CLOCK_MONOTONIC, ts) == 0;
}

int
calculate_next_interval_msecs(
    struct timespec const * const previous_start_time,
    unsigned const desired_interval_msecs)
{
    int next_interval_msecs;
    struct timespec now;

    if (!get_timestamp(&now))
    {
        /* Failed to get the time. Just use the full interval time. */
        next_interval_msecs = desired_interval_msecs;
    }
    else
    {
        int64_t const elapsed_time_msecs =
            difftimespec_msec(&now, previous_start_time);

        if (elapsed_time_msecs >= desired_interval_msecs)
        {
            next_interval_msecs = 0;
        }
        else
        {
            next_interval_msecs = desired_interval_msecs - elapsed_time_msecs;
        }
    }

    debug("next interval msecs %u\n", next_interval_msecs);

    return next_interval_msecs;
}

