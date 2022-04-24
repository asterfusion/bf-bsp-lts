/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_bd_cfg/bf_bd_cfg.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_qsfp.h>
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_pres (
    ucli_context_t *uc)
{
    uint32_t lower_ports, upper_ports, cpu_ports;

    UCLI_COMMAND_INFO (uc, "get-pres", 0, "get-pres");
    if (bf_qsfp_get_transceiver_pres (&lower_ports,
                                      &upper_ports, &cpu_ports)) {
        aim_printf (&uc->pvs,
                    "error getting qsfp presence\n");
        return 0;
    }
    aim_printf (&uc->pvs,
                "qsfp presence lower: 0x%08x upper: 0x%08x cpu: 0x%08x\n",
                lower_ports,
                upper_ports,
                cpu_ports);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_int (ucli_context_t
                                  *uc)
{
    uint32_t lower_ports, upper_ports, cpu_ports;

    UCLI_COMMAND_INFO (uc, "get-int", 0, "get-int");
    bf_qsfp_get_transceiver_int (&lower_ports,
                                 &upper_ports, &cpu_ports);
    aim_printf (&uc->pvs,
                "qsfp interrupt lower: 0x%08x upper: 0x%08x cpu: 0x%08x\n",
                lower_ports,
                upper_ports,
                cpu_ports);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_lpmode (
    ucli_context_t *uc)
{
    uint32_t lower_ports, upper_ports, cpu_ports;

    UCLI_COMMAND_INFO (uc, "get-lpmode", 0,
                       "get-lpmode");
    if (bf_qsfp_get_transceiver_lpmode (&lower_ports,
                                        &upper_ports, &cpu_ports)) {
        aim_printf (&uc->pvs,
                    "error getting the lpmode\n");
    } else {
        aim_printf (&uc->pvs,
                    "qsfp lpmode lower: 0x%08x upper: 0x%08x cpu: 0x%08x\n",
                    lower_ports,
                    upper_ports,
                    cpu_ports);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_set_lpmode (
    ucli_context_t *uc)
{
    int port, lpmode;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (
        uc, "set-lpmode", 2,
        "set-lpmode <port> <1: lpmode 0 : no lpmode>");
    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }
    lpmode = atoi (uc->pargs->args[1]);
    if (bf_qsfp_set_transceiver_lpmode (port,
                                        (lpmode ? true : false))) {
        aim_printf (&uc->pvs,
                    "error setting the lpmode\n");
    } else {
        aim_printf (&uc->pvs,
                    "qsfp port %d lpmode set to %d\n", port, lpmode);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_init (ucli_context_t
                               *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-init", 0,
                       "qsfp-init");
    bf_pltfm_qsfp_init (NULL);
    return 0;
}

#if 0
// This hangs. Disable until debugged - TBD
static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_xver_info (
    ucli_context_t *uc)
{
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    qsfp_transciever_info_t info;

    UCLI_COMMAND_INFO (uc, "get-xver-info", 1,
                       "get-xver-info <port>");
    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    bf_qsfp_get_transceiver_info (port, &info);

    return 0;
}
#endif

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_tx_disable_set (
    ucli_context_t *uc)
{
    int port, ch, val, max_media_ch;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (
        uc, "tx-disable-set", 3,
        "tx-disable-set <port> <ch> <val,0-disable, 1-enable>");
    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }
    ch = atoi (uc->pargs->args[1]);
    max_media_ch = bf_qsfp_get_media_ch_cnt (port);
    if ((ch < 0) || (ch > max_media_ch)) {
        aim_printf (&uc->pvs, "ch must be 0-%d\n",
                    max_media_ch);
        return 0;
    }
    val = atoi (uc->pargs->args[2]);
    if ((val < 0) || (val > 1)) {
        aim_printf (&uc->pvs,
                    "val must be 0-1 for port %d\n", port);
        return 0;
    }

    if (val) {
        qsfp_fsm_ch_enable (0, port, ch);
    } else {
        qsfp_fsm_ch_disable (0, port, ch);
    }
    return 0;
}

ucli_context_t *dump_uc;
void dump_qsfp_oper_array (uint8_t *arr)
{
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    for (port = 1; port <= max_port; port++) {
        if (arr[port]) {
            aim_printf (&dump_uc->pvs, "%2x|", arr[port]);
        } else {
            aim_printf (&dump_uc->pvs, "  |");
        }
    }
    aim_printf (&dump_uc->pvs, "\n");
}

static ucli_status_t qsfp_dump_pg0_info (
    ucli_context_t *uc, bool detail)
{
    int port;
    int first_port = 1;
    int last_port = bf_qsfp_get_max_qsfp_ports();
    uint8_t pg0_lower[128], pg0_upper[128];
    bool present[66], pg3_present;
    uint8_t pg3[128];
    uint8_t intl[66] = {0};
    uint8_t dnr[66] = {0};
    uint8_t tx_los[66] = {0};
    uint8_t rx_los[66] = {0};
    uint8_t tx_eq_flt[66] = {0};
    uint8_t rx_eq_flt[66] = {0};
    uint8_t tx_lol[66] = {0};
    uint8_t rx_lol[66] = {0};
    uint8_t tx_dis[66] = {0};
    uint8_t pwr[66] = {0};
    uint8_t tx_cdr_en[66] = {0};
    uint8_t rx_cdr_en[66] = {0};
    uint8_t out_emph_lo[66] = {0};
    uint8_t out_emph_hi[66] = {0};
    uint8_t in_eq_lo[66] = {0};
    uint8_t in_eq_hi[66] = {0};
    uint8_t out_amp_lo[66] = {0};
    uint8_t out_amp_hi[66] = {0};

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        if (port < 1 || port > last_port) {
            aim_printf (&uc->pvs, "port must be 1-%d\n",
                        last_port);
            return 0;
        }
        first_port = last_port = port;
    }
    dump_uc = uc;  // hack

    for (port = first_port; port <= last_port;
         port++) {
        int byte;

        qsfp_oper_info_get (port, &present[port],
                            pg0_lower, pg0_upper);

        if (!present[port]) {
            continue;
        }

        qsfp_oper_info_get_pg3 (port, &pg3_present, pg3);

        if (detail) {
            aim_printf (&uc->pvs, "\nQSFP %d:\n", port);
        }
        // dump ports QSFP oper state
        if (1 || pg0_lower[2] & 0x3) {
            if (detail) {
                aim_printf (
                    &uc->pvs, "Byte 2: [1:1]: %d : IntL\n",
                    (pg0_lower[2] >> 1) & 1);
                aim_printf (&uc->pvs,
                            "Byte 2: [0:0]: %d : Data_Not_Ready\n",
                            (pg0_lower[2] >> 0) & 1);
            }
            intl[port] = (pg0_lower[2] >> 1) & 1;
            dnr[port] = (pg0_lower[2] >> 0) & 1;
        }
        if (1 || pg0_lower[3] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 3: [7:4]: %01x : L-Tx4-1 LOS\n",
                            (pg0_lower[3] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 3: [3:0]: %01x : L-Rx4-1 LOS\n",
                            (pg0_lower[3] >> 0) & 0xF);
            }
            tx_los[port] = (pg0_lower[3] >> 4) & 0xF;
            rx_los[port] = (pg0_lower[3] >> 0) & 0xF;
        }
        if (1 || pg0_lower[4] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 4: [7:4]: %01x : L-Tx4-1 Adapt EQ Fault\n",
                            (pg0_lower[4] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 4: [3:0]: %01x : L-Rx4-1 Adapt EQ Fault\n",
                            (pg0_lower[4] >> 0) & 0xF);
            }
            tx_eq_flt[port] = (pg0_lower[4] >> 4) & 0xF;
            rx_eq_flt[port] = (pg0_lower[4] >> 0) & 0xF;
        }
        if (1 || pg0_lower[5] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 5: [7:4]: %01x : L-Tx4-1 LOL\n",
                            (pg0_lower[5] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 5: [3:0]: %01x : L-Rx4-1 LOL\n",
                            (pg0_lower[5] >> 0) & 0xF);
            }
            tx_lol[port] = (pg0_lower[5] >> 4) & 0xF;
            rx_lol[port] = (pg0_lower[5] >> 0) & 0xF;
        }
        if (1 || pg0_lower[86] & 0xF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 86: [3:0]: %01x : Tx4-1 Disable\n",
                            (pg0_lower[86] >> 0) & 0xF);
            }
            tx_dis[port] = (pg0_lower[86] >> 0) & 0xF;
        }
        if (1 || pg0_lower[93] & 0xFF) {
            if (detail) {
                aim_printf (
                    &uc->pvs,
                    "Byte 93: [2:2]: %01x : High Power Class Enable (Classes 5-7)\n",
                    (pg0_lower[93] >> 2) & 0x1);
                aim_printf (&uc->pvs,
                            "Byte 93: [1:1]: %01x : Power set\n",
                            (pg0_lower[93] >> 1) & 0x1);
                aim_printf (&uc->pvs,
                            "Byte 93: [0:0]: %01x : Power override\n",
                            (pg0_lower[93] >> 0) & 0x1);
            }
            pwr[port] = pg0_lower[93] & 0x7;
        }
        if (1 || pg0_lower[98] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Byte 98: [7:4]: %01x : Tx4-1_CDR_control\n",
                            (pg0_lower[98] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Byte 98: [3:0]: %01x : Rx4-1_CDR_control\n",
                            (pg0_lower[98] >> 0) & 0xF);
            }
            tx_cdr_en[port] = (pg0_lower[98] >> 4) & 0xF;
            rx_cdr_en[port] = (pg0_lower[98] >> 0) & 0xF;
        }

        // Page 03 info
        if (1 || pg3[234 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 234: [7:4]: %01x : Input Eq (dB)\n",
                            (pg3[234 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 234: [3:0]: %01x : Input Eq (dB)\n",
                            (pg3[234 - 128] >> 0) & 0xF);
            }
            in_eq_lo[port] = (pg3[234 - 128] >> 4) & 0xF;
            in_eq_lo[port] = (pg3[234 - 128] >> 0) & 0xF;
        }
        if (1 || pg3[235 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 235: [7:4]: %01x : Input Eq (dB)\n",
                            (pg3[235 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 235: [3:0]: %01x : Input Eq (dB)\n",
                            (pg3[235 - 128] >> 0) & 0xF);
            }
            in_eq_hi[port] = (pg3[235 - 128] >> 4) & 0xF;
            in_eq_hi[port] = (pg3[235 - 128] >> 0) & 0xF;
        }

        if (1 || pg3[236 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 236: [7:4]: %01x : Output Emphasis (dB)\n",
                            (pg3[236 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 236: [3:0]: %01x : Output Emphasis (dB)\n",
                            (pg3[236 - 128] >> 0) & 0xF);
            }
            out_emph_lo[port] = (pg3[236 - 128] >> 4) & 0xF;
            out_emph_lo[port] = (pg3[236 - 128] >> 0) & 0xF;
        }
        if (1 || pg3[237 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 237: [7:4]: %01x : Output Emphasis (dB)\n",
                            (pg3[237 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 237: [3:0]: %01x : Output Emphasis (dB)\n",
                            (pg3[237 - 128] >> 0) & 0xF);
            }
            out_emph_hi[port] = (pg3[237 - 128] >> 4) & 0xF;
            out_emph_hi[port] = (pg3[237 - 128] >> 0) & 0xF;
        }

        if (1 || pg3[238 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 238: [7:4]: %01x : Output Amplitude (dB)\n",
                            (pg3[238 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 238: [3:0]: %01x : Output Amplitude (dB)\n",
                            (pg3[238 - 128] >> 0) & 0xF);
            }
            out_amp_lo[port] = (pg3[238 - 128] >> 4) & 0xF;
            out_amp_lo[port] = (pg3[238 - 128] >> 0) & 0xF;
        }
        if (1 || pg3[239 - 128] & 0xFF) {
            if (detail) {
                aim_printf (&uc->pvs,
                            "Pg3 Byte 239: [7:4]: %01x : Output Emphasis (dB)\n",
                            (pg3[239 - 128] >> 4) & 0xF);
                aim_printf (&uc->pvs,
                            "Pg3 Byte 239: [3:0]: %01x : Output Emphasis (dB)\n",
                            (pg3[239 - 128] >> 0) & 0xF);
            }
            out_amp_hi[port] = (pg3[239 - 128] >> 4) & 0xF;
            out_amp_hi[port] = (pg3[239 - 128] >> 0) & 0xF;
        }

        if (detail) {
            aim_printf (&uc->pvs, "\nPage 0:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg0_lower[byte]);
            }
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg0_upper[byte]);
            }
            aim_printf (&uc->pvs, "\nPage 3:\n");
            for (byte = 0; byte < 128; byte++) {
                if ((byte % 16) == 0) {
                    aim_printf (&uc->pvs, "\n%3d : ", 128 + byte);
                }
                aim_printf (&uc->pvs, "%02x ", pg3[byte]);
            }
            aim_printf (&uc->pvs, "\n");
        }
    }
    aim_printf (&uc->pvs,
                "                                  :                            1 "
                " 1  1  1  1  1  1  1  1  1  2  2  2  2  2  2  2  2  2  2  3  3  "
                "3  3  3  3  3  3  3  3  4  4  4  4  4  4  4  4  4  4  5  5  5  5 "
                " 5  5  5  5  5  5  6  6  6  6  6  6\n");
    aim_printf (&uc->pvs,
                "Byte Bit(s) Field                 : 1  2  3  4  5  6  7  8  9  0 "
                " 1  2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  "
                "2  3  4  5  6  7  8  9  0  1  2  3  4  5  6  7  8  9  0  1  2  3 "
                " 4  5  6  7  8  9  0  1  2  3  4  5\n");
    aim_printf (&uc->pvs,
                "                                  "
                ":---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+"
                "--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--"
                "+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+-"
                "-+\n");
    aim_printf (&uc->pvs,
                "  2: [1:1]: IntL                  : ");
    dump_qsfp_oper_array (intl);
    aim_printf (&uc->pvs,
                "  2: [0:0]: Data_Not_Ready        : ");
    dump_qsfp_oper_array (dnr);
    aim_printf (&uc->pvs,
                "  3: [7:4]: L-Tx4-1 LOS           : ");
    dump_qsfp_oper_array (tx_los);
    aim_printf (&uc->pvs,
                "  3: [3:0]: L-Rx4-1 LOS           : ");
    dump_qsfp_oper_array (rx_los);
    aim_printf (&uc->pvs,
                "  4: [7:4]: L-Tx4-1 Adapt EQ Fault: ");
    dump_qsfp_oper_array (tx_eq_flt);
    aim_printf (&uc->pvs,
                "  4: [3:0]: L-Rx4-1 Adapt EQ Fault: ");
    dump_qsfp_oper_array (rx_eq_flt);
    aim_printf (&uc->pvs,
                "  5: [7:4]: L-Tx4-1 LOL           : ");
    dump_qsfp_oper_array (tx_lol);
    aim_printf (&uc->pvs,
                "  5: [3:0]: L-Rx4-1 LOL           : ");
    dump_qsfp_oper_array (rx_lol);
    aim_printf (&uc->pvs,
                " 86: [3:0]: Tx4-1 Disable         : ");
    dump_qsfp_oper_array (tx_dis);
    aim_printf (&uc->pvs,
                " 93: [1:1]: Power                 : ");
    dump_qsfp_oper_array (pwr);
    aim_printf (&uc->pvs,
                " 98: [7:4]: Tx4-1_CDR_control     : ");
    dump_qsfp_oper_array (tx_cdr_en);
    aim_printf (&uc->pvs,
                " 98: [3:0]: Rx4-1_CDR_control     : ");
    dump_qsfp_oper_array (rx_cdr_en);
    aim_printf (&uc->pvs,
                "234: [7:0]: Input Eq    (dB)      : ");
    dump_qsfp_oper_array (in_eq_lo);
    aim_printf (&uc->pvs,
                "235: [7:0]: Input Eq    (dB)      : ");
    dump_qsfp_oper_array (in_eq_hi);
    aim_printf (&uc->pvs,
                "236: [7:0]: Output Emph (dB)      : ");
    dump_qsfp_oper_array (out_emph_lo);
    aim_printf (&uc->pvs,
                "237: [7:0]: Output Emph (dB)      : ");
    dump_qsfp_oper_array (out_emph_hi);
    aim_printf (&uc->pvs,
                "238: [7:0]: Output Amp  (dB)      : ");
    dump_qsfp_oper_array (out_amp_lo);
    aim_printf (&uc->pvs,
                "239: [7:0]: Output Amp  (dB)      : ");
    dump_qsfp_oper_array (out_amp_hi);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_pg0 (ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "pg0", -1, "pg0 [port]");
    qsfp_dump_pg0_info (uc, true /*detail*/);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_oper (ucli_context_t
                               *uc)
{
    UCLI_COMMAND_INFO (uc, "oper", -1, "oper [port]");
    qsfp_dump_pg0_info (uc, false /*summary*/);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_pg3 (ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "pg3", -1, "pg3 [port]");

    return 0;
}

uint8_t *sfp_string (uint8_t *buf, size_t offset,
                     size_t len)
{
    uint8_t *start = buf + offset;
    while (len > 0 && start[len - 1] == ' ') {
        --len;
    }
    * (start + len - 1) = 0; /* NULL terminate it */

    return start;
}

/* copy  len bytes from src+offset to dst and adds a NULL to dst
 * dst must have len+1 space in it
 */
static void sfp_copy_string (uint8_t *dst,
                             uint8_t *src,
                             size_t offset,
                             size_t len)
{
    memcpy (dst, src + offset, len);
    dst[len] = '\0';
}

void printChannelMonitor (ucli_context_t *uc,
                          unsigned int index,
                          const uint8_t *buf,
                          unsigned int rxMSB,
                          unsigned int rxLSB,
                          unsigned int txMSB,
                          unsigned int txLSB,
                          unsigned int txpwrMSB,
                          unsigned int txpwrLSB)
{
    /* Notes about DDM register values:
        - MSB in lower byte
        - For Tx/Rx power, each unit is 1uW
        - Converting to dBm is 10*log( mW)
     */
    uint16_t rxValue = (buf[rxMSB] << 8) | buf[rxLSB];
    uint16_t txValue = (buf[txMSB] << 8) | buf[txLSB];
    uint16_t txpwrValue = (buf[txpwrMSB] << 8) |
                          buf[txpwrLSB];

    /* RX power ranges from 0mW to 6.5535mW*/
    double rxPower = 10 * log10 (0.0001 * rxValue);

    /* TX power ranges from 0mW to 6.5535mW */
    double txPower = 10 * log10 (0.0001 * txpwrValue);

    /* TX bias ranges from 0mA to 131mA */
    double txBias = (131.0 * txValue) / 65535;

    aim_printf (&uc->pvs,
                "    Channel %d:   %9.2fdBm  %9.2fdBm  %10.2fmA\n",
                index,
                rxPower,
                txPower,
                txBias);
}

void printDdm (ucli_context_t *uc,
               unsigned int port, const uint8_t *buf)
{
    double temp;
    double volt;
    double txBias;
    double txPower;
    double rxPower;
    uint16_t txBiasValue;
    uint16_t txValue;
    uint16_t rxValue;

    temp = (buf[22]) + (buf[23] / 256.0);
    volt = ((buf[26] << 8) | buf[27]) / 10000.0;

    for (int i = 0; i < 4; i++) {
        txBiasValue = (buf[42 + i * 2] << 8) | buf[43 + i
                      * 2];
        txValue = (buf[50 + i * 2] << 8) | buf[51 + i *
                                               2];
        rxValue = (buf[34 + i * 2] << 8) | buf[35 + i *
                                               2];

        txBias = (131.0 * txBiasValue) / 65535;
        txPower = 10 * log10 (0.0001 * txValue);
        rxPower = 10 * log10 (0.0001 * rxValue);

        aim_printf (&uc->pvs,
                    "      %d/%d %10.1f %15.2f %10.2f %16.2f %16.2f\n",
                    port,
                    i,
                    temp,
                    volt,
                    txBias,
                    txPower,
                    rxPower);
    }
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_get_ddm (ucli_context_t
                                  *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port_start, port_end;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (uc, "get-ddm", 2,
                       "get-ddm <port_start> <max_port>");
    port_start = atoi (uc->pargs->args[0]);
    port_end = atoi (uc->pargs->args[1]);

    if (port_start < 1 || port_start > max_port) {
        aim_printf (&uc->pvs, "port_start must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (port_end < 1 || port_end > max_port) {
        aim_printf (&uc->pvs, "max_port must be 1-%d\n",
                    max_port);
        return 0;
    }

    aim_printf (
        &uc->pvs, "QSFP DDM Info for ports %d to %d\n",
        port_start, port_end);
    aim_printf (&uc->pvs,
                "%10s %10s %15s %10s %16s %16s\n",
                "-------",
                "--------",
                "-----------",
                "--------",
                "--------------",
                "--------------");
    aim_printf (&uc->pvs,
                "%10s %10s %15s %10s %16s %16s\n",
                "Port/Ch",
                "Temp (C)",
                "Voltage (V)",
                "Bias(mA)",
                "Tx Power (dBm)",
                "Rx Power (dBm)");
    aim_printf (&uc->pvs,
                "%10s %10s %15s %10s %16s %16s\n",
                "-------",
                "--------",
                "-----------",
                "--------",
                "--------------",
                "--------------");

    for (int i = port_start; i <= port_end; i++) {
        if (!bf_qsfp_is_present (i)/* ||
            (!bf_qsfp_is_optical (i))*/) {
            continue;
        }

        bf_qsfp_update_data (i);
        if (bf_qsfp_get_cached_info (i, QSFP_PAGE0_LOWER,
                                     buf)) {
            continue;  // probably not present
        }
        if (bf_qsfp_get_cached_info (
                i, QSFP_PAGE0_UPPER, buf + MAX_QSFP_PAGE_SIZE)) {
            continue;  // probably not present
        }

        printDdm (uc, i, buf);
        aim_printf (&uc->pvs,
                    "%10s %10s %15s %10s %16s %16s\n",
                    "-------",
                    "--------",
                    "-----------",
                    "--------",
                    "--------------",
                    "--------------");
    }

    return 0;
}

static void qsfp_type_to_display_get (
    bf_pltfm_qsfp_type_t qsfp_type,
    char *str,
    int len)
{
    switch (qsfp_type) {
        case BF_PLTFM_QSFP_CU_0_5_M:
            strcpy (str, "Copper 0.5 m");
            break;
        case BF_PLTFM_QSFP_CU_1_M:
            strcpy (str, " Copper 1 m ");
            break;
        case BF_PLTFM_QSFP_CU_2_M:
            strcpy (str, " Copper 2 m ");
            break;
        case BF_PLTFM_QSFP_CU_3_M:
            strcpy (str, " Copper 3 m ");
            break;
        case BF_PLTFM_QSFP_CU_LOOP:
            strcpy (str, " Copper Loop");
            break;
        case BF_PLTFM_QSFP_OPT:
            strcpy (str, "   Optical  ");
            break;
        default:
            strcpy (str, "   Unknown  ");
    }
    str[len - 1] = '\0';
}
char *qsfp_power_class (uint8_t B129)
{
    if ((B129 & 0x3) == 3) {
        return " 7 (5.0 W max.)";
    }
    if ((B129 & 0x3) == 2) {
        return " 6 (4.5 W max.)";
    }
    if ((B129 & 0x3) == 1) {
        return " 5 (4.0 W max.)";
    }
    if ((B129 & 0xC0) == 0xC0) {
        return " 4 (3.5 W max.)";
    }
    if ((B129 & 0xC0) == 0x80) {
        return " 3 (2.5 W max.)";
    }
    if ((B129 & 0xC0) == 0x40) {
        return " 2 (2.0 W max.)";
    }
    if ((B129 & 0xC0) == 0x00) {
        return " 1 (1.5 W max.)";
    }
    return "...           ";
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_summary (ucli_context_t
                                  *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port;
    uint8_t conn_serial_nbr[BF_PLAT_MAX_QSFP][16] = {{0}};
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (uc, "info", 0, "info");

    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Info for QSFP  :\n");
    aim_printf (&uc->pvs, "================\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "----------------------------------------------------\n");
    aim_printf (&uc->pvs,
                "                                                             "
                "Date     Nominal                 Power\n");
    aim_printf (&uc->pvs,
                "Port  Vendor           PN               rev Serial#          "
                "code     Bit Rate     OUI        Class            Media\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "----------------------------------------------------\n");

    memset ((char *)conn_serial_nbr, 0,
            sizeof (conn_serial_nbr));

    for (port = 1; port <= max_port; port++) {
        if (!bf_qsfp_is_present (port)) {
            continue;
        }

        bf_qsfp_update_data (port);

        if (bf_qsfp_get_cached_info (port,
                                     QSFP_PAGE0_LOWER, buf)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (
                port, QSFP_PAGE0_UPPER,
                buf + MAX_QSFP_PAGE_SIZE)) {
            continue;
        }

        /* print lower page data */
        aim_printf (&uc->pvs, " %2d:  ", port);
        uint8_t vendor[17];
        uint8_t vendor_pn[17];
        uint8_t vendor_rev[3];
        uint8_t vendor_sn[17];
        uint8_t vendor_date[9];

        sfp_copy_string (vendor, buf, 148, 16);
        sfp_copy_string (vendor_pn, buf, 168, 16);
        sfp_copy_string (vendor_rev, buf, 184, 2);
        sfp_copy_string (vendor_sn, buf, 196, 16);
        sfp_copy_string (vendor_date, buf, 212, 8);

        // save serial # for connection check
        memcpy (conn_serial_nbr[port], vendor_sn, 16);

        aim_printf (&uc->pvs, "%s ", vendor);
        aim_printf (&uc->pvs, "%s ", vendor_pn);
        aim_printf (&uc->pvs, "%s  ", vendor_rev);
        aim_printf (&uc->pvs, "%s ", vendor_sn);
        aim_printf (&uc->pvs, "%s ", vendor_date);
        aim_printf (&uc->pvs, "%5d MBps ",
                    buf[140] * 100);
        aim_printf (&uc->pvs,
                    "  %02x:%02x:%02x",
                    buf[165 - 128],
                    buf[166 - 128],
                    buf[167 - 128]);
        aim_printf (&uc->pvs, "  %s ",
                    qsfp_power_class (buf[129]));
        if (buf[146]) {
            aim_printf (&uc->pvs, "  (Copper): %d m ",
                        buf[146]);
        }
        if (buf[142]) {
            aim_printf (&uc->pvs, "  (SMF): %d km ",
                        buf[142]);
        }
        if (buf[143]) {
            aim_printf (&uc->pvs, "  (OM3): %d m ",
                        buf[143] * 2);
        }
        if (buf[144]) {
            aim_printf (&uc->pvs, "  (OM2): %d m ",
                        buf[144]);
        }
        if (buf[145]) {
            aim_printf (&uc->pvs, "  (OM1): %d m ",
                        buf[145]);
        }
        aim_printf (&uc->pvs, "\n");
    }

    // dump any local connections
    aim_printf (&uc->pvs, "\nLocal connections:\n");
    for (port = 1; port <= max_port; port++) {
        int other_port;
        for (other_port = port + 1; other_port < max_port;
             other_port++) {
            // skip missing qsfps
            if (conn_serial_nbr[port][0] == 0) {
                continue;
            }
            if (conn_serial_nbr[other_port][0] == 0) {
                continue;
            }

            if (strncmp ((char *)conn_serial_nbr[port],
                         (char *)conn_serial_nbr[other_port],
                         16) == 0) {
                aim_printf (&uc->pvs, " Port %2d <--> Port %2d\n",
                            port, other_port);
            }
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_show (ucli_context_t
                               *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();
    uint8_t vendor[17];
    uint8_t vendor_pn[17];
    char optical_lstr[16];
    bf_pltfm_qsfp_type_t qsfp_type =
        BF_PLTFM_QSFP_UNKNOWN;
    char display_qsfp_type[13];

    UCLI_COMMAND_INFO (uc, "show", 0,
                       "show qsfp or qsfp-dd summary information");

    aim_printf (&uc->pvs,
                "Max ports supported ==> %2d", max_port);
    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Show for QSFP  :\n");
    aim_printf (&uc->pvs, "================\n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    aim_printf (&uc->pvs,
                "port  id    vendor           part_num       eth  ext-eth Om-len "
                "Cu-Len  Qsfp-type\n");
    aim_printf (&uc->pvs,
                "                                           cmpl   cmpl"
                "               \n");
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");
    for (port = 1; port <= max_port; port++) {
        if (!bf_qsfp_is_present (port)) {
            continue;
        }

        if (bf_qsfp_get_cached_info (port,
                                     QSFP_PAGE0_LOWER, buf)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (
                port, QSFP_PAGE0_UPPER,
                buf + MAX_QSFP_PAGE_SIZE)) {
            continue;
        }
#if 0		
        if (bf_qsfp_is_cmis (port)) {
            continue;
        }
#endif
        sfp_copy_string (vendor, buf, 148, 16);
        sfp_copy_string (vendor_pn, buf, 168, 16);
        /* determine optical domain length to display */
        if (buf[142]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "SMF %d km", buf[142]);
        } else if (buf[143]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "OM3 %d m", buf[143] * 2);
        } else if (buf[144]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "OM2 %d m", buf[144]);
        } else if (buf[145]) {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "OM1 %d m", buf[145]);
        } else {
            snprintf (optical_lstr, sizeof (optical_lstr),
                      "--0--");
        }
        /* get qsfp_type */
        if (bf_qsfp_type_get (port, &qsfp_type) != 0) {
            qsfp_type = BF_PLTFM_QSFP_UNKNOWN;
        }
        qsfp_type_to_display_get (
            qsfp_type, display_qsfp_type,
            sizeof (display_qsfp_type));
        /* now display the stuff */
        aim_printf (
            &uc->pvs, " %2d  0x%02x %16s %16s", port, buf[0],
            vendor, vendor_pn);
        aim_printf (&uc->pvs,
                    "0x%02x  0x%02x %10s  %d m",
                    buf[131],
                    buf[192],
                    optical_lstr,
                    buf[146]);
        aim_printf (&uc->pvs, " %s\n", display_qsfp_type);
    }
    aim_printf (&uc->pvs,
                "-----------------------------------------------------------------"
                "---------------\n");

    //bf_pltfm_ucli_ucli__qsfpdd_show (uc);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_dump_info (
    ucli_context_t *uc)
{
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2];
    int port;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (uc, "dump-info", 1,
                       "dump-info <port>");
    port = atoi (uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }
    if (!bf_qsfp_is_present (port)) {
        return 0;
    }

    aim_printf (&uc->pvs, "%-30s = %d\n", "Port",
                port);
    bf_qsfp_update_data (port);

    if (bf_qsfp_get_cached_info (port,
                                 QSFP_PAGE0_LOWER, buf)) {
        aim_printf (&uc->pvs,
                    "error getting lower page data\n");
        return 0;
    }
    if (bf_qsfp_get_cached_info (
            port, QSFP_PAGE0_UPPER,
            buf + MAX_QSFP_PAGE_SIZE)) {
        aim_printf (&uc->pvs,
                    "error getting upper page data\n");
        return 0;
    }

    /* print lower page data */
    aim_printf (&uc->pvs, "Port %d\n", port);
    aim_printf (&uc->pvs, "  ID: %#04x\n", buf[0]);
    aim_printf (&uc->pvs, "  Status: 0x%02x 0x%02x\n",
                buf[1], buf[2]);

    aim_printf (&uc->pvs, "  Interrupt Flags:\n");
    aim_printf (&uc->pvs, "    LOS: 0x%02x\n",
                buf[3]);
    aim_printf (&uc->pvs, "    Fault: 0x%02x\n",
                buf[4]);
    aim_printf (&uc->pvs, "    Temp: 0x%02x\n",
                buf[6]);
    aim_printf (&uc->pvs, "    Vcc: 0x%02x\n",
                buf[7]);
    aim_printf (&uc->pvs,
                "    Rx Power: 0x%02x 0x%02x\n", buf[9], buf[10]);
    aim_printf (&uc->pvs,
                "    Tx Bias: 0x%02x 0x%02x\n", buf[11], buf[12]);
    aim_printf (&uc->pvs,
                "    Reserved Set 3: 0x%02x 0x%02x\n", buf[13],
                buf[14]);
    aim_printf (&uc->pvs,
                "    Reserved Set 4: 0x%02x 0x%02x\n", buf[15],
                buf[16]);
    aim_printf (&uc->pvs,
                "    Reserved Set 5: 0x%02x 0x%02x\n", buf[17],
                buf[18]);
    aim_printf (&uc->pvs,
                "    Vendor Defined: 0x%02x 0x%02x 0x%02x\n",
                buf[19],
                buf[20],
                buf[21]);

    double temp = (buf[22]) + (buf[23] / 256.0);
    aim_printf (&uc->pvs, "  Temperature: %f C\n",
                temp);
    double voltage = (buf[26] << 8) | buf[27];
    aim_printf (&uc->pvs, "  Supply Voltage: %f V\n",
                voltage / 10000.0);

    aim_printf (&uc->pvs,
                "  Channel Data:  %12s     %9s   %11s\n",
                "RX Power",
                "TX Power",
                "TX Bias");
    printChannelMonitor (uc, 1, buf, 34, 35, 42, 43,
                         50, 51);
    printChannelMonitor (uc, 2, buf, 36, 37, 44, 45,
                         52, 53);
    printChannelMonitor (uc, 3, buf, 38, 39, 46, 47,
                         54, 55);
    printChannelMonitor (uc, 4, buf, 40, 41, 48, 49,
                         56, 57);
    aim_printf (&uc->pvs,
                "    Reported RX Power is %s\n",
                (buf[220] & 0x08) ? "average power" : "OMA");

    aim_printf (&uc->pvs,
                "  Power set:  0x%02x\tExtended ID:  0x%02x\t"
                "Ethernet Compliance:  0x%02x\n"
                "  Extended Ethernet Compliance:  0x%02x\n",
                buf[93],
                buf[129],
                buf[131],
                buf[192]);
    aim_printf (&uc->pvs,
                "  TX disable bits: 0x%02x\n", buf[86]);
    aim_printf (&uc->pvs,
                "  TX rate select bits: 0x%02x\n", buf[88]);

    uint8_t vendor[17];
    uint8_t vendor_pn[17];
    uint8_t vendor_rev[3];
    uint8_t vendor_sn[17];
    uint8_t vendor_date[9];

    sfp_copy_string (vendor, buf, 148, 16);
    sfp_copy_string (vendor_pn, buf, 168, 16);
    sfp_copy_string (vendor_rev, buf, 184, 2);
    sfp_copy_string (vendor_sn, buf, 196, 16);
    sfp_copy_string (vendor_date, buf, 212, 8);

    aim_printf (&uc->pvs, "  Connector: 0x%02x\n",
                buf[130]);
    aim_printf (&uc->pvs,
                "  Spec compliance: "
                "0x%02x 0x%02x 0x%02x 0x%02x"
                "0x%02x 0x%02x 0x%02x 0x%02x\n",
                buf[131],
                buf[132],
                buf[133],
                buf[134],
                buf[135],
                buf[136],
                buf[137],
                buf[138]);
    aim_printf (&uc->pvs, "  Encoding: 0x%02x\n",
                buf[139]);
    aim_printf (&uc->pvs,
                "  Nominal Bit Rate: %d MBps\n", buf[140] * 100);
    aim_printf (&uc->pvs,
                "  Ext rate select compliance: 0x%#02x\n",
                buf[141]);
    aim_printf (&uc->pvs, "  Length (SMF): %d km\n",
                buf[142]);
    aim_printf (&uc->pvs, "  Length (OM3): %d m\n",
                buf[143] * 2);
    aim_printf (&uc->pvs, "  Length (OM2): %d m\n",
                buf[144]);
    aim_printf (&uc->pvs, "  Length (OM1): %d m\n",
                buf[145]);
    aim_printf (&uc->pvs, "  Length (Copper): %d m\n",
                buf[146]);
    aim_printf (&uc->pvs, "  Device Tech: 0x%02x\n",
                buf[147]);
    aim_printf (&uc->pvs, "  Ext Module: 0x%02x\n",
                buf[164]);
    aim_printf (
        &uc->pvs, "  Wavelength tolerance: 0x%02x 0x%02x\n",
        buf[188], buf[189]);
    aim_printf (&uc->pvs, "  Max case temp: %dC\n",
                buf[190]);
    aim_printf (&uc->pvs, "  CC_BASE: 0x%02x\n",
                buf[191]);
    aim_printf (&uc->pvs,
                "  Options: 0x%02x 0x%02x 0x%02x 0x%02x\n",
                buf[192],
                buf[193],
                buf[194],
                buf[195]);
    aim_printf (&uc->pvs, "  DOM Type: 0x%02x\n",
                buf[220]);
    aim_printf (&uc->pvs,
                "  Enhanced Options: 0x%02x\n", buf[221]);
    aim_printf (&uc->pvs, "  Reserved: 0x%02x\n",
                buf[222]);
    aim_printf (&uc->pvs, "  CC_EXT: 0x%02x\n",
                buf[223]);
    aim_printf (&uc->pvs, "  Vendor Specific:\n");
    aim_printf (&uc->pvs,
                "    %02x %02x %02x %02x %02x %02x %02x %02x"
                "  %02x %02x %02x %02x %02x %02x %02x %02x\n",
                buf[224],
                buf[225],
                buf[226],
                buf[227],
                buf[228],
                buf[229],
                buf[230],
                buf[231],
                buf[232],
                buf[233],
                buf[234],
                buf[235],
                buf[236],
                buf[237],
                buf[238],
                buf[239]);
    aim_printf (&uc->pvs,
                "    %02x %02x %02x %02x %02x %02x %02x %02x"
                "  %02x %02x %02x %02x %02x %02x %02x %02x\n",
                buf[240],
                buf[241],
                buf[242],
                buf[243],
                buf[244],
                buf[245],
                buf[246],
                buf[247],
                buf[248],
                buf[249],
                buf[250],
                buf[251],
                buf[252],
                buf[253],
                buf[254],
                buf[255]);

    aim_printf (&uc->pvs, "  Vendor: %s\n", vendor);
    aim_printf (&uc->pvs,
                "  Vendor OUI: %02x:%02x:%02x\n",
                buf[165 - 128],
                buf[166 - 128],
                buf[167 - 128]);
    aim_printf (&uc->pvs, "  Vendor PN: %s\n",
                vendor_pn);
    aim_printf (&uc->pvs, "  Vendor Rev: %s\n",
                vendor_rev);
    aim_printf (&uc->pvs, "  Vendor SN: %s\n",
                vendor_sn);
    aim_printf (&uc->pvs, "  Date Code: %s\n",
                vendor_date);

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_detect_xver (
    ucli_context_t *uc)
{
    int port;
    bool is_present;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (uc, "detect-xver", 1,
                       "detect-xver <port>");

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        if (bf_qsfp_detect_transceiver (port,
                                        &is_present) == 0) {
            aim_printf (&uc->pvs,
                        "port %d %s\n",
                        port,
                        (is_present ? "detected" : "removed"));
        } else {
            aim_printf (&uc->pvs, "error detecting port %d\n",
                        port);
        }
    } else if (port == -1) {
        int i, cnt = max_port;
        for (i = 0; i < cnt; i++) {
            if (bf_qsfp_detect_transceiver (i,
                                            &is_present) == 0) {
                aim_printf (
                    &uc->pvs, "port %d %s\n", i,
                    (is_present ? "detected" : "removed"));
            } else {
                aim_printf (&uc->pvs, "error detecting port %d\n",
                            i);
            }
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_type_show (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-type", 1,
                       "qsfp-type <port>");
    int port;
    bf_pltfm_qsfp_type_t qsfp_type =
        BF_PLTFM_QSFP_UNKNOWN;
    char display_qsfp_type[13];
    int port_begin, max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        port_begin = port;
        max_port = port;
    } else {
        port_begin = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }

    for (port = port_begin; port <= max_port;
         port++) {
        if (bf_qsfp_type_get (port, &qsfp_type) != 0) {
            qsfp_type = BF_PLTFM_QSFP_UNKNOWN;
        }
        qsfp_type_to_display_get (
            qsfp_type, display_qsfp_type,
            sizeof (display_qsfp_type));
        aim_printf (&uc->pvs, "port %d %s\n", port,
                    display_qsfp_type);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_read_reg (
    ucli_context_t *uc)
{
    int port, page, offset;
    uint8_t val;
    int max_port = bf_qsfp_get_max_qsfp_ports();
	int err;

    UCLI_COMMAND_INFO (uc, "read-reg", 3,
                       "read-reg <port> <page> <offset>");
    port = atoi (uc->pargs->args[0]);
    page = atoi (uc->pargs->args[1]);
    offset = strtol (uc->pargs->args[2], NULL, 0);

    aim_printf (
        &uc->pvs,
        "read-reg: read-reg <port=%d> <page=%d> <offset=0x%x>\n",
        port,
        page,
        offset);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present %d\n",
                    port);
        return 0;
    }

    err = bf_pltfm_qsfp_read_reg (port, page, offset,
                                  &val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) reading register offset=0x%02x\n", err,
                    offset);
    } else {
        aim_printf (&uc->pvs,
                    "page %d offset 0x%x read 0x%02x\n", page, offset,
                    val);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_write_reg (
    ucli_context_t *uc)
{
    int port, page, offset;
    uint8_t val;
    int err;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    UCLI_COMMAND_INFO (
        uc, "write-reg", 4,
        "write-reg <port> <page> <offset> <val>");
    port = atoi (uc->pargs->args[0]);
    page = atoi (uc->pargs->args[1]);
    offset = strtol (uc->pargs->args[2], NULL, 0);
    val = strtol (uc->pargs->args[3], NULL, 0);

    aim_printf (
        &uc->pvs,
        "write-reg: write-reg <port=%d> <page=%d> <offset=0x%x> "
        "<val=0x%x>\n",
        port,
        page,
        offset,
        val);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    if (!bf_qsfp_is_present (port)) {
        aim_printf (&uc->pvs, "Module not present %d\n",
                    port);
        return 0;
    }

    err = bf_pltfm_qsfp_write_reg (port, page, offset,
                                   val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) writing register offset=0x%02x\n", err,
                    offset);
    } else {
        aim_printf (
            &uc->pvs, "page %d offset 0x%x written 0x%0x\n",
            page, offset, val);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_reset (ucli_context_t
                                *uc)
{
    UCLI_COMMAND_INFO (
        uc, "qsfp-reset", 2,
        "qsfp-reset <port> <1=reset, 0=unreset>");
    int port;
    int port_begin, max_port;
    int reset_val;
    bool reset;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        port_begin = port;
        max_port = port;
    } else {
        port_begin = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }
    reset_val = atoi (uc->pargs->args[1]);
    reset = (reset_val == 0) ? false : true;

    for (port = port_begin; port <= max_port;
         port++) {
        bf_qsfp_reset (port, reset);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_lpmode_hw (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "qsfp-lpmode-hw", 2,
        "qsfp-lpmode-hw <port> <1=reset, 0=unreset>");
    int port;
    int port_begin, max_port;
    int lpmode_val;
    bool lpmode;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        port_begin = port;
        max_port = port;
    } else {
        port_begin = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }
    lpmode_val = atoi (uc->pargs->args[1]);
    lpmode = (lpmode_val == 0) ? false : true;

    for (port = port_begin; port <= max_port;
         port++) {
        bf_qsfp_set_transceiver_lpmode (port, lpmode);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_lpmode_sw (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "qsfp-lpmode-sw",
                       2,
                       "qsfp-lpmode-sw <port> <1: lpmode 0 : no lpmode>");
    int port;
    int port_begin, max_port;
    bool lpmode;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        port_begin = port;
        max_port = port;
    } else {
        port_begin = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }
    lpmode = atoi (uc->pargs->args[1]);

    for (port = port_begin; port <= max_port;
         port++) {
        qsfp_lpmode_sw_set (0, port, lpmode);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_remove_all (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-remove-all", 0,
                       "qsfp-remove-all ");

    bf_pltfm_pm_qsfp_simulate_all_removed();
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_lpbk (ucli_context_t
                               *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-lpbk", 2,
                       "qsfp-lpbk port [near/elec far/opt]");
    int port;
    bool lpbk_near = true;
    int max_port = bf_qsfp_get_max_qsfp_ports();

    port = atoi (uc->pargs->args[0]);
    lpbk_near = (uc->pargs->args[1][0] == 'n')
                ? true
                : (uc->pargs->args[1][0] == 'e') ? true : false;
    if (port > max_port) {
        return 0;
    }
    qsfp_luxtera_lpbk (port, lpbk_near);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_capture (ucli_context_t
                                  *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-capture", 1,
                       "qsfp-capture port");
    int port, ofs;
    uint8_t arr_0x3k[0x3000];
    char printable_strs[17] = {0};

    port = atoi (uc->pargs->args[0]);
    bf_pm_qsfp_luxtera_state_capture (port, arr_0x3k);

    for (ofs = 0; ofs < 12288; ofs++) {
        if ((ofs % 16) == 0) {
            printable_strs[16] = 0;
            aim_printf (&uc->pvs, " %s\n%6d : ",
                        printable_strs, ofs);
        }
        aim_printf (&uc->pvs, "%02x ", arr_0x3k[ofs]);
        printable_strs[ (ofs % 16)] = isprint (
                                          arr_0x3k[ofs]) ? arr_0x3k[ofs] : '.';
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_tmr_stop (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-tmr-stop", 0,
                       "qsfp-tmr-stop");
    bf_pltfm_pm_qsfp_scan_poll_stop();
    bf_sys_sleep (
        1); /* wait for on going qsfp scan to be over */
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_tmr_start (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-tmr-start", 0,
                       "qsfp-tmr-start");
    bf_pltfm_pm_qsfp_scan_poll_start();
    return 0;
}

static int bf_qsfp_ucli_qsfp_type_get (
    const char *str1,
    bf_pltfm_qsfp_type_t *qsfp_type)
{
    int i = 0;
    char str[25];

    if ((!qsfp_type) || (!str1)) {
        return -1;
    }

    memset (str, '\0', sizeof (str));

    while ((str1[i]) && i < 25) {
        str[i] = tolower (str1[i]);
        i++;
    }

    if (strcmp (str, "copper-0.5m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_0_5_M;
    } else if (strcmp (str, "copper-1m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_1_M;
    } else if (strcmp (str, "copper-2m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_2_M;
    } else if (strcmp (str, "copper-3m") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_3_M;
    } else if (strcmp (str, "copper-loopback") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
    } else if (strcmp (str, "optical") == 0) {
        *qsfp_type = BF_PLTFM_QSFP_OPT;
    } else {
        return -1;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_special_case_set (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "qsfp-type-set",
                       2,
                       "qsfp-type-set <port> "
                       "<copper-0.5, copper-1m, copper-2m, copper-3m, "
                       "copper-loopback, optical>");
    char usage[] =
        "qsfp-type-set <port> "
        "<copper-0.5, copper-1m, copper-2m, copper-3m, copper-loopback, optical>";

    int port;
    int port_begin, max_port;
    bf_pltfm_qsfp_type_t qsfp_type;
    int ret = 0;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        port_begin = port;
        max_port = port;
    } else {
        port_begin = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    ret = bf_qsfp_ucli_qsfp_type_get (
              uc->pargs->args[1], &qsfp_type);
    if (ret != 0) {
        aim_printf (&uc->pvs, "Usage : %s\n", usage);
        return 0;
    };

    for (port = port_begin; port <= max_port;
         port++) {
        // expand if more info needs to set
        bf_qsfp_special_case_set (port, qsfp_type, true);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_special_case_clear (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "qsfp-type-clear", 1,
                       "qsfp-type-clear <port> ");
    int port;
    int port_begin, max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        port_begin = port;
        max_port = port;
    } else {
        port_begin = 1;
        max_port = bf_qsfp_get_max_qsfp_ports();
    }

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    for (port = port_begin; port <= max_port;
         port++) {
        bf_qsfp_special_case_set (port,
                                  BF_PLTFM_QSFP_UNKNOWN, false);
    }
    return 0;
}

extern int sff_db_get (sff_db_entry_t **entries,
                       int *count);
extern int bf_pltfm_get_qsfp_ctx (struct
                                  qsfp_ctx_t
                                  **qsfp_ctx);

static void
sff_info_show (sff_info_t *info,
               ucli_context_t *uc)
{
    aim_printf (&uc->pvs,
                "Vendor: %s Model: %s SN: %s Type: %s Module: %s Media: %s Length: %d\n",
                info->vendor, info->model, info->serial,
                info->sfp_type_name,
                info->module_type_name, info->media_type_name,
                info->length);
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_db (ucli_context_t
                             *uc)
{
    sff_info_t *sff;
    sff_eeprom_t *se;
    sff_db_entry_t *entry;
    int num = 0;
    int i;

    UCLI_COMMAND_INFO (uc, "db", 0,
                       "Display suported QSFP database.");

    sff_db_get (&entry, &num);
    for (i = 0; i < num; i ++) {
        se = &entry[i].se;
        sff = &se->info;
        if (sff->sfp_type == SFF_SFP_TYPE_QSFP ||
            sff->sfp_type == SFF_SFP_TYPE_QSFP_PLUS ||
            sff->sfp_type == SFF_SFP_TYPE_QSFP28) {
            sff_info_show (sff, uc);
        }
    }
    return BF_PLTFM_SUCCESS;
}

static ucli_status_t
bf_pltfm_ucli_ucli__qsfp_map (ucli_context_t
                              *uc)
{
    struct qsfp_ctx_t *qsfp, *qsfp_ctx;

    UCLI_COMMAND_INFO (uc, "map", 0,
                       "Display QSFP map.");

    if (!bf_pltfm_get_qsfp_ctx (&qsfp_ctx)) {
        /* Dump the map of QSFP <-> QSFP. */
        for (int i = 0;
             i < bf_qsfp_get_max_qsfp_ports(); i ++) {
            qsfp = &qsfp_ctx[i];
            if (strcmp(qsfp->desc, "EMPTY")) {
                aim_printf (&uc->pvs, "%-3s   <->   %02d : %s\n",
                            qsfp->desc, qsfp->conn_id, bf_qsfp_is_present (qsfp->conn_id) ? "present" : "not present");
            }
        }
    }
    return BF_PLTFM_SUCCESS;
}

/* <auto.ucli.handlers.start> */
static ucli_command_handler_f
bf_pltfm_qsfp_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__qsfp_detect_xver,
    // bf_pltfm_ucli_ucli__qsfp_get_xver_info,
    bf_pltfm_ucli_ucli__qsfp_dump_info,
    bf_pltfm_ucli_ucli__qsfp_show,
    bf_pltfm_ucli_ucli__qsfp_init,
    bf_pltfm_ucli_ucli__qsfp_get_int,
    bf_pltfm_ucli_ucli__qsfp_get_lpmode,
    bf_pltfm_ucli_ucli__qsfp_set_lpmode,
    bf_pltfm_ucli_ucli__qsfp_get_pres,
    bf_pltfm_ucli_ucli__qsfp_get_ddm,
    bf_pltfm_ucli_ucli__qsfp_type_show,
    bf_pltfm_ucli_ucli__qsfp_tx_disable_set,
    bf_pltfm_ucli_ucli__qsfp_read_reg,
    bf_pltfm_ucli_ucli__qsfp_write_reg,
    bf_pltfm_ucli_ucli__qsfp_pg0,
    bf_pltfm_ucli_ucli__qsfp_pg3,
    bf_pltfm_ucli_ucli__qsfp_oper,
    bf_pltfm_ucli_ucli__qsfp_summary,
    bf_pltfm_ucli_ucli__qsfp_reset,
    bf_pltfm_ucli_ucli__qsfp_lpmode_hw,
    bf_pltfm_ucli_ucli__qsfp_lpmode_sw,
    bf_pltfm_ucli_ucli__qsfp_remove_all,
    bf_pltfm_ucli_ucli__qsfp_lpbk,
    bf_pltfm_ucli_ucli__qsfp_capture,
    bf_pltfm_ucli_ucli__qsfp_tmr_stop,
    bf_pltfm_ucli_ucli__qsfp_tmr_start,
    bf_pltfm_ucli_ucli__qsfp_special_case_set,
    bf_pltfm_ucli_ucli__qsfp_special_case_clear,
    bf_pltfm_ucli_ucli__qsfp_db,
    bf_pltfm_ucli_ucli__qsfp_map,
    NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t bf_pltfm_qsfp_ucli_module__
= {
    "qsfp_ucli", NULL, bf_pltfm_qsfp_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_qsfp_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_qsfp_ucli_module__);
    n = ucli_node_create ("qsfp", m,
                          &bf_pltfm_qsfp_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("qsfp"));
    return n;
}
