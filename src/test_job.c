#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        printf("Child: running date command\n");

        execlp("date", "date", NULL);

        perror("exec");
        exit(1);
    }

    else
    {
        printf("Parent: waiting for child\n");

        int status;

        waitpid(pid, &status, 0);

        printf("Parent: child finished\n");
    }

    return 0;
}
