/*!
 * @file bf_pltfm_uart.c
 * @date 2021/07/05
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include "bf_pltfm_uart.h"

#include "pltfm_types.h"

static bf_sys_rmutex_t uart_lock;
#define UART_LOCK   \
    bf_sys_rmutex_lock(&uart_lock)
#define UART_UNLOCK \
    bf_sys_rmutex_unlock(&uart_lock)

bool uart_debug = true;

struct bf_pltfm_uart_ctx_t uart_ctx = {
    .fd = -1,
    .dev = "/dev/ttyS1",
    .rate = 9600,
    .flags = 0,
};

static const unsigned short ccitt_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static unsigned short
CRC16 (unsigned char *q,
       int len)
{
    unsigned short crc = 0;

    while (len-- > 0) {
        crc = (crc << 8) ^ ccitt_table[ ((crc >> 8) ^
                                                     *q++) & 0xff];
    }

    return crc;
}

static int
rx_crc (unsigned char *rx_buf, int rx_len)
{
    unsigned char rx_crc[2] = {rx_buf[rx_len - 2], rx_buf[rx_len - 1]};
    unsigned short crc = CRC16 (rx_buf,
                                rx_len - 2);
    if (rx_crc[0] == ((crc & 0xff00) >> 8)
        && rx_crc[1] == (crc & 0xff)) {
        return 0;
    } else {
        return -1;
    }
}

static void
uart_close (struct bf_pltfm_uart_ctx_t *ctx)
{
    if (ctx->fd > 0) {
        close (ctx->fd);
    }
    ctx->fd = -1;
}

static int
set_opt (int fd, int nSpeed, int nBits,
         char nEvent, int nStop)
{
    struct termios newtio, oldtio;
    if (tcgetattr (fd, &oldtio) != 0) {
        LOG_ERROR ("SetupSerial 1");
        return -1;
    }

    bzero (&newtio, sizeof (newtio));
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch (nBits) {
        case 7:
            newtio.c_cflag |= CS7;
            break;
        case 8:
            newtio.c_cflag |= CS8;
            break;
    }

    switch (nEvent) {
        case 'O':
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
            break;

        case 'E':
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;

        case 'N':
            newtio.c_cflag &= ~PARENB;
            break;
    }

    switch (nSpeed) {
        case 2400:
            cfsetispeed (&newtio, B2400);
            cfsetospeed (&newtio, B2400);
            break;
        case 4800:
            cfsetispeed (&newtio, B4800);
            cfsetospeed (&newtio, B4800);
            break;
        case 9600:
            cfsetispeed (&newtio, B9600);
            cfsetospeed (&newtio, B9600);
            break;
        case 115200:
            cfsetispeed (&newtio, B115200);
            cfsetospeed (&newtio, B115200);
            break;
        case 460800:
            cfsetispeed (&newtio, B460800);
            cfsetospeed (&newtio, B460800);
            break;
        default:
            cfsetispeed (&newtio, B9600);
            cfsetospeed (&newtio, B9600);
            break;
    }

    if (nStop == 1) {
        newtio.c_cflag &= ~CSTOPB;
    } else if (nStop == 2) {
        newtio.c_cflag |= CSTOPB;
    }

    //newtio.c_cflag &= ~(ICANON |ISIG| ICRNL|IGNCR);
    newtio.c_lflag &= ~ (ICANON | ISIG);
    newtio.c_iflag &= ~ (ICRNL | IGNCR);
    newtio.c_cc[VTIME] = 128;
    newtio.c_cc[VMIN] = 100;

    tcflush (fd,
             TCIOFLUSH);

    if ((tcsetattr (fd, TCSANOW, &newtio)) != 0) {
        LOG_ERROR ("com set error!");
        return -1;
    }
    return 0;
}

static void
hex_dump (uint8_t *buf, uint32_t len)
{
    uint8_t byte;

    for (byte = 0; byte < len; byte++) {
        if ((byte % 16) == 0) {
            fprintf (stdout, "\n%3d : ", byte);
        }
        fprintf (stdout, "%02x ", buf[byte]);
    }
    fprintf (stdout,  "\n");
}

static void
tx_crc (unsigned char *tx_buf,
        OUT size_t *tx_len)
{
    char crc[10] = "\0";
    unsigned short tx_crc = CRC16 (tx_buf, *tx_len);

    *tx_len += sprintf (crc, "_%x%x#",
                        (tx_crc & 0xff00) >> 8, tx_crc & 0xff);
    strcat ((char *)&tx_buf[0], crc);
}

static int
xmit (struct bf_pltfm_uart_ctx_t *ctx,
    unsigned char *tx_buf, size_t tx_len)
{
    ssize_t rc = 0;

    rc = write (ctx->fd, (void *)tx_buf, tx_len);
    if (rc == (ssize_t)tx_len) {
        /* Success. */
    } else if (rc == -1) {
        LOG_ERROR (
            "%s[%d], "
            "write(%s)"
            "\n",
            __FILE__, __LINE__, oryx_safe_strerror (errno));
    } else if (rc < (ssize_t)tx_len) {
        /* This shall never happen if tx_len{PIPE_BUF}. If it does
         * happen (with nbyte>{PIPE_BUF}), this volume of POSIX.1‐2008
         * does not guarantee atomicity, even if ret≤{PIPE_BUF},
         * because atomicity is guaranteed according to the amount requested,
         * not the amount written.
         */
    }

    return (rc < 0) ? rc : 0;
}

static int
recv (struct bf_pltfm_uart_ctx_t *ctx,
    unsigned char *rx_buf, size_t rx_len)
{
    ssize_t rc = 0;

    rc = read (ctx->fd, (void *)rx_buf, rx_len);
    tcflush (ctx->fd, TCIOFLUSH);
    if (rc >= 0) {
        /* Success. */
    } else if (rc == -1) {
        LOG_ERROR (
            "%s[%d], "
            "read(%s) : %d : %d"
            "\n",
            __FILE__, __LINE__, oryx_safe_strerror (errno),
            ctx->fd, rx_len);
    }

    return (int)rc;
}

static int
uart_open (struct bf_pltfm_uart_ctx_t *ctx)
{
    char *dev = (char *)&ctx->dev[0];

    if (ctx->fd > 0) {
        fprintf(stdout, "=========== Opened : %d\n", ctx->fd);
        uart_close (ctx);
        //return 0;
    }

    if ((ctx->fd = open (dev,
                         O_RDWR | O_NOCTTY | O_NONBLOCK)) <
        0) {
        LOG_ERROR (
            "%s[%d], "
            "open(%s)"
            "\n",
            __FILE__, __LINE__, strerror (errno));
        return -1;
    }

    return set_opt (ctx->fd, ctx->rate, 8, 'N', 1);
}

static int
uart_send (struct bf_pltfm_uart_ctx_t *ctx,
    unsigned char cmd,
    unsigned char *tx_buf,
    unsigned char tx_len)
{
    unsigned char buf[50] = {0};
    size_t l = 0;
    int rc = 0;

    if (tx_len > 2) {
        LOG_ERROR (
            "%s[%d], "
            "uart.send(%s)"
            "\n",
            __FILE__, __LINE__, "Unknown CMD.");
        if (uart_debug) {
            fprintf (stdout, "\n---> %02x:\n", cmd);
            hex_dump (buf, l);
            fprintf (stdout, "\n---> done\n");
        }
        return -1;
    }

    if (tx_len == 0) {
        l = sprintf ((char *)&buf[0],
                     "uart_0x%02x", cmd);
    } else if (tx_len == 1) {
        l = sprintf ((char *)&buf[0],
                     "uart_0x%02x_0x%02x", cmd,
                     tx_buf[0]);
    } else if (tx_len == 2) {
        l = sprintf ((char *)&buf[0],
                     "uart_0x%02x_0x%02x_0x%02x",
                     cmd, tx_buf[0], tx_buf[1]);
    }

    tx_crc (buf, &l);

    rc = xmit (ctx, buf, l);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "uart.send(%s)"
            "\n",
            __FILE__, __LINE__, "Failed to write uart.");
        fprintf (stdout, "\n<--- %02x:\n", cmd);
        hex_dump (buf, l);
        fprintf (stdout, "\n<--- done\n");
    }

    return rc;
}

static int
uart_recv (struct bf_pltfm_uart_ctx_t *ctx,
    unsigned char *rx_buf,
    unsigned char rx_len)
{
    int rc = 0;

    rc = recv (ctx, rx_buf, rx_len);
    if (rc >= 0) {
        int cc = rx_crc (rx_buf, rc);
        if (cc) {
            rc = -1;
            goto end;
        }
        rx_buf[rc - 2] = 0;
        rc -= 2;
    }
end:
    return rc;
}

static bool
no_return_cmd (unsigned char cmd)
{
    /*
     * Skip setting.
     * 7  = FAN RPM setting.
     * 9  = Reboot BMC.
     * 10 = Payload shutdown or reboot.
     * 12 = Config WDT.
     * 15 = cp2112/superio selecttion.
     * 16 = cp2112 hard reset.
     */
    if ((cmd == 7) || (cmd == 9) || (cmd == 10) ||
        (cmd == 12) || (cmd == 15) || (cmd == 16)) {
        return true;
    }

    return false;
}


/*
 * READ:
 * Upon successful completion, the function shall return a non-negative
 * integer indicating the number of bytes actually read from UART. Otherwise,
 * the functions shall return ?1 and set errno to indicate the error.
 *
 * WRITE:
 * Upon successful completion, the function shall always return zero.
 *
 */
int bf_pltfm_bmc_uart_write_read (
    uint8_t cmd,
    uint8_t *tx_buf,
    uint8_t tx_len,
    uint8_t *rx_buf,
    uint8_t rx_len,
    int usec)
{
    struct bf_pltfm_uart_ctx_t *ctx = &uart_ctx;

    int rc = -1;

    UART_LOCK;

    if (uart_open(ctx)) {
        rc = -2;
        goto end;
    }

    if (uart_send (ctx, cmd, tx_buf, tx_len)) {
        goto end;
    }

    if (no_return_cmd (cmd)) {
        rc = 0;
    } else {
        /* Wait for data ready. */
        usleep (usec);
        rc = uart_recv (ctx, rx_buf, rx_len);
    }

end:
    usleep (5000);
    uart_close(ctx);
    UART_UNLOCK;

    return rc;
}

int bf_pltfm_bmc_uart_util_write_read (
    struct bf_pltfm_uart_ctx_t *ctx,
    uint8_t cmd,
    uint8_t *tx_buf,
    uint8_t tx_len,
    uint8_t *rx_buf,
    uint8_t rx_len,
    int usec)
{
    int rc = -1;

    UART_LOCK;

    if (uart_debug) {
        fprintf (stdout, "\n---> %02x:\n", cmd);
        hex_dump (tx_buf, tx_len);
        fprintf (stdout, "\n---> done\n");
    }

    if (uart_open(ctx)) {
        rc = -2;
        goto end;
    }

    if (uart_send (ctx, cmd, tx_buf, tx_len)) {
        goto end;
    }

    if (no_return_cmd (cmd)) {
        rc = 0;
    } else {
        /* Wait for data ready. */
        usleep (usec);
        rc = uart_recv (ctx, rx_buf, rx_len);
        if (uart_debug) {
            fprintf (stdout, "\n<--- %02x:\n", cmd);
            hex_dump (rx_buf, rx_len);
            fprintf (stdout, "\n<--- done\n");
        }
    }

end:
    usleep (5000);
    uart_close(ctx);
    UART_UNLOCK;

    return rc;
}

int bf_pltfm_uart_util_init (struct bf_pltfm_uart_ctx_t *ctx)
{
    if (unlikely (! (ctx->flags &
                     AF_PLAT_UART_ENABLE))) {
        fprintf (stdout, "Skip ...\n");
        return 0;
    }

    /* Check UART */
    if (likely (access ((char *)&ctx->dev[0],
                        F_OK))) {
        fprintf (stdout,
                 "%s[%d], "
                 "access(%s)"
                 "\n",
                 __FILE__, __LINE__, oryx_safe_strerror (errno));
        return -1;
    }

    bf_pltfm_mgr_ctx()->flags |= AF_PLAT_CTRL_BMC_UART;
    LOG_WARNING ("Seems that you want to access BMC thru Uart.");

    if (bf_sys_rmutex_init (&uart_lock) != 0) {
        LOG_ERROR ("pltfm_mgr: uart lock init failed\n");
        return -1;
    }

    return 0;
}

int bf_pltfm_uart_util_de_init (struct bf_pltfm_uart_ctx_t *ctx)
{
    if (unlikely (! (ctx->flags &
                     AF_PLAT_UART_ENABLE))) {
        fprintf (stdout, "Skip ...\n");
        return 0;
    }
    uart_close(ctx);
    bf_sys_rmutex_del (&uart_lock);
    bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_CTRL_BMC_UART;
    return 0;
}

int bf_pltfm_uart_init ()
{
    /* Should we keep openning /dev/ttySx ? */
    struct bf_pltfm_uart_ctx_t *ctx = &uart_ctx;

    fprintf (stdout,
             "\n\n================== %s ==================\n", "UART INIT");
    if (unlikely (! (ctx->flags &
                     AF_PLAT_UART_ENABLE))) {
        fprintf (stdout, "Skip ...\n");
        return 0;
    } else {
        fprintf (stdout, "Using UART : %s\n",
                 (char *)&ctx->dev[0]);
    }

    /* Check UART */
    if (likely (access ((char *)&ctx->dev[0],
                        F_OK))) {
        fprintf (stdout,
                 "%s[%d], "
                 "access(%s)"
                 "\n",
                 __FILE__, __LINE__, oryx_safe_strerror (errno));
        return -1;
    }

    fprintf (stdout, "Uart %s init done !\n",
             ctx->dev);
    LOG_DEBUG ("Uart %s init done !",
               ctx->dev);

    bf_pltfm_mgr_ctx()->flags |= AF_PLAT_CTRL_BMC_UART;
    LOG_WARNING ("Seems that you want to access BMC thru Uart.");

    if (bf_sys_rmutex_init (&uart_lock) != 0) {
        LOG_ERROR ("pltfm_mgr: uart lock init failed\n");
        return -1;
    }

    return 0;
}

int bf_pltfm_uart_de_init ()
{
    struct bf_pltfm_uart_ctx_t *ctx = &uart_ctx;

    fprintf(stdout, "================== Deinit .... %48s ================== \n",
        __func__);

    if (unlikely (! (ctx->flags &
                     AF_PLAT_UART_ENABLE))) {
        fprintf (stdout, "Skip ...\n");
        return 0;
    }

    uart_close(ctx);
    bf_sys_rmutex_del (&uart_lock);

    fprintf(stdout, "================== Deinit done %48s ================== \n",
        __func__);

    return 0;
}

