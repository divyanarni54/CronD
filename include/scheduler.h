#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <time.h>

#include "parser.h"

int job_matches_time(
    const CronJob *job,
    const struct tm *timeinfo
);

time_t get_next_minute(void);

int sleep_until(time_t target);

#endif
