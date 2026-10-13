#include <stdio.h>
#include <time.h>

#include "../include/parser.h"
#include "../include/scheduler.h"

int main()
{
    time_t now = time(NULL);

    time_t next = get_next_minute();

    printf("Current time : %s", ctime(&now));
    printf("Next minute  : %s", ctime(&next));

    return 0;
}
