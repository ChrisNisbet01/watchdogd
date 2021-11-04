#pragma once

#include <stdbool.h>
#include <time.h>

bool
get_timestamp(struct timespec * const ts);

int
calculate_next_interval_msecs(
    struct timespec const * previous_start_time,
    unsigned desired_interval_msecs);

