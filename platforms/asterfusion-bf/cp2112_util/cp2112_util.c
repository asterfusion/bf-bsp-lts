#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>  //strlen
#include <sys/socket.h>
#include <arpa/inet.h>  //inet_addr
#include <unistd.h>     //write
#include <errno.h>

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_bd_map.h>
#include <bf_pltfm_cp2112_intf.h>
#include <linux/tcp.h>


int reg_chnl = 0;
bool dbg_print = false;

/*********************************************************************
* create_server
*********************************************************************/
int create_server (int listen_port)
{
    int socket_desc, client_sock, c;
    struct sockaddr_in server, client;
    int so_reuseaddr = 1;

    // Create socket
    socket_desc = socket (AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf ("ERROR: cp2112_server could not create socket");
        return -1;
    }
    printf ("cp2112_server: listen socket created\n");

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons (listen_port);

    setsockopt (socket_desc,
                SOL_SOCKET,
                SO_REUSEADDR,
                &so_reuseaddr,
                sizeof so_reuseaddr);

    // Bind
    if (bind (socket_desc, (struct sockaddr *)&server,
              sizeof (server)) < 0) {
        perror ("bind failed. Error");
        close (socket_desc);
        return -1;
    }
    printf ("cp2112_server: bind done on port %d, listening...\n",
            listen_port);
    // Listen
    listen (socket_desc, 1);

    // Accept an incoming connection
    puts ("cp2112_server: waiting for incoming connections...");
    c = sizeof (struct sockaddr_in);

    // accept connection from an incoming client
    client_sock =
        accept (socket_desc, (struct sockaddr *)&client,
                (socklen_t *)&c);
    if (client_sock < 0) {
        perror ("accept failed");
        close (socket_desc);
        return -1;
    }
    printf ("cp2112_server: connection accepted on port %d\n",
            listen_port);

    return client_sock;
}

/*********************************************************************
* write_to_socket
*********************************************************************/
void write_to_socket (int sock, uint8_t *buf,
                      int len)
{
    if (send (sock, buf, len, 0) < 0) {
        puts ("Send failed");
        exit (3);
    }
}

/*********************************************************************
* read_from_socket
*********************************************************************/
int read_from_socket (int sock, uint8_t *buf,
                      int len)
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
        }
        setsockopt (sock, IPPROTO_TCP, TCP_QUICKACK,
                    (void *)&i, sizeof (i));

    } while (n_read < len);
    return n_read;
}

#define DEFAULT_TIMEOUT_MS 500

/*static char *commands[] = {
  "read",
  "write",
  "addr-read",
  "addr-read-unsafe", //Use with caution; might cause cp2112 to hang
  "detect"};*/

static char *usage[] = {
    "cp2112_util <Montara->0, Mavericks->1> read <dev(Lower->0, Upper->1)> "
    "<i2c_addr> <length>",
    "cp2112_util <Montara->0, Mavericks->1> write <dev(Lower->0, Upper->1)> "
    "<i2c_addr> <length> <byte1> "
    "[<byte2> ...]",
    "cp2112_util <Montara->0, Mavericks->1> addr-read <dev(Lower->0, "
    "Upper->1)> <i2c_addr> <read_length> "
    "<write_length> <byte1> [<byte2> ...]",
    "cp2112_util <Montara->0, Mavericks->1> addr-read-unsafe <dev(Lower->0, "
    "Upper->1)> <i2c_addr> "
    "<read_length> <write_length> <byte1> [<byte2> ...]",
    "cp2112_util <Montara->0, Mavericks->1> detect <dev(Lower->0, Upper->1)>"
};

static int proc_read (int count, char *cmd[])
{
    if (count < 3) {
        printf ("Error: Insufficient arguments\nUsage : %s\n",
                usage[0]);
        return -1;
    }

    uint8_t i2c_addr;
    uint32_t byte_buf_size;
    uint8_t *byte_buf;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;
    uint32_t i;

    cp2112_id_ = (uint8_t)strtol (cmd[0], NULL, 16);
    i2c_addr = (uint8_t)strtol (cmd[1], NULL, 16);
    byte_buf_size = (uint8_t)strtol (cmd[2], NULL,
                                     0);

    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        printf ("Error: read failed \nUsage: %s\n",
                usage[0]);
        return -1;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        printf ("Error: unable to get the handle for dev %d\n",
                cp2112_id_);
        return -1;
    }

    byte_buf = (uint8_t *)malloc (byte_buf_size *
                                  sizeof (uint8_t));

    bf_pltfm_status_t sts = bf_pltfm_cp2112_read (
                                cp2112_dev, i2c_addr, byte_buf, byte_buf_size,
                                DEFAULT_TIMEOUT_MS);
    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: read failed for dev %d addr %02x \n",
                cp2112_id_,
                (unsigned)i2c_addr);
        free (byte_buf);
        return -1;
    } else {
        for (i = 0; i < byte_buf_size; i++) {
            printf ("%02x ", (unsigned)byte_buf[i]);
            if ((i + 1) % 16 == 0) {
                printf ("\n");
            }
        }
        printf ("\n");
    }

    free (byte_buf);
    return 0;
}

static int proc_write (int count, char *cmd[])
{
    if (count < 3) {
        printf ("Error: Insufficient arguments\nUsage: %s\n",
                usage[1]);
        return -1;
    }

    uint8_t i2c_addr;
    uint32_t byte_buf_size;
    uint8_t *byte_buf;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;
    uint32_t i;

    cp2112_id_ = (uint8_t)strtol (cmd[0], NULL, 16);
    i2c_addr = (uint8_t)strtol (cmd[1], NULL, 16);
    byte_buf_size = (uint8_t)strtol (cmd[2], NULL,
                                     16);

    if (count < (int) (3 + byte_buf_size)) {
        printf ("Error: Insufficient Arguments \nUsage: %s\n",
                usage[1]);
        return 0;
    }

    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        printf ("Error: write failed \nUsage: %s\n",
                usage[1]);
        return -1;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        printf ("Error: unable to get the handle for dev %d\n",
                cp2112_id_);
        return -1;
    }

    byte_buf = (uint8_t *)malloc (byte_buf_size *
                                  sizeof (uint8_t));

    for (i = 0; i < byte_buf_size; i++) {
        byte_buf[i] = strtol (cmd[3 + i], NULL, 16);
    }

    bf_pltfm_status_t sts = bf_pltfm_cp2112_write (
                                cp2112_dev, i2c_addr, byte_buf, byte_buf_size,
                                DEFAULT_TIMEOUT_MS);
    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: write failed for dev %d addr %02x \n",
                cp2112_id_,
                (unsigned)i2c_addr);
        free (byte_buf);
        return -1;
    }

    free (byte_buf);
    return 0;
}

int proc_addr_read (int count, char *cmd[],
                    bool is_unsafe, uint8_t *rb)
{
    if (count < 4) {
        if (is_unsafe == false) {
            printf ("Error: Insufficient arguments\nUsage: %s\n",
                    usage[2]);
        } else {
            printf ("Error: Insufficient arguments\nUsage: %s\n",
                    usage[3]);
        }
        return -1;
    }

    uint8_t i2c_addr;
    uint32_t read_byte_buf_size, write_byte_buf_size;
    uint8_t *read_byte_buf, *write_byte_buf;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    cp2112_id_ = (uint8_t)strtol (cmd[0], NULL, 16);
    i2c_addr = (uint8_t)strtol (cmd[1], NULL, 16);
    read_byte_buf_size = (uint8_t)strtol (cmd[2],
                                          NULL, 16);
    write_byte_buf_size = (uint8_t)strtol (cmd[3],
                                           NULL, 16);

    if (read_byte_buf_size > 256 ||
        write_byte_buf_size > 60) {
        printf ("Error: unsupported read or write data size\n");
        return -1;
    }

    if (count < (int) (4 + write_byte_buf_size)) {
        printf ("Error: Insufficient arguments\nUsage: %s\n",
                usage[2]);
        return -1;
    }

    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        printf ("Error: addr read failed\nUsage: %s\n",
                usage[2]);
        return -1;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        printf ("Error: unable to get the handle for dev %d\n",
                cp2112_id_);
        return -1;
    }

    if (rb == NULL) {
        read_byte_buf = (uint8_t *)malloc (
                            read_byte_buf_size * sizeof (uint8_t));
    } else {
        read_byte_buf = rb;
    }
    write_byte_buf = (uint8_t *)malloc (
                         write_byte_buf_size * sizeof (uint8_t));

    unsigned int i;
    for (i = 0; i < write_byte_buf_size; i++) {
        write_byte_buf[i] = strtol (cmd[4 + i], NULL, 16);
    }

    bf_pltfm_status_t sts;
    if (is_unsafe == true) {
        sts = bf_pltfm_cp2112_write_read_unsafe (
                  cp2112_dev,
                  i2c_addr,
                  write_byte_buf,
                  read_byte_buf,
                  write_byte_buf_size,
                  read_byte_buf_size,
                  DEFAULT_TIMEOUT_MS);
    } else {
        sts = bf_pltfm_cp2112_write (cp2112_dev,
                                     i2c_addr,
                                     write_byte_buf,
                                     write_byte_buf_size,
                                     DEFAULT_TIMEOUT_MS);
        sts |= bf_pltfm_cp2112_read (cp2112_dev,
                                     i2c_addr,
                                     read_byte_buf,
                                     read_byte_buf_size,
                                     DEFAULT_TIMEOUT_MS);
    }

    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: addr read failed for dev %d addr %02x \n",
                cp2112_id_,
                (unsigned)i2c_addr);
        if (rb == NULL) {
            free (read_byte_buf);
        }
        free (write_byte_buf);
        return -1;
    } else {
        if (rb == NULL) {
            for (i = 0; i < read_byte_buf_size; i++) {
                printf ("%02x ", (unsigned)read_byte_buf[i]);
            }
            printf ("\n");
        }
    }

    if (rb == NULL) {
        free (read_byte_buf);
    }
    free (write_byte_buf);
    return 0;
}

static int proc_detect (int count, char *cmd[])
{
    if (count < 1) {
        printf ("Error: Insufficient arguments\nUsage : %s\n",
                usage[4]);
        return -1;
    }

    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    cp2112_id_ = (uint8_t)strtol (cmd[0], NULL, 16);

    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        printf ("Error: detect failed \nUsage: %s\n",
                usage[4]);
        return -1;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        printf ("Error: unable to get the handle for dev %d\n",
                cp2112_id_);
        return -1;
    }

    uint8_t i, j, addr;

    printf ("      00 02 04 06 08 0a 0c 0e\n");
    for (i = 0; i < 128; i += 8) {
        printf ("0x%02x: ", i << 1);
        for (j = 0; j < 8; j++) {
            addr = (i + j) << 1;
            if (bf_pltfm_cp2112_addr_scan (cp2112_dev,
                                           addr)) {
                printf ("%02x ", addr);
            } else {
                printf ("-- ");
            }
        }
        printf ("\n");
    }

    return 0;
}

// static def for server rd/wr
uint8_t static_byte_buf[64] = {0};

static uint32_t proc_server_reg_read (
    uint32_t addr)
{
    uint16_t ret_val;
    uint8_t i2c_addr;
    uint32_t byte_buf_size;
    uint8_t *byte_buf = &static_byte_buf[0];
    ;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;
    uint32_t i;

    cp2112_id_ = 1;
    i2c_addr = 0x38;  // 0x70;//0x38; //0xE0;
    byte_buf_size = 2;
    cp2112_id = CP2112_ID_1;

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        printf ("Error: unable to get the handle for dev %d\n",
                cp2112_id_);
        return 0x0bad;
    }

    byte_buf[0] = (addr >> 8) & 0xff;
    byte_buf[1] = (addr >> 0) & 0xff;

    if (dbg_print) {
        printf ("cp2112_server 0 write %d %d %02x %02x %02x\n",
                cp2112_id ? 0 : 1,
                byte_buf_size,
                i2c_addr,
                byte_buf[0],
                byte_buf[1]);
    }

    bf_pltfm_status_t sts = bf_pltfm_cp2112_write (
                                cp2112_dev, i2c_addr, byte_buf, byte_buf_size,
                                DEFAULT_TIMEOUT_MS);

    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: write failed <sts=%d> for dev %d addr %02x \n",
                sts,
                cp2112_id_,
                (unsigned)i2c_addr);
        return -1;
    }
    byte_buf[0] = 0xee;
    byte_buf[1] = 0xee;

    if (dbg_print) {
        printf ("cp2112_server 0 read %d %02x %d\n",
                cp2112_id ? 0 : 1,
                i2c_addr,
                byte_buf_size);
    }

    sts = bf_pltfm_cp2112_read (
              cp2112_dev, i2c_addr, byte_buf, byte_buf_size,
              DEFAULT_TIMEOUT_MS);

    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: read failed <sts=%d> for dev %d addr %02x \n",
                sts,
                cp2112_id_,
                (unsigned)i2c_addr);
        return 0x0bad;
    } else {
        if (dbg_print) {
            for (i = 0; i < byte_buf_size; i++) {
                printf ("%02x ", (unsigned)byte_buf[i]);
            }
            printf ("\n");
        }
    }
    ret_val = * ((uint16_t *)&byte_buf[0]);
    return ret_val;
}

static int proc_server_reg_write (uint32_t addr,
                                  uint32_t val)
{
    uint8_t i2c_addr;
    uint32_t byte_buf_size;
    uint8_t *byte_buf = &static_byte_buf[0];
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    cp2112_id_ = 0;
    i2c_addr = 0x38;  // 0x70; //0x38; //0xE0;
    byte_buf_size = 4;
    cp2112_id = CP2112_ID_1;

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        printf ("Error: unable to get the handle for dev %d\n",
                cp2112_id_);
        return -1;
    }

    byte_buf[0] = (addr >> 8) & 0xff;
    byte_buf[1] = (addr >> 0) & 0xff;
    byte_buf[2] = (val >> 8) & 0xff;
    byte_buf[3] = (val >> 0) & 0xff;

    if (dbg_print) {
        printf ("cp2112_server 0 write %d %d %02x %02x %02x %02x %02x\n",
                cp2112_id ? 0 : 1,
                byte_buf_size,
                i2c_addr,
                byte_buf[0],
                byte_buf[1],
                byte_buf[2],
                byte_buf[3]);
    }

    bf_pltfm_status_t sts = bf_pltfm_cp2112_write (
                                cp2112_dev, i2c_addr, byte_buf, byte_buf_size,
                                DEFAULT_TIMEOUT_MS);
    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: write failed for dev %d addr %02x \n",
                cp2112_id_,
                (unsigned)i2c_addr);
        return -1;
    }
    return 0;
}

/*****************************************************************
* proc_server
*
* This option provides a faster way to process I2C requests in
* support of the Credo eval boards.
* Credo supplies python scripts to configure their eval boards.
* We run these scripts on a switch CPU and connect the CP2112
* of that switch to the I2C pins of the eval board (using the
* CPU port QSFP channel).
*
* Because the driver is in python, the protocol is string based,
* using colons as separators.
*
* The command protocol is as follows:
*   Read: "r:<address>"
*  Write: "w:<address>:<data>"
*
* E.g.
*   "w:0x9818:0x0666"
*
* means "0x9818 = 0x0666"
*****************************************************************/
static int proc_server (int count, char *cmd[])
{
    reg_chnl = create_server (9001);
    if (-1 == reg_chnl) {
        /* Indicates bind/accept error in create server */
        printf ("Socket already in use. Exiting the application\n");
        exit (1);
    }
    while (true) {
        char cmd[132] = {0};
        int cmd_len, n_read;

        memset (cmd, 0, sizeof (cmd));
        n_read = read_from_socket (
                     reg_chnl, (uint8_t *)cmd,
                     4);  // read command length r/w
        if (dbg_print) {
            printf ("%d bytes read from socket: %02x %02x %02x %02x \n",
                    n_read,
                    cmd[0],
                    cmd[1],
                    cmd[2],
                    cmd[3]);
        }
        cmd[4] = 0;
        n_read = strtoul (cmd, NULL, 16);
        if (n_read > 0) {
            // cmd_len = atoi(cmd);
            cmd_len = n_read;
            if (dbg_print) {
                printf ("cmd_len=%d\n", cmd_len);
            }
            n_read = read_from_socket (
                         reg_chnl, (uint8_t *)cmd,
                         cmd_len);  // read command r/w
            if (n_read == cmd_len) {
                if (cmd[0] == 'r') {
                    if (dbg_print) {
                        printf ("%s\n", cmd);
                    }
                    uint8_t conv_buf[7];
                    for (int i = 0; i < 6; i++) {
                        conv_buf[i] = cmd[2 + i];
                    }
                    conv_buf[6] = 0;
                    uint16_t addr = (uint16_t)strtoul (
                                        (const char *)conv_buf, NULL,
                                        16);  //*((uint16_t*)&cmd[4]);
                    if (dbg_print) {
                        printf ("Reg addr=%04x : %02x %02x, %02x, %02x, %02x, %02x, %02x\n",
                                addr,
                                conv_buf[0],
                                conv_buf[1],
                                conv_buf[2],
                                conv_buf[3],
                                conv_buf[4],
                                conv_buf[5],
                                conv_buf[6]);
                    }
                    uint16_t val = proc_server_reg_read (addr);
                    char rtn_data[7];
                    snprintf (rtn_data, 7, "0x%04x", val);
                    write_to_socket (reg_chnl, (uint8_t *)rtn_data,
                                     7);
                } else if (cmd[0] == 'w') {
                    if (dbg_print) {
                        printf ("%s\n", cmd);
                    }
                    uint8_t conv_buf[7];
                    for (int i = 0; i < 6; i++) {
                        conv_buf[i] = cmd[2 + i];
                    }
                    conv_buf[6] = 0;
                    uint16_t addr = (uint16_t)strtoul (
                                        (const char *)conv_buf, NULL,
                                        16);  //*((uint16_t*)&cmd[4]);
                    if (dbg_print) {
                        printf ("Reg addr=%04x : %02x %02x, %02x, %02x, %02x, %02x, %02x\n",
                                addr,
                                conv_buf[0],
                                conv_buf[1],
                                conv_buf[2],
                                conv_buf[3],
                                conv_buf[4],
                                conv_buf[5],
                                conv_buf[6]);
                    }
                    for (int i = 0; i < 6; i++) {
                        conv_buf[i] = cmd[9 + i];
                    }
                    conv_buf[6] = 0;
                    uint16_t val = (uint16_t)strtoul (
                                       (const char *)conv_buf, NULL,
                                       16);  //*((uint16_t*)&cmd[4]);
                    if (dbg_print) {
                        printf ("Reg addr=%04x : %02x %02x, %02x, %02x, %02x, %02x, %02x\n",
                                val,
                                conv_buf[0],
                                conv_buf[1],
                                conv_buf[2],
                                conv_buf[3],
                                conv_buf[4],
                                conv_buf[5],
                                conv_buf[6]);
                    }
                    proc_server_reg_write (addr, val);
                    char rtn_data[7];
                    snprintf (rtn_data, 7, "0x%04x", val);
                    write_to_socket (reg_chnl, (uint8_t *)rtn_data,
                                     7);
                } else {
                    printf ("%s\n", cmd);
                    exit (1);
                }
            }
        }
    }

    (void)count;
    (void)cmd;
    return reg_chnl;
}

int process_request (int count, char *cmd[])
{
    if (strcmp (cmd[1], "read") == 0) {
        return proc_read (count, &cmd[2]);
    } else if (strcmp (cmd[1], "write") == 0) {
        return proc_write (count, &cmd[2]);
    } else if (strcmp (cmd[1], "addr-read") == 0) {
        return proc_addr_read (count, &cmd[2], false,
                               NULL);
    } else if (strcmp (cmd[1],
                       "addr-read-unsafe") == 0) {
        return proc_addr_read (count, &cmd[2], true,
                               NULL);
    } else if (strcmp (cmd[1], "detect") == 0) {
        return proc_detect (count, &cmd[2]);
    } else if (strcmp (cmd[1], "server") == 0) {
        return proc_server (count, &cmd[2]);
    } else {
        printf ("Error: Invalid command\n");
        return -1;
    }
}

#ifndef NO_MAIN
int main (int argc, char *argv[])
{
    if (argc < 4) {
        uint32_t i = 0;
        printf ("Usage: \n");
        for (i = 0;
             i < (sizeof (usage) / sizeof (usage[0])); i++) {
            printf ("%s\n", usage[i]);
        }
        return 1;
    }

    bf_pltfm_status_t sts;
    uint8_t board_id = (uint8_t)strtol (argv[1], NULL,
                                        16);
    bf_pltfm_board_id_t bd_id =
        BF_PLTFM_BD_ID_MAVERICKS_P0B;
    if (board_id == 0) {
        bd_id = BF_PLTFM_BD_ID_MONTARA_P0B;
    } else if (board_id == 1) {
        bd_id = BF_PLTFM_BD_ID_MAVERICKS_P0B;
    }

    sts = bf_pltfm_cp2112_util_init (bd_id);
    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: Not able to initialize the cp2112 device(s)\n");
        return 1;
    }

    int ret = process_request (argc - 2, &argv[1]);

    sts = bf_pltfm_cp2112_de_init();
    if (sts != BF_PLTFM_SUCCESS) {
        printf ("Error: Not able to de-initialize the cp2112 device(s)\n");
        return 1;
    }

    return ret;
}
#endif
/* stub functions to make cp2112_util build */
int platform_num_ports_get (void)
{
    return 0;
}

pltfm_bd_map_t *platform_pltfm_bd_map_get (
    int *rows)
{
    *rows = 0;
    return NULL;
}
