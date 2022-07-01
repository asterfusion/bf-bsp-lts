#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>
#include <bf_bd_cfg/bf_bd_cfg.h>
#include <bf_pltfm_bd_cfg.h>

static int port_num_is_valid (uint32_t conn,
                              uint32_t chnl)
{
    // Depending on the type of the board, check if the number of ports is valid
    if (conn < 1 ||
        conn > (uint32_t)bf_bd_cfg_bd_num_port_get()) {
        return -1;
    }
    if (chnl >= QSFP_NUM_CHN) {
        return -1;
    }
    return 0;
}

static bf_pltfm_status_t str_to_port_info_map (
    const char *str,
    bf_pltfm_port_info_t *port_info)
{
    bf_pltfm_status_t sts = BF_PLTFM_SUCCESS;
    int len = strlen (str);
    char *tmp_str = strdup (str);
    if (tmp_str == NULL) {
        sts = BF_PLTFM_INVALID_ARG;
        goto err_cleanup;
    }

    char *ptr = tmp_str;
    int i = 0;

    while (*ptr != '/' && i < len) {
        ptr++;
        i++;
    }
    if (i == len) {
        sts = BF_PLTFM_INVALID_ARG;
        goto err_cleanup;
    }
    *ptr = '\0';
    port_info->conn_id = strtoul (tmp_str, NULL, 10);
    ptr++;
    port_info->chnl_id = strtoul (ptr, NULL, 10);

    if (port_num_is_valid (port_info->conn_id,
                           port_info->chnl_id) != 0) {
        sts = BF_PLTFM_INVALID_ARG;
        goto err_cleanup;
    }

err_cleanup:
    if (tmp_str) {
        free (tmp_str);
    }
    return sts;
}

static int conn_and_chnl_get (const char *str,
                              int *conn, int *chnl)
{
    bf_pltfm_port_info_t port_info;
    bf_pltfm_status_t sts;

    sts = str_to_port_info_map (str, &port_info);
    if (sts != BF_PLTFM_SUCCESS) {
        return -1;
    } else {
        *conn = port_info.conn_id;
        *chnl = port_info.chnl_id;
        return 0;
    }
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_mac_get__ (
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    uint32_t mac_id;
    uint32_t mac_chnl_id;
    int ret;
    char usage[] = "Usage: mac_get <conn_id/chnl_id>";

    UCLI_COMMAND_INFO (uc, "mac_get", 1,
                       "<conn_id/chnl_id>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: mac_get <%d/%d>\n",
                port_info.conn_id,
                port_info.chnl_id);

    bf_pltfm_status_t sts =
        bf_bd_cfg_port_mac_get (&port_info, &mac_id,
                                &mac_chnl_id);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the MAC block for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (
            &uc->pvs, "MAC block id :%d \nMAC channel :%d\n",
            mac_id, mac_chnl_id);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_tx_phy_lane_get__
(
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    uint32_t tx_phy_lane_id;
    int ret;
    char usage[] =
        "Usage: tx_phy_lane_get <conn_id/chnl_id>";

    UCLI_COMMAND_INFO (uc, "tx_phy_lane_get", 1,
                       "<conn_id/chnl_id>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: tx_phy_lane_get <%d/%d>\n",
                port_info.conn_id,
                port_info.chnl_id);

    bf_pltfm_status_t sts =
        bf_bd_cfg_port_tx_phy_lane_get (&port_info,
                                        &tx_phy_lane_id);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the TX PHY for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (&uc->pvs, "TX PHY lane id :%d\n",
                    tx_phy_lane_id);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_rx_phy_lane_get__
(
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    uint32_t rx_phy_lane_id;
    int ret;
    char usage[] =
        "Usage: rx_phy_lane_get <conn_id/chnl_id>";

    UCLI_COMMAND_INFO (uc, "rx_phy_lane_get", 1,
                       "<conn_id/chnl_id>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: rx_phy_lane_get <%d/%d>\n",
                port_info.conn_id,
                port_info.chnl_id);

    bf_pltfm_status_t sts =
        bf_bd_cfg_port_rx_phy_lane_get (&port_info,
                                        &rx_phy_lane_id);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the RX PHY for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (&uc->pvs, "RX PHY lane id :%d\n",
                    rx_phy_lane_id);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_tx_pn_swap_get__
(
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    bool tx_pn_swap;
    int ret;
    char usage[] =
        "Usage: tx_pn_swap_get <conn_id/chnl_id>";

    UCLI_COMMAND_INFO (uc, "tx_pn_swap_get", 1,
                       "<conn_id/chnl_id>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: tx_pn_swap_get <%d/%d>\n",
                port_info.conn_id,
                port_info.chnl_id);

    bf_pltfm_status_t sts =
        bf_bd_cfg_port_tx_pn_swap_get (&port_info,
                                       &tx_pn_swap);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the TX PN swap for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (&uc->pvs, "TX PN swap :%s\n",
                    tx_pn_swap ? "true" : "false");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_rx_pn_swap_get__
(
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    bool rx_pn_swap;
    int ret;
    char usage[] =
        "Usage: rx_pn_swap_get <conn_id/chnl_id>";

    UCLI_COMMAND_INFO (uc, "rx_pn_swap_get", 1,
                       "<conn_id/chnl_id>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: rx_pn_swap_get <%d/%d>\n",
                port_info.conn_id,
                port_info.chnl_id);

    bf_pltfm_status_t sts =
        bf_bd_cfg_port_rx_pn_swap_get (&port_info,
                                       &rx_pn_swap);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the RX PN swap for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (&uc->pvs, "RX PN swap :%s\n",
                    rx_pn_swap ? "true" : "false");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_mac_lane_info_get__
(
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    bf_pltfm_mac_lane_info_t lane_info;
    bf_pltfm_qsfp_type_t qsfp_type = 0;
    int ret;
    char usage[] =
        "Usage: lane_info_get <conn_id/chnl_id> <qsfp_type:0->Cu_0.5m, 1->Cu_1m, "
        "2->Cu_2m, 3->Cu_3m, 4->Cu_loop, 5->Opt>";

    UCLI_COMMAND_INFO (uc,
                       "lane_info_get",
                       2,
                       "<conn_id/chnl_id> <qsfp_type:0->Cu_0.5m, 1->Cu_1m, "
                       "2->Cu_2m, 3->Cu_3m, 4->Cu_loop, 5->Opt>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    qsfp_type = strtoul (uc->pargs->args[1], NULL,
                         10);
    if (qsfp_type > BF_PLTFM_QSFP_OPT) {
        qsfp_type = BF_PLTFM_QSFP_CU_0_5_M;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: lane_info_get <%d/%d>\n",
                port_info.conn_id,
                port_info.chnl_id);

    bf_pltfm_status_t sts =
        bf_bd_cfg_port_mac_lane_info_get (&port_info,
                                          qsfp_type, &lane_info);

    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the mac lane info for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (&uc->pvs, "TX PHY lane id :%d\n",
                    lane_info.tx_phy_lane_id);
        aim_printf (&uc->pvs,
                    "TX PN swap :%s\n",
                    lane_info.tx_phy_pn_swap ? "true" : "false");
        aim_printf (&uc->pvs, "RX PHY lane id :%d\n",
                    lane_info.rx_phy_lane_id);
        aim_printf (&uc->pvs,
                    "RX PN swap :%s\n",
                    lane_info.rx_phy_pn_swap ? "true" : "false");
        aim_printf (&uc->pvs, "TX Attenuation :%d\n",
                    lane_info.tx_attn);
        aim_printf (&uc->pvs, "TX Pre Emphasis :%d\n",
                    lane_info.tx_pre);
        aim_printf (&uc->pvs, "TX Post Emphasis:%d\n",
                    lane_info.tx_post);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_rptr_info_get__
(
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_mode_t mode;
    bf_pltfm_rptr_info_t rptr_info;
    bf_pltfm_qsfp_type_t qsfp_type = 0;
    int ret;
    char usage[] =
        "Usage: rptr_info_get <conn_id/chnl_id> <Egress->0, Ingress->1> "
        "<qsfp_type:0->Cu_0.5m, 1->Cu_1m, 2->Cu_2m, 3->Cu_3m, 4->Cu_loop, "
        "5->Opt>";

    UCLI_COMMAND_INFO (uc,
                       "rptr_info_get",
                       3,
                       "<conn_id/chnl_id> <Egress->0, Ingress->1> "
                       "<qsfp_type:0->Cu_0.5m, 1->Cu_1m, 2->Cu_2m, 3->Cu_3m, "
                       "4->Cu_loop, 5->Opt>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    mode = (uint8_t)strtol (uc->pargs->args[1], NULL,
                            10);
    qsfp_type = strtoul (uc->pargs->args[2], NULL,
                         10);
    if (qsfp_type > BF_PLTFM_QSFP_OPT) {
        qsfp_type = BF_PLTFM_QSFP_CU_0_5_M;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: rptr_info_get <%d/%d> <mode=%d>\n",
                port_info.conn_id,
                port_info.chnl_id,
                mode);

    bf_pltfm_status_t sts =
        bf_pltfm_bd_cfg_rptr_info_get (&port_info, mode,
                                       qsfp_type, &rptr_info);

    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (
            &uc->pvs,
            "Repeater num :%d \nChannel num :%d \nEq boost 1 :%d \nEq boost 2 :%d "
            "\nEq bw :%d \nEq bypass boost :%d \nVOD :%d \nRegF :%d\nHigh_Gain "
            ":%d\n",
            rptr_info.rptr_num,
            rptr_info.chnl_num,
            rptr_info.eq_bst1,
            rptr_info.eq_bst2,
            rptr_info.eq_bw,
            rptr_info.eq_bypass_bst1,
            rptr_info.vod,
            rptr_info.regF,
            rptr_info.high_gain);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_rtmr_info_get__
(
    ucli_context_t *uc)
{
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_info_t rtmr_info;
    int ret;
    char usage[] =
        "Usage: rtmr_info_get <conn_id/chnl_id>";
    bf_pltfm_status_t sts;
    bool rtmr_present;

    UCLI_COMMAND_INFO (uc, "rtmr_info_get", 1,
                       "<conn_id/chnl_id>");

    ret = conn_and_chnl_get (
              uc->pargs->args[0], (int *)&port_info.conn_id,
              (int *)&port_info.chnl_id);
    if (ret != 0) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }

    aim_printf (&uc->pvs,
                "Bd Cfg:: rtmr_info_get <%d/%d>\n",
                port_info.conn_id,
                port_info.chnl_id);

    sts = bf_bd_cfg_port_has_rtmr (&port_info,
                                   &rtmr_present);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (
            &uc->pvs,
            "ERROR: Unable to get the Retimer presence info for port %d/%d\n",
            port_info.conn_id,
            port_info.chnl_id);
    }

    if (!rtmr_present) {
        aim_printf (&uc->pvs,
                    "ERROR: Retimer absent for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
        return 0;
    }

    sts = bf_bd_cfg_rtmr_info_get (&port_info,
                                   &rtmr_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Retimer info for port %d/%d\n",
                    port_info.conn_id,
                    port_info.chnl_id);
    } else {
        aim_printf (&uc->pvs,
                    "Retimer num : %d \nRetimer I2C addr : %d (0x%x) \nHost side "
                    "lane num : %d \nHost side TX PN Swap : %d "
                    "\nHost side RX PN Swap : %d \nLine side lane num : %d \nLine "
                    "side TX PN Swap : %d \nLine side RX PN Swap : %d\n",
                    rtmr_info.num,
                    rtmr_info.i2c_addr,
                    rtmr_info.i2c_addr,
                    rtmr_info.host_side_lane,
                    rtmr_info.host_side_tx_pn_swap,
                    rtmr_info.host_side_rx_pn_swap,
                    rtmr_info.line_side_lane,
                    rtmr_info.line_side_tx_pn_swap,
                    rtmr_info.line_side_rx_pn_swap);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_bd_type_get__ (
    ucli_context_t *uc)
{
    char name[128];
    UCLI_COMMAND_INFO (uc, "bd_type_get", 0, "");
    platform_name_get_str (name, sizeof (name));
    aim_printf (&uc->pvs, "Board type: %s\n", name);
    return 0;
}

static void print_detailed_info (ucli_context_t
                                 *uc,
                                 bf_pltfm_port_info_t *port_info,
                                 bf_pltfm_qsfp_type_t qsfp_type)
{
    uint32_t mac_id;
    uint32_t mac_chnl_id;
    bf_pltfm_mac_lane_info_t lane_info;
    bf_pltfm_rptr_info_t e_rptr_info, i_rptr_info;
    bf_pltfm_status_t sts;
    bool has_rptr;
    uint8_t ingress_i2c_addr, egress_i2c_addr;

    sts = bf_bd_cfg_port_mac_get (port_info, &mac_id,
                                  &mac_chnl_id);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the MAC block for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts =
        bf_bd_cfg_port_mac_lane_info_get (port_info,
                                          qsfp_type, &lane_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the mac lane info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_bd_cfg_port_has_rptr (port_info,
                                   &has_rptr);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_pltfm_bd_cfg_rptr_info_get (
              port_info, BF_PLTFM_RPTR_EGRESS, qsfp_type,
              &e_rptr_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_pltfm_bd_cfg_rptr_info_get (
              port_info, BF_PLTFM_RPTR_INGRESS, qsfp_type,
              &i_rptr_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_bd_cfg_rptr_addr_get (
              port_info, BF_PLTFM_RPTR_EGRESS,
              &egress_i2c_addr);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_bd_cfg_rptr_addr_get (
              port_info, BF_PLTFM_RPTR_INGRESS,
              &ingress_i2c_addr);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    aim_printf (&uc->pvs, "QSFP Port : %d\n",
                port_info->conn_id);
    aim_printf (&uc->pvs, "QSFP Lane : %d\n",
                port_info->chnl_id);
    aim_printf (&uc->pvs, "MAC id : %d\n", mac_id);
    aim_printf (&uc->pvs, "MAC channel id : %d\n",
                mac_chnl_id);
    aim_printf (&uc->pvs, "TX PHY lane id : %d\n",
                lane_info.tx_phy_lane_id);
    aim_printf (&uc->pvs, "TX PN swap : %d\n",
                lane_info.tx_phy_pn_swap);
    aim_printf (&uc->pvs, "RX PHY lane id : %d\n",
                lane_info.rx_phy_lane_id);
    aim_printf (&uc->pvs, "RX PN swap : %d\n",
                lane_info.rx_phy_pn_swap);
    aim_printf (&uc->pvs, "TX Attenuation : %d\n",
                lane_info.tx_attn);
    aim_printf (&uc->pvs, "TX Pre Emphasis : %d\n",
                lane_info.tx_pre);
    aim_printf (&uc->pvs, "TX Post Emphasis : %d\n",
                lane_info.tx_post);
    aim_printf (&uc->pvs, "Repeater present : %d\n",
                has_rptr);
    aim_printf (&uc->pvs,
                "Egress Repeater Number : %d\n",
                e_rptr_info.rptr_num);
    aim_printf (&uc->pvs,
                "Egress Repeater I2C Address : %d\n",
                egress_i2c_addr);
    aim_printf (&uc->pvs,
                "Egress Repeater Channel : %d\n",
                e_rptr_info.chnl_num);
    aim_printf (
        &uc->pvs, "Egress Repeater EQ Boost 1 : %d\n",
        e_rptr_info.eq_bst1);
    aim_printf (
        &uc->pvs, "Egress Repeater EQ Boost 2 : %d\n",
        e_rptr_info.eq_bst2);
    aim_printf (&uc->pvs,
                "Egress Repeater EQ BW : %d\n",
                e_rptr_info.eq_bw);
    aim_printf (&uc->pvs,
                "Egress Repeater EQ Bypass Boost 1 : %d\n",
                e_rptr_info.eq_bypass_bst1);
    aim_printf (&uc->pvs,
                "Egress Repeater VOD : %d\n", e_rptr_info.vod);
    aim_printf (&uc->pvs,
                "Egress Repeater RegF : %d\n", e_rptr_info.regF);
    aim_printf (
        &uc->pvs, "Egress Repeater High Gain : %d\n",
        e_rptr_info.high_gain);
    aim_printf (&uc->pvs,
                "Ingress Repeater Number : %d\n",
                i_rptr_info.rptr_num);
    aim_printf (&uc->pvs,
                "Ingress Repeater I2C Address : %d\n",
                ingress_i2c_addr);
    aim_printf (&uc->pvs,
                "Ingress Repeater Channel : %d\n",
                i_rptr_info.chnl_num);
    aim_printf (
        &uc->pvs, "Ingress Repeater EQ Boost 1 : %d\n",
        i_rptr_info.eq_bst1);
    aim_printf (
        &uc->pvs, "Ingress Repeater EQ Boost 2 : %d\n",
        i_rptr_info.eq_bst2);
    aim_printf (&uc->pvs,
                "Ingress Repeater EQ BW : %d\n",
                i_rptr_info.eq_bw);
    aim_printf (&uc->pvs,
                "Ingress Repeater EQ Bypass Boost 1 : %d\n",
                i_rptr_info.eq_bypass_bst1);
    aim_printf (&uc->pvs,
                "Ingress Repeater VOD : %d\n", i_rptr_info.vod);
    aim_printf (&uc->pvs,
                "Ingress Repeater RegF : %d\n", i_rptr_info.regF);
    aim_printf (
        &uc->pvs, "Ingress Repeater High Gain : %d\n",
        i_rptr_info.high_gain);
}

static void print_repeater_lane_info (
    ucli_context_t *uc,
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type)
{
    bf_pltfm_rptr_info_t e_rptr_info, i_rptr_info;
    bf_pltfm_status_t sts;
    bool has_rptr;
    uint8_t ingress_i2c_addr, egress_i2c_addr;

    sts = bf_bd_cfg_port_has_rptr (port_info,
                                   &has_rptr);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    if (has_rptr == false) {
        return;
    }

    sts = bf_pltfm_bd_cfg_rptr_info_get (
              port_info, BF_PLTFM_RPTR_EGRESS, qsfp_type,
              &e_rptr_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_pltfm_bd_cfg_rptr_info_get (
              port_info, BF_PLTFM_RPTR_INGRESS, qsfp_type,
              &i_rptr_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_bd_cfg_rptr_addr_get (
              port_info, BF_PLTFM_RPTR_EGRESS,
              &egress_i2c_addr);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_bd_cfg_rptr_addr_get (
              port_info, BF_PLTFM_RPTR_INGRESS,
              &ingress_i2c_addr);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    aim_printf (&uc->pvs,
                "%2" PRIu64 "/%1" PRIu64 "|%2" PRIu64 "| %2"
                PRIx64 "| %2" PRIu64
                "| %2" PRIu64 " | %2" PRIu64 " |%2" PRIu64 "| %2"
                PRIu64
                " | %2" PRIu64 "|%2" PRIu64 "| %2" PRIx64 "| %2"
                PRIu64
                "| %2" PRIu64 " | %2" PRIu64 " |%2" PRIu64 "| %2"
                PRIu64
                " | %2" PRIu64 "\n",
                (uint64_t)port_info->conn_id,
                (uint64_t)port_info->chnl_id,
                (uint64_t)e_rptr_info.rptr_num,
                (uint64_t)egress_i2c_addr,
                (uint64_t)e_rptr_info.chnl_num,
                (uint64_t)e_rptr_info.eq_bst1,
                (uint64_t)e_rptr_info.eq_bst2,
                (uint64_t)e_rptr_info.eq_bw,
                (uint64_t)e_rptr_info.eq_bypass_bst1,
                (uint64_t)e_rptr_info.vod,
                (uint64_t)i_rptr_info.rptr_num,
                (uint64_t)ingress_i2c_addr,
                (uint64_t)i_rptr_info.chnl_num,
                (uint64_t)i_rptr_info.eq_bst1,
                (uint64_t)i_rptr_info.eq_bst2,
                (uint64_t)i_rptr_info.eq_bw,
                (uint64_t)i_rptr_info.eq_bypass_bst1,
                (uint64_t)i_rptr_info.vod);
}

static void print_lane_info_summary (
    ucli_context_t *uc,
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type)
{
    uint32_t mac_id;
    uint32_t mac_chnl_id;
    bf_pltfm_mac_lane_info_t lane_info;
    bf_pltfm_status_t sts;
    bool has_rptr;

    sts = bf_bd_cfg_port_mac_get (port_info, &mac_id,
                                  &mac_chnl_id);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the MAC block for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts =
        bf_bd_cfg_port_mac_lane_info_get (port_info,
                                          qsfp_type, &lane_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the mac lane info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    sts = bf_bd_cfg_port_has_rptr (port_info,
                                   &has_rptr);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: Unable to get the Repeater info for conn %d chnl %d\n",
                    port_info->conn_id,
                    port_info->chnl_id);
    }

    aim_printf (&uc->pvs,
                "%2" PRIu64 "/%1" PRIu64 "| %2" PRIu64 " |  %1"
                PRIu64
                " |  %2" PRIu64 " |   %1" PRIu64 " |  %2" PRIu64
                " |   %1" PRIu64
                " |  %2" PRIu64 "  |  %2" PRIu64 "  |   %2" PRIu64
                "  |  %1" PRIu64
                " \n",
                (uint64_t)port_info->conn_id,
                (uint64_t)port_info->chnl_id,
                (uint64_t)mac_id,
                (uint64_t)mac_chnl_id,
                (uint64_t)lane_info.tx_phy_lane_id,
                (uint64_t)lane_info.tx_phy_pn_swap,
                (uint64_t)lane_info.rx_phy_lane_id,
                (uint64_t)lane_info.rx_phy_pn_swap,
                (uint64_t)lane_info.tx_attn,
                (uint64_t)lane_info.tx_pre,
                (uint64_t)lane_info.tx_post,
                (uint64_t)has_rptr);
}

static void dump_info (ucli_context_t *uc,
                       int num_port,
                       int num_lane,
                       int dflag,
                       int rflag,
                       bf_pltfm_qsfp_type_t qsfp_type)
{
    int port_id, lane_id;
    int port_begin, port_end, lane_begin, lane_end;
    bf_pltfm_port_info_t port_info;
    if (num_port == -1) {
        port_begin = 1;
        port_end = bf_bd_cfg_bd_num_port_get();
    } else {
        port_begin = num_port;
        port_end = num_port;
    }

    if (num_lane == -1) {
        lane_begin = 0;
        lane_end = 3;
    } else {
        lane_begin = num_lane;
        lane_end = num_lane;
    }

    if (dflag) {
        aim_printf (
            &uc->pvs,
            "======================================================================"
            "==========\n");
        for (port_id = port_begin; port_id <= port_end;
             port_id++) {
            port_info.conn_id = port_id;
            for (lane_id = lane_begin; lane_id <= lane_end;
                 lane_id++) {
                port_info.chnl_id = lane_id;
                print_detailed_info (uc, &port_info, qsfp_type);
                if (lane_id != lane_end) {
                    aim_printf (
                        &uc->pvs,
                        "----------------------------------------------------------------"
                        "----------------\n");
                }
            }
            aim_printf (
                &uc->pvs,
                "===================================================================="
                "============\n");
        }
        return;
    } else if (rflag) {
        aim_printf (
            &uc->pvs,
            "----+--+---+---+----+----+--+----+---+--+---+---+----+----+--+----+---"
            "\n");
        aim_printf (
            &uc->pvs,
            "PORT|EG|EG |EG |EG  |EG  |EG|EG  |EG |IG|IG |IG |IG  |IG  |IG|IG  |IG "
            "\n");
        aim_printf (
            &uc->pvs,
            "    "
            "|NO|ADD|PRT|BST1|BST2|BW|BYPS|VOD|NO|ADD|PRT|BST1|BST2|BW|BYPS|VOD\n");
        aim_printf (
            &uc->pvs,
            "----+--+---+---+----+----+--+----+---+--+---+---+----+----+--+----|---"
            "\n");
    } else {
        aim_printf (
            &uc->pvs,
            "\n----+----+----+-----+-----+-----+-----+------+------+-------+----"
            "\n");
        aim_printf (&uc->pvs,
                    "PORT|QUAD|CHNL|TX LN|TX PN|RX LN|RX PN|TX ATT|TX PRE|TX "
                    "POST|RPTR\n");
        aim_printf (
            &uc->pvs,
            "----+----+----+-----+-----+-----+-----+------+------+-------+----"
            "\n");
    }

    for (port_id = port_begin; port_id <= port_end;
         port_id++) {
        port_info.conn_id = port_id;
        for (lane_id = lane_begin; lane_id <= lane_end;
             lane_id++) {
            port_info.chnl_id = lane_id;
            if (rflag) {
                print_repeater_lane_info (uc, &port_info,
                                          qsfp_type);
            } else {
                print_lane_info_summary (uc, &port_info,
                                         qsfp_type);
            }
        }
    }
}

static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_dump__ (
    ucli_context_t *uc)
{
    extern char *optarg;
    extern int optind;
    int c, pflag, lflag, dflag, rflag, qflag;
    char *p_arg;
    static char usage[] =
        "Usage: dump -p <conn_id/chnl_id> -q <qsfp_type:0->Cu_0.5m, 1->Cu_1m, "
        "2->Cu_2m, 3->Cu_3m, 4->Cu_loop, 5->Opt> [-d -r]\n";
    bf_pltfm_port_info_t port_info;
    int argc;
    char *const *argv;
    int ret;
    bf_pltfm_qsfp_type_t qsfp_type = 0;

    UCLI_COMMAND_INFO (uc,
                       "dump",
                       -1,
                       "Usage: show -p <conn_id/chnl_id> -q "
                       "<qsfp_type:0->Cu_0.5m, 1->Cu_1m, 2->Cu_2m, 3->Cu_3m, "
                       "4->Cu_loop, 5->Opt> [-d -r]");

    pflag = lflag = dflag = rflag = qflag = 0;
    p_arg = NULL;
    optind = 0; /* reset optind value */
    argc = (uc->pargs->count + 1);
    argv = (char *const *) & (uc->pargs->args__);

    while ((c = getopt (argc, argv,
                        "p:q:dr")) != -1) {
        switch (c) {
            case 'p':
                pflag = 1;
                p_arg = optarg;
                if (!p_arg) {
                    aim_printf (&uc->pvs, "%s", usage);
                    return UCLI_STATUS_OK;
                }
                break;
            case 'd':
                dflag = 1;
                break;
            case 'r':
                rflag = 1;
                break;
            case 'q':
                qsfp_type = strtoul (optarg, NULL, 10);
                break;
            default:
                aim_printf (&uc->pvs, "%s", usage);
        }
    }

    if (pflag == 1) {
        ret = conn_and_chnl_get (
                  p_arg, (int *)&port_info.conn_id,
                  (int *)&port_info.chnl_id);
        if (ret != 0) {
            aim_printf (&uc->pvs, "%s\n", usage);
            return 0;
        }
        if (p_arg == NULL) {
            aim_printf (&uc->pvs, "p_arg is NULL\n");
            return 0;
        }
        if (strchr (p_arg, '/')[1] >= '0' &&
            strchr (p_arg, '/')[1] <= '3') {
            lflag = 1;
        }

        if (port_info.conn_id < 1 ||
            port_info.conn_id > (uint32_t)
            bf_bd_cfg_bd_num_port_get()) {
            aim_printf (&uc->pvs,
                        "show: Invalid port number %d\n",
                        port_info.conn_id);
            return 0;
        }

        if (lflag == 1) {
            if (port_info.chnl_id > 3) {
                aim_printf (
                    &uc->pvs, "show: Invalid channel number %d\n",
                    port_info.chnl_id);
                return 0;
            }
        }
    }

    int num_port = 0, num_lane = 0;

    if ((pflag == 1) && (lflag == 1)) {
        num_port = port_info.conn_id;
        num_lane = port_info.chnl_id;
    } else if ((pflag == 1) && (lflag == 0)) {
        num_port = port_info.conn_id;
        num_lane = -1;
    } else if (pflag == 0) {
        num_port = -1;
        num_lane = -1;
    }

    dump_info (uc, num_port, num_lane, dflag, rflag,
               qsfp_type);
    return 0;
}


static ucli_status_t
bf_pltfm_bd_cfg_ucli_ucli__tx_sds_eq_set__ (
    ucli_context_t *uc)
{

    bf_pltfm_status_t sts;
    uint32_t conn_begin, conn_end;
    uint32_t chnl_begin, chnl_end;
    bf_pltfm_mac_lane_info_t serdes_info;
    int conn = 0, chnl = 0;
    uint32_t i, j;
    bf_pltfm_port_info_t port_info;
    static char usage[] =
        "Usage: tx_sds_eq_set <conn_id/chnl_id> <tx_attn> <tx_pre> <tx_post>";

    UCLI_COMMAND_INFO (uc, "tx_sds_eq_set", 4,
                       "<conn_id/chnl_id> <tx_attn> <tx_pre> <tx_post>");

    sts = pltfm_port_str_to_info (uc->pargs->args[0],
                                  &port_info);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs, "%s\n", usage);
        return 0;
    }
    conn = (int)port_info.conn_id;
    chnl = (int)port_info.chnl_id;

    if (conn == -1) {
        conn_begin = 1;
        conn_end = (uint32_t)bf_bd_cfg_bd_num_port_get();
    } else {
        conn_begin = conn;
        conn_end = conn;
    }
    if (chnl == -1) {
        chnl_begin = 0;
        chnl_end = QSFP_NUM_CHN - 1;
    } else {
        chnl_begin = chnl;
        chnl_end = chnl;
    }

    serdes_info.tx_attn = strtol (uc->pargs->args[1],
                                  NULL, 10);
    serdes_info.tx_pre = strtol (uc->pargs->args[2],
                                 NULL, 10);
    serdes_info.tx_post = strtol (uc->pargs->args[3],
                                  NULL, 10);

    for (i = conn_begin; i <= conn_end; i++) {
        for (j = chnl_begin; j <= chnl_end; j++) {
            port_info.conn_id = i;
            port_info.chnl_id = j;

            sts = bf_bd_cfg_serdes_info_set (&port_info,
                                             &serdes_info);
            if (sts != BF_PLTFM_SUCCESS) {
                aim_printf (
                    &uc->pvs,
                    "ERROR %s (%d): Unable to set serdes lane info for port : %d/%d\n",
                    bf_pltfm_err_str (sts),
                    sts,
                    i,
                    j);
            }
        }
    }
    return 0;
}



static ucli_command_handler_f
bf_pltfm_bd_cfg_ucli_ucli_handlers__[] = {
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_mac_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_tx_phy_lane_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_rx_phy_lane_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_tx_pn_swap_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_rx_pn_swap_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_port_mac_lane_info_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_rptr_info_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_rtmr_info_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_bd_type_get__,
    bf_pltfm_bd_cfg_ucli_ucli__bd_cfg_dump__,
    bf_pltfm_bd_cfg_ucli_ucli__tx_sds_eq_set__,
    NULL
};

static ucli_module_t bf_pltfm_bd_cfg_ucli_module__
= {
    "bf_pltfm_bd_cfg_ucli",
    NULL,
    bf_pltfm_bd_cfg_ucli_ucli_handlers__,
    NULL,
    NULL,
};

ucli_node_t *bf_bd_cfg_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_bd_cfg_ucli_module__);
    n = ucli_node_create ("bd_cfg", m,
                          &bf_pltfm_bd_cfg_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("bd_cfg"));
    return n;
}
