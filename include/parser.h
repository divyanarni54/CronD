#ifndef PARSER_H
#define PARSER_H

#define MAX_COMMAND 256

typedef struct
{
    char minute[32];
    char hour[32];
    char day[32];
    char month[32];
    char weekday[32];

    char command[MAX_COMMAND];

} CronJob;

int parse_cron_line(const char *line, CronJob *job);

#endif
