#include <stdio.h>

#include "../include/parser.h"

int main()
{
    CronJob job;

    const char *line = "* * * * * date";

    if (parse_cron_line(line, &job) != 0)
    {
        printf("Parser failed\n");
        return 1;
    }

    printf("Minute  : %s\n", job.minute);
    printf("Hour    : %s\n", job.hour);
    printf("Day     : %s\n", job.day);
    printf("Month   : %s\n", job.month);
    printf("Weekday : %s\n", job.weekday);
    printf("Command : %s\n", job.command);

    return 0;
}
