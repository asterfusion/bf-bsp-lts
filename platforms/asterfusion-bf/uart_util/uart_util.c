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

static void help(char *progname)
{
    fprintf (stdout, "Usage:  \n");
    fprintf (stdout,
        "%s </dev/ttySX> <BMC CMD>\n"
        "Eg: uart_util /dev/ttyS1 0xd 0xaa 0xaa\n"
        "    uart_util /dev/ttyS1 bmc-ota /root/X308P-T-V1-BMC-V3.1R01.rbl\n",
        progname);
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
    uint8_t wr_buf[2] = {0xAA, 0xAA};
    uint8_t wr_len = 0;
    int i;

    if (argc < 2) {
        help (argv[0]);
        return 0;
    }

    /* Test cmd. */
    if (argc == 2) {
        if (!strcmp (argv[1], "help")) {
            fprintf (stdout, "Good\n");
            help (argv[0]);
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

    if (memcmp (argv[2], "bmc-ota", 7) == 0) {
        bool bmc_support = false;
        int sec = 30;

        ret = bf_pltfm_bmc_uart_util_write_read (ctx, 0x0D, wr_buf,
                                    2, rd_buf, 128 - 1, 500000);
        if ((ret != 4) || (rd_buf[0] != 3) ) {
            fprintf (stdout, "read failed<%d>\n", ret);
        } else {
            if ((rd_buf[1] << 16) + (rd_buf[2] << 8) + rd_buf[3] <= 0x030100) {
                fprintf (stdout, "Not supported on this BMC version yet!\n"
                                 "Required BMC version in 3.1.x and later.\n"
                                 "Please upgrade BMC via Ymodem.\n");
            } else {
                fprintf (stdout, "\nCurrent BMC version: v%d.%d.%d\n\n",
                         rd_buf[1], rd_buf[2], rd_buf[3]);
                bmc_support = true;
            }
        }

        if (bmc_support) {
            char c = 'N';

            fprintf (stdout, "\nAre you sure you want to upgrade BMC firmware? Y/N: ");
            int x = scanf("%c", &c); x = x;
            if ((c != 'Y') && (c != 'y')) {
                fprintf (stdout, "Abort\n");
            } else {
                sleep (1);

                fprintf (stdout, "Upload BMC firmware ... \n");

                if (bf_pltfm_bmc_uart_ymodem_send_file(argv[3]) > 0) {
                    fprintf (stdout, "Done\n");
                    fprintf (stdout, "Performing upgrade, %d sec(s) ...\n", sec);

                    sleep(sec);

                    ret = bf_pltfm_bmc_uart_util_write_read (ctx, 0x0D, wr_buf,
                                                2, rd_buf, 128 - 1, 500000);
                    if ((ret != 4) || (rd_buf[0] != 3) ) {
                        fprintf (stdout, "read failed<%d>\n", ret);
                    } else {
                        fprintf (stdout, "\nNew BMC version: v%d.%d.%d\n\n",
                                 rd_buf[1], rd_buf[2], rd_buf[3]);
                    }
                } else {
                    fprintf (stdout, "Failed.\n");
                }
            }
        }
    } else {
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
                for (i = 0; i < ret; i ++) {
                    if (need_converto_char (cmd)) {
                        fprintf (stdout, "%c", rd_buf[i]);
                    } else {
                        fprintf (stdout, "%x ", rd_buf[i]);
                    }
                }
                fprintf (stdout, "\n");
                //fprintf (stdout, "\nSucceed\n");
            }
        }
    }
    bf_pltfm_uart_util_de_init (ctx);
    return 0;
}

