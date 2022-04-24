/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <sched.h>
#include <linux/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>    //strlen
#include <inttypes.h>  //strlen
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>  //inet_addr
#include <unistd.h>     //write
#include <errno.h>

#include <bf_types/bf_types.h>
#include <bfsys/bf_sal/bf_sys_intf.h>
#include <lld/lld_reg_if.h>
#include <dru_sim/dru_sim.h>

// Default TCP port base. Can be overridden by
// command-line.
//
#define DV_REG_CHNL_PORT_DEFAULT 8008

// Offset from g_tcp_port_base
#define DV_REG_CHNL_PORT 7

static int dv_reg_chnl = -1;
static int g_debug_mode = 0;
static int g_needs_reconnect = 0;
static int socket_desc = 0;

/*********************************************************************
* read_from_dv_socket
*********************************************************************/
static int read_from_dv_socket (int sock,
                                uint8_t *buf, int len)
{
    int n_read = 0, n_read_this_time = 0, i = 1;

    // Receive a message from client
    do {
        n_read_this_time =
            recv (sock, (buf + n_read), (len - n_read),
                  0 /*MSG_WAITALL*/);
        if (n_read_this_time > 0) {
            n_read += n_read_this_time;
            if (n_read < len) {
                printf ("Partial recv: %d of %d so far..\n",
                        n_read, len);
            }
        } else {
            g_needs_reconnect = 1;
            return 0;
        }
        setsockopt (sock, IPPROTO_TCP, TCP_QUICKACK,
                    (void *)&i, sizeof (i));

    } while (n_read < len);
    return n_read;
}

/*********************************************************************
* write_to_dv_socket
*********************************************************************/
static void write_to_dv_socket (int sock,
                                uint8_t *buf, int len)
{
    if (send (sock, buf, len, 0) < 0) {
        printf ("Tcl socket send failed.");
        g_needs_reconnect = 1;
    }
}

/*********************************************************************
* write_reg_chnl
*********************************************************************/
static void write_dv_reg_chnl (uint8_t *buf,
                               int len)
{
    write_to_dv_socket (dv_reg_chnl, buf, len);
}

/*********************************************************************
* read_reg_chnl
*********************************************************************/
static int read_dv_reg_chnl (uint8_t *buf,
                             int len)
{
    return read_from_dv_socket (dv_reg_chnl, buf,
                                len);
}

/*********************************************************************
* create_server
*********************************************************************/
static int create_tcl_server_socket (
    int listen_port)
{
    int client_sock, c;
    struct sockaddr_in server, client;
    int so_reuseaddr = 1;
    int so_keepalive = 1;

    // Create socket
    socket_desc = socket (AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf ("ERROR: Tcl server could not create socket");
    }
    printf ("Tcl server: listen socket created\n");

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons (listen_port);

    setsockopt (socket_desc,
                SOL_SOCKET,
                SO_REUSEADDR,
                &so_reuseaddr,
                sizeof so_reuseaddr);

    setsockopt (socket_desc,
                SOL_SOCKET,
                SO_KEEPALIVE,
                &so_keepalive,
                sizeof so_keepalive);

    // Bind
    if (bind (socket_desc, (struct sockaddr *)&server,
              sizeof (server)) < 0) {
        // print the error message
        perror ("bind failed. Error");
        return -1;
    }
    printf ("Tcl server: bind done on port %d, listening...\n",
            listen_port);
    // Listen
    listen (socket_desc, 1);

    // Accept an incoming connection
    puts ("Tcl server: waiting for incoming connections...");
    c = sizeof (struct sockaddr_in);

    // accept connection from an incoming client
    client_sock =
        accept (socket_desc, (struct sockaddr *)&client,
                (socklen_t *)&c);
    if (client_sock < 0) {
        perror ("Tcl server: accept failed");
        return -1;
    }
    printf ("Tcl server: connection accepted on port %d\n",
            listen_port);

    return client_sock;
}

static void tcl_server_loop (void)
{
    pcie_msg_t msg_storage, *msg = &msg_storage;
    int better_be_positive, len = sizeof (pcie_msg_t);
    uint32_t reg, data;

    while (1) {
        if (g_needs_reconnect) {
            return;
        }

        better_be_positive = read_dv_reg_chnl ((
                unsigned char *)msg, len);
        if (better_be_positive <= 0) {
            g_needs_reconnect = 1;
            return;
        }

        if (msg->typ == pcie_op_rd) {  // read

            if (g_debug_mode) {
                printf ("Tcl Rd: asic=%d: addr=%" PRIx64 "..\n",
                        msg->asic, msg->addr);
            }
            reg = (uint32_t)msg->addr;
            lld_read_register (msg->asic, reg, &data);

            msg->value = data;
            write_dv_reg_chnl ((unsigned char *)msg, len);

            if (g_debug_mode) {
                printf ("Tcl Rd: asic=%d: addr=%" PRIx64
                        " = %08x\n",
                        msg->asic,
                        msg->addr,
                        msg->value);
            }
        } else if (msg->typ == pcie_op_wr) {
            if (g_debug_mode) {
                printf ("Tcl Wr: asic=%d: addr=%" PRIx64
                        " = %08x\n",
                        msg->asic,
                        msg->addr,
                        msg->value);
            }
            reg = (uint32_t)msg->addr;
            lld_write_register (msg->asic, reg, msg->value);
        }
    }
}

static void tcl_server_run (void)
{
    printf ("Tcl server started..\n");

    while (1) {
        g_needs_reconnect = 0;

        // create the listen socket. Returns when connected
        dv_reg_chnl = create_tcl_server_socket (
                          DV_REG_CHNL_PORT_DEFAULT);

        // drop into the servicing loop
        tcl_server_loop();

        printf ("Tcl server disconnecting..\n");

        // if above returns socket was closed
        if (dv_reg_chnl >= 0) {
            close (dv_reg_chnl);
            dv_reg_chnl = -1;
        }
        if (socket_desc) {
            close (socket_desc);
            socket_desc = 0;
        }
        sleep (2); // give sockets a chance to close
        printf ("Tcl server ready to accept connections..\n");
    }
}

/* tcl-server thread exit callback API */
static void tcl_server_exit_cleanup (void *arg)
{
    (void)arg;
    if (dv_reg_chnl >= 0) {
        close (dv_reg_chnl);
        dv_reg_chnl = -1;
    }
    if (socket_desc) {
        close (socket_desc);
        socket_desc = 0;
    }
    bf_sys_usleep (
        10000); /* give sockets a chance to close */
}

/* Init function  */
void *tcl_server_init (void *arg)
{
    (void)arg;
    /* Register cleanup handler */
    pthread_cleanup_push (tcl_server_exit_cleanup,
                          "TCL-server Cleanup handler");
    tcl_server_run();
    pthread_cleanup_pop (0);
    return NULL;
}
