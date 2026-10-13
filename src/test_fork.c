#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("Before fork\n");

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        printf("I am the child process\n");
    }
    else
    {
        printf("I am the parent process\n");
    }

    return 0;
}
