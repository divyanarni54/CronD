#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/poll.h>

#include "../include/parser.h"
#include "../include/scheduler.h"
#include "../include/command.h"
#include "../include/control.h"

#define MAX_JOBS 100
#define PID_FILE "/tmp/crond.pid"
#define LOG_DIR "logs"
#define CRONTAB_FILE "config/crontab"

static CronJob jobs[MAX_JOBS];

static int job_count = 0;

static pid_t running_pids[MAX_JOBS];

static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t child_requested = 0;
static volatile sig_atomic_t shutdown_requested = 0;


/* ---------------------------------------------------------
   Signal handlers
   --------------------------------------------------------- */

static void handle_sighup(int signal_number)
{
    (void)signal_number;

    reload_requested = 1;
}


static void handle_sigchld(int signal_number)
{
    (void)signal_number;

    child_requested = 1;
}


static void handle_sigterm(int signal_number)
{
    (void)signal_number;

    shutdown_requested = 1;
}


/* ---------------------------------------------------------
   Install signal handlers
   --------------------------------------------------------- */

static void setup_signals(void)
{
    struct sigaction action;

    memset(
        &action,
        0,
        sizeof(action)
    );

    sigemptyset(
        &action.sa_mask
    );

    action.sa_flags = 0;

    action.sa_handler = handle_sighup;

    sigaction(
        SIGHUP,
        &action,
        NULL
    );


    action.sa_handler = handle_sigchld;

    sigaction(
        SIGCHLD,
        &action,
        NULL
    );


    action.sa_handler = handle_sigterm;

    sigaction(
        SIGTERM,
        &action,
        NULL
    );

    sigaction(
        SIGINT,
        &action,
        NULL
    );
}


/* ---------------------------------------------------------
   Check PID file
   --------------------------------------------------------- */

static int check_existing_process(void)
{
    FILE *file;

    pid_t old_pid;

    file = fopen(
        PID_FILE,
        "r"
    );

    if (file == NULL)
    {
        return 0;
    }

    if (
        fscanf(
            file,
            "%d",
            &old_pid
        ) != 1
    )
    {
        fclose(file);

        return 0;
    }

    fclose(file);


    if (
        kill(
            old_pid,
            0
        ) == 0
    )
    {
        fprintf(
            stderr,
            "CronD is already running. PID: %d\n",
            old_pid
        );

        return -1;
    }

    return 0;
}


/* ---------------------------------------------------------
   Write PID file
   --------------------------------------------------------- */

static int write_pid_file(void)
{
    FILE *file;

    file = fopen(
        PID_FILE,
        "w"
    );

    if (file == NULL)
    {
        perror("fopen pid file");

        return -1;
    }

    fprintf(
        file,
        "%d\n",
        getpid()
    );

    fclose(file);

    return 0;
}


/* ---------------------------------------------------------
   Remove PID file
   --------------------------------------------------------- */

static void remove_pid_file(void)
{
    unlink(PID_FILE);
}


/* ---------------------------------------------------------
   Daemonize
   --------------------------------------------------------- */

static int daemonize_process(void)
{
    pid_t pid;

    pid = fork();

    if (pid < 0)
    {
        perror("fork");

        return -1;
    }

    if (pid > 0)
    {
        exit(0);
    }


    if (
        setsid() < 0
    )
    {
        perror("setsid");

        return -1;
    }


    pid = fork();

    if (pid < 0)
    {
        perror("fork");

        return -1;
    }

    if (pid > 0)
    {
        exit(0);
    }


    umask(0);


    if (
        chdir(".") < 0
    )
    {
        perror("chdir");

        return -1;
    }


    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);


    open(
        "/dev/null",
        O_RDONLY
    );

    open(
        "/dev/null",
        O_WRONLY
    );

    open(
        "/dev/null",
        O_WRONLY
    );

    return 0;
}


/* ---------------------------------------------------------
   Load crontab
   --------------------------------------------------------- */

static int load_crontab(void)
{
    FILE *file;

    char line[512];

    int count = 0;


    file = fopen(
        CRONTAB_FILE,
        "r"
    );

    if (file == NULL)
    {
        perror("fopen crontab");

        return -1;
    }


    while (
        fgets(
            line,
            sizeof(line),
            file
        ) != NULL
    )
    {
        CronJob job;

        char *ptr;


        if (count >= MAX_JOBS)
        {
            break;
        }


        ptr = line;

        while (
            *ptr == ' ' ||
            *ptr == '\t'
        )
        {
            ptr++;
        }


        if (
            *ptr == '\0' ||
            *ptr == '\n' ||
            *ptr == '#'
        )
        {
            continue;
        }


        memset(
            &job,
            0,
            sizeof(job)
        );


        if (
            parse_cron_line(
                ptr,
                &job
            ) == 0
        )
        {
            jobs[count] = job;

            count++;
        }
        else
        {
            fprintf(
                stderr,
                "Invalid crontab line: %s",
                line
            );
        }
    }


    fclose(file);


    job_count = count;

    /*
     * IMPORTANT:
     *
     * Do NOT clear running_pids here.
     *
     * Existing jobs must continue running
     * when SIGHUP reloads the schedule.
     */


    return 0;
}


/* ---------------------------------------------------------
   Create log directory
   --------------------------------------------------------- */

static void create_log_directory(void)
{
    mkdir(
        LOG_DIR,
        0755
    );
}


/* ---------------------------------------------------------
   Check whether a job is already running
   --------------------------------------------------------- */

static int is_job_running(int job_index)
{
    if (
        job_index < 0 ||
        job_index >= MAX_JOBS
    )
    {
        return 0;
    }


    if (
        running_pids[job_index] <= 0
    )
    {
        return 0;
    }


    if (
        kill(
            running_pids[job_index],
            0
        ) == 0
    )
    {
        return 1;
    }


    if (
        errno == EPERM
    )
    {
        return 1;
    }


    return 0;
}


/* ---------------------------------------------------------
   Launch one job
   --------------------------------------------------------- */

static void run_job(int job_index)
{
    pid_t pid;

    char command_copy[MAX_COMMAND];

    char *argv[MAX_ARGS];

    char log_path[256];

    int log_fd;


    if (
        job_index < 0 ||
        job_index >= job_count
    )
    {
        return;
    }


    /*
     * No-overlap protection.
     */

    if (
        is_job_running(job_index)
    )
    {
        FILE *log_file;

        snprintf(
            log_path,
            sizeof(log_path),
            "%s/job_%d.log",
            LOG_DIR,
            job_index + 1
        );


        log_file = fopen(
            log_path,
            "a"
        );


        if (log_file != NULL)
        {
            fprintf(
                log_file,
                "[JOB %d] Already running. Skipping this run.\n",
                job_index + 1
            );

            fclose(log_file);
        }

        return;
    }


    pid = fork();


    if (pid < 0)
    {
        perror("fork");

        return;
    }


    if (pid == 0)
    {
        /*
         * Child process.
         */

        snprintf(
            log_path,
            sizeof(log_path),
            "%s/job_%d.log",
            LOG_DIR,
            job_index + 1
        );


        log_fd = open(
            log_path,
            O_WRONLY | O_CREAT | O_APPEND,
            0644
        );


        if (log_fd < 0)
        {
            _exit(1);
        }


        dup2(
            log_fd,
            STDOUT_FILENO
        );

        dup2(
            log_fd,
            STDERR_FILENO
        );


        close(log_fd);


        printf(
            "[JOB %d] Running: %s\n",
            job_index + 1,
            jobs[job_index].command
        );

        fflush(stdout);


        /*
         * Make a writable copy because
         * parse_command() modifies the string.
         */

        strncpy(
            command_copy,
            jobs[job_index].command,
            sizeof(command_copy) - 1
        );

        command_copy[
            sizeof(command_copy) - 1
        ] = '\0';


        parse_command(
            command_copy,
            argv
        );


        if (
            argv[0] == NULL
        )
        {
            _exit(1);
        }


        execvp(
            argv[0],
            argv
        );


        perror("execvp");

        _exit(127);
    }


    /*
     * Parent process.
     */

    running_pids[job_index] = pid;


    snprintf(
        log_path,
        sizeof(log_path),
        "%s/job_%d.log",
        LOG_DIR,
        job_index + 1
    );


    FILE *log_file = fopen(
        log_path,
        "a"
    );


    if (log_file != NULL)
    {
        fprintf(
            log_file,
            "[JOB %d] Started. PID: %d\n",
            job_index + 1,
            pid
        );

        fclose(log_file);
    }
}


/* ---------------------------------------------------------
   Reap finished children
   --------------------------------------------------------- */

static void reap_children(void)
{
    int status;

    pid_t pid;


    while (1)
    {
        pid = waitpid(
            -1,
            &status,
            WNOHANG
        );


        if (pid <= 0)
        {
            break;
        }


        int job_index;

        for (
            job_index = 0;
            job_index < MAX_JOBS;
            job_index++
        )
        {
            if (
                running_pids[job_index] == pid
            )
            {
                char log_path[256];

                snprintf(
                    log_path,
                    sizeof(log_path),
                    "%s/job_%d.log",
                    LOG_DIR,
                    job_index + 1
                );


                FILE *log_file = fopen(
                    log_path,
                    "a"
                );


                if (log_file != NULL)
                {
                    int exit_status = -1;


                    if (
                        WIFEXITED(status)
                    )
                    {
                        exit_status =
                            WEXITSTATUS(status);
                    }


                    fprintf(
                        log_file,
                        "[JOB %d] Finished. PID: %d. Exit status: %d\n",
                        job_index + 1,
                        pid,
                        exit_status
                    );


                    fclose(log_file);
                }


                running_pids[job_index] = 0;

                break;
            }
        }
    }
}


/* ---------------------------------------------------------
   Run jobs scheduled for current minute
   --------------------------------------------------------- */

static void run_due_jobs(void)
{
    time_t now;

    struct tm timeinfo;

    int job_index;


    now = time(NULL);


    if (
        localtime_r(
            &now,
            &timeinfo
        ) == NULL
    )
    {
        return;
    }


    for (
        job_index = 0;
        job_index < job_count;
        job_index++
    )
    {
        if (
            job_matches_time(
                &jobs[job_index],
                &timeinfo
            )
        )
        {
            run_job(job_index);
        }
    }
}


/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

int main(void)
{
    int control_fd;

    time_t next_run;

    int result;

    int timeout;


    /*
     * Prevent multiple CronD instances.
     */

    if (
        check_existing_process() < 0
    )
    {
        return 1;
    }


    /*
     * Daemonize.
     */

    if (
        daemonize_process() < 0
    )
    {
        return 1;
    }


    /*
     * PID file.
     */

    if (
        write_pid_file() < 0
    )
    {
        return 1;
    }


    /*
     * Signals.
     */

    setup_signals();


    /*
     * Logs.
     */

    create_log_directory();


    /*
     * Initial crontab load.
     */

    if (
        load_crontab() < 0
    )
    {
        remove_pid_file();

        return 1;
    }


    /*
     * Unix control socket.
     */

    control_fd =
        setup_control_socket();


    if (
        control_fd < 0
    )
    {
        remove_pid_file();

        return 1;
    }


    /*
     * First scheduler target:
     * next minute boundary.
     */

    next_run =
        get_next_minute();


    /*
     * Main scheduler loop.
     */

    while (
        !shutdown_requested
    )
    {
        struct pollfd poll_fd;


        /*
         * Calculate milliseconds until
         * the next scheduled minute.
         */

        time_t now =
            time(NULL);


        if (
            next_run > now
        )
        {
            timeout =
                (int)(
                    (next_run - now)
                    * 1000
                );
        }
        else
        {
            timeout = 0;
        }


        /*
         * Never wait for a negative timeout.
         */

        if (
            timeout < 0
        )
        {
            timeout = 0;
        }


        poll_fd.fd =
            control_fd;

        poll_fd.events =
            POLLIN;

        poll_fd.revents =
            0;


        result =
            poll(
                &poll_fd,
                1,
                timeout
            );


        /*
         * SIGHUP:
         * reload configuration.
         */

        if (
            reload_requested
        )
        {
            reload_requested = 0;


            load_crontab();


            /*
             * Start counting from the next
             * minute after reload.
             */

            next_run =
                get_next_minute();


            continue;
        }


        /*
         * SIGCHLD:
         * reap completed jobs.
         */

        if (
            child_requested
        )
        {
            child_requested = 0;

            reap_children();
        }


        /*
         * Control socket request.
         */

        if (
            result > 0 &&
            (
                poll_fd.revents
                & POLLIN
            )
        )
        {
            int client_fd;


            client_fd =
                accept(
                    control_fd,
                    NULL,
                    NULL
                );


            if (
                client_fd >= 0
            )
            {
                handle_control_request(
                    client_fd,
                    job_count
                );


                close(client_fd);
            }
        }


        /*
         * Stop requested.
         */

        if (
            shutdown_requested
        )
        {
            break;
        }


        /*
         * Scheduler timeout:
         * run jobs for this minute.
         */

        if (
            result == 0
        )
        {
            run_due_jobs();


            /*
             * Calculate the next minute.
             * This avoids cumulative drift.
             */

            next_run =
                get_next_minute();
        }
    }


    /*
     * Reap any children that already finished.
     */

    reap_children();


    /*
     * Close control socket.
     */

    close(control_fd);


    /*
     * Remove Unix socket.
     */

    unlink(
        SOCKET_PATH
    );


    /*
     * Remove PID file.
     */

    remove_pid_file();


    return 0;
}
