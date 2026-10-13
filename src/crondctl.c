#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../include/control.h"


int main(int argc, char *argv[])
{
    int socket_fd;

    struct sockaddr_un address;

    char response[
        MAX_CONTROL_RESPONSE
    ];


    /*
     * Check command
     */

    if (argc != 2)
    {
        printf("Usage: %s status\n", argv[0]);

        return 1;
    }


    /*
     * Create Unix socket
     */

    socket_fd = socket(
        AF_UNIX,
        SOCK_STREAM,
        0
    );


    if (socket_fd < 0)
    {
        perror("socket");

        return 1;
    }


    /*
     * Clear address structure
     */

    memset(
        &address,
        0,
        sizeof(address)
    );


    address.sun_family =
        AF_UNIX;


    strncpy(
        address.sun_path,
        SOCKET_PATH,
        sizeof(address.sun_path) - 1
    );


    /*
     * Connect to CronD
     */

    if (
        connect(
            socket_fd,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0
    )
    {
        perror("connect");

        close(socket_fd);

        return 1;
    }


    /*
     * Send command
     */

    write(
        socket_fd,
        argv[1],
        strlen(argv[1])
    );


    /*
     * Read response
     */

    memset(
        response,
        0,
        sizeof(response)
    );


    ssize_t bytes_read =
        read(
            socket_fd,
            response,
            sizeof(response) - 1
        );


    if (bytes_read > 0)
    {
        response[bytes_read] = '\0';

        printf(
            "%s",
            response
        );
    }


    /*
     * Close connection
     */

    close(socket_fd);


    return 0;
}
