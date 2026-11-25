#pragma once

#include <stdbool.h>

// Local 24-hour time as minutes since midnight, or -1 until set.
int zjh_clock_get_minutes(void);
bool zjh_clock_set_minutes(int minutes);
