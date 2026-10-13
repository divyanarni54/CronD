#include <stdio.h>
#include <time.h>

#include "../include/scheduler.h"

int main()
{
    time_t now = time(NULL);

    time_t next = get_next_minute();

    printf("Current time : %s", ctime(&now));
    printf("Sleeping until: %s", ctime(&next));

    printf("Waiting...\n");

    sleep_until(next);

    printf("Woke up!\n");

    time_t after = time(NULL);

    printf("Current time : %s", ctime(&after));

    return 0;
}
