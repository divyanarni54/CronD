#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../include/control.h"


/*
 * Create Unix control socket
 */
int setup_control_socket(void)
{
    int server_fd;

    struct sockaddr_un address;

    server_fd = socket(
        AF_UNIX,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0)
    {
        perror("[CROND] socket");
        return -1;
    }

    unlink(SOCKET_PATH);

    memset(
        &address,
        0,
        sizeof(address)
    );

    address.sun_family = AF_UNIX;

    strncpy(
        address.sun_path,
        SOCKET_PATH,
        sizeof(address.sun_path) - 1
    );

    if (
        bind(
            server_fd,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0
    )
    {
        perror("[CROND] bind");

        close(server_fd);

        return -1;
    }

    if (
        listen(
            server_fd,
            5
        ) < 0
    )
    {
        perror("[CROND] listen");

        close(server_fd);

        unlink(SOCKET_PATH);

        return -1;
    }

    return server_fd;
}


/*
 * Handle client request
 */
int handle_control_request(
    int client_fd,
    int job_count
)
{
    char command[MAX_CONTROL_COMMAND];

    memset(
        command,
        0,
        sizeof(command)
    );

    ssize_t bytes_read =
        read(
            client_fd,
            command,
            sizeof(command) - 1
        );

    if (bytes_read <= 0)
    {
        return -1;
    }

    command[bytes_read] = '\0';


    /*
     * Remove newline
     */
    command[
        strcspn(
            command,
            "\r\n"
        )
    ] = '\0';


    /*
     * -----------------------------------------
     * STATUS
     * -----------------------------------------
     */
    if (
        strcmp(
            command,
            "status"
        ) == 0
    )
    {
        char response[
            MAX_CONTROL_RESPONSE
        ];

        snprintf(
            response,
            sizeof(response),

            "CronD is running\n"
            "PID: %d\n"
            "Jobs loaded: %d\n",

            getpid(),
            job_count
        );

        write(
            client_fd,
            response,
            strlen(response)
        );

        return 0;
    }


    /*
     * -----------------------------------------
     * RELOAD
     * -----------------------------------------
     */
    if (
        strcmp(
            command,
            "reload"
        ) == 0
    )
    {
        if (
            kill(
                getpid(),
                SIGHUP
            ) < 0
        )
        {
            const char *response =
                "Reload failed\n";

            write(
                client_fd,
                response,
                strlen(response)
            );

            return -1;
        }

        const char *response =
            "Reload requested\n";

        write(
            client_fd,
            response,
            strlen(response)
        );

        return 0;
    }


    /*
     * -----------------------------------------
     * STOP
     * -----------------------------------------
     */
    if (
        strcmp(
            command,
            "stop"
        ) == 0
    )
    {
        if (
            kill(
                getpid(),
                SIGTERM
            ) < 0
        )
        {
            const char *response =
                "Stop failed\n";

            write(
                client_fd,
                response,
                strlen(response)
            );

            return -1;
        }

        const char *response =
            "CronD stopping\n";

        write(
            client_fd,
            response,
            strlen(response)
        );

        return 0;
    }


    /*
     * -----------------------------------------
     * Unknown command
     * -----------------------------------------
     */
    const char *response =
        "Unknown command\n";

    write(
        client_fd,
        response,
        strlen(response)
    );

    return 0;
}
