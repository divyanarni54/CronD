#ifndef CONTROL_H
#define CONTROL_H

#define SOCKET_PATH "/tmp/crond.sock"

#define MAX_CONTROL_COMMAND 128
#define MAX_CONTROL_RESPONSE 512

int setup_control_socket(void);

int handle_control_request(
    int client_fd,
    int job_count
);

#endif
