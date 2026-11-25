#include "zjh_clock.h"

#include <sys/time.h>

#define MINUTES_PER_DAY 1440
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_DAY (MINUTES_PER_DAY * SECONDS_PER_MINUTE)
// The offline clock tracks time of day. Start it on a known day to distinguish
// a user-set clock from the Unix epoch on power-up.
#define CLOCK_BASE 1704067200LL

int zjh_clock_get_minutes(void)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) != 0 || now.tv_sec < CLOCK_BASE) return -1;
    return (int)((now.tv_sec % SECONDS_PER_DAY) / SECONDS_PER_MINUTE);
}

bool zjh_clock_set_minutes(int minutes)
{
    if (minutes < 0 || minutes >= MINUTES_PER_DAY) return false;
    const struct timeval now = { .tv_sec = CLOCK_BASE + minutes * SECONDS_PER_MINUTE };
    return settimeofday(&now, NULL) == 0;
}
