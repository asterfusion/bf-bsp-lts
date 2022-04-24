/*---------------------------------------------------------------------------
 * File:       swutil.c
 * Purpose:    the client program of switch driver process.
 *
 * Notes:
 * History:
 *  2014/05/15    -- Aaron Lien, Initial version.
 *  2014/07/03    -- Aaron Lien,
 *                  (1). add new control command to restart switch driver.
 *                  (2). do limited retry if connection is failed.
 *
 * Copyright (C) 2014  Asterfusion Corporation
 */
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>  //inet_addr

#if 1
#define DB(x) x
#else
#define DB(x) \
  do {        \
  } while (0)
#endif

#define CFG_SWDRV_LOG_FILE "/tmp/swdrv.log"
#define CFG_SWDRV_PATH "/usr/local/bin"
#define CFG_SWDRV_FILE "bcm.user"

#define CFG_SWDRV_MAX_BOOT_TIME \
  180 /*- max. booting time of switch driver, seconds */
#define CFG_SWDRV_MAX_RETRY 20 /*- max. limited retry for making connection */
#define MAX_STR_LEN 256

#define TCP_PORT_BASE_DEFAULT 17000 /* server port */
#define CLIENT_CMD_MAX_SIZE 128
#define BUF_SIZE 1000

char *usage[] = {
    "%s\t - the switch utility\n",
    "Usage: %s <sub-command> [<args>]\n"
    "<sub-command> := <Control command> | <Operation command>\n"
    "Operation command - The native command of the switch driver.\n"
    "Control command   - The command for switch driver control.\n"
    "Control command format: -C <option> [<args>]\n"
    "option:\n"
    "\t -r --install-path=<path>  Reload switch driver.\n"
    "\t -S --install-path=<path>  Start switch driver.\n"
    "\t -s                        Shutdown switch driver.\n"
    "\t -h                        This help message.\n",
    NULL
};
#define CMD_OPTION_LIST "rSshH"

static char g_option = -1;
static char g_option_args[MAX_STR_LEN];

static void show_help (char *name);
static int get_user_cmd_option (int argc,
                                char *argv[]);
static int do_op_command (int argc, char *argv[]);
static int swdrv_start (char *options);
static int swdrv_reload (char *options);
static pid_t g_swdrv_pid = -1;

static void show_help (char *name)
{
    int i;

    for (i = 0; usage[i]; i++) {
        printf (usage[i], name);
    }
}

static int get_user_cmd_option (int argc,
                                char *argv[])
{
    int opt;
    int mgm_argc;
    char **mgm_argv;

    if (argc < 2) {
        return -1;
    }

    if (strcmp (argv[1], "-C") != 0) {
        g_option = 0;
        return EXIT_SUCCESS;
    }

    argv[1] = argv[0];
    mgm_argc = argc - 1;
    mgm_argv = &argv[1];

    g_option = -1;
    opterr = 0;
    while ((opt = getopt (mgm_argc, mgm_argv,
                          CMD_OPTION_LIST)) != -1) {
        DB (printf ("[argc] %d, [optind] %d, argv[%d]=%s, optarg=%s\n",
                    mgm_argc,
                    optind,
                    optind,
                    mgm_argv[optind],
                    optarg));

        switch (opt) {
            case 'r':
            case 'S':
                g_option = opt;
                if (optind >= mgm_argc) {
                    g_option_args[0] = 0;
                } else {
                    strncpy (g_option_args, mgm_argv[optind],
                             MAX_STR_LEN);
                }
                return 0;

            case 's':
            case 'H':
            case 'h':
                g_option = opt;
                return EXIT_SUCCESS;

            default:
                return EXIT_FAILURE;
        }
    }

    return EXIT_FAILURE;
}

static bool is_driver_running (void)
{
    int ret = 0;

    ret = system ("ps ax | grep -v grep | grep bf_switchd > /dev/null");
    if (ret == 0) {
        return true;
    }

    return false;
}

static pid_t get_driver_process_id (void)
{
    pid_t pid = 0;
    char buf[512];
    FILE *cmd_pipe = popen ("pidof -s bf_switchd",
                            "r");

    fgets (buf, 512, cmd_pipe);
    pid = strtoul (buf, NULL, 10);

    pclose (cmd_pipe);

    printf ("PID of bf_switchd is %d \n", pid);

    return pid;
}

static int do_op_command (int argc, char *argv[])
{
    int t, len, i;
    int fd_socket, ret = EXIT_FAILURE;
    char ip_addr_str[] = "127.0.0.1";
    struct sockaddr_in server;
    char str[MAX_STR_LEN], cmd[MAX_STR_LEN],
         *cmd_ascii;
    uint32_t cmd_len, network_cmd_len;
    ;

    if (argc < 2) {
        return EXIT_FAILURE;
    }
    if ((fd_socket = socket (AF_INET, SOCK_STREAM,
                             0)) == -1) {
        perror ("socket");
        printf ("could not create socket!!!\n");
        return EXIT_FAILURE;
    }

    DB (printf ("Trying to connect switch driver...\n"));
    server.sin_addr.s_addr = inet_addr (ip_addr_str);
    server.sin_family = AF_INET;
    server.sin_port = htons (TCP_PORT_BASE_DEFAULT);
    /*- make connection, if failed then do limited retry */
    for (i = 0; i < CFG_SWDRV_MAX_RETRY; i++) {
        t = connect (fd_socket,
                     (struct sockaddr *)&server, sizeof (server));
        if (!t) {
            break;
        }

        printf (".");
        fflush (stdout);
        sleep (2 + i);
    }

    if (i) {
        printf ("\n");
    }

    if (i >= CFG_SWDRV_MAX_RETRY) {
        perror ("connect");
        printf ("failed to make connection to switch driver!\n");
        return EXIT_FAILURE;
    }

    DB (printf ("Switch driver Connected.\n"));

    /* first four bytes sent indicate the length of the payload following */
    cmd_ascii = str + sizeof (uint32_t);
    sprintf (cmd_ascii, "%s", argv[1]);
    for (i = 2; i < argc; i++) {
        sprintf (cmd, " %s", argv[i]);
        strncat (cmd_ascii, cmd, MAX_STR_LEN);
    }
    /* prepend the length in the first 4 bytes */
    cmd_len = strlen (cmd_ascii) + 1;
    network_cmd_len = htonl (cmd_len);
    memcpy (str, &network_cmd_len, sizeof (uint32_t));

    if (send (fd_socket, str,
              cmd_len + sizeof (uint32_t), 0) < 0) {
        perror ("send");
        printf ("failed to send command(%s) to switch driver!\n",
                str);
        return EXIT_FAILURE;
    }
    DB (printf ("Command sent to Switch driver.\n"));
    if ((t = recv (fd_socket, str, MAX_STR_LEN,
                   0)) > 0) {
        str[t] = '\0';
        ret = str[0];
        sprintf (cmd, "cat %s", str + 1);
        DB (printf ("Client=> %s\n", cmd));
        system (cmd);
    } else {
        if (t < 0) {
            perror ("recv");
            printf ("No response from switch driver!\n");
        } else {
            printf ("Switch driver closed connection!\n");
        }
    }
    DB (printf ("Closing socket.\n"));
    close (fd_socket);

    return ret;
}

static int swdrv_start (char *options)
{
    int ret = 0;
    const char delimiters[] = "=";
    char *key, *install_path;
    char exe_file[BUF_SIZE], conf_file[BUF_SIZE],
         install_path_arg[BUF_SIZE];
    int len = 0;
    pid_t pid = 0;
    char *parmList[5];
    int fd[2], new_fd = 0;

    printf ("Start switch driver...\n");
    if (is_driver_running()) {
        printf ("Driver already running \n");
        return 0;
    }

    key = strtok (options, delimiters);
    (void)key;
    install_path = strtok (NULL, delimiters);
    if (install_path == NULL) {
        printf ("install_path is NULL, exiting \n");
        return -1;
    }
    printf ("install_path is %s \n", install_path);
    snprintf (exe_file, BUF_SIZE, "%s/bin/bf_switchd",
              install_path);
    printf ("exe_file is %s \n", exe_file);
    snprintf (install_path_arg, BUF_SIZE,
              "--install-dir=%s", install_path);
    printf ("install_path_arg is %s \n",
            install_path_arg);
    snprintf (conf_file,
              BUF_SIZE,
              "--conf-file=%s/share/p4/targets/tofino/mav_diag.conf",
              install_path);
    printf ("conf_file is %s \n", conf_file);
    parmList[0] = exe_file;
    parmList[1] = install_path_arg;
    parmList[2] = conf_file;
    /* If --background is passed, bf-shell does not work */
    // parmList[3] = "--background";
    parmList[3] = NULL;

    if (pipe (fd)) {
        perror ("pipe");
        return -1;
    }
    pid = fork();
    if (pid == -1) {
        perror ("fork failed");
    } else if (pid == 0) {
        printf (" In Child process \n");
        close (fd[1]);
        new_fd = dup2 (fd[0], STDIN_FILENO);
        if (new_fd == -1) {
            perror ("dup2 failed");
            exit (0);
        }
        fcntl (new_fd, F_SETFD, FD_CLOEXEC);
        close (fd[0]);
        execv (exe_file, parmList);
        printf ("Drivers started \n");
    } else {
        printf (" In parent process \n");
        close (fd[0]);
        close (fd[1]);
        sleep (CFG_SWDRV_MAX_BOOT_TIME);
        return ret;
    }
}

static int swdrv_shutdown (void)
{
    int ret = 0;
    pid_t pid = 0;
    char cmd[BUF_SIZE];

    if (!is_driver_running()) {
        printf ("Driver not running \n");
        return 0;
    }

    printf ("Shutdown switch driver...\n");
    pid = get_driver_process_id();
    snprintf (cmd, BUF_SIZE, "sudo kill -9 %d", pid);
    ret = system (cmd);

    return ret;
}

static int swdrv_reload (char *options)
{
    int ret;

    printf ("Reload switch driver...\n");
    if (!is_driver_running()) {
        printf ("Driver not running \n");
        return 0;
    }
    ret = swdrv_shutdown();
    sleep (10);
    if (is_driver_running()) {
        printf ("Driver still running, shutdown failed \n");
        return 0;
    }
    ret = swdrv_start (options);

    return ret;
}

int main (int argc, char *argv[])
{
    int ret;

    if (get_user_cmd_option (argc, argv) < 0) {
        show_help (argv[0]);
        return EXIT_FAILURE;
    }

    ret = EXIT_SUCCESS;
    switch (g_option) {
        case 0:
            if (!is_driver_running()) {
                printf ("Driver not running \n");
                return 0;
            }
            ret = do_op_command (argc, argv);
            break;

        case 'r':
            swdrv_reload (g_option_args);
            break;

        case 'S':
            ret = swdrv_start (g_option_args);
            break;

        case 's':
            ret = swdrv_shutdown();
            break;

        default:
            show_help (argv[0]);
    }

    return ret;
}
