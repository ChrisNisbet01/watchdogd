#pragma once

#include <stdbool.h>

void
disable_watchdog(int fd);

void
stroke_watchdog(int fd);

bool
set_watchdog_secs(int fd, int secs);

int
get_watchdog_secs(int fd);

int
watchdog_open(void);

void
watchdog_close(int fd, bool disable);

