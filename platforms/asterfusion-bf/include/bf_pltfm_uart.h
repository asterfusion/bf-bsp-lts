/*!
 * @file bf_pltfm_uart.h
 * @date 2021/07/05
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#ifndef _BF_PLTFM_UART_H
#define _BF_PLTFM_UART_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

struct bf_pltfm_uart_ctx_t {
    int fd;
#define MAX_DEV_NAME    32
    char dev[MAX_DEV_NAME];
    int rate;

#define AF_PLAT_UART_ENABLE (1 << 0)
    uint32_t flags;
};

extern struct bf_pltfm_uart_ctx_t uart_ctx;

int bf_pltfm_uart_init();
int bf_pltfm_uart_de_init ();

int bf_pltfm_bmc_uart_write_read (
    uint8_t cmd,
    uint8_t *tx_buf,
    uint8_t tx_len,
    uint8_t *rx_buf,
    uint8_t rx_len,
    int usec);

int bf_pltfm_bmc_uart_ymodem_send_file (char *filename);

#ifdef __cplusplus
}
#endif /* C++ */
#endif /* _BF_PLTFM_UART_H */
