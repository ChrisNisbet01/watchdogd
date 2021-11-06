#include "watchdog_fd.h"

#include <ubus_utils/ubus_utils.h>

#include <fcntl.h>
#include <linux/watchdog.h>
#include <sys/ioctl.h>

void
disable_watchdog(int const fd)
{
    if (fd >= 0)
    {
        char const V = 'V';

        TEMP_FAILURE_RETRY(write(fd, &V, sizeof V));
    }
}

void
stroke_watchdog(int const fd)
{
    if (fd >= 0)
    {
        int dummy;

        ioctl(fd, WDIOC_KEEPALIVE, &dummy);
    }
}

bool
set_watchdog_secs(int const fd, int const secs)
{
    bool const success = fd >= 0 && ioctl(fd, WDIOC_SETTIMEOUT, &secs) == 0;

    return success;
}

int
get_watchdog_secs(int const fd)
{
    int watchdog_secs;

    if (fd >= 0)
    {
        int const err = ioctl(fd, WDIOC_GETTIMEOUT, &watchdog_secs);

        if (err != 0)
        {
            watchdog_secs = err;
        }
    }
    else
    {
        watchdog_secs = -1;
    }

    return watchdog_secs;
}

int
watchdog_open(void)
{
    return open("/dev/watchdog", O_WRONLY | O_CLOEXEC);
}

void
watchdog_close(int const fd, bool const disable)
{
    if (fd >= 0)
    {
        if (disable)
        {
            disable_watchdog(fd);
        }
        close(fd);
    }
}

