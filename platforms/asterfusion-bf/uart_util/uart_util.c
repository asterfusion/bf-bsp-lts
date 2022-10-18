#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

#include <bf_pltfm_uart.h>

static char *usage[] = {
    "uart_util </dev/ttySX> <BMC CMD> ",
    "Example: uart_util /dev/ttyS1 0xd 0xaa 0xaa"
};

extern bool uart_debug;
int bf_pltfm_uart_util_init(struct bf_pltfm_uart_ctx_t *);
int bf_pltfm_uart_util_de_init (struct bf_pltfm_uart_ctx_t *ctx);
int bf_pltfm_bmc_uart_util_write_read (
    struct bf_pltfm_uart_ctx_t *ctx,
    uint8_t cmd,
    uint8_t *tx_buf,
    uint8_t tx_len,
    uint8_t *rx_buf,
    uint8_t rx_len,
    int usec);

static void help()
{
    fprintf (stdout, "%s\n", usage[0]);
}

static bool bf_pltfm_tty_detected (const char *path) {
    /* In some case USB is not right removed /dev/sdx may be different with /dev/sdb* */
    return (0 == access (path, F_OK));
}

static int need_converto_char(uint8_t cmd) {
    /*
     * 1  -> eeprom
     * 14 -> RTC */
    return (cmd == 1 || cmd == 14);
}

/* uart_util /dev/ttyS1 0xd 0xaa 0xaa.
 * by tsihang, 2022-05-26. */
int main (int argc, char **argv)
{
    uart_debug = 0;

    int ret = 0;
    //const char *tty = "/dev/ttyS1";
    char *dev = (char *)argv[1];
    struct bf_pltfm_uart_ctx_t *ctx = &uart_ctx;
    uint8_t rd_buf[128] = {0};
    uint8_t cmd = 0x00;
    uint8_t wr_buf[2] = {0x01, 0xAA};
    uint8_t wr_len = 0;
    int i;

    /* Test cmd. */
    if (argc == 2) {
        if (!strcmp (argv[1], "help")) {
            fprintf (stdout, "Good\n");
            help ();
            return 0;
        }
        exit (0);
    }
    if (uart_debug) {
        for (i = 0; i < argc; i++) {
            fprintf (stdout, "argv[%d] : %s\n", i, argv[i]);
        }
        fprintf (stdout, "\n");
    }
    memcpy ((void *)&ctx->dev[0], (void *)dev, strlen (dev));

    /* Try to find */
    if (!bf_pltfm_tty_detected ((const char *)&ctx->dev[0])) {
        fprintf (stdout, "No such tty : %s\n", ctx->dev);
        exit (0);
    }

    /* Init ctx. */
    ctx->flags |= AF_PLAT_UART_ENABLE;
    ret = bf_pltfm_uart_util_init (ctx);
    if (ret) {
        fprintf (stdout, "Init tty failed : %s\n", ctx->dev);
        exit (0);
    }

    cmd = (uint8_t)strtol (argv[2], NULL, 16);

    for (i = 0; i < argc - 3; i ++) {
        wr_buf[i] = (uint8_t)strtol (argv[i + 3], NULL, 16);
        wr_len ++;
    }

    ret = bf_pltfm_bmc_uart_util_write_read (ctx, cmd, wr_buf,
                                  wr_len, rd_buf, 128 - 1, 500000);
    if (ret < 0) {
        fprintf (stdout, "read failed<%d>\n", ret);
    } else {
        if (ret == 0) {
            /* Maybe no return cmd executed. */
        } else {
            if (cmd == 5) {
                for (i = 0; i < ret; i ++) {
                    fprintf (stdout, "%x ", rd_buf[i]);
                }
            } else {
                for (i = 0; i < ret; i ++) {
                    if (need_converto_char (cmd)) {
                        fprintf (stdout, "%c", rd_buf[i]);
                    } else {
                        fprintf (stdout, "%x ", rd_buf[i]);
                    }
                }
            }
            fprintf (stdout, "\n");
            //fprintf (stdout, "\nSucceed\n");
        }
    }
    bf_pltfm_uart_util_de_init (ctx);
    return 0;
}

