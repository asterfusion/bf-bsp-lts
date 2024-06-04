/* sfp cli source file
 * author : luyi
 * date: 2020/07/02
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>

#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_bd_cfg/bf_bd_cfg.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_sfp.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>

#define BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, prefix) \
    if ((first_port) > (last_port)) {   \
        (port)      = (last_port);      \
        (last_port) = (first_port);     \
        (first_port)= (port);           \
    }                                   \
    if ((first_port) < 1 || (last_port) > (max_port)) { \
        aim_printf (&uc->pvs, "%s must be 1-%d\n",      \
                    (prefix), (max_port));              \
        return 0;                                       \
    }

#define BF_UCLI_CH_VALID(ch, first_ch, last_ch, max_ch, prefix) \
    if ((first_ch) > (last_ch)) {   \
        (ch)      = (last_ch);      \
        (last_ch) = (first_ch);     \
        (first_ch)= (ch);           \
    }                                   \
    if ((first_ch) < 0 || (last_ch) > (max_ch)) {       \
        aim_printf (&uc->pvs, "%s must be 0-%d\n",      \
                    (prefix), ((max_ch) - 1));          \
        return 0;                                       \
    }

extern int bf_pltfm_get_sfp_ctx (struct sfp_ctx_t
                                 **sfp_ctx);


/* See bf_qsfp.h */
static char *bf_sfp_ctrlmask_str[] = {
    "BF_TRANS_CTRLMASK_RX_CDR_OFF       ",//(1 << 0)
    "BF_TRANS_CTRLMASK_TX_CDR_OFF       ",//(1 << 1)
    "BF_TRANS_CTRLMASK_LASER_OFF        ",//(1 << 2)
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "BF_TRANS_CTRLMASK_OVERWRITE_DEFAULT",//(1 << 7)
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "BF_TRANS_CTRLMASK_IGNORE_RX_LOS    ",//(1 << 16)
    "BF_TRANS_CTRLMASK_IGNORE_RX_LOL    ",//(1 << 17)
    "BF_TRANS_CTRLMASK_FSM_LOG_ENA      ",//(1 << 18)
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ", //(1 << 32, never reach here)
};

#if 0
static void check_check_code (ucli_context_t *uc,
                           uint8_t *buf, int start, int end)
{
    int i;
    uint32_t sum = 0;
    for (i = start; i <= end; i++) {
        sum += buf[i];
    }
    aim_printf (&uc->pvs,
        "sum of Byte %d-%d is 0x%llx\n", start, end, sum);
    aim_printf (&uc->pvs, "checksum is 0x%x\n",
        buf[end + 1]);
    if ((sum & 0xff) == buf[end + 1]) {
        aim_printf (&uc->pvs, "check_code correct!\n");
    } else {
        aim_printf (&uc->pvs, "check_code incorrect!\n");
    }
    return;
}
#endif

extern void bf_sfp_ddm_convert (const uint8_t *a2h,
             double *temp, double *volt, double *txBias, double *txPower, double *rxPower);

static void hex_dump (ucli_context_t *uc,
                   uint8_t *buf, uint32_t len)
{
    uint8_t byte;

    for (byte = 0; byte < len; byte++) {
        if ((byte % 16) == 0) {
            aim_printf (&uc->pvs, "\n%3d : ", byte);
        }
        aim_printf (&uc->pvs, "%02x ", buf[byte]);
    }
    aim_printf (&uc->pvs, "\n");
}

extern int sff_db_get (sff_db_entry_t **entries,
                       int *count);

static void
sff_info_show (sff_info_t *info,
               ucli_context_t *uc)
{
    aim_printf (&uc->pvs,
                "Vendor: %s Model: %s SN: %s Type: %s Module: %s Media: %s Length: %d %s\n",
                info->vendor, info->model, info->serial,
                info->sfp_type_name,
                info->module_type_name, info->media_type_name,
                info->length,
                info->length_desc);
}

/* see SFF-8472. */
static char *sfp_power_class (uint8_t b64)
{
    /* Maximum power is declared in A2h, byte 66. */
    if ((b64 & 0xC0) == 0x80) {
      return " 3 (2.0 W max.)";
    }
    if ((b64 & 0xC0) == 0x40) {
      return " 2 (1.5 W max.)";
    }
    if ((b64 & 0xC0) == 0x00) {
      return " 1 (1.0 W max.)";
    }
    return "...           ";
}

bool bf_sfp_get_spec_rev_str (uint8_t *idprom,
                              char *rev_str)
{
    uint8_t rev = idprom[94];
    if (!SFF8472_DOM_SUPPORTED (idprom)) {
        /* Not SFF-8472 ? */
        return false;
    }

    switch (rev) {
        case 0x0:
            strcpy (rev_str, "Not specified");
            break;
        case 0x1:
            strcpy (rev_str, "9.3");
            break;
        case 0x2:
            strcpy (rev_str, "9.5");
            break;
        case 0x3:
            strcpy (rev_str, "10.2");
            break;
        case 0x4:
            strcpy (rev_str, "10.4");
            break;
        case 0x5:
            strcpy (rev_str, "11.0");
            break;
        case 0x6:
            strcpy (rev_str, "11.3");
            break;
        case 0x7:
            strcpy (rev_str, "11.4");
            break;
        case 0x8:
            strcpy (rev_str, "12.3");
            break;
        case 0x9:
            strcpy (rev_str, "12.4");
            break;
        default:
            strcpy (rev_str, "Unknown");
            break;
    }

    return true;
}

static ucli_status_t
bf_sfp_dump_info (ucli_context_t
                                 *uc, int port, uint8_t *idprom)
{
    uint8_t *a0h, *a2h;
    sff_info_t *sff;
    sff_eeprom_t *se, eeprom;
    sff_dom_info_t sdi;
    int rc;

    se = &eeprom;
    sff = &se->info;
    memset (se, 0, sizeof (sff_eeprom_t));
    rc = sff_eeprom_parse (se, idprom);
    if (!se->identified) {
        aim_printf (&uc->pvs,
                  " SFP    %02d: non-standard <rc=%d>\n",
                  port, rc);
        /* sff_eeprom_parse is quite an importand API for most sfp.
         * but it is still not ready for all kind of sfps.
         * so override the failure here and keep tracking.
         * by tsihang, 2022-06-17. */
        //return 0;
    }

    a0h = idprom;
    a2h = idprom + MAX_SFP_PAGE_SIZE;
    sff_dom_info_get (&sdi, sff, a0h, a2h);

    /* keep same with qsfp dump. And the info below is quite enough for almost all scene. */
    aim_printf (&uc->pvs, "%-30s = %02d(Y%d)\n",
              "Port",
              port, port);

    aim_printf (&uc->pvs, "%-30s = %s(0x%02x)\n",
              "Module type identifier", sff->sfp_type_name,
              a0h[0]);
    aim_printf (&uc->pvs, "%-30s = %02x\n",
               "Ext. Identifer", a0h[1]);

    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Module", sff->module_type_name);
    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Media Type", sff->media_type_name);
    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Connector Type", sff_connector_type_name (a0h[2]));

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor name", sff->vendor);
    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor P/N", sff->model);
    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor S/N", sff->serial);
    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Vendor Rev", sff->rev);
    aim_printf (&uc->pvs, "%-30s = %02x-%02x-%02x\n",
              "Vendor OUI", sff->oui[0], sff->oui[1],
              sff->oui[2]);

    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Date & lot code",
              sff->date);

    aim_printf (&uc->pvs, "%-30s = ",
              "Memory map format");
    if (sdi.spec == SFF_DOM_SPEC_SFF8472) {
        aim_printf (&uc->pvs, "SFF-8472\n");
    } else {
        aim_printf (&uc->pvs, "Unknown\n");
    }

    char memmap_spec_rev[25] = "Unknown";
    bf_sfp_get_spec_rev_str (a0h, memmap_spec_rev);
    aim_printf (&uc->pvs, "%-30s = %s\n",
              "Memory map spec rev", memmap_spec_rev);

    aim_printf (&uc->pvs, "%-30s = %02x(%02x)\n",
              "CC_BASE", se->cc_base, a0h[63]);
    aim_printf (&uc->pvs, "%-30s = %02x(%02x)\n",
              "CC_EXT", se->cc_ext, a0h[95]);

    aim_printf (&uc->pvs, "  Length (SMF): %d km\n",
              _sff8472_length_sm(a0h) / 1000);
    aim_printf (&uc->pvs, "  Length (OM3): %d m\n",
              _sff8472_length_om3(a0h));
    aim_printf (&uc->pvs, "  Length (OM2): %d m\n",
              _sff8472_length_om2(a0h));
    aim_printf (&uc->pvs, "  Length (OM1): %d m\n",
              _sff8472_length_om1(a0h));
    aim_printf (&uc->pvs, "  Length (Copper): %d m\n",
              _sff8472_length_cu(a0h));

    aim_printf (&uc->pvs, "\n");
    if (!bf_sfp_is_optical (port)) {
        aim_printf (&uc->pvs,
                  "Passive copper.\n");
    } else {
        /* A2h */
        aim_printf (&uc->pvs,
                  "## MODULE EXT PROPERTIES\n");

        aim_printf (&uc->pvs, "%-30s = %5d Mbps\n",
                  "Nominal Bit Rate",
                  a0h[12] * 100);

        aim_printf (&uc->pvs, "%-30s = %02x\n",
                  "Rate Identifier",
                  a0h[13]);

        aim_printf (&uc->pvs, "%-30s = %u nm\n",
                  "Laser Wavelength",
                  (a0h[60] << 8) | a0h[61]);

        double temp;
        double volt;
        double txBias;
        double txPower;
        double rxPower;
        bf_sfp_ddm_convert (a2h, &temp, &volt, &txBias, &txPower, &rxPower);

        aim_printf (
            &uc->pvs, " SFP DDM Info for ports %d to %d\n",
            port, port);
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
                "Port",
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

        aim_printf (&uc->pvs,
                   "      %d %10.1f %15.2f %10.2f %16.2f %16.2f\n",
                   port,
                   temp,
                   volt,
                   txBias,
                   txPower,
                   rxPower);

        aim_printf (&uc->pvs,
                  "%10s %10s %15s %10s %16s %16s\n",
                  "-------",
                  "--------",
                  "-----------",
                  "--------",
                  "--------------",
                  "--------------");

        aim_printf (&uc->pvs, "Page: %d\n", a2h[127]);
    }

    aim_printf (&uc->pvs, "\nA0h:\n");
    hex_dump (uc, a0h, MAX_SFP_PAGE_SIZE);

    aim_printf (&uc->pvs, "\nA2h:\n");
    hex_dump (uc, a2h, MAX_SFP_PAGE_SIZE);

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_get_pres (
 ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "get-pres", 0,
        "get module present state");

    uint32_t lower_ports, upper_ports;

    if (bf_sfp_get_transceiver_pres (&lower_ports,
                                   &upper_ports)) {
        aim_printf (&uc->pvs,
                 "error getting sfp presence\n");
        return 0;
    }
    aim_printf (&uc->pvs,
             "qsfp presence lower: 0x%08x upper: 0x%08x\n",
             lower_ports,
             upper_ports);
    return 0;
}

 static ucli_status_t
 bf_pltfm_ucli_ucli__sfp_tx_disable_set (
     ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "tx-disable-set", 2,
        "<port OR -1 for all ports> "
        "<0: laser on, 1: laser off>");

    int val;
    int max_port = bf_sfp_get_max_sfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    val = atoi (uc->pargs->args[1]);
    if ((val < 0) || (val > 1)) {
        aim_printf (&uc->pvs,
                 "val must be 0-1 for port %d\n", port);
        return 0;
    }

    for (port = first_port; port <= last_port;
         port ++) {
        if (!bf_sfp_is_present (port))
            continue;

        if (val) {
            bf_sfp_tx_disable (port, true);
        } else {
            bf_sfp_tx_disable (port, false);
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_reset (ucli_context_t
                                 *uc)
{
    UCLI_COMMAND_INFO (uc,
        "sfp-reset", 2,
        "<port OR -1 for all ports> "
        "<1: reset, 0: unreset>");

    int reset_val;
    bool reset;
    int max_port = bf_sfp_get_max_sfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    reset_val = atoi (uc->pargs->args[1]);
    reset = (reset_val == 0) ? false : true;

    for (port = first_port; port <= last_port;
         port ++) {
        aim_printf (
            &uc->pvs,
            "Not supported.\n");
        bf_sfp_reset (port, reset);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_read_reg (ucli_context_t
                                  *uc)
{
    UCLI_COMMAND_INFO (uc,
        "read-reg", 4,
        "<port> <addr, 0xa0/0xa2> <page,0/1/2> <offset>");

    int err;
    int offset;
    uint8_t addr;
    uint8_t page;
    uint8_t val;
    int port;
    int max_port = bf_sfp_get_max_sfp_ports();

    port = strtol (uc->pargs->args[0], NULL, 0);
    addr = strtol (uc->pargs->args[1], NULL, 0);
    page = strtol (uc->pargs->args[2], NULL, 0);
    offset = strtol (uc->pargs->args[3], NULL, 0);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    aim_printf (
        &uc->pvs,
        "read-reg: read-reg <port=%d> <addr=0x%x> <page=%d> <offset=0x%x>\n",
        port,
        addr,
        page,
        offset);

    if (addr != 0xa0 && addr != 0xa2) {
        aim_printf (&uc->pvs,
                    "only addr 0xa0 or 0xa2 can be read!\n");
        return -1;
    }

    offset = (addr == 0xa0) ? offset : offset +
             MAX_SFP_PAGE_SIZE;
    err = bf_pltfm_sfp_read_reg (port, page, offset,
                                 &val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) reading register offset=0x%02x\n", err,
                    offset);
        return -1;
    } else {
        aim_printf (&uc->pvs,
                    "SFP%d : page %d offset 0x%x read 0x%02x\n",
                    port, page,
                    offset, val);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_write_reg (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "write-reg", 5,
        "<port> <addr, 0xa0/0xa2> <page,0/1/2> <offset> <val>");

    int err;
    int offset;
    uint8_t addr;
    uint8_t page;
    uint8_t val;
    int port;
    int max_port = bf_sfp_get_max_sfp_ports();

    port = strtol (uc->pargs->args[0], NULL, 0);
    addr = strtol (uc->pargs->args[1], NULL, 0);
    page = strtol (uc->pargs->args[2], NULL, 0);
    offset = strtol (uc->pargs->args[3], NULL, 0);
    val = strtol (uc->pargs->args[4], NULL, 0);

    if (port < 1 || port > max_port) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    max_port);
        return 0;
    }

    aim_printf (
        &uc->pvs,
        "write-reg: write-reg <port=%d> <addr=0x%x> <page=%d> <offset=0x%x> "
        "<val=0x%x>\n",
        port,
        addr,
        page,
        offset,
        val);

    if (addr != 0xa0 && addr != 0xa2) {
        aim_printf (&uc->pvs,
                    "only addr 0xa0 or 0xa2 can be read!\n");
        return -1;
    }

    offset = (addr == 0xa0) ? offset : offset +
             MAX_SFP_PAGE_SIZE;
    err = bf_pltfm_sfp_write_reg (port, page,
                                  offset, val);
    if (err) {
        aim_printf (&uc->pvs,
                    "error(%d) writing register offset=0x%02x\n", err,
                    offset);
        return -1;
    } else {
        aim_printf (&uc->pvs,
                    "SFP%d : page %d offset 0x%x written 0x%0x\n",
                    port, page,
                    offset, val);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_dump_info (ucli_context_t
                                   *uc)
{
    int port;
    int rc;
    uint8_t idprom[MAX_SFP_PAGE_SIZE * 2 + 1] = {0};

    UCLI_COMMAND_INFO (uc,
        "dump-info", 1,
        "<port>");

    port = strtol (uc->pargs->args[0], NULL, 0);
    if (port < 1 || port > bf_sfp_get_max_sfp_ports()) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    bf_sfp_get_max_sfp_ports());
        return 0;
    }

    if (!bf_sfp_is_present (port)) {
        return 0;
    }

    /* A0h */
    rc = bf_pltfm_sfp_read_module (port, 0,
                                      MAX_SFP_PAGE_SIZE, idprom);
    if (rc) {
        return 0;
    }

    /* A2h, what should we do if there's no A2h ? */
    if (SFF8472_DOM_SUPPORTED (
            idprom)) {
        rc = bf_pltfm_sfp_read_module (port,
                                      MAX_SFP_PAGE_SIZE, MAX_SFP_PAGE_SIZE,
                                      idprom + MAX_SFP_PAGE_SIZE);
        if (rc) {
            return 0;
        }
    }
    return bf_sfp_dump_info (uc, port, idprom);
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_show (ucli_context_t
                              *uc)
{
    UCLI_COMMAND_INFO (uc,
        "show", 0,
        "show sfp or sfp28 summary information");

    uint32_t flags = bf_pltfm_mgr_ctx()->flags;
    bool temper_monitor_en =
       (0 != (flags & AF_PLAT_MNTR_SFP_REALTIME_DDM));

    aim_printf (&uc->pvs,
               "Max ports supported  ==> %2d\n", bf_sfp_get_max_sfp_ports());
    aim_printf (&uc->pvs,
               "Temperature monitor enabled ==> %s",
               temper_monitor_en ? "YES" : "NO");
    if (temper_monitor_en) {
       aim_printf (&uc->pvs,
                   " (interval %d seconds)\n",
                   bf_port_sfp_mgmnt_temper_monitor_period_get());
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_show_module (
    ucli_context_t
    *uc)
{
    UCLI_COMMAND_INFO (uc,
        "module-show", 1,
        "<port>");

    int port;
    int rc;
    uint8_t idprom[MAX_SFP_PAGE_SIZE * 2 + 1] = {0};

    port = strtol (uc->pargs->args[0], NULL, 0);
    if (port < 1 || port > bf_sfp_get_max_sfp_ports()) {
        aim_printf (&uc->pvs, "port must be 1-%d\n",
                    bf_sfp_get_max_sfp_ports());
        return 0;
    }

    if (!bf_sfp_is_present (port)) {
        return 0;
    }

    /* A0h */
    rc = bf_sfp_get_cached_info (port, 0, idprom);
    if (rc) {
        aim_printf (&uc->pvs,
                    "Unknown module : %2d\n",
                    port);
        return 0;
    }

    /* A2h */
    rc = bf_sfp_get_cached_info (port, 1,
                                 idprom + MAX_SFP_PAGE_SIZE);
    if (rc) {
        aim_printf (&uc->pvs,
                    "Unknown module : %2d\n",
                    port);
        return 0;
    }

    return bf_sfp_dump_info (uc, port, idprom);
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_summary (ucli_context_t
                                  *uc)
{
    int port;
    int rc;
    uint8_t *a0h, *a2h;
    sff_info_t *sff;
    sff_eeprom_t *se, eeprom;
    sff_dom_info_t sdi;
    uint8_t idprom[MAX_SFP_PAGE_SIZE * 2 + 1] = {0};

    UCLI_COMMAND_INFO (uc, "info", 0, "info");

    aim_printf (&uc->pvs, "\n================\n");
    aim_printf (&uc->pvs, "Info for  SFP  :\n");
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

    for (port = 1; port <= bf_sfp_get_max_sfp_ports(); port++) {
        if (!bf_sfp_is_present (port)) {
            continue;
        }

        /* A0h */
        rc = bf_sfp_get_cached_info (port, 0, idprom);
        if (rc) {
            aim_printf (&uc->pvs,
                        "Unknown module : %2d\n",
                        port);
            return 0;
        }

        /* A2h */
        rc = bf_sfp_get_cached_info (port, 1,
                                     idprom + MAX_SFP_PAGE_SIZE);
        if (rc) {
            aim_printf (&uc->pvs,
                        "Unknown module : %2d\n",
                        port);
            return 0;
        }

        se = &eeprom;
        sff = &se->info;
        memset (se, 0, sizeof (sff_eeprom_t));
        rc = sff_eeprom_parse (se, idprom);
        if (!se->identified) {
            aim_printf (&uc->pvs,
                        " SFP    %02d: non-standard <rc=%d>\n",
                        port, rc);
            /* sff_eeprom_parse is quite an importand API for most sfp.
             * but it is still not ready for all kind of sfps.
             * so override the failure here and keep tracking.
             * by tsihang, 2022-06-17. */
            //return 0;
        }

        a0h = idprom;
        a2h = idprom + MAX_SFP_PAGE_SIZE;
        sff_dom_info_get (&sdi, sff, a0h, a2h);

        /* print lower page data */
        aim_printf (&uc->pvs, " %2d:  ", port);

        aim_printf (&uc->pvs, "%s ", sff->vendor);
        aim_printf (&uc->pvs, "%s ", sff->model);
        aim_printf (&uc->pvs, "%s ", sff->rev);
        aim_printf (&uc->pvs, "%s ", sff->serial);
        aim_printf (&uc->pvs, "%s ", sff->date);
        aim_printf (&uc->pvs, "%5d MBps ",
                    a0h[12] * 100);
        aim_printf (&uc->pvs,
                    "  %02x:%02x:%02x",
                    sff->oui[0],
                    sff->oui[1],
                    sff->oui[2]);
        aim_printf (&uc->pvs, "  %s ",
                    sfp_power_class (a0h[64]));

        if (bf_sfp_is_optical (port)) {
            if (_sff8472_length_sm(a0h)) {
                /* a0h[14] in units km, max 254 km. */
                /* a0h[15] in units 100m, max 25.4 km. */
                aim_printf (&uc->pvs, "  (SMF): %d km ",
                        _sff8472_length_sm(a0h) / 1000);

            }
            if (_sff8472_length_om4 (a0h)) {
                aim_printf (&uc->pvs, "  (OM4): %d m ",
                            _sff8472_length_om4(a0h) * 2);
            }
            if (_sff8472_length_om3(a0h)) {
                aim_printf (&uc->pvs, "  (OM3): %d m ",
                            _sff8472_length_om3(a0h) * 2);
            }
            if (_sff8472_length_om2(a0h)) {
                aim_printf (&uc->pvs, "  (OM2): %d m ",
                            _sff8472_length_om2(a0h));
            }
            if (_sff8472_length_om1(a0h)) {
                aim_printf (&uc->pvs, "  (OM1): %d m ",
                            _sff8472_length_om1(a0h));
            }
        } else {
            if (_sff8472_length_cu(a0h)) {
                aim_printf (&uc->pvs, "  (Copper): %d m ",
                            _sff8472_length_cu(a0h));
            }
        }
        aim_printf (&uc->pvs, "\n");
    }
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_get_ddm (ucli_context_t
                                *uc)
{
    UCLI_COMMAND_INFO (uc,
        "get-ddm", -1,
        "[sport] [dport]");

    uint8_t idprom[MAX_SFP_PAGE_SIZE * 2 + 1] = {0};
    uint8_t *a2h = &idprom[MAX_SFP_PAGE_SIZE];
    int rc;
    int max_port = bf_sfp_get_max_sfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    aim_printf (
        &uc->pvs, " SFP DDM Info for ports %d to %d\n",
        first_port, last_port);
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
              "Port",
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

    for (port = first_port; port <= last_port; port++) {
        if (!bf_sfp_is_present (port) ||
            (!bf_sfp_is_optical (port))) {
            continue;
        }

        if (bf_sfp_is_sff8472 (port)) {
            memset (idprom, 0, sizeof (idprom));

            /* A2h, what should we do if there's no A2h ? */
            rc = bf_pltfm_sfp_read_module (port,
                                          MAX_SFP_PAGE_SIZE + 96, 10,
                                          idprom + MAX_SFP_PAGE_SIZE + 96);
            if (rc) {
                return 0;
            }

            double temp;
            double volt;
            double txBias;
            double txPower;
            double rxPower;
            bf_sfp_ddm_convert (a2h, &temp, &volt, &txBias, &txPower, &rxPower);
            aim_printf (&uc->pvs,
                       "      %d %10.1f %15.2f %10.2f %16.2f %16.2f\n",
                       port,
                       temp,
                       volt,
                       txBias,
                       txPower,
                       rxPower);
/*
            sfp_transciever_info_t info, *pinfo;
            pinfo = &info;
            memset (pinfo, 0, sizeof (sfp_transciever_info_t));
            bf_sfp_get_transceiver_info (port, pinfo);
            aim_printf (&uc->pvs,
                   "      %d %10.1f %15.2f %10.2f %16.2f %16.2f===\n",
                   port,
                   pinfo->sensor.temp.value,
                   pinfo->sensor.vcc.value,
                   pinfo->chn[0].sensors.tx_bias.value,
                   pinfo->chn[0].sensors.tx_pwr.value,
                   pinfo->chn[0].sensors.rx_pwr.value);
*/
        }

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

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_db (ucli_context_t
                            *uc)
{
    UCLI_COMMAND_INFO (uc,
        "db", 0,
        "display SFP database.");

    sff_info_t *sff;
    sff_eeprom_t *se;
    sff_db_entry_t *entry;
    int num = 0;
    int i;

    sff_db_get (&entry, &num);
    for (i = 0; i < num; i ++) {
        se = &entry[i].se;
        sff = &se->info;
        if (sff->sfp_type == SFF_SFP_TYPE_SFP ||
            sff->sfp_type == SFF_SFP_TYPE_SFP28) {
            sff_info_show (sff, uc);
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_map (ucli_context_t
                             *uc)
{
    UCLI_COMMAND_INFO (uc,
        "map", 0,
        "display SFP map.");

    int port, err;
    uint32_t conn_id, chnl_id = 0;
    char alias[16] = {0}, connc[16] = {0};

    /* Dump the map of Module <-> Alias <-> QSFP/CH <-> Present. */
    aim_printf (&uc->pvs, "%12s%12s%20s%12s\n",
                "MODULE", "ALIAS", "PORT", "TX_DISABLED");

    for (int i = 0; i < bf_sfp_get_max_sfp_ports();
         i ++) {
        port = (i + 1);
        err = bf_pltfm_sfp_lookup_by_module (port,
                                             &conn_id, &chnl_id);
        if (!err) {
            sprintf (alias, "Y%d",
                     port % (BF_PLAT_MAX_QSFP * 4));
            sprintf (connc, "%2d/%d",
                     (conn_id % BF_PLAT_MAX_QSFP),
                     (chnl_id % MAX_CHAN_PER_CONNECTOR));
            aim_printf (&uc->pvs, "%12d%12s%20s%12s\n",
                        port, alias, connc,
                        bf_sfp_is_present (port) ? bf_sfp_tx_is_disabled (port) ? "true" : "false" : "----");
        }
    }

    /* vSFP, always true. */
    aim_printf (&uc->pvs, "%12s\n",
                "===vSFP===");
    for (int i = 0;
         i < bf_pltfm_get_max_xsfp_ports(); i ++) {
        port = (i + 1);
        err  = bf_pltfm_xsfp_lookup_by_module (port,
                                               &conn_id,
                                               &chnl_id);
        if (!err) {
            sprintf (alias, "X%d", port);
            sprintf (connc, "%2d/%d",
                     (conn_id % BF_PLAT_MAX_QSFP),
                     (chnl_id % MAX_CHAN_PER_CONNECTOR));
            aim_printf (&uc->pvs, "%12d%12s%20s%12s\n",
                        bf_sfp_get_max_sfp_ports() + port, alias,
                        connc, "false");
        }
    }

    return 0;
}

extern char *bf_pm_intf_sfp_fsm_st_get (int port);
static ucli_status_t
bf_pltfm_ucli_ucli__sfp_fsm (ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "fsm", -1,
        "[sport] [dport]");

    int max_port = bf_sfp_get_max_sfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        first_port = last_port = port;
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            last_port = atoi (uc->pargs->args[1]);
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    aim_printf (&uc->pvs, "%s | ", "Port");
    aim_printf (&uc->pvs, "%-19s | ", "Module FSM");
    aim_printf (&uc->pvs, "%-19s | ", "CH ");
    aim_printf (&uc->pvs, "%-19s | \n", "PM_INTF FSM");

    for (port = first_port; port <= last_port;
      port++) {
        aim_printf (&uc->pvs, "%-4d | %s | ", port,
                    sfp_module_fsm_st_get (port));
        aim_printf (&uc->pvs, "%-19s | ",
                    sfp_channel_fsm_st_get (port));
        if (platform_type_equal(AFN_X732QT)) {
            aim_printf (&uc->pvs, "%-19s | ",
                        bf_pm_intf_sfp_fsm_st_get (port));
        } else {
            aim_printf (&uc->pvs, "%-19s | ",
                        "----------");
        }

        aim_printf (&uc->pvs, "\n");
    }
    return 0;
}
static ucli_status_t
bf_pltfm_ucli_ucli__sfp_special_case_ctrlmask_set (
        ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "ctrlmask-set", -1,
        "[port OR -1 for all ports] "
        "[ctrlmask, hex value]. Dump all ctrlmask if no input arguments.");

    uint32_t ctrlmask = 0, ctrlmasks[BF_PLAT_MAX_SFP + 1] = {0};
    int max_port = bf_sfp_get_max_sfp_ports();
    int port, first_port = 1, last_port = max_port;

    if (uc->pargs->count > 0) {
        port = atoi (uc->pargs->args[0]);
        if (port > 0) {
            first_port = last_port = port;
        } else {
            // 1 - max_port;
        }
        /* only parse first 2 args. */
        if (uc->pargs->count > 1) {
            ctrlmask = strtol (uc->pargs->args[1], NULL, 16);;
        }
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port++) {
        bf_sfp_ctrlmask_get (port, &ctrlmasks[port]);
    }

    if (uc->pargs->count <= 1) goto dump;

    for (port = first_port; port <= last_port;
         port++) {
        ctrlmasks[port] = ctrlmask;
        bf_sfp_ctrlmask_set (port, ctrlmasks[port]);
    }

dump:
    for (port = first_port; port <= last_port;
         port++) {
        bf_sfp_ctrlmask_get (port, &ctrlmasks[port]);
        aim_printf (&uc->pvs, "\n\nPort : %2d, 0x%-8X\n\n",
            port, ctrlmasks[port]);
        for (int bit = 0; bit < 32; bit ++) {
            if (bf_sfp_ctrlmask_str[bit][0] != ' ') {
                aim_printf (&uc->pvs, "%35s (bit=%2d) -> %s\n",
                    bf_sfp_ctrlmask_str[bit],
                    bit,
                    ((ctrlmasks[port] >> bit) & 1) ? "enabled" : "disabled");
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_soft_remove (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "soft-remove", 2,
        "<port OR -1 for all ports> "
        "<set/clear>");

    bool remove_flag = 0;
    int max_port = bf_sfp_get_max_sfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    if (strcmp (uc->pargs->args[1], "set") == 0) {
        remove_flag = true;
    } else if (strcmp (uc->pargs->args[1],
                       "clear") == 0) {
        remove_flag = false;
    } else {
        aim_printf (&uc->pvs,
                    "Usage: front-panel port-num <1 to %d or -1 for all ports> <set "
                    "or clear>\n",
                    max_port);
        return 0;
    }

    for (port = first_port; port <= last_port;
         port++) {
        if (bf_port_sfp_mgmnt_temper_high_alarm_flag_get (
                port) &&
            bf_sfp_is_present (port)) {
            bf_sfp_set_pwr_ctrl (port, 0);
        }
        bf_sfp_soft_removal_set (port, remove_flag);
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_soft_remove_show (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "soft-remove-show", 1,
        "<port OR -1 for all ports>");

    int max_port = bf_sfp_get_max_sfp_ports();
    int port, first_port = 1, last_port = max_port;

    port = atoi (uc->pargs->args[0]);
    if (port > 0) {
        first_port = last_port = port;
    }
    BF_UCLI_PORT_VALID(port, first_port, last_port, max_port, "port");

    for (port = first_port; port <= last_port;
         port++) {
        aim_printf (&uc->pvs,
                    "port %d soft-removed %s\n",
                    port,
                    bf_sfp_soft_removal_get (port) ? "Yes" : "No");
    }
    return 0;
}

static ucli_status_t bf_pltfm_ucli_ucli__sfp_rxlos_debounce_set(
    ucli_context_t *uc) {

    UCLI_COMMAND_INFO(uc,
        "sfp-rxlos-debounce-set", 2,
        "<port> <count>");

    int port, count, max_port;

    max_port = bf_sfp_get_max_sfp_ports();
    port = atoi(uc->pargs->args[0]);
    if (port < 1 || port > max_port) {
        aim_printf(&uc->pvs, "port must be 1-%d\n", max_port);
        return 0;
    }

    count = atoi(uc->pargs->args[1]);
    if (count < 0) {
        aim_printf(&uc->pvs, "count must be 0 or more\n");
        return 0;
    }

    bf_sfp_rxlos_debounce_set(port, count);
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_mgmnt_temper_monit_period_set (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "sfp-temper-period-set", 1,
        "<poll period in sec. min:1> Default 5-sec");

    int64_t period_secs;

    if (uc->pargs->count == 1) {
        period_secs = strtol (uc->pargs->args[0], NULL,
                              10);
        if ((period_secs != LONG_MIN) &&
            (period_secs != LONG_MAX) &&
            (period_secs > 0)) {
            bf_port_sfp_mgmnt_temper_monitor_period_set (
                period_secs);
            return 0;
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_mgmnt_temper_monit_log_enable (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "sfp-realtime-ddm-log", 1,
        "<1=Enable, 0=Disable>");

    bool enable;

    enable = atoi (uc->pargs->args[0]);
    if (uc->pargs->count == 1) {
        if (enable) {
            bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_SFP_REALTIME_DDM_LOG;
        } else {
            bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_MNTR_SFP_REALTIME_DDM_LOG;
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__sfp_mgmnt_temper_monitor_enable (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
        "sfp-realtime-ddm-monit-enable", 1,
        "<1=Enable, 0=Disable>");

    bool enable;

    enable = atoi (uc->pargs->args[0]);
    if (uc->pargs->count == 1) {
        if (enable) {
            bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_SFP_REALTIME_DDM;
        } else {
            bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_MNTR_SFP_REALTIME_DDM;
        }
    }

    return 0;
}

#if 0
/* Unused CMD if cc_base and cc_ext have been dumped by cmd dump-info.
 * by tsihang, 2021-07-29. */
static ucli_status_t
bf_pltfm_ucli_ucli__check_reg (ucli_context_t
                               *uc)
{
    // define
    int module;
    uint8_t a0h[MAX_SFP_PAGE_SIZE + 1] = {0};
    uint8_t a2h[MAX_SFP_PAGE_SIZE + 1] = {0};
    // convert
    UCLI_COMMAND_INFO (uc, "check-reg", 1,
                       "check-reg <module>");
    module = strtol (uc->pargs->args[0], NULL, 0);

    if (!bf_sfp_is_present (module)) {
        return 0;
    }

    // read eeprom info directly
    if (bf_pltfm_sfp_read_module (module, 0,
                                  MAX_SFP_PAGE_SIZE, a0h)) {
        aim_printf (&uc->pvs,
                    "cannot get SFP %d eeprom info, there's maybe something wrong\n",
                    module);
        return BF_PLTFM_INVALID_ARG;
    }
    if (bf_pltfm_sfp_read_module (module,
                                  MAX_SFP_PAGE_SIZE, MAX_SFP_PAGE_SIZE, a2h)) {
        aim_printf (&uc->pvs,
                    "cannot get SFP %d eeprom info, there's maybe something wrong\n",
                    module);
        return BF_PLTFM_INVALID_ARG;
    }

    // show it all
    aim_printf (&uc->pvs, "SFP Y%02d:\n", module);
    aim_printf (&uc->pvs,
                "=======================\n");
    aim_printf (&uc->pvs, "0xA0:\n");
    hex_dump (uc, a0h, MAX_SFP_PAGE_SIZE);
    check_check_code (uc, a0h, 0, 62);
    check_check_code (uc, a0h, 64, 94);
    aim_printf (&uc->pvs,
                "\n=======================\n");
    aim_printf (&uc->pvs, "0xA2:\n");
    hex_dump (uc, a2h, MAX_SFP_PAGE_SIZE);
    check_check_code (uc, a2h, 0, 94);
    // return
    return 0;
}
#endif

static ucli_command_handler_f
bf_pltfm_sfp_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__sfp_read_reg,
    bf_pltfm_ucli_ucli__sfp_write_reg,
    bf_pltfm_ucli_ucli__sfp_dump_info,
    bf_pltfm_ucli_ucli__sfp_show_module,
    bf_pltfm_ucli_ucli__sfp_show,
    bf_pltfm_ucli_ucli__sfp_summary,
    bf_pltfm_ucli_ucli__sfp_get_ddm,
    bf_pltfm_ucli_ucli__sfp_db,
    bf_pltfm_ucli_ucli__sfp_map,
    bf_pltfm_ucli_ucli__sfp_fsm,
    bf_pltfm_ucli_ucli__sfp_special_case_ctrlmask_set,
    bf_pltfm_ucli_ucli__sfp_soft_remove,
    bf_pltfm_ucli_ucli__sfp_soft_remove_show,
    bf_pltfm_ucli_ucli__sfp_get_pres,
    bf_pltfm_ucli_ucli__sfp_tx_disable_set,
    bf_pltfm_ucli_ucli__sfp_reset,
    bf_pltfm_ucli_ucli__sfp_mgmnt_temper_monit_period_set,
    bf_pltfm_ucli_ucli__sfp_mgmnt_temper_monit_log_enable,
    bf_pltfm_ucli_ucli__sfp_mgmnt_temper_monitor_enable,
    bf_pltfm_ucli_ucli__sfp_rxlos_debounce_set,
    /* Closed by tsihang since dump-info include the CC. */
    //bf_pltfm_ucli_ucli__check_reg,
    NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t bf_pltfm_sfp_ucli_module__
= {
    "sfp_ucli", NULL, bf_pltfm_sfp_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_sfp_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_sfp_ucli_module__);
    n = ucli_node_create ("sfp", m,
                          &bf_pltfm_sfp_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("sfp"));
    return n;
}
