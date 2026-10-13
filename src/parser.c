#include <stdio.h>
#include <string.h>

#include "parser.h"

int parse_cron_line(const char *line, CronJob *job)
{
    int result;

    result = sscanf(
        line,
        "%31s %31s %31s %31s %31s %255[^\n]",
        job->minute,
        job->hour,
        job->day,
        job->month,
        job->weekday,
        job->command
    );

    if (result != 6)
    {
        return -1;
    }

    return 0;
}
