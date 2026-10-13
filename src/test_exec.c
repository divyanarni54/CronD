#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("Before exec\n");

    execlp("date", "date", NULL);

    perror("execlp failed");

    return 1;
}
