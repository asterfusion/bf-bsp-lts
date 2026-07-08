#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <lld/bf_ts_if.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_syscpld.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>
#include <bf_pltfm_qsfp.h>
#include <bf_pltfm_sfp.h>
#include <pltfm_types.h>
#include "8a34004.h"

#define DEFAULT_TIMEOUT_MS 500

/* For YH and S02 CME. */
static bf_pltfm_cp2112_device_ctx_t *g_cpld_cp2112_hndl;
static char g_cpld_index_version[MAX_SYSCPLDS][8] = {{0}};

extern int bf_pltfm_get_qsfp_ctx (struct
                                  qsfp_ctx_t
                                  **qsfp_ctx);

extern int bf_pltfm_get_sfp_ctx (struct
                                 sfp_ctx_t
                                 **sfp_ctx);


static void bf_pltfm_cpld_show (ucli_context_t *uc,
    uint8_t cpld_index,
    uint8_t *buf, uint8_t size)
{
    uint8_t byte;

    aim_printf (&uc->pvs, "\ncpld %d:\n",
                cpld_index);

    for (byte = 0; byte < size; byte++) {
      if ((byte % 16) == 0) {
          aim_printf (&uc->pvs, "\n%3d : ", byte);
      }
      aim_printf (&uc->pvs, "%02x ", buf[byte]);
    }
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
}

static void bf_pltfm_cpld_decode_x308p (ucli_context_t *uc) {
    int i, j;
    uint8_t buf[1][128];
    uint8_t cpld_num = 1;
    uint8_t cpld_page_size = 128;

    struct st_ctx_t *st_ctx;
    struct qsfp_ctx_t *qsfp_ctx;
    bf_pltfm_get_qsfp_ctx (&qsfp_ctx);

    struct sfp_ctx_st_t *st_ctx_st;
    struct sfp_ctx_t *sfp_ctx;
    bf_pltfm_get_sfp_ctx (&sfp_ctx);

    for (i = 0; i < cpld_num; i ++) {
        for (j = 0; j < cpld_page_size; j ++) {
            if (bf_pltfm_cpld_read_byte (i + 1, j, &buf[i][j])) {
                break;
            }
        }

        bf_pltfm_cpld_show (uc, i + 1, &buf[i][0], cpld_page_size);
    }

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* QSFP */
    for (i = 0; i < 8; i ++) {
        aim_printf (&uc->pvs, " C%-2d", i + 1);
    }
    aim_printf (&uc->pvs, "\n");

    /* RST */
    for (i = 0; i < 8; i ++) {
        st_ctx = &qsfp_ctx[i].rst;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? '*' : ' ');
    }
    aim_printf (&uc->pvs, "|       RST");
    aim_printf (&uc->pvs, "\n");

    /* PRS */
    for (i = 0; i < 8; i ++) {
        st_ctx = &qsfp_ctx[i].pres;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       PRS");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* SFP */
    for (i = 0; i < 48; i ++) {
        aim_printf (&uc->pvs, " Y%-2d", i + 1);
    }
    aim_printf (&uc->pvs, "\n");

    /* RX_LOS */
    for (i = 0; i < 48; i ++) {
        st_ctx_st = sfp_ctx[i].st;
        st_ctx = &st_ctx_st->rx_los;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? '*' : ' ');
    }
    aim_printf (&uc->pvs, "|       LOS");
    aim_printf (&uc->pvs, "\n");

    /* PRS */
    for (i = 0; i < 48; i ++) {
        st_ctx_st = sfp_ctx[i].st;
        st_ctx = &st_ctx_st->pres;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       PRS");
    aim_printf (&uc->pvs, "\n");

    /* TX_DIS */
    for (i = 0; i < 48; i ++) {
        st_ctx_st = sfp_ctx[i].st;
        st_ctx = &st_ctx_st->tx_dis;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       TXON");
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* Aux */
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%5s   %5s   %2s   %13s   %15s   %3s   %3s\n", "DPU-2", "DPU-1", "BF", "PCA9548-4/5/6", "PCA9548-0/1/2/3", "BMC",
                platform_subtype_equal (V3P0) ? "PTP" : "");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%5s   %5s   %2s   %13s   %15s   %3s   %3s\n", "-----", "-----", "--", "-------------", "---------------", "---",
                platform_subtype_equal (V3P0) ? "---" : "");
    aim_printf (&uc->pvs, "         RST");
    aim_printf (&uc->pvs, "%5s   %5s   %2s   %13s   %15s   %3s   %3s\n",
        buf[0][2] & 0x80 ? "*" : " ",
        buf[0][2] & 0x40 ? "*" : " ",
        buf[0][2] & 0x20 ? "*" : " ",
        buf[0][2] & 0x04 ? "*" : " ",
        buf[0][2] & 0x02 ? "*" : " ",
        buf[0][2] & 0x01 ? "*" : " ",
        (platform_subtype_equal (V3P0) && buf[0][3] & 0x80) ? "*" : " ");

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
}

static void bf_pltfm_cpld_decode_x532p (ucli_context_t *uc) {
    int i, j;
    uint8_t buf[2][128];
    uint8_t cpld_num = 2;
    uint8_t cpld_page_size = 128;

    struct st_ctx_t *st_ctx;
    struct qsfp_ctx_t *qsfp_ctx;
    bf_pltfm_get_qsfp_ctx (&qsfp_ctx);

    struct sfp_ctx_st_t *st_ctx_st;
    struct sfp_ctx_t *sfp_ctx;
    bf_pltfm_get_sfp_ctx (&sfp_ctx);

    for (i = 0; i < cpld_num; i ++) {
        for (j = 0; j < cpld_page_size; j ++) {
            if (bf_pltfm_cpld_read_byte (i + 1, j, &buf[i][j])) {
                break;
            }
        }

        bf_pltfm_cpld_show (uc, i + 1, &buf[i][0], cpld_page_size);
    }

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* QSFP */
    for (i = 0; i < 32; i ++) {
        aim_printf (&uc->pvs, " C%-2d", i + 1);
    }
    aim_printf (&uc->pvs, "\n");

    /* RST */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].rst;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? '*' : ' ');
    }
    aim_printf (&uc->pvs, "|       RST");
    aim_printf (&uc->pvs, "\n");

    /* PRS */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].pres;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       PRS");
    aim_printf (&uc->pvs, "\n");

    /* INT */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].intr;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       INT");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* SFP */
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%3s   %5s   %3s   %4s\n", "LOS", "FAULT", "PRS", "TXON");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%3s   %5s   %3s   %4s\n", "---", "-----", "---", "----");

    for (i = 0; i < 2; i ++) {
        aim_printf (&uc->pvs, "         Y%02d", i + 1);

        st_ctx_st = sfp_ctx[i].st;

        st_ctx = &st_ctx_st->rx_los;
        aim_printf (&uc->pvs, "%3s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? "*" : " ");
        st_ctx = &st_ctx_st->tx_fault;
        aim_printf (&uc->pvs, "%5s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? "*" : " ");
        st_ctx = &st_ctx_st->pres;
        aim_printf (&uc->pvs, "%3s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? " " : "*");
        st_ctx = &st_ctx_st->tx_dis;
        aim_printf (&uc->pvs, "%4s\n",  buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? " " : "*");
    }

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%7s   %7s   %7s   %7s\n", "C01-C08", "C09-C16", "C17-C24", "C25-C32");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%7s   %7s   %7s   %7s\n", "-------", "-------", "-------", "-------");
    aim_printf (&uc->pvs, "PCA9548 RST ");
    aim_printf (&uc->pvs, "%7s   %7s   %7s   %7s\n",
        buf[1][14] & 0x01 ? "*" : " ",
        buf[1][14] & 0x02 ? "*" : " ",
        buf[1][14] & 0x04 ? "*" : " ",
        buf[1][14] & 0x08 ? "*" : " ");

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
}

static void bf_pltfm_cpld_decode_x564p (ucli_context_t *uc) {
    int i, j;
    uint8_t buf[3][128];
    uint8_t cpld_num = 3;
    uint8_t cpld_page_size = 128;

    struct st_ctx_t *st_ctx;
    struct qsfp_ctx_t *qsfp_ctx;
    bf_pltfm_get_qsfp_ctx (&qsfp_ctx);

    struct sfp_ctx_st_t *st_ctx_st;
    struct sfp_ctx_t *sfp_ctx;
    bf_pltfm_get_sfp_ctx (&sfp_ctx);

    for (i = 0; i < cpld_num; i ++) {
        for (j = 0; j < cpld_page_size; j ++) {
            if (bf_pltfm_cpld_read_byte (i + 1, j, &buf[i][j])) {
                break;
            }
        }

        bf_pltfm_cpld_show (uc, i + 1, &buf[i][0], cpld_page_size);
    }

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* QSFP */
    for (i = 0; i < 32; i ++) {
        aim_printf (&uc->pvs, " C%-2d", i + 1);
    }
    aim_printf (&uc->pvs, "\n");
    /* RST */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].rst;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? '*' : ' ');
    }
    aim_printf (&uc->pvs, "|       RST");
    aim_printf (&uc->pvs, "\n");

    /* PRS */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].pres;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       PRS");
    aim_printf (&uc->pvs, "\n");

    /* INT */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].intr;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       INT");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* QSFP */
    for (i = 32; i < 64; i ++) {
        aim_printf (&uc->pvs, " C%-2d", i + 1);
    }
    aim_printf (&uc->pvs, "\n");
    /* RST */
    for (i = 32; i < 64; i ++) {
        st_ctx = &qsfp_ctx[i].rst;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? '*' : ' ');
    }
    aim_printf (&uc->pvs, "|       RST");
    aim_printf (&uc->pvs, "\n");

    /* PRS */
    for (i = 32; i < 64; i ++) {
        st_ctx = &qsfp_ctx[i].pres;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       PRS");
    aim_printf (&uc->pvs, "\n");

    /* INT */
    for (i = 32; i < 64; i ++) {
        st_ctx = &qsfp_ctx[i].intr;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       INT");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* SFP */
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%3s   %5s   %3s   %4s\n", "LOS", "FAULT", "PRS", "TXON");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%3s   %5s   %3s   %4s\n", "---", "-----", "---", "----");

    for (i = 0; i < 2; i ++) {
        aim_printf (&uc->pvs, "         Y%02d", i + 1);

        st_ctx_st = sfp_ctx[i].st;

        st_ctx = &st_ctx_st->rx_los;
        aim_printf (&uc->pvs, "%3s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? "*" : " ");
        st_ctx = &st_ctx_st->tx_fault;
        aim_printf (&uc->pvs, "%5s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? "*" : " ");
        st_ctx = &st_ctx_st->pres;
        aim_printf (&uc->pvs, "%3s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? " " : "*");
        st_ctx = &st_ctx_st->tx_dis;
        aim_printf (&uc->pvs, "%4s\n",  buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? " " : "*");
    }

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%9s   %9s   %9s   %9s\n", "PCA9548-0", "PCA9548-1", "PCA9548-2", "PCA9548-3");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%9s   %9s   %9s   %9s\n", "---------", "---------", "---------", "---------");
    aim_printf (&uc->pvs, "PCA9548 RST ");
    aim_printf (&uc->pvs, "%9s   %9s   %9s   %9s\n",
        buf[1][14] & 0x01 ? "*" : " ",
        buf[1][14] & 0x02 ? "*" : " ",
        buf[1][14] & 0x04 ? "*" : " ",
        buf[1][14] & 0x08 ? "*" : " ");

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
}

static void bf_pltfm_cpld_decode_x732q (ucli_context_t *uc) {
    int i, j;
    uint8_t buf[2][128];
    uint8_t cpld_num = 1;
    uint8_t cpld_page_size = 128;

    struct st_ctx_t *st_ctx;
    struct qsfp_ctx_t *qsfp_ctx;
    bf_pltfm_get_qsfp_ctx (&qsfp_ctx);

    struct sfp_ctx_st_t *st_ctx_st;
    struct sfp_ctx_t *sfp_ctx;
    bf_pltfm_get_sfp_ctx (&sfp_ctx);

    for (i = 0; i < cpld_num; i ++) {
        for (j = 0; j < cpld_page_size; j ++) {
            if (bf_pltfm_cpld_read_byte (i + 1, j, &buf[i][j])) {
                break;
            }
        }

        bf_pltfm_cpld_show (uc, i + 1, &buf[i][0], cpld_page_size);
    }

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* QSFP */
    for (i = 0; i < 32; i ++) {
        aim_printf (&uc->pvs, " Q%-2d", i + 1);
    }
    aim_printf (&uc->pvs, "\n");

    /* RST */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].rst;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       RST");
    aim_printf (&uc->pvs, "\n");

    /* PRS */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].pres;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       PRS");
    aim_printf (&uc->pvs, "\n");

    /* INT */
    for (i = 0; i < 32; i ++) {
        st_ctx = &qsfp_ctx[i].intr;
        aim_printf (&uc->pvs, "| %c ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? ' ' : '*');
    }
    aim_printf (&uc->pvs, "|       INT");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    /* SFP */
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%3s   %3s   %4s\n", "LOS", "PRS", "TXON");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%3s   %3s   %4s\n", "---", "---", "----");

    for (i = 0; i < 2; i ++) {
        aim_printf (&uc->pvs, "         Y%02d", i + 1);

        st_ctx_st = sfp_ctx[i].st;

        st_ctx = &st_ctx_st->rx_los;
        aim_printf (&uc->pvs, "%3s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? "*" : " ");
        st_ctx = &st_ctx_st->pres;
        aim_printf (&uc->pvs, "%3s   ", buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? " " : "*");
        st_ctx = &st_ctx_st->tx_dis;
        aim_printf (&uc->pvs, "%4s\n",  buf[st_ctx->cpld_sel - 1][st_ctx->off] & (1 << st_ctx->off_b) ? " " : "*");
    }

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%7s   %7s   %7s   %7s\n", "Q01-Q08", "Q09-Q16", "Q17-Q24", "Q25-Q32");
    aim_printf (&uc->pvs, "            ");
    aim_printf (&uc->pvs, "%7s   %7s   %7s   %7s\n", "-------", "-------", "-------", "-------");
    aim_printf (&uc->pvs, "PCA9548 RST ");
    aim_printf (&uc->pvs, "%7s   %7s   %7s   %7s\n",
        buf[1][14] & 0x01 ? "*" : " ",
        buf[1][14] & 0x02 ? "*" : " ",
        buf[1][14] & 0x04 ? "*" : " ",
        buf[1][14] & 0x08 ? "*" : " ");

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");

    if (platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "            ");
        aim_printf (&uc->pvs, "PTP module\n");
        aim_printf (&uc->pvs, "            ");
        aim_printf (&uc->pvs, "----------\n");

        aim_printf (&uc->pvs, "%11s%6s\n", "PRES",      (buf[0][24] & 0x01) == 0x01 ? "" : "*");
        aim_printf (&uc->pvs, "%11s%9s\n", "CLK SRC",   (buf[0][25] & 0x03) == 0x02 ? "CRYSTAL" : "  PTP  ");
        aim_printf (&uc->pvs, "%11s%7s\n", "BARE SYNC", (buf[0][26] & 0x02) == 0x02 ? "ENB" : "DIS");

        aim_printf (&uc->pvs, "\n");
        aim_printf (&uc->pvs, "\n");
    }
}

/** read bytes from slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to read
*  @param val
*   value to read into
*  @return
*   0 on success and otherwise in error
*/
int bf_pltfm_cp2112_reg_read_block (
  uint8_t addr,
  uint8_t reg,
  uint8_t *read_buf,
  uint32_t read_buf_size)
{
  uint8_t wr_val;

  wr_val = reg;
  return (bf_pltfm_cp2112_write_read_unsafe (g_cpld_cp2112_hndl,
                      addr << 1, &wr_val, read_buf, 1, read_buf_size, 100));
}

/** write a byte to slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to write
*  @param val
*   value to write
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cp2112_reg_write_byte (
  uint8_t addr,
  uint8_t reg,
  uint8_t val)
{
  uint8_t wr_val[2] = {0};

  wr_val[0] = reg;
  wr_val[1] = val;
  return (bf_pltfm_cp2112_write (g_cpld_cp2112_hndl,
                      addr << 1, wr_val, 2, 1000));
}

/** write bytes to slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to write
*  @param val
*   value to write
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cp2112_reg_write_block (
    uint8_t addr,
    uint8_t reg,
    uint8_t *write_buf,
    uint32_t write_buf_size)
{
    uint32_t tx_len = write_buf_size % 256;
    uint8_t wr_val[256] = {0};

    wr_val[0] = reg;
    //wr_val[1] = val;
    memcpy (wr_val + 1, write_buf, tx_len);
    return (bf_pltfm_cp2112_write (g_cpld_cp2112_hndl,
                        addr << 1, wr_val, tx_len, 100));
}

/** write PCA9548 to select channel which connects to syscpld
*
*  @param cpld_index
*   index of cpld for a given platform
*  @return
*   0 on success otherwise in error
*/
int select_cpld (uint8_t cpld_index)
{
    /* for X312P and X308P(v1.0/1.1/2.0), there's no need to select CPLD */
    if (platform_type_equal (AFN_X312PT) ||
        (platform_type_equal (AFN_X308PT) &&
        (platform_subtype_equal(V1P0) || platform_subtype_equal(V1P1) || platform_subtype_equal(V2P0)))) {
        return 0;
    }

    int rc = -1;
    int chnl = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR;

    if ((cpld_index <= 0) ||
        (cpld_index > bf_pltfm_mgr_ctx()->cpld_count)) {
        return -2;
    }

    chnl = cpld_index - 1;

    if (platform_type_equal (AFN_X732QT)) {
        /* for X732Q, master pca9548 is 0x74 */
        pca9548_addr = 0x74;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
        rc = bf_pltfm_cp2112_reg_write_byte (
                pca9548_addr, 0x00, 1 << chnl);
    } else {
        /* Switch channel */
        rc = bf_pltfm_master_i2c_write_byte (
                pca9548_addr, 0x00, 1 << chnl);
    }

    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "select cpld(%d : %s : %d)"
            "\n",
            __FILE__, __LINE__, cpld_index,
            "Failed to write PCA9548", rc);
    }

    return rc;
}

/** write PCA9548 back to zero to de-select syscpld
*
*  @return
*   0 on success otherwise in error
*/
int unselect_cpld()
{
    /* for X312P and X308P(v1.0/1.1/2.0), there's no need to select CPLD */
    if (platform_type_equal (AFN_X312PT) ||
        (platform_type_equal (AFN_X308PT) &&
        (platform_subtype_equal(V1P0) || platform_subtype_equal(V1P1) || platform_subtype_equal(V2P0)))) {
        return 0;
    }

    int rc = -1;
    int retry_times = 0;
    uint8_t pca9548_value = 0xFF;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR;

    if (platform_type_equal (AFN_X732QT)) {
        /* for X732Q, master pca9548 is 0x74 */
        pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
        for (retry_times = 0; retry_times < 10; retry_times ++) {
            // unselect PCA9548
            if (bf_pltfm_cp2112_reg_write_byte (
                    pca9548_addr, 0x00, 0x00)) {
                rc = -2;
                break;
            }

            bf_sys_usleep (5000);

            // readback PCA9548 to ensure PCA9548 is closed
            if (bf_pltfm_cp2112_reg_read_block (
                    pca9548_addr, 0x00, &pca9548_value, 0x01)) {
                rc = -3;
                break;
            }

            if (pca9548_value == 0) {
                rc = 0;
                break;
            }

            bf_sys_usleep (5000);
        }
    } else {
        for (retry_times = 0; retry_times < 10; retry_times ++) {
            // unselect PCA9548
            if (bf_pltfm_master_i2c_write_byte (
                    pca9548_addr, 0x00, 0x00)) {
                rc = -2;
                break;
            }

            bf_sys_usleep (5000);

            // readback PCA9548 to ensure PCA9548 is closed
            if (bf_pltfm_master_i2c_read_byte (
                    pca9548_addr, 0x00, &pca9548_value)) {
                rc = -3;
                break;
            }

            if (pca9548_value == 0) {
                rc = 0;
                break;
            }

            bf_sys_usleep (5000);
        }
    }

    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "de-select cpld(%s : %d)"
            "\n",
            __FILE__, __LINE__, "Failed to write PCA9548", rc);
    }

    return rc;
}

static inline int cpld2addr (uint8_t cpld_index, uint8_t *i2caddr)
{
    uint8_t addr = 0xFF;

    if (platform_type_equal (AFN_X312PT)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X312P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X312P_SYSCPLD2_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD3:
                addr = X312P_SYSCPLD3_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD4:
                addr = X312P_SYSCPLD4_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD5:
                addr = X312P_SYSCPLD5_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (AFN_X308PT)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X308P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X308P_SYSCPLD2_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (AFN_X564PT)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X564P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X564P_SYSCPLD2_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD3:
                addr = X564P_SYSCPLD3_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (AFN_X532PT)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X532P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X532P_SYSCPLD2_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (AFN_X732QT)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X732Q_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X732Q_SYSCPLD2_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        /* TBD */
        return -1;
    }
    *i2caddr = addr;
    return 0;
}

/** read a byte from syscpld register
*
*  @param cpld_index
*   index of cpld for a given platform
*  @param offset
*   syscpld register to read
*  @param buf
*   buffer to read to
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cpld_read_byte (
    uint8_t cpld_index,
    uint8_t offset,
    uint8_t *buf)
{
    int rc = -1;
    uint8_t addr = 0;

    rc = cpld2addr (cpld_index, &addr);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "cpld2addr(%d : %s : %d)"
            "\n",
            __FILE__, __LINE__, cpld_index,
            "Failed to get cpld's i2caddr", rc);
        return rc;
    }

    if (platform_type_equal (AFN_X312PT) &&
        cpld_index == BF_MAV_SYSCPLD2) {
        *buf = 0x00;
        return 0;
    }

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {
        if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
            rc = bf_pltfm_cp2112_reg_read_block (
                                            addr, offset, buf, 1);
        } else {
            rc = bf_pltfm_master_i2c_read_byte (addr,
                                            offset, buf);
        }
        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    return rc;
}

/** write a byte to syscpld register
*
*  @param cpld_index
*   index of cpld for a given platform
*  @param offset
*   syscpld register to read
*  @param buf
*   buffer to write from
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cpld_write_byte (
    uint8_t cpld_index,
    uint8_t offset,
    uint8_t val)
{
    int rc = -1;
    uint8_t addr = 0;

    rc = cpld2addr (cpld_index, &addr);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "cpld2addr(%d : %s : %d)"
            "\n",
            __FILE__, __LINE__, cpld_index,
            "Failed to get cpld's i2caddr", rc);
        return rc;
    }

    if (platform_type_equal (AFN_X312PT) &&
        cpld_index == BF_MAV_SYSCPLD2) {
        return 0;
    }

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {
        if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
            rc = bf_pltfm_cp2112_reg_write_byte (
                                    addr, offset, val);
        } else {
            rc = bf_pltfm_master_i2c_write_byte (
                                    addr, offset, val);
        }
        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    return rc;
}

/** reset syscpld for a given platform
*
*  @param none
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_syscpld_reset()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    uint8_t offset = 0x2;
    int usec = 2 * 1000 * 1000;

    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                    offset, &val0);
    if (rc) {
        goto end;
    }
    bf_sys_usleep (usec);

    val = val0;
    if (platform_type_equal (AFN_X532PT)) {
        /* offset = 2 and bit[2] to reset cpld2.
         * by tsihang, 2020-05-25 */
        offset = 0x2;
        /* reset cpld2 */
        val |= (1 << 2);
    } else if (platform_type_equal (AFN_X564PT)) {
        /* offset = 2 and bit[4], bit[5] to reset cpld2 and cpld3.
         * by tsihang, 2020-05-25 */
        offset = 0x2;
        /* reset cpld2 */
        val |= (1 << 4);
        /* reset cpld3 */
        val |= (1 << 5);

    } else if (platform_type_equal (AFN_HC36Y24C)) {
        /* offset = 3 and bit[3], bit[4] to reset cpld2 and cpld3.
         * by tsihang, 2020-05-25 */
        offset = 0x3;
        /* reset cpld2 */
        val |= (1 << 3);
        /* reset cpld3 */
        val |= (1 << 4);
    } else if (platform_type_equal (AFN_X312PT)) {
        offset = 0x0C;
        val = 0x5f;
        // CPLD3,CPLD4,CPLD5 reset timing=1000ms
    } else if (platform_type_equal (AFN_X732QT)) {
        // No entry to reset cpld.
    } else {
        return 0;
    }

    rc = bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                    offset, val);
    if (rc) {
        goto end;
    }
    bf_sys_usleep (usec);

    /* read back for check and log. */
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                    offset, &val1);
    if (rc) {
        goto end;
    }

    /* there's no need to check for X312P as it will auto de-assert after 1000ms, so return here. */
    if (platform_type_equal (AFN_X312PT)) {
        LOG_DEBUG ("CPLD3-5 RST(auto de-reset) : (0x%02x -> 0x%02x =? 0x%02x) : %s",
                   val0, val, val1, rc ? "failed" : "success");

        return 0;
    }
    LOG_DEBUG ("CPLD +RST : (0x%02x -> 0x%02x)", val0,
               val1);

    val = val1;
    if (platform_type_equal (AFN_X532PT)) {
        /* !reset cpld2 */
        val &= ~ (1 << 2);
    } else if (platform_type_equal (AFN_X564PT)) {
        /* !reset cpld2 */
        val &= ~ (1 << 4);
        /* !reset cpld3 */
        val &= ~ (1 << 5);
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        /* !reset cpld2 */
        val &= ~ (1 << 3);
        /* !reset cpld3 */
        val &= ~ (1 << 4);
    } else if (platform_type_equal (AFN_X732QT)) {
        // No entry to reset cpld.
    } else {
        return 0;
    }

    rc = bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                    offset, val);
    if (rc) {
        goto end;
    }

    bf_sys_usleep (usec);

    /* read back for check and log. */
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                    offset, &val0);
    if (rc) {
        goto end;
    }
    LOG_DEBUG ("CPLD -RST : (0x%02x -> 0x%02x)", val1,
               val0);

end:
    return rc;
}

/* Reset PCA9548 for QSFP/SFP control.
 * offset = 14, and
 * bit[0] reset PCA9548 for C8~C1
 * bit[1] reset PCA9548 for C16~C9
 * bit[2] reset PCA9548 for C24~C17
 * bit[3] reset PCA9548 for C32~C25
 * by tsihang, 2020-05-25
 *
 *
 * offset = 2, and
 * bit[0] reset PCA9548 for Q15~Q1
 * bit[1] reset PCA9548 for Q32~Q16.
 * by tsihang, 2023-12-22
 */
int bf_pltfm_pca9548_reset_03()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;
    uint8_t syscpld = BF_MAV_SYSCPLD2;
    uint8_t off = 14;

    if (platform_type_equal (AFN_X732QT)) {
        syscpld = BF_MAV_SYSCPLD1;
        off = 2;
    }
    rc |= bf_pltfm_cpld_read_byte (syscpld,
                off, &val0);
    val = val0;
    if (platform_type_equal (AFN_X732QT)) {
        val &= ~ (1 << 0);
        val &= ~ (1 << 1);
    } else {
        val |= (1 << 0);
        val |= (1 << 1);
    }
    if (platform_type_equal (AFN_X564PT) ||
        platform_type_equal (AFN_X532PT)) {
        val |= (1 << 2);
        val |= (1 << 3);
    }

    rc |= bf_pltfm_cpld_write_byte (syscpld,
                off, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (syscpld,
                off, &val1);
    if (rc != 0) {
        goto end;
    }
    LOG_DEBUG ("PCA9548(0-3) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    if (platform_type_equal (AFN_X732QT)) {
        val |= (1 << 0);
        val |= (1 << 1);
    } else {
        val &= ~ (1 << 0);
        val &= ~ (1 << 1);
    }
    if (platform_type_equal (AFN_X564PT) ||
        platform_type_equal (AFN_X532PT)) {
        val &= ~ (1 << 2);
        val &= ~ (1 << 3);
    }

    rc |= bf_pltfm_cpld_write_byte (syscpld,
                off, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (syscpld,
                off, &val0);
    if (rc) {
        goto end;
    }
    LOG_DEBUG ("PCA9548(0-3) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

/* Reset PCA9548 for QSFP/SFP control.
 * CPLD1 offset = 2, and
 * bit[3] reset PCA9548 for C8~C1
 * bit[2] reset PCA9548 for C16~C9
 * bit[1] reset PCA9548 for C24~C17
 * bit[0] reset PCA9548 for C32~C25
 * by tsihang, 2020-05-25 */
int bf_pltfm_pca9548_reset_47()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);

    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val1);
    if (rc != 0) {
        goto end;
    }
    LOG_DEBUG ("PCA9548(4-7) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    val &= ~ (1 << 0);
    val &= ~ (1 << 1);
    val &= ~ (1 << 2);
    val &= ~ (1 << 3);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);
    if (rc) {
        goto end;
    }

    LOG_DEBUG ("PCA9548(4-7) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

/* Reset PCA9548 for QSFP/SFP control.
 * CPLD1 offset = 2, and
 * bit[2] reset PCA9548 for Y33~Y48, C1~C8
 * bit[1] reset PCA9548 for Y1~Y32
 * by yanghuachen, 2022-02-25 */
int bf_pltfm_pca9548_reset_x308p()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                2, &val0);

    val = val0;
    val |= (1 << 1);
    val |= (1 << 2);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                2, &val1);
    if (rc != 0) {
        goto end;
    }

    LOG_DEBUG ("PCA9548(0-6) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    val &= ~ (1 << 1);
    val &= ~ (1 << 2);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                2, &val0);
    if (rc) {
        goto end;
    }

    LOG_DEBUG ("PCA9548(0-6) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

int bf_pltfm_pca9548_reset_x312p()
{
    int rc = 0;
    uint8_t ret_value;

    // select to level1 pca9548 ch1, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 1);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    LOG_DEBUG("L1[1]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x74, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x74, 0x0, &ret_value);
    LOG_DEBUG("L1[1]: L2_0x74: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch2, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 2);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    LOG_DEBUG("L1[2]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    LOG_DEBUG("L1[2]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch3, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 3);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    LOG_DEBUG("L1[3]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    LOG_DEBUG("L1[3]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch4, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 4);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    LOG_DEBUG("L1[4]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    LOG_DEBUG("L1[4]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch5, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 5);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    LOG_DEBUG("L1[5]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);

    // unselect level1 pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 0x0);
    bf_sys_usleep (500);

    LOG_DEBUG ("Reset all PCA9548 : %s!",
               rc ? "failed" : "success");

    return rc;
}

/* Reset PCA9548 for QSFP/SFP control.
 * CPLD1 offset = 2
 */
int bf_pltfm_pca9548_reset_hc()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);

    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);
    val |= (1 << 4);
    val |= (1 << 5);
    val |= (1 << 6);
    val |= (1 << 7);

    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val1);
    if (rc != 0) {
        goto end;
    }
    LOG_DEBUG ("PCA9548(0-7) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    val &= ~ (1 << 0);
    val &= ~ (1 << 1);
    val &= ~ (1 << 2);
    val &= ~ (1 << 3);
    val &= ~ (1 << 4);
    val &= ~ (1 << 5);
    val &= ~ (1 << 6);
    val &= ~ (1 << 7);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);
    if (rc) {
        goto end;
    }
    LOG_DEBUG ("PCA9548(0-7) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

int bf_pltfm_get_cpld_ver (uint8_t cpld_index, char *version, bool forced)
{
    int rc = -1;
    /* 0xFF means invalid. */
    uint8_t veroff = 0xFF;
    uint8_t val = 0;
    char *cpld_version = g_cpld_index_version[cpld_index - 1];

    /* Read cached CPLD version to caller. */
    if ((!forced) && (cpld_version[0] != 0)) {
        memcpy (version, cpld_version, 8);
        return 0;
    }

    if (platform_type_equal (AFN_X312PT)) {
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                veroff = 0x13;
                break;
            case BF_MAV_SYSCPLD2:
                /* We can not get CPLD2 from COMe, make the off invalid. */
                veroff = 0xFF;
                break;
            case BF_MAV_SYSCPLD3:
            case BF_MAV_SYSCPLD4:
                veroff = 0x0E;
                break;
            case BF_MAV_SYSCPLD5:
                veroff = 0x0A;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (AFN_X308PT)) {
        veroff = 0;
    } else if (platform_type_equal (AFN_X532PT)) {
        veroff = 0;
    } else if (platform_type_equal (AFN_X564PT)) {
        veroff = 0;
    } else if (platform_type_equal (AFN_X732QT)) {
        veroff = 0;
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        veroff = 0;
    }

    if (veroff == 0XFF) {
        sprintf(cpld_version, "%s", "N/A");
        return 0;
    }

    rc = bf_pltfm_cpld_read_byte (cpld_index, veroff, &val);
    if (rc) {
        sprintf(cpld_version, "N/A");
    } else {
        if (platform_type_equal (AFN_X312PT)) {
            sprintf(cpld_version, "v%d.0", val);
        } else if (platform_type_equal (AFN_X308PT)) {
            uint8_t ver = 0;
            /* cpld1 : [3:0], cpld2 : [7:4] */
            ver = (cpld_index == 1 ? (val & 0x0F) : ((val >> 4) & 0x0F));
            sprintf(cpld_version, "v%d.0", ver);
        } else if (platform_type_equal (AFN_X532PT)) {
            sprintf(cpld_version, "v%d.0", val);
        } else if (platform_type_equal (AFN_X564PT)) {
            sprintf(cpld_version, "v%d.0", val);
        } else if (platform_type_equal (AFN_X732QT)) {
            sprintf(cpld_version, "v%d.0", val);
        } else if (platform_type_equal (AFN_HC36Y24C)) {
            sprintf(cpld_version, "v%d.0", val);
        }
    }
    memcpy (version, cpld_version, 8);

    return 0;
}
// Not thread-safe.
static void bf_pltfm_read_devts_bsync() {
    uint64_t devts_cnt_cap= 0;
    uint64_t devts_cnt_set= 0;
    uint64_t devts_off_set= 0;
    uint32_t devts_inc_set= 0;

    uint64_t bsync_cnt_cap= 0;
    uint64_t bsync_cnt_set= 0;
    uint32_t bsync_inc_set= 0;
    uint32_t bsync_inc_f  = 0;
    uint32_t bsync_inc_f_d= 0;

    bool devts_enb, bsync_enb;
    uint32_t rst_cnt_thrs = 0, debounce_cnt = 0;

    static uint64_t devts_cnt_cap_last, bsync_cnt_cap_last;

    bf_ts_global_ts_value_get(0, &devts_cnt_set);
    bf_ts_global_ts_offset_get(0, &devts_off_set);
    bf_ts_global_ts_inc_value_get(0, &devts_inc_set);
    (void)devts_off_set;
    (void)devts_inc_set;

    bf_ts_baresync_reset_value_get(0, &bsync_cnt_set);

    if(platform_type_equal(AFN_X732QT)) {
#if SDE_VERSION_GT(9131)
        bf_tof2_ts_baresync_increment_get(0, &bsync_inc_set, &bsync_inc_f, &bsync_inc_f_d);
#endif
    }

    (void)bsync_inc_f;
    (void)bsync_inc_f_d;

    bf_ts_global_baresync_ts_get(0, &devts_cnt_cap, &bsync_cnt_cap);

    bf_ts_baresync_state_get(0, &rst_cnt_thrs, &debounce_cnt, &bsync_enb);
    bf_ts_global_ts_state_get(0, &devts_enb);

    fprintf(stdout, "\n%16s%16d\n", "bsync_enb", bsync_enb);
    fprintf(stdout, "%16s%16u (F %u : D %u)   (rst %u : dbc %u)\n", "bsync_inc_set", bsync_inc_set, bsync_inc_f, bsync_inc_f_d, rst_cnt_thrs, debounce_cnt);
    fprintf(stdout, "%16s%16lu, cap %16lu, incr %lu\n", "bsync_cnt_set", bsync_cnt_set, bsync_cnt_cap, bsync_cnt_cap - bsync_cnt_cap_last);

    fprintf(stdout, "\n%16s%16d\n", "devts_enb", devts_enb);
    fprintf(stdout, "%16s%16lu, cap %16lu, incr %lu\n", "devts_cnt_set", devts_cnt_set, devts_cnt_cap, devts_cnt_cap - devts_cnt_cap_last);

    devts_cnt_cap_last = devts_cnt_cap;
    bsync_cnt_cap_last = bsync_cnt_cap;
}

int bf_pltfm_read_ptp_reg(uint8_t page, uint8_t reg, uint8_t* value) {
    int rc = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = 0x58;

    MASTER_I2C_LOCK;
    // Select channel 5 to access PTP module
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20)) {
        fprintf (stdout, "\nFailed to select PTP.\n");
        rc = -1;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, 0xFD, page)) {
        fprintf (stdout, "\nFailed to select PTP page.\n");
        rc = -2;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_read_block (ptp_addr, reg, value, 1)) {
        fprintf (stdout, "\nFailed to read PTP reg.\n");
        rc = -3;
    }

finish:
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00)) {
        fprintf (stdout, "\nFailed to de-select PTP.\n");
        rc = -4;
    }

    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_write_ptp_reg(uint8_t page, uint8_t reg, uint8_t value) {
    int rc = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = 0x58;

    MASTER_I2C_LOCK;
    // Select channel 5 to access PTP module
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20)) {
        fprintf (stdout, "\nFailed to select PTP.\n");
        rc = -1;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, 0xFD, page)) {
        fprintf (stdout, "\nFailed to select PTP page.\n");
        rc = -2;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, reg, value)) {
        fprintf (stdout, "\nFailed to write PTP reg.\n");
        rc = -3;
    }

finish:
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00)) {
        fprintf (stdout, "\nFailed to de-select PTP.\n");
        rc = -4;
    }

    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_write_ptp_reg_burst(uint8_t page, uint8_t reg, uint8_t *value, size_t l) {
    int rc = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = 0x58;

    MASTER_I2C_LOCK;
    // Select channel 5 to access PTP module
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20)) {
        fprintf (stdout, "\nFailed to select PTP.\n");
        rc = -1;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, 0xFD, page)) {
        fprintf (stdout, "\nFailed to select PTP page.\n");
        rc = -2;
        goto finish;
    }

    for (size_t i = 0; i < l; i++) {
        if (bf_pltfm_cp2112_reg_write_byte(ptp_addr, reg, value[i]) < 0) {
            fprintf (stdout, "\nFailed to write PTP reg.\n");
            return -3;
        }
    }

finish:
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00)) {
        fprintf (stdout, "\nFailed to de-select PTP.\n");
        rc = -4;
    }

    MASTER_I2C_UNLOCK;
    return rc;
}

// for X732Q-T-V2.0 only
int bf_pltfm_set_clk(bf_pltfm_clk_source clk_source) {
    bf_status_t bf_status = 0;

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0))
       goto finish;

    if (clk_source == CLK_SMU) {
        // absent = ptp absent ? see 0x18
        // source selected ? see 0x19
        if(bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED) {
            if (bf_pltfm_cpld_write_byte (1, 0x19, 0x01)) {
                fprintf (stdout, "\nFailed to write CPLD.\n");
                return -1;
            }
            bf_pltfm_mgr_ctx()->flags |= AF_PLAT_CTRL_CLK_SMU;
        }
    } else {
        // source selected ? see 0x19
        if (bf_pltfm_cpld_write_byte (1, 0x19, 0x02)) {
            fprintf (stdout, "\nFailed to write CPLD.\n");
            return -1;
        }
        bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_CTRL_CLK_SMU;
    }

finish:
    fprintf (stdout, "\nCLK=%s\n\n",
            (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CLK_SMU) ? "CLK_SMU" : "CLK_CRYSTAL");
    return bf_status;
}

int bf_pltfm_tx_rst_pulse()
{
    bf_status_t bf_status = false;
    uint8_t bsync_ctrl, val = 0;

    if(!(bf_pltfm_mgr_ctx()->flags &
            AF_PLAT_CTRL_CLK_SMU)) {
        goto finish;
    }

    // 0x1A[1] is not neccessary if bsync_tx_rst_pulse is triggered by software.

    // bsync_tx_rst_pulse, see 0x1A[0]
    bsync_ctrl = 0x03;
    if (bf_pltfm_cpld_write_byte (1, 0x1A, bsync_ctrl)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bf_pltfm_cpld_read_byte (1, 0x1A, &val)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bsync_ctrl != val) {
        fprintf(stdout, "%d %x != %x\n", __LINE__, bsync_ctrl, val);
    }

    //bf_sys_usleep (500);

    bsync_ctrl = 0x02;
    if (bf_pltfm_cpld_write_byte (1, 0x1A, bsync_ctrl)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bf_pltfm_cpld_read_byte (1, 0x1A, &val)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bsync_ctrl != val) {
        fprintf(stdout, "%d %x != %x\n", __LINE__, bsync_ctrl, val);
    }

    bf_status = 0;

finish:
    return bf_status;
}

int bf_pltfm_syscpld_init()
{
    fprintf (stdout,
             "\n\nInitializing CPLD   ...\n");

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
        /* get cp2112 handler for CPLD access ONLY . */
        g_cpld_cp2112_hndl =
         bf_pltfm_cp2112_get_handle (CP2112_ID_2);
        if (platform_type_equal (AFN_X732QT)) {
            if (platform_subtype_equal (V1P0)) {
                g_cpld_cp2112_hndl =
                 bf_pltfm_cp2112_get_handle (CP2112_ID_1);
            }
            /* Next HDW there would be two cp2112.
             * CP2112_ID_1 for module IO.
             * CP2112_ID_2 for CPLD IO.
             * by Hang Tsi, 2024/01/16. */
        }

        BUG_ON (g_cpld_cp2112_hndl == NULL);
        LOG_DEBUG("The USB dev dev %p.\n",
                (void *)g_cpld_cp2112_hndl->usb_device);
        LOG_DEBUG("The USB dev contex %p.\n",
                (void *)g_cpld_cp2112_hndl->usb_context);
        LOG_DEBUG("The USB dev handle %p.\n",
                (void *)g_cpld_cp2112_hndl->usb_device_handle);
    }

    if (platform_type_equal (AFN_X564PT)) {
        bf_pltfm_syscpld_reset();
        bf_pltfm_pca9548_reset_47();
        bf_pltfm_pca9548_reset_03();
    } else if (platform_type_equal (AFN_X532PT)) {
        bf_pltfm_syscpld_reset();
        bf_pltfm_pca9548_reset_03();
    } else if (platform_type_equal (AFN_X732QT)) {
        bf_pltfm_pca9548_reset_03();
    } else if (platform_type_equal (AFN_X308PT)) {
        bf_pltfm_pca9548_reset_x308p();
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        bf_pltfm_syscpld_reset();
        bf_pltfm_pca9548_reset_hc();
    } else if (platform_type_equal (AFN_X312PT)) {
        /* reset cpld */
        bf_pltfm_syscpld_reset();
        /* reset PCA9548 */
        bf_pltfm_pca9548_reset_x312p();
    }

    /* cache cpld version */
    foreach_element (0, bf_pltfm_mgr_ctx()->cpld_count) {
        char ver[8] = {0};
        bf_pltfm_get_cpld_ver((uint8_t)(each_element + 1), ver, true);
        fprintf (stdout, "CPLD%d : %s\n", (each_element + 1), ver);
    }

    uint8_t absent;
    if (platform_type_equal (AFN_X732QT) &&
        platform_subtype_equal (V2P0)) {
        if (bf_pltfm_cpld_read_byte (1, 0x18, &absent)) {
             fprintf (stdout, "\nFailed to read CPLD.\n");
             return -1;
        }
        if(! absent) {
            bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_PTPX_INSTALLED;

            // enable bsync_ctrl.
            if (bf_pltfm_cpld_write_byte (1, 0x1A, 0x02)) {
                fprintf (stdout, "\nFailed to write CPLD.\n");
                return -1;
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__set_clk (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "set-clk", 1, "set-clk <crystal/ptp>");

    if (!memcmp(uc->pargs->args[0], "crystal", strlen("crystal"))) {
        if (bf_pltfm_set_clk(CLK_CRYSTAL) == 0) {
        }
    } else if (!memcmp(uc->pargs->args[0], "ptp", strlen("ptp"))) {
        if (bf_pltfm_set_clk(CLK_SMU) == 0) {
        }
    } else {
        aim_printf (&uc->pvs,
                    "Usage: set-clk <crystal/ptp>.\n");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__tx_rst_pulse (
    ucli_context_t *uc)
{
    uint32_t ctrl;

    UCLI_COMMAND_INFO (uc, "tx-rst-pulse", 1, " <ctrl, 0|1, 0: ignore, 1: Tx RST pulse one time>");

    ctrl = strtoul (uc->pargs->args[0], NULL, 10);

    if(ctrl) {
    	bf_pltfm_tx_rst_pulse();
    }
    bf_pltfm_read_devts_bsync();
    return 0;
}

// debug only
static ucli_status_t
bf_pltfm_cpld_ucli_ucli__bsync (
    ucli_context_t *uc)
{
    bool bsync_enb, z;
    uint32_t ctrl = 0;
    uint32_t rst_cnt_thrs = 0, debounce_cnt = 0, x = 0, y = 0;


    UCLI_COMMAND_INFO (uc, "bsync", 3, " <rst_cnt_thrs, 0~127> <debounce_cnt, 0~0xFFFFFF> <enb, 0|1, 0: disable, 1: enable>");

    rst_cnt_thrs = strtoul (uc->pargs->args[0], NULL, 10);
    debounce_cnt = strtoul (uc->pargs->args[1], NULL, 10);
    ctrl = strtoul (uc->pargs->args[2], NULL, 10);

    bf_ts_baresync_state_get(0, &x, &y, &z);

    if(ctrl) {
        bsync_enb = true;
    } else {
        bsync_enb = false;
    }

    if((int32_t)rst_cnt_thrs == -1)
        rst_cnt_thrs = x;
    if((int32_t)debounce_cnt == -1)
        debounce_cnt = y;

    bf_ts_baresync_state_set(0, rst_cnt_thrs, debounce_cnt, bsync_enb);

    aim_printf (&uc->pvs, "bsync    %s\n",
                bsync_enb ?
                "enabled" : "disabled");
    bf_pltfm_read_devts_bsync();
    return 0;
}
// debug only
static ucli_status_t
bf_pltfm_cpld_ucli_ucli__devts (
    ucli_context_t *uc)
{
    uint32_t ctrl = 0;
    bool devts_enb;

    UCLI_COMMAND_INFO (uc, "devts", 1, " <0/1> 0: disable , 1: enable");

    ctrl = strtoul (uc->pargs->args[0], NULL, 10);

    bf_ts_global_ts_state_get(0, &devts_enb);

    if(ctrl) {
        devts_enb = true;
    } else {
        devts_enb = false;
    }

    bf_ts_global_ts_state_set(0, devts_enb);

    aim_printf (&uc->pvs, "devts    %s\n",
                devts_enb ?
                "enabled" : "disabled");
    bf_pltfm_read_devts_bsync();
    return 0;
}

// debug only
static ucli_status_t
bf_pltfm_cpld_ucli_ucli__devts_cnt (
    ucli_context_t *uc)
{
    uint64_t devts_cnt_set = 0;

    UCLI_COMMAND_INFO (uc, "devts-cnt", 1, " <uint64_t>");

    devts_cnt_set = strtoul (uc->pargs->args[0], NULL, 10);

    bf_ts_global_ts_value_set(0, devts_cnt_set);

    bf_pltfm_read_devts_bsync();
    return 0;
}

// debug only
static ucli_status_t
bf_pltfm_cpld_ucli_ucli__bsync_cnt (
    ucli_context_t *uc)
{
    uint64_t bsync_cnt_set = 0;

    UCLI_COMMAND_INFO (uc, "bsync-cnt", 1, " <uint64_t>");

    bsync_cnt_set = strtoul (uc->pargs->args[0], NULL, 10);

    bf_ts_baresync_reset_value_set(0, bsync_cnt_set);

    bf_pltfm_read_devts_bsync();
    return 0;
}

// For TOF2 PTP, by Hang Tsi, 2025/12/15
static ucli_status_t bf_pltfm_cpld_ucli_ucli__clkobs_strength_set__(
    ucli_context_t *uc) {

  int dev_id;
  int drive_strength;

  UCLI_COMMAND_INFO(uc,
                    "clkobs_strength_set",
                    2,
                    "Set drive strength "
                    "<dev> <strength: 0-15>");

  dev_id = 0;
  drive_strength = strtol(uc->pargs->args[1], NULL, 0);

  if(drive_strength < 0 || drive_strength > 15)
    return -1;
  (void)dev_id;
  (void)drive_strength;
#if SDE_VERSION_GT(9131)
  if (bf_port_clkobs_drive_strength_set(dev_id, drive_strength) != BF_SUCCESS) {
    return -1;
  }
#endif
  return 0;
}

// debug only
// https://www.renesas.com/en/products/8a34004?tab=documentation
// I2C 2B Mode. See page 5 of REN_8A3xxxx=Family-Prog-Guide-v4p8p7_GDE_20210615.pdf
static ucli_status_t
bf_pltfm_cpld_ucli_ucli__ptp_page (
    ucli_context_t *uc)
{
    uint8_t buf[BUFSIZ] = { 0 };
    uint16_t byte;
    uint16_t ptp_reg_size = 256;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = 0x58;
    uint8_t page, page_start, page_end;

    UCLI_COMMAND_INFO (uc, "ptp-page", -1,
                       "ptp-page <page: 0xC0-0xCF>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if(! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
       return 0;
    }

    if (uc->pargs->count == 1) {
        page_start = strtoul (uc->pargs->args[0], NULL, 0);
        page_end = page_start;
    } else {
        page_start = 0xC0;
        page_end = 0xCF;
    }

    MASTER_I2C_LOCK;
    bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20);

    // List all pages.
    for (page = page_start; page <= page_end; page++) {
        bf_pltfm_cp2112_reg_write_byte(ptp_addr, 0xFD, page);

        for (byte = 0; byte < ptp_reg_size; byte ++) {
            bf_pltfm_cp2112_reg_read_block (ptp_addr, byte, &buf[byte], 1);
        }

        aim_printf (&uc->pvs, "\nPAGE: %02X ", page);
        for (byte = 0; byte < ptp_reg_size; byte++) {
            if ((byte % 16) == 0) {
                aim_printf (&uc->pvs, "\n0x%02X : ", byte);
            }
            aim_printf (&uc->pvs, "%02x ", buf[byte]);
        }
        aim_printf (&uc->pvs, "\n");
    }

    bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00);
    MASTER_I2C_UNLOCK;

    aim_printf (&uc->pvs, "\n");

    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__ptp_reg(ucli_context_t *uc)
{
    uint8_t page, reg, val = 0, regval = 0;
    bool write_op = false;

    UCLI_COMMAND_INFO(uc, "ptp-reg", -1,
                      "ptp-reg <page> <reg> [<val>]");

    if (!platform_type_equal(AFN_X732QT) ||
        !platform_subtype_equal(V2P0)) {
        aim_printf(&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (!(bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf(&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count == 2) {
        write_op = false;
    } else if (uc->pargs->count == 3) {
        write_op = true;
    } else {
        aim_printf(&uc->pvs, "ptp-reg <page> <reg> [<val>]\n");
        return 0;
    }

    page = strtoul(uc->pargs->args[0], NULL, 0);
    reg  = strtoul(uc->pargs->args[1], NULL, 0);

    if (write_op) {
        val = strtoul(uc->pargs->args[2], NULL, 0);
        if (bf_pltfm_read_ptp_reg(page, reg, &regval))
            return 0;
        if (bf_pltfm_write_ptp_reg(page, reg, val))
            return 0;
        if (bf_pltfm_read_ptp_reg(page, reg, &val))
            return 0;
        aim_printf(&uc->pvs, "PAGE=0x%02x, REG=0x%02x VAL=0x%02x -> 0x%02x\n",
                   page, reg, regval, val);
    } else {
        if (bf_pltfm_read_ptp_reg(page, reg, &regval))
            return 0;
        aim_printf(&uc->pvs, "PAGE=0x%02x, REG=0x%02x VAL=0x%02x\n",
                   page, reg, regval);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__ptp (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "ptp", -1,
                       "Summary the PTP module configuration and status");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot support on this device!\n");
        return 0;
    }

    if(! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
       return 0;
    }

    /* Production ID */
    uint16_t product_id = 0;
    bf_ptp_get_product_id(&product_id);
    aim_printf (&uc->pvs, "GENERAL_STATUS.PRODUCT_ID  : %x\n", product_id);

    /* Release Number */
    uint8_t major = 0, minor = 0, rev = 0;
    bf_ptp_get_release_no(&major, &minor, &rev);
    aim_printf (&uc->pvs, "GENERAL_STATUS.RELEASE     : v%x.%x.%x\n", major, minor, rev);

    /* Configuration of INPUT_0 and INPUT_1 */
    double input_0 = 0.0, input_1 = 0.0;
    bf_ptp_get_input_freq(0, &input_0);
    bf_ptp_get_input_freq(1, &input_1);
    aim_printf (&uc->pvs, "INPUT_0.IN_FREQ(OBSCLK 0)  : %.6f\n", input_0);
    aim_printf (&uc->pvs, "INPUT_1.IN_FREQ(OBSCLK 1)  : %.6f\n", input_1);
    aim_printf (&uc->pvs, "\n");

    /* SCRATCH3 from Asterfusion */
    uint8_t page, reg, regval;
    page = 0xCF;
    reg  = 0x50;
    bf_pltfm_read_ptp_reg(page, reg + 0x0E, &regval);
    aim_printf (&uc->pvs, "SCRATCH.SCRATCH3           : %x\n", regval);
    aim_printf (&uc->pvs, "\n");

    /* Status of DPLL5 (SyncE / Frequency) */
    uint8_t dpll5_ref = 0;
    uint8_t dpll5_status = 0;
    uint8_t dpll5_mode = 0;
    int32_t dpll5_phase = 0;
    double dpll5_phase_err = 0.0;

    bf_ptp_get_dpll5_ref_stat(&dpll5_ref);
    aim_printf (&uc->pvs, "STATUS.DPLL5_REF_STAT      : CLK%d\n", dpll5_ref);

    bf_ptp_get_dpll5_status(&dpll5_status);
    aim_printf (&uc->pvs, "STATUS.DPLL5_STATUS        : %x, %s\n", 
                dpll5_status, dpll_status_str(5, dpll5_status));

    bf_ptp_get_dpll5_phase_status(&dpll5_phase_err);
    aim_printf (&uc->pvs, "STATUS.DPLL5_PHASE_STATUS  : %.6f\n", dpll5_phase_err);

    bf_ptp_get_dpll5_mode(&dpll5_mode);
    aim_printf (&uc->pvs, "DPLL_5.DPLL_MODE           : %x, %s, %s\n", 
                dpll5_mode, dpll_mode_op_str(dpll5_mode), dpll_mode_sm_str(dpll5_mode));

    bf_ptp_get_dpll5_phase(&dpll5_phase);
    aim_printf (&uc->pvs, "DPLL_PHASE_5               : %d\n", dpll5_phase);

    bool dpll5_sync_en = false;
    uint8_t dpll5_sync_src = 0;
    bf_ptp_get_dpll_tod_sync_cfg(5, &dpll5_sync_en, &dpll5_sync_src);
    aim_printf (&uc->pvs, "DPLL_5.DPLL_TOD_SYNC_CFG   : %s, ToD Source: ToD%d\n", 
                dpll5_sync_en ? "Enabled" : "Disabled", dpll5_sync_src);
    aim_printf (&uc->pvs, "\n");


    /* Status of DPLL6 (PTP / Phase / Time-of-Day) */
    uint8_t dpll6_ref = 0;
    uint8_t dpll6_status = 0;
    uint8_t dpll6_mode = 0;
    int32_t dpll6_phase = 0;
    double dpll6_phase_err = 0.0;

    bf_ptp_get_dpll6_ref_stat(&dpll6_ref);
    aim_printf (&uc->pvs, "STATUS.DPLL6_REF_STAT      : CLK%d\n", dpll6_ref);

    bf_ptp_get_dpll6_status(&dpll6_status);
    aim_printf (&uc->pvs, "STATUS.DPLL6_STATUS        : %x, %s\n", 
                dpll6_status, dpll_status_str(6, dpll6_status));

    bf_ptp_get_dpll6_phase_status(&dpll6_phase_err);
    aim_printf (&uc->pvs, "STATUS.DPLL6_PHASE_STATUS  : %.6f\n", dpll6_phase_err);

    bf_ptp_get_dpll6_mode(&dpll6_mode);
    aim_printf (&uc->pvs, "DPLL_6.DPLL_MODE           : %x, %s, %s\n", 
                dpll6_mode, dpll_mode_op_str(dpll6_mode), dpll_mode_sm_str(dpll6_mode));

    bf_ptp_get_dpll6_phase(&dpll6_phase);
    aim_printf (&uc->pvs, "DPLL_PHASE_6               : %d\n", dpll6_phase);

    bool dpll6_sync_en = false;
    uint8_t dpll6_sync_src = 0;
    bf_ptp_get_dpll_tod_sync_cfg(6, &dpll6_sync_en, &dpll6_sync_src);
    aim_printf (&uc->pvs, "DPLL_6.DPLL_TOD_SYNC_CFG   : %s, ToD Source: ToD%d\n", 
                dpll6_sync_en ? "Enabled" : "Disabled", dpll6_sync_src);
    aim_printf (&uc->pvs, "\n");

    aim_printf(&uc->pvs, "\n=========================================================================================================\n");
    aim_printf(&uc->pvs, "                        8A34004 TIMING PLANE: TIME-OF-DAY (ToD) MULTI-AXIS MATRIX                       \n");
    aim_printf(&uc->pvs, "=========================================================================================================\n");
    aim_printf(&uc->pvs, "IDX |   CLOCK HEART    | ENABLE |   PPS MODE   | OUT_SYNC |  WRITE_CNTR  |      CAPTURED WALL-TIME (ToD)   \n");
    aim_printf(&uc->pvs, "---------------------------------------------------------------------------------------------------------\n");
    for (uint8_t idx = 0; idx < 4; idx++) {
        uint8_t clk_src = 0;
        uint8_t write_counter = 0;
        bool enabled = false, even_pps = false, sync_disabled = false;
        uint32_t seconds = 0, nanoseconds = 0;

        /* Step A API: Fetch live driving clock source (DPLL linkage) */
        if (bf_ptp_get_tod_clk_src(idx, &clk_src) != 0) {
            clk_src = 0;
        }

        /* Step B API: Fetch main engine configuration switches */
        if (bf_ptp_get_tod_cfg(idx, &enabled, &even_pps, &sync_disabled) != 0) {
            enabled = false; even_pps = false; sync_disabled = false;
        }

        /* Step C API: Fetch historical write completion metrics from CB block */
        if (bf_ptp_get_tod_write_counter(idx, &write_counter) != 0) {
            write_counter = 0;
        }

        /* Step D API: Execute atomic 10-byte burst snapshot capture across CC block */
        if (bf_ptp_get_tod_time(idx, &seconds, &nanoseconds) != 0) {
            seconds = 0; nanoseconds = 0;
        }

        /* Step E: Stream the fully resolved telemetry metrics row to the CLI */
        aim_printf(&uc->pvs, " %d  |  Driven by DPLL%d |  %-5s | %-12s | %-8s |    0x%02X    | %u.%09u sec\n",
                   idx,
                   clk_src,
                   enabled ? "TRUE" : "FALSE",
                   even_pps ? "2-Sec even" : "1-Sec std",
                   sync_disabled ? "DISABLE" : "ENABLE",
                   write_counter,
                   seconds, nanoseconds);
    }
    aim_printf (&uc->pvs, "\n");

    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__read_cpld (
    ucli_context_t *uc)
{
    int i = 0;
    uint8_t buf[BUFSIZ];
    uint8_t cpld_index;
    uint8_t cpld_page_size = 128;

    UCLI_COMMAND_INFO (uc, "read-cpld", 1,
                       "read-cpld <cpld_idx>");

    cpld_index = strtol (uc->pargs->args[0], NULL, 0);

    if ((cpld_index <= 0) ||
        (cpld_index > bf_pltfm_mgr_ctx()->cpld_count)) {
        aim_printf (&uc->pvs,
                    "invalid cpld_idx:%d, should be 1~%d\n",
                    cpld_index, bf_pltfm_mgr_ctx()->cpld_count);
        return -1;
    }

    for (i = 0; i < cpld_page_size; i ++) {
        if (bf_pltfm_cpld_read_byte (cpld_index, i,
                                        &buf[i])) {
            aim_printf (&uc->pvs,
                        "Failed to read CPLD.\n");
            break;
        }
    }
    /* Check total */
    if (i == cpld_page_size) {
        bf_pltfm_cpld_show (uc, cpld_index,
            buf, cpld_page_size);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__write_cpld (
    ucli_context_t *uc)
{
    uint8_t cpld_index;
    uint8_t offset;
    uint8_t val_wr, val_rd;

    UCLI_COMMAND_INFO (uc, "write-cpld", 3,
                       "write-cpld <cpld_idx> <offset> <val>");

    cpld_index = strtol (uc->pargs->args[0], NULL, 0);
    offset     = strtol (uc->pargs->args[1], NULL, 0);
    val_wr     = strtol (uc->pargs->args[2], NULL, 0);

    if ((cpld_index <= 0) ||
        (cpld_index > bf_pltfm_mgr_ctx()->cpld_count)) {
        aim_printf (&uc->pvs,
                    "invalid cpld_idx:%d, should be 1~%d\n",
                    cpld_index, bf_pltfm_mgr_ctx()->cpld_count);
        return -1;
    }

    if (bf_pltfm_cpld_write_byte (cpld_index, offset,
                                  val_wr)) {
        aim_printf (&uc->pvs,
                    "Failed to write CPLD.\n");
        return -1;
    }

    if (bf_pltfm_cpld_read_byte (cpld_index, offset,
                                 &val_rd)) {
        aim_printf (&uc->pvs,
                    "Failed to read CPLD.\n");
        return -1;
    }

    if (offset == 1) {
        if ((val_wr ^ val_rd) == 0xFF) {
            aim_printf (&uc->pvs,
                        "cpld%d write test register successfully\n",
                        cpld_index);
            return 0;
        } else {
            aim_printf (&uc->pvs,
                        "cpld%d write test register failure\n",
                        cpld_index);
            return -1;
        }
    } else {
        if (val_wr == val_rd) {
            aim_printf (&uc->pvs,
                        "cpld write successfully\n");
            return 0;
        } else {
            aim_printf (&uc->pvs,
                        "cpld write failure, maybe read-only or invalid address\n");
            return -1;
        }
    }
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__decode_cpld (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "decode-cpld", 0, "decode-cpld");

    if (platform_type_equal (AFN_X532PT)) {
        bf_pltfm_cpld_decode_x532p (uc);
    } else if (platform_type_equal (AFN_X564PT)) {
        bf_pltfm_cpld_decode_x564p (uc);
    } else if (platform_type_equal (AFN_X308PT)) {
        bf_pltfm_cpld_decode_x308p (uc);
    } else if (platform_type_equal (AFN_X732QT)) {
        bf_pltfm_cpld_decode_x732q (uc);
    }

    return 0;
}

static ucli_command_handler_f
bf_pltfm_cpld_ucli_ucli_handlers__[] = {
    bf_pltfm_cpld_ucli_ucli__read_cpld,
    bf_pltfm_cpld_ucli_ucli__write_cpld,
    bf_pltfm_cpld_ucli_ucli__decode_cpld,

    // ptp on x732q-t-ptp.
    bf_pltfm_cpld_ucli_ucli__set_clk,
    bf_pltfm_cpld_ucli_ucli__tx_rst_pulse,
    bf_pltfm_cpld_ucli_ucli__clkobs_strength_set__,

    // debug
    bf_pltfm_cpld_ucli_ucli__bsync,
    bf_pltfm_cpld_ucli_ucli__devts,
    bf_pltfm_cpld_ucli_ucli__devts_cnt,
    bf_pltfm_cpld_ucli_ucli__bsync_cnt,
    bf_pltfm_cpld_ucli_ucli__ptp_page,
    bf_pltfm_cpld_ucli_ucli__ptp_reg,
    bf_pltfm_cpld_ucli_ucli__ptp,

    NULL
};

static ucli_module_t bf_pltfm_cpld_ucli_module__
= {
    "bf_pltfm_cpld_ucli",
    NULL,
    bf_pltfm_cpld_ucli_ucli_handlers__,
    NULL,
    NULL,
};

ucli_node_t *bf_pltfm_cpld_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_cpld_ucli_module__);
    n = ucli_node_create ("cpld", m,
                          &bf_pltfm_cpld_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("cpld"));
    return n;
}
