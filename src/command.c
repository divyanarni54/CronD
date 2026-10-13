#include <string.h>

#include "command.h"

int parse_command(char *command, char *argv[])
{
    int count = 0;

    char *token = strtok(command, " \t\n");

    while (token != NULL && count < MAX_ARGS - 1)
    {
        argv[count] = token;
        count++;

        token = strtok(NULL, " \t\n");
    }

    argv[count] = NULL;

    return count;
}
