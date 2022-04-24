/*!
 * @file sff.c
 * @date 2021/07/15
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <bf_qsfp/sff_standards.h>


extern int
sff_nonstandard_lookup (sff_info_t *info);

sff_sfp_type_t
sff_sfp_type_get (const uint8_t *eeprom)
{
    if (eeprom) {
        if (SFF8472_MODULE_SFP (eeprom)) {
            return SFF_SFP_TYPE_SFP;
        }
        if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)) {
            return SFF_SFP_TYPE_QSFP_PLUS;
        }
        if (SFF8636_MODULE_QSFP28 (eeprom)) {
            return SFF_SFP_TYPE_QSFP28;
        }
    }
    return SFF_SFP_TYPE_INVALID;
}

sff_module_type_t
sff_module_type_get (const uint8_t *eeprom)
{
    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_AOC (eeprom)) {
        return SFF_MODULE_TYPE_100G_AOC;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_SR4 (eeprom)) {
        return SFF_MODULE_TYPE_100G_BASE_SR4;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_LR4 (eeprom)) {
        return SFF_MODULE_TYPE_100G_BASE_LR4;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && (SFF8636_MEDIA_100GE_CR4 (eeprom) ||
            SFF8636_MEDIA_25GE_CR_S (eeprom) ||
            SFF8636_MEDIA_25GE_CR_N (eeprom))) {
        return SFF_MODULE_TYPE_100G_BASE_CR4;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_ER4 (eeprom)) {
        return SFF_MODULE_TYPE_100G_BASE_ER4;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_CWDM4 (eeprom)) {
        return SFF_MODULE_TYPE_100G_CWDM4;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_PSM4 (eeprom)) {
        return SFF_MODULE_TYPE_100G_PSM4;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_SWDM4 (eeprom)) {
        return SFF_MODULE_TYPE_100G_SWDM4;
    }

    if (SFF8636_MODULE_QSFP28 (eeprom)
        && SFF8636_MEDIA_EXTENDED (eeprom)
        && SFF8636_MEDIA_100GE_PAM4_BIDI (eeprom)) {
        return SFF_MODULE_TYPE_100G_PAM4_BIDI;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && SFF8436_MEDIA_40GE_CR4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_CR4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && SFF8436_MEDIA_40GE_SR4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_SR4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_sr4_aoc_pre (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_SR4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && SFF8436_MEDIA_40GE_LR4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_LR4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && SFF8436_MEDIA_40GE_ACTIVE (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_ACTIVE;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_aoc_breakout (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_SR4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && SFF8436_MEDIA_40GE_CR (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_CR;
    }

    /* pre-standard finisar optics */
    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_pre (eeprom)
        && (SFF8436_TECH_FC_FIBER_LONG (eeprom)
            || SFF8436_MEDIA_FC_FIBER_SM (eeprom))) {
        return SFF_MODULE_TYPE_40G_BASE_LR4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_pre (eeprom)
        && (SFF8436_TECH_FC_FIBER_SHORT (eeprom)
            || SFF8436_MEDIA_FC_FIBER_MM (eeprom))) {
        return SFF_MODULE_TYPE_40G_BASE_SR4;
    }

    /* pre-standard QSFP-BiDi optics */
    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_sr2_bidi_pre (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_SR2;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_lm4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_LM4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_sm4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_SM4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && SFF8636_MEDIA_40GE_ER4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_ER4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && _sff8436_qsfp_40g_er4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_ER4;
    }

    if (SFF8436_MODULE_QSFP_PLUS_V2 (eeprom)
        && SFF8636_MEDIA_40GE_SWDM4 (eeprom)) {
        return SFF_MODULE_TYPE_40G_BASE_SWDM4;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_sfp28_cr (eeprom)) {
        return SFF_MODULE_TYPE_25G_BASE_CR;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_sfp28_sr (eeprom)) {
        return SFF_MODULE_TYPE_25G_BASE_SR;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_sfp28_lr (eeprom)) {
        return SFF_MODULE_TYPE_25G_BASE_LR;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_sfp28_aoc (eeprom)) {
        return SFF_MODULE_TYPE_25G_BASE_AOC;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_XGE_SR (eeprom)
        && !_sff8472_media_gbe_sx_fc_hack (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_SR;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_XGE_LR (eeprom)
        && !_sff8472_media_gbe_lx_fc_hack (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_LR;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_XGE_LRM (eeprom)
        && !_sff8472_media_gbe_lx_fc_hack (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_LRM;
    }

    /*
     * XXX roth -- PAN-934 -- DAC cable erroneously reports ER,
     * so we need to disallow infiniband features when matching here.
     * See also _sff8472_media_cr_passive, which encodes some
     * additional workarounds for these cables.
     */
    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_XGE_ER (eeprom)
        && !_sff8472_inf_1x_cu_active (eeprom)
        && !_sff8472_inf_1x_cu_passive (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_ER;
    }

    /* XXX roth - not sure on this one */
    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_cr_passive (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_CR;
    }

    /* added 10GBase AOC, by tsihang, 2021-07-26. */
    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_sfp_10g_aoc (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_AOC;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_cr_active (eeprom)) {
        if (_sff8472_sfp_10g_aoc (eeprom)) {
            return SFF_MODULE_TYPE_10G_BASE_SR;
        } else {
            return SFF_MODULE_TYPE_10G_BASE_CR;
        }
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_sfp_10g_base_t (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_T;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_GBE_SX (eeprom)) {
        return SFF_MODULE_TYPE_1G_BASE_SX;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_GBE_LX (eeprom)) {
        if (SFF8472_MEDIA_LENGTH_ZX (eeprom)) {
            return SFF_MODULE_TYPE_1G_BASE_ZX;
        } else {
            return SFF_MODULE_TYPE_1G_BASE_LX;
        }
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_GBE_CX (eeprom)) {
        return SFF_MODULE_TYPE_1G_BASE_CX;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_GBE_T (eeprom)) {
        return SFF_MODULE_TYPE_1G_BASE_T;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_CBE_LX (eeprom)) {
        return SFF_MODULE_TYPE_100_BASE_LX;
    }

    if (SFF8472_MODULE_SFP (eeprom)
        && SFF8472_MEDIA_CBE_FX (eeprom)) {
        return SFF_MODULE_TYPE_100_BASE_FX;
    }

    /* non-standard (e.g. Finisar) ZR media */
    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_zr (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_ZR;
    }

    /* non-standard (e.g. Finisar) SRL media */
    if (SFF8472_MODULE_SFP (eeprom)
        && _sff8472_media_srlite (eeprom)) {
        return SFF_MODULE_TYPE_10G_BASE_SRL;
    }

    return SFF_MODULE_TYPE_INVALID;
}

sff_media_type_t
sff_media_type_get (sff_module_type_t mt)
{
    switch (mt) {
        case SFF_MODULE_TYPE_100G_BASE_CR4:
        case SFF_MODULE_TYPE_40G_BASE_CR4:
        case SFF_MODULE_TYPE_40G_BASE_CR:
        case SFF_MODULE_TYPE_25G_BASE_CR:
        case SFF_MODULE_TYPE_10G_BASE_CR:
        case SFF_MODULE_TYPE_10G_BASE_T:
        case SFF_MODULE_TYPE_1G_BASE_CX:
        case SFF_MODULE_TYPE_1G_BASE_T:
            return SFF_MEDIA_TYPE_COPPER;

        case SFF_MODULE_TYPE_100G_AOC:
        case SFF_MODULE_TYPE_100G_BASE_SR4:
        case SFF_MODULE_TYPE_100G_BASE_LR4:
        case SFF_MODULE_TYPE_100G_BASE_ER4:
        case SFF_MODULE_TYPE_100G_CWDM4:
        case SFF_MODULE_TYPE_100G_PSM4:
        case SFF_MODULE_TYPE_100G_SWDM4:
        case SFF_MODULE_TYPE_100G_PAM4_BIDI:
        case SFF_MODULE_TYPE_40G_BASE_SR4:
        case SFF_MODULE_TYPE_40G_BASE_LR4:
        case SFF_MODULE_TYPE_40G_BASE_LM4:
        case SFF_MODULE_TYPE_40G_BASE_ACTIVE:
        case SFF_MODULE_TYPE_40G_BASE_SR2:
        case SFF_MODULE_TYPE_40G_BASE_SM4:
        case SFF_MODULE_TYPE_40G_BASE_ER4:
        case SFF_MODULE_TYPE_40G_BASE_SWDM4:
        case SFF_MODULE_TYPE_25G_BASE_SR:
        case SFF_MODULE_TYPE_25G_BASE_LR:
        case SFF_MODULE_TYPE_25G_BASE_AOC:
        case SFF_MODULE_TYPE_10G_BASE_AOC:
        case SFF_MODULE_TYPE_10G_BASE_SR:
        case SFF_MODULE_TYPE_10G_BASE_LR:
        case SFF_MODULE_TYPE_10G_BASE_LRM:
        case SFF_MODULE_TYPE_10G_BASE_ER:
        case SFF_MODULE_TYPE_10G_BASE_SX:
        case SFF_MODULE_TYPE_10G_BASE_LX:
        case SFF_MODULE_TYPE_10G_BASE_ZR:
        case SFF_MODULE_TYPE_10G_BASE_SRL:
        case SFF_MODULE_TYPE_1G_BASE_SX:
        case SFF_MODULE_TYPE_1G_BASE_LX:
        case SFF_MODULE_TYPE_1G_BASE_ZX:
        case SFF_MODULE_TYPE_100_BASE_LX:
        case SFF_MODULE_TYPE_100_BASE_FX:
        case SFF_MODULE_TYPE_4X_MUX:
            return SFF_MEDIA_TYPE_FIBER;

        case SFF_MODULE_TYPE_COUNT:
        case SFF_MODULE_TYPE_INVALID:
            return SFF_MEDIA_TYPE_INVALID;
    }

    return SFF_MEDIA_TYPE_INVALID;
}

int
sff_module_caps_get (sff_module_type_t mt,
                     uint32_t *caps)
{
    if (caps == NULL) {
        return -1;
    }

    *caps = 0;

    switch (mt) {
        case SFF_MODULE_TYPE_100G_AOC:
        case SFF_MODULE_TYPE_100G_BASE_SR4:
        case SFF_MODULE_TYPE_100G_BASE_LR4:
        case SFF_MODULE_TYPE_100G_BASE_CR4:
        case SFF_MODULE_TYPE_100G_BASE_ER4:
        case SFF_MODULE_TYPE_100G_CWDM4:
        case SFF_MODULE_TYPE_100G_PSM4:
        case SFF_MODULE_TYPE_100G_SWDM4:
        case SFF_MODULE_TYPE_100G_PAM4_BIDI:
            *caps |= SFF_MODULE_CAPS_F_100G;
            return 0;

        case SFF_MODULE_TYPE_40G_BASE_CR4:
        case SFF_MODULE_TYPE_40G_BASE_SR4:
        case SFF_MODULE_TYPE_40G_BASE_LR4:
        case SFF_MODULE_TYPE_40G_BASE_LM4:
        case SFF_MODULE_TYPE_40G_BASE_ACTIVE:
        case SFF_MODULE_TYPE_40G_BASE_CR:
        case SFF_MODULE_TYPE_40G_BASE_SR2:
        case SFF_MODULE_TYPE_40G_BASE_SM4:
        case SFF_MODULE_TYPE_40G_BASE_ER4:
        case SFF_MODULE_TYPE_40G_BASE_SWDM4:
            *caps |= SFF_MODULE_CAPS_F_40G;
            return 0;

        case SFF_MODULE_TYPE_25G_BASE_CR:
        case SFF_MODULE_TYPE_25G_BASE_SR:
        case SFF_MODULE_TYPE_25G_BASE_AOC:
            *caps |= SFF_MODULE_CAPS_F_25G;
            return 0;

        case SFF_MODULE_TYPE_10G_BASE_SR:
        case SFF_MODULE_TYPE_10G_BASE_LR:
        case SFF_MODULE_TYPE_10G_BASE_LRM:
        case SFF_MODULE_TYPE_10G_BASE_ER:
        case SFF_MODULE_TYPE_10G_BASE_CR:
        case SFF_MODULE_TYPE_10G_BASE_SX:
        case SFF_MODULE_TYPE_10G_BASE_LX:
        case SFF_MODULE_TYPE_10G_BASE_ZR:
        case SFF_MODULE_TYPE_10G_BASE_SRL:
        case SFF_MODULE_TYPE_10G_BASE_T:
            *caps |= SFF_MODULE_CAPS_F_10G;
            return 0;

        case SFF_MODULE_TYPE_1G_BASE_SX:
        case SFF_MODULE_TYPE_1G_BASE_LX:
        case SFF_MODULE_TYPE_1G_BASE_ZX:
        case SFF_MODULE_TYPE_1G_BASE_CX:
        case SFF_MODULE_TYPE_1G_BASE_T:
            *caps |= SFF_MODULE_CAPS_F_1G;
            return 0;

        case SFF_MODULE_TYPE_100_BASE_LX:
        case SFF_MODULE_TYPE_100_BASE_FX:
            *caps |= SFF_MODULE_CAPS_F_100;
            return 0;

        default:
            return -1;
    }
}

static void
make_printable__ (char *string, int size)
{
    char *p;
    for (p = string; p && *p && size; p++) {
        if (!isprint (*p)) {
            *p = '?';
        }
        size --;
    }
}

/**
 * @brief Initialize an SFF module information structure.
 * @param rv [out] Receives the data.
 * @param eeprom Raw EEPROM data.
 * @note if eeprom is != NULL it will be copied into rv->eeprom first.
 * @note if eeprom is NULL it is assumed the rv->eeprom buffer
 * has already been initialized.
 */
static int
sff_eeprom_parse_standard__ (sff_eeprom_t *se,
                             uint8_t *eeprom)
{
    if (se == NULL) {
        return -1;
    }
    se->identified = 0;

    if (eeprom) {
        memcpy (se->eeprom, eeprom, 256);
    }

    if (SFF8472_MODULE_SFP (se->eeprom)) {
        /* See SFF-8472 pp22, pp28 */
        int i;
        for (i = 0, se->cc_base = 0; i < 63; ++i) {
            se->cc_base = (se->cc_base + se->eeprom[i]) &
                          0xFF;
        }
        for (i = 64, se->cc_ext = 0; i < 95; ++i) {
            se->cc_ext = (se->cc_ext + se->eeprom[i]) & 0xFF;
        }
    } else if (SFF8436_MODULE_QSFP_PLUS_V2 (
                   se->eeprom) ||
               SFF8636_MODULE_QSFP28 (se->eeprom)) {
        /* See SFF-8436 pp72, pp73 */
        int i;
        for (i = 128, se->cc_base = 0; i < 191; ++i) {
            se->cc_base = (se->cc_base + se->eeprom[i]) &
                          0xFF;
        }
        for (i = 192, se->cc_ext = 0; i < 223; ++i) {
            se->cc_ext = (se->cc_ext + se->eeprom[i]) & 0xFF;
        }
    }

    if (!sff_eeprom_validate (se, 1)) {
        return -1;
    }

    se->info.sfp_type = sff_sfp_type_get (se->eeprom);
    if (se->info.sfp_type == SFF_SFP_TYPE_INVALID) {
        printf ("sff_eeprom_parse() failed: invalid sfp type");
        return -1;
    }
    se->info.sfp_type_name = sff_sfp_type_desc (
                                 se->info.sfp_type);

    const uint8_t *vendor = NULL, *model = NULL, *serial = NULL,
        *date = NULL, *rev = NULL, *oui = NULL;

    switch (se->info.sfp_type) {
        case SFF_SFP_TYPE_QSFP_PLUS:
        case SFF_SFP_TYPE_QSFP28:
            vendor = se->eeprom + 148;
            model = se->eeprom + 168;
            serial = se->eeprom + 196;
            break;

        case SFF_SFP_TYPE_SFP:
        default:
            vendor = se->eeprom + 20;
            model = se->eeprom + 40;
            serial = se->eeprom + 68;
            date = se->eeprom + 84;
            rev = se->eeprom + 56;
            oui = se->eeprom + 37;
            break;
    }

    /* handle NULL fields, they should actually be space-padded */
    const char *empty = "                ";
    if (*vendor) {
        strncpy (se->info.vendor, (char *)vendor,
                 strlen ((char *)vendor));
        make_printable__ (se->info.vendor,
                          sizeof (se->info.vendor));
        se->info.vendor[16] = '\0';
    } else {
        strncpy (se->info.vendor, empty, strlen (empty));
        se->info.vendor[16] = '\0';
    }

    if (*model) {
        strncpy (se->info.model, (char *)model,
                 strlen ((char *)model));
        make_printable__ (se->info.model,
                          sizeof (se->info.model));
        se->info.model[16] = '\0';
    } else {
        strncpy (se->info.model, empty, strlen (empty));
        se->info.model[16] = '\0';
    }

    if (*serial) {
        strncpy (se->info.serial, (char *)serial,
                 strlen ((char *)serial));
        make_printable__ (se->info.serial,
                          sizeof (se->info.serial));
        se->info.serial[16] = '\0';
    } else {
        strncpy (se->info.serial, empty, strlen (empty));
        se->info.serial[16] = '\0';
    }

    if (*date) {
        strncpy (se->info.date, (char *)date,
                 strlen ((char *)date));
        make_printable__ (se->info.date,
                          sizeof (se->info.date));
        se->info.date[8] = '\0';
    } else {
        strncpy (se->info.date, empty, 8);
        se->info.date[8] = '\0';
    }

    if (*rev) {
        strncpy (se->info.rev, (char *)rev,
                 strlen ((char *)rev));
        make_printable__ (se->info.rev,
                          sizeof (se->info.rev));
        se->info.rev[4] = '\0';
    } else {
        strncpy (se->info.rev, empty, 4);
        se->info.rev[4] = '\0';
    }

    if (*oui) {
        strncpy (se->info.oui, (char *)oui,
                 strlen ((char *)oui));
        se->info.oui[3] = '\0';
    } else {
        strncpy (se->info.oui, empty, 3);
        se->info.oui[3] = '\0';
    }

    se->info.module_type = sff_module_type_get (
                               se->eeprom);
    if (se->info.module_type ==
        SFF_MODULE_TYPE_INVALID) {
        return -1;
    }

    if (sff_info_init (&se->info,
                       se->info.module_type, NULL, NULL, NULL, 0) < 0) {
        return -1;
    }

    int aoc_length;
    switch (se->info.media_type) {
        case SFF_MEDIA_TYPE_COPPER:
            switch (se->info.sfp_type) {
                case SFF_SFP_TYPE_QSFP_PLUS:
                case SFF_SFP_TYPE_QSFP28:
                    se->info.length = se->eeprom[146];
                    break;
                case SFF_SFP_TYPE_SFP:
                case SFF_SFP_TYPE_SFP28:
                    se->info.length = se->eeprom[18];
                    break;
                default:
                    se->info.length = -1;
                    break;
            }
            break;

        case SFF_MEDIA_TYPE_FIBER:
            switch (se->info.sfp_type) {
                case SFF_SFP_TYPE_QSFP28:
                    aoc_length = _sff8636_qsfp28_100g_aoc_length (
                                     se->eeprom);
                    se->info.length = aoc_length;
                    break;
                case SFF_SFP_TYPE_QSFP_PLUS:
                case SFF_SFP_TYPE_SFP:
                case SFF_SFP_TYPE_SFP28:
                    aoc_length = _sff8436_qsfp_40g_aoc_length (
                                     se->eeprom);
                    if (aoc_length < 0) {
                        aoc_length = _sff8472_sfp_10g_aoc_length (
                                         se->eeprom);
                    }
                    if (aoc_length > 0) {
                        se->info.length = aoc_length;
                    } else {
                        se->info.length = -1;
                    }
                    break;
                default:
                    se->info.length = -1;
                    break;
            }
            break;
        default:
            se->info.length = -1;
    }

    if (se->info.length == -1) {
        se->info.length_desc[0] = 0;
    } else {
        snprintf (se->info.length_desc,
                  sizeof (se->info.length_desc), "%dm",
                  se->info.length);
    }
    se->identified = 1;

    return 0;
}

int
sff_info_from_module_type (sff_info_t *info,
                           sff_sfp_type_t st, sff_module_type_t mt)
{
    info->sfp_type = st;
    info->sfp_type_name = sff_sfp_type_desc (st);

    info->module_type = mt;
    info->module_type_name = sff_module_type_desc (
                                 mt);

    info->media_type = sff_media_type_get (mt);
    info->media_type_name = sff_media_type_desc (
                                info->media_type);

    if (sff_module_caps_get (info->module_type,
                             &info->caps) < 0) {
        printf ("sff_info_init() failed: invalid module caps");
        return -1;
    }
    return 0;
}
#if 0
void
sff_info_show (sff_info_t *info, aim_pvs_t *pvs)
{
    aim_printf (pvs,
                "Vendor: %s Model: %s SN: %s Type: %s Module: %s Media: %s Length: %d\n",
                info->vendor, info->model, info->serial,
                info->sfp_type_name,
                info->module_type_name, info->media_type_name,
                info->length);
}
#endif
int
sff_eeprom_parse_file (sff_eeprom_t *se,
                       const char *fname)
{
    int rv;
    FILE *fp;

    memset (se, 0, sizeof (*se));

    if ( (fp = fopen (fname, "r")) == NULL) {
        printf ("Failed to open eeprom file %s\n",
                strerror (errno));
        return -1;
    }

    if ( (rv = fread (se->eeprom, 1, 256, fp)) > 0) {
        if ( (rv = sff_eeprom_parse (se, NULL)) < 0) {
            printf ("sff_init() failed on data from file %s: %d\n",
                    fname, rv);
            rv = -1;
        }
        rv = 0;
    } else {
        rv = -1;
    }
    fclose (fp);
    return rv;
}

void
sff_eeprom_invalidate (sff_eeprom_t *se)
{
    memset (se->eeprom, 0xFF, 256);
    se->cc_base = 0xFF;
    se->cc_ext = 0xFF;
    se->identified = 0;
}

int
sff_eeprom_validate (sff_eeprom_t *se,
                     int verbose)
{
    if (SFF8436_MODULE_QSFP_PLUS_V2 (se->eeprom) ||
        SFF8636_MODULE_QSFP28 (se->eeprom)) {

        if (se->cc_base != se->eeprom[191]) {
            if (verbose) {
                printf ("sff_eeprom_validate() failed: invalid base QSFP checksum (0x%x should be 0x%x)",
                        se->eeprom[191], se->cc_base);
            }
            return 0;
        }

#if SFF_CONFIG_INCLUDE_EXT_CC_CHECK == 1
        if (se->cc_ext != se->eeprom[223]) {
            if (verbose) {
                printf ("sff_info_valid() failed: invalid extended QSFP checksum (0x%x should be 0x%x)",
                        se->eeprom[223], se->cc_ext);
            }
            return 0;
        }
#endif

    } else if (SFF8472_MODULE_SFP (se->eeprom)) {

        if (se->cc_base != se->eeprom[63]) {
            if (verbose) {
                printf ("sff_info_valid() failed: invalid base SFP checksum (0x%x should be 0x%x)",
                        se->eeprom[63], se->cc_base);
            }
            return 0;
        }

#if SFF_CONFIG_INCLUDE_EXT_CC_CHECK == 1
        if (se->cc_ext != se->eeprom[95]) {
            if (verbose) {
                printf ("sff_info_valid() failed: invalid extended SFP checksum (0x%x should be 0x%x)",
                        se->eeprom[95], se->cc_ext);
            }
            return 0;
        }
#endif

    } else {

        if (verbose) {
            printf ("sff_info_valid() failed: invalid module type");
        }

        return 0;
    }

    return 1;
}

static int
sff_eeprom_parse_nonstandard__ (sff_eeprom_t *se,
                                uint8_t *eeprom)
{
    if (se == NULL) {
        return -1;
    }
    se->identified = 0;

    if (eeprom) {
        memcpy (se->eeprom, eeprom, 256);
    }

    if (strncmp (se->info.vendor, "Amphenol",
                 8) == 0 &&
        (strncmp (se->info.model, "625960001", 9) == 0 ||
         strncmp (se->info.model, "659900001", 9) == 0) &&
        (se->eeprom[240] == 0x0f) &&
        (se->eeprom[241] == 0x10) &&
        ((se->eeprom[243] & 0xF0) == 0xE0)) {

        /* 4X_MUX */
        se->identified  = 1;
        se->info.module_type = SFF_MODULE_TYPE_4X_MUX;
        se->info.module_type_name = sff_module_type_desc (
                                        se->info.module_type);
        se->info.media_type = SFF_MEDIA_TYPE_COPPER;
        se->info.media_type_name = sff_media_type_desc (
                                       se->info.media_type);
        se->info.caps = SFF_MODULE_CAPS_F_1G;
        se->info.length = se->eeprom[146];
        snprintf (se->info.length_desc,
                  sizeof (se->info.length_desc), "%dm",
                  se->info.length);
        return 0;
    }

    if (sff_nonstandard_lookup (&se->info) == 0) {
        se->identified = 1;
        if (se->info.length == -1) {
            se->info.length_desc[0] = 0;
        } else {
            snprintf (se->info.length_desc,
                      sizeof (se->info.length_desc), "%dm",
                      se->info.length);
        }
        return 0;
    }

    return -1;
}

int
sff_eeprom_parse (sff_eeprom_t *se,
                  uint8_t *eeprom)
{
    int rv = sff_eeprom_parse_standard__ (se, eeprom);
    if (!se->identified) {
        rv = sff_eeprom_parse_nonstandard__ (se, eeprom);
    }
    return rv;
}

int
sff_info_init (sff_info_t *info,
               sff_module_type_t mt,
               const char *vendor, const char *model,
               const char *serial,
               int length)
{
    info->module_type = mt;

    switch (mt) {
        case SFF_MODULE_TYPE_100G_AOC:
        case SFF_MODULE_TYPE_100G_BASE_SR4:
        case SFF_MODULE_TYPE_100G_BASE_LR4:
        case SFF_MODULE_TYPE_100G_BASE_ER4:
        case SFF_MODULE_TYPE_100G_CWDM4:
        case SFF_MODULE_TYPE_100G_PSM4:
        case SFF_MODULE_TYPE_100G_SWDM4:
        case SFF_MODULE_TYPE_100G_PAM4_BIDI:
            info->sfp_type = SFF_SFP_TYPE_QSFP28;
            info->media_type = SFF_MEDIA_TYPE_FIBER;
            info->caps = SFF_MODULE_CAPS_F_100G;
            break;

        case SFF_MODULE_TYPE_100G_BASE_CR4:
            info->sfp_type = SFF_SFP_TYPE_QSFP28;
            info->media_type = SFF_MEDIA_TYPE_COPPER;
            info->caps = SFF_MODULE_CAPS_F_100G;
            break;

        case SFF_MODULE_TYPE_40G_BASE_SR4:
        case SFF_MODULE_TYPE_40G_BASE_LR4:
        case SFF_MODULE_TYPE_40G_BASE_LM4:
        case SFF_MODULE_TYPE_40G_BASE_ACTIVE:
        case SFF_MODULE_TYPE_40G_BASE_SR2:
        case SFF_MODULE_TYPE_40G_BASE_SM4:
        case SFF_MODULE_TYPE_40G_BASE_ER4:
        case SFF_MODULE_TYPE_40G_BASE_SWDM4:
        case SFF_MODULE_TYPE_4X_MUX:
            info->sfp_type = SFF_SFP_TYPE_QSFP_PLUS;
            info->media_type = SFF_MEDIA_TYPE_FIBER;
            info->caps = SFF_MODULE_CAPS_F_40G;
            break;

        case SFF_MODULE_TYPE_40G_BASE_CR4:
        case SFF_MODULE_TYPE_40G_BASE_CR:
            info->sfp_type = SFF_SFP_TYPE_QSFP_PLUS;
            info->media_type = SFF_MEDIA_TYPE_COPPER;
            info->caps = SFF_MODULE_CAPS_F_40G;
            break;

        case SFF_MODULE_TYPE_25G_BASE_CR:
            info->sfp_type = SFF_SFP_TYPE_SFP28;
            info->media_type = SFF_MEDIA_TYPE_COPPER;
            info->caps = SFF_MODULE_CAPS_F_25G;
            break;

        case SFF_MODULE_TYPE_25G_BASE_SR:
        case SFF_MODULE_TYPE_25G_BASE_LR:
        case SFF_MODULE_TYPE_25G_BASE_AOC:
            info->sfp_type = SFF_SFP_TYPE_SFP28;
            info->media_type = SFF_MEDIA_TYPE_FIBER;
            info->caps = SFF_MODULE_CAPS_F_25G;
            break;

        case SFF_MODULE_TYPE_10G_BASE_T:
        case SFF_MODULE_TYPE_10G_BASE_CR:
            info->sfp_type = SFF_SFP_TYPE_SFP;
            info->media_type = SFF_MEDIA_TYPE_COPPER;
            info->caps = SFF_MODULE_CAPS_F_10G;
            break;

        case SFF_MODULE_TYPE_10G_BASE_AOC:
        case SFF_MODULE_TYPE_10G_BASE_SR:
        case SFF_MODULE_TYPE_10G_BASE_LR:
        case SFF_MODULE_TYPE_10G_BASE_LRM:
        case SFF_MODULE_TYPE_10G_BASE_ER:
        case SFF_MODULE_TYPE_10G_BASE_SX:
        case SFF_MODULE_TYPE_10G_BASE_LX:
        case SFF_MODULE_TYPE_10G_BASE_ZR:
        case SFF_MODULE_TYPE_10G_BASE_SRL:
            info->sfp_type = SFF_SFP_TYPE_SFP;
            info->media_type = SFF_MEDIA_TYPE_FIBER;
            info->caps = SFF_MODULE_CAPS_F_10G;
            break;

        case SFF_MODULE_TYPE_1G_BASE_SX:
        case SFF_MODULE_TYPE_1G_BASE_LX:
        case SFF_MODULE_TYPE_1G_BASE_ZX:
            info->sfp_type = SFF_SFP_TYPE_SFP;
            info->media_type = SFF_MEDIA_TYPE_FIBER;
            info->caps = SFF_MODULE_CAPS_F_1G;
            break;

        case SFF_MODULE_TYPE_1G_BASE_CX:
        case SFF_MODULE_TYPE_1G_BASE_T:
            info->sfp_type = SFF_SFP_TYPE_SFP;
            info->media_type = SFF_MEDIA_TYPE_COPPER;
            info->caps = SFF_MODULE_CAPS_F_1G;
            break;

        case SFF_MODULE_TYPE_100_BASE_LX:
        case SFF_MODULE_TYPE_100_BASE_FX:
            info->sfp_type = SFF_SFP_TYPE_SFP;
            info->media_type = SFF_MEDIA_TYPE_FIBER;
            info->caps = SFF_MODULE_CAPS_F_100;
            break;

        case SFF_MODULE_TYPE_COUNT:
        case SFF_MODULE_TYPE_INVALID:
            return -1;
    }

    info->sfp_type_name = sff_sfp_type_desc (
                              info->sfp_type);
    info->module_type_name = sff_module_type_desc (
                                 info->module_type);
    info->media_type_name = sff_media_type_desc (
                                info->media_type);

    if (vendor) {
        strncpy (info->vendor, vendor,
                 sizeof (info->vendor));
    }
    if (model) {
        strncpy (info->model, model,
                 sizeof (info->model));
    }
    if (serial) {
        strncpy (info->serial, serial,
                 sizeof (info->serial));
    }

    info->length = length;
    snprintf (info->length_desc,
              sizeof (info->length_desc), "%dm", length);
    return 0;
}

