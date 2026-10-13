#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "../include/scheduler.h"


/*
 * =========================================================
 * Check whether a cron field matches the current value
 * =========================================================
 *
 * Supported formats:
 *
 *   *           -> every value
 *   5           -> only value 5
 *   every-5     -> every 5 values
 *   1,15,30     -> 1 OR 15 OR 30
 *   1-5         -> values from 1 to 5
 *
 * =========================================================
 */
static int field_matches(
    const char *field,
    int value
)
{
    /*
     * -----------------------------------------------------
     * Wildcard
     * -----------------------------------------------------
     */

    if (
        strcmp(
            field,
            "*"
        ) == 0
    )
    {
        return 1;
    }


    /*
     * -----------------------------------------------------
     * Step expression
     *
     * Example:
     * every 5 values
     * -----------------------------------------------------
     */

    if (
        strncmp(
            field,
            "*/",
            2
        ) == 0
    )
    {
        int step =
            atoi(
                field + 2
            );


        /*
         * Invalid step
         */

        if (
            step <= 0
        )
        {
            return 0;
        }


        /*
         * Check the step
         */

        return (
            value % step
        ) == 0;
    }


    /*
     * -----------------------------------------------------
     * Comma-separated values
     *
     * Example:
     *
     * 1,15,30
     * -----------------------------------------------------
     */

    if (
        strchr(
            field,
            ','
        ) != NULL
    )
    {
        char copy[64];


        /*
         * Copy field because strtok()
         * modifies the string.
         */

        strncpy(
            copy,
            field,
            sizeof(copy) - 1
        );


        copy[
            sizeof(copy) - 1
        ] = '\0';


        char *token =
            strtok(
                copy,
                ","
            );


        while (
            token != NULL
        )
        {
            /*
             * Check current value
             */

            if (
                atoi(token) == value
            )
            {
                return 1;
            }


            /*
             * Get next value
             */

            token =
                strtok(
                    NULL,
                    ","
                );
        }


        /*
         * No value matched
         */

        return 0;
    }


    /*
     * -----------------------------------------------------
     * Range
     *
     * Example:
     *
     * 1-5
     * -----------------------------------------------------
     */

    if (
        strchr(
            field,
            '-'
        ) != NULL
    )
    {
        int start;
        int end;


        /*
         * Read start and end
         */

        if (
            sscanf(
                field,
                "%d-%d",
                &start,
                &end
            ) == 2
        )
        {
            /*
             * Check whether value
             * is inside the range.
             */

            return (
                value >= start &&
                value <= end
            );
        }


        /*
         * Invalid range
         */

        return 0;
    }


    /*
     * -----------------------------------------------------
     * Single number
     *
     * Example:
     *
     * 5
     * -----------------------------------------------------
     */

    return (
        atoi(field) == value
    );
}


/*
 * =========================================================
 * Check whether a cron job should run at this time
 * =========================================================
 */

int job_matches_time(
    const CronJob *job,
    const struct tm *timeinfo
)
{
    /*
     * -----------------------------------------------------
     * Get current minute
     *
     * Range: 0 - 59
     * -----------------------------------------------------
     */

    int minute =
        timeinfo->tm_min;


    /*
     * -----------------------------------------------------
     * Get current hour
     *
     * Range: 0 - 23
     * -----------------------------------------------------
     */

    int hour =
        timeinfo->tm_hour;


    /*
     * -----------------------------------------------------
     * Get day of month
     *
     * Range: 1 - 31
     * -----------------------------------------------------
     */

    int day =
        timeinfo->tm_mday;


    /*
     * -----------------------------------------------------
     * Get month
     *
     * tm_mon is 0 - 11.
     *
     * Add 1 to get 1 - 12.
     * -----------------------------------------------------
     */

    int month =
        timeinfo->tm_mon + 1;


    /*
     * -----------------------------------------------------
     * Get weekday
     *
     * 0 = Sunday
     * 1 = Monday
     * 2 = Tuesday
     * 3 = Wednesday
     * 4 = Thursday
     * 5 = Friday
     * 6 = Saturday
     * -----------------------------------------------------
     */

    int weekday =
        timeinfo->tm_wday;


    /*
     * -----------------------------------------------------
     * Check minute
     * -----------------------------------------------------
     */

    if (
        !field_matches(
            job->minute,
            minute
        )
    )
    {
        return 0;
    }


    /*
     * -----------------------------------------------------
     * Check hour
     * -----------------------------------------------------
     */

    if (
        !field_matches(
            job->hour,
            hour
        )
    )
    {
        return 0;
    }


    /*
     * -----------------------------------------------------
     * Check day
     * -----------------------------------------------------
     */

    if (
        !field_matches(
            job->day,
            day
        )
    )
    {
        return 0;
    }


    /*
     * -----------------------------------------------------
     * Check month
     * -----------------------------------------------------
     */

    if (
        !field_matches(
            job->month,
            month
        )
    )
    {
        return 0;
    }


    /*
     * -----------------------------------------------------
     * Check weekday
     * -----------------------------------------------------
     */

    if (
        !field_matches(
            job->weekday,
            weekday
        )
    )
    {
        return 0;
    }


    /*
     * All five fields matched.
     */

    return 1;
}


/*
 * =========================================================
 * Get the next minute boundary
 * =========================================================
 */

time_t get_next_minute(void)
{
    /*
     * Get current time.
     */

    time_t now =
        time(NULL);


    struct tm next;


    /*
     * Convert time_t to local time.
     */

    if (
        localtime_r(
            &now,
            &next
        ) == NULL
    )
    {
        return (
            (time_t)-1
        );
    }


    /*
     * Set seconds to zero.
     */

    next.tm_sec = 0;


    /*
     * Move to the next minute.
     */

    next.tm_min++;


    /*
     * Convert local time back
     * to time_t.
     */

    return mktime(
        &next
    );
}


/*
 * =========================================================
 * Sleep until an absolute time
 * =========================================================
 */

int sleep_until(
    time_t target
)
{
    struct timespec ts;


    /*
     * Set seconds.
     */

    ts.tv_sec =
        target;


    /*
     * No nanoseconds.
     */

    ts.tv_nsec =
        0;


    /*
     * Sleep until the exact
     * absolute time.
     */

    return clock_nanosleep(
        CLOCK_REALTIME,
        TIMER_ABSTIME,
        &ts,
        NULL
    );
}
