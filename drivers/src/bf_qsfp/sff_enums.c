#include <bf_qsfp/sff_standards.h>


/* <auto.start.enum(ALL).source> */
aim_map_si_t sff_dom_field_flag_map[] = {
    { "TEMP", SFF_DOM_FIELD_FLAG_TEMP },
    { "VOLTAGE", SFF_DOM_FIELD_FLAG_VOLTAGE },
    { "BIAS_CUR", SFF_DOM_FIELD_FLAG_BIAS_CUR },
    { "RX_POWER", SFF_DOM_FIELD_FLAG_RX_POWER },
    { "RX_POWER_OMA", SFF_DOM_FIELD_FLAG_RX_POWER_OMA },
    { "TX_POWER", SFF_DOM_FIELD_FLAG_TX_POWER },
    { NULL, 0 }
};

aim_map_si_t sff_dom_field_flag_desc_map[] = {
    { "None", SFF_DOM_FIELD_FLAG_TEMP },
    { "None", SFF_DOM_FIELD_FLAG_VOLTAGE },
    { "None", SFF_DOM_FIELD_FLAG_BIAS_CUR },
    { "None", SFF_DOM_FIELD_FLAG_RX_POWER },
    { "None", SFF_DOM_FIELD_FLAG_RX_POWER_OMA },
    { "None", SFF_DOM_FIELD_FLAG_TX_POWER },
    { NULL, 0 }
};

const char *
sff_dom_field_flag_name (sff_dom_field_flag_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e,
                      sff_dom_field_flag_map, 0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_dom_field_flag'";
    }
}

int
sff_dom_field_flag_value (const char *str,
                          sff_dom_field_flag_t *e, int substr)
{
    int i;
    AIM_REFERENCE (substr);
    if (aim_map_si_s (&i, str, sff_dom_field_flag_map,
                      0)) {
        /* Enum Found */
        *e = i;
        return 0;
    } else {
        return -1;
    }
}

const char *
sff_dom_field_flag_desc (sff_dom_field_flag_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e,
                      sff_dom_field_flag_desc_map, 0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_dom_field_flag'";
    }
}

int
sff_dom_field_flag_valid (sff_dom_field_flag_t e)
{
    return aim_map_si_i (NULL, e,
                         sff_dom_field_flag_map, 0) ? 1 : 0;
}


aim_map_si_t sff_dom_spec_map[] = {
    { "UNSUPPORTED", SFF_DOM_SPEC_UNSUPPORTED },
    { "SFF8436", SFF_DOM_SPEC_SFF8436 },
    { "SFF8472", SFF_DOM_SPEC_SFF8472 },
    { "SFF8636", SFF_DOM_SPEC_SFF8636 },
    { NULL, 0 }
};

aim_map_si_t sff_dom_spec_desc_map[] = {
    { "None", SFF_DOM_SPEC_UNSUPPORTED },
    { "None", SFF_DOM_SPEC_SFF8436 },
    { "None", SFF_DOM_SPEC_SFF8472 },
    { "None", SFF_DOM_SPEC_SFF8636 },
    { NULL, 0 }
};

const char *
sff_dom_spec_name (sff_dom_spec_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e, sff_dom_spec_map,
                      0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_dom_spec'";
    }
}

int
sff_dom_spec_value (const char *str,
                    sff_dom_spec_t *e, int substr)
{
    int i;
    AIM_REFERENCE (substr);
    if (aim_map_si_s (&i, str, sff_dom_spec_map, 0)) {
        /* Enum Found */
        *e = i;
        return 0;
    } else {
        return -1;
    }
}

const char *
sff_dom_spec_desc (sff_dom_spec_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e, sff_dom_spec_desc_map,
                      0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_dom_spec'";
    }
}


aim_map_si_t sff_media_type_map[] = {
    { "COPPER", SFF_MEDIA_TYPE_COPPER },
    { "FIBER", SFF_MEDIA_TYPE_FIBER },
    { NULL, 0 }
};

aim_map_si_t sff_media_type_desc_map[] = {
    { "Copper", SFF_MEDIA_TYPE_COPPER },
    { "Fiber", SFF_MEDIA_TYPE_FIBER },
    { NULL, 0 }
};

aim_map_si_t sff_connector_type_desc_map[] = {
    { "SC               ",   SFF8472_CONN_SC},
    { "FC Copper Style 1",   SFF8472_CONN_FC1_CU},
    { "FC Copper Style 2",   SFF8472_CONN_FC2_CU},
    { "BNC/TNC",             SFF8472_CONN_BNC},
    { "FC",                  SFF8472_CONN_FC_COAX},
    { "Fiber Jack",          SFF8472_CONN_FJ},
    { "LC",                  SFF8472_CONN_LC},
    { "MT-RJ",               SFF8472_CONN_MT_RJ},
    { "MU",                  SFF8472_CONN_MU},
    { "SG",                  SFF8472_CONN_SG},
    { "Optical Pigtail",     SFF8472_CONN_SI_PIGTAIL},
    { "MPO 1x12",            SFF8472_CONN_MPO_1X12},
    { "MPO 2x16",            SFF8472_CONN_MPO_2X16},
    { "HSSDC II",            SFF8472_CONN_HSSDC_II},
    { "Copper pigtail",      SFF8472_CONN_CU_PIGTAIL},
    { "RJ45",                SFF8472_CONN_RJ45},
    { "NOSEP",               SFF8472_CONN_NOSEP},
    { "MXC 2x16",            SFF8472_CONN_MXC_2X16},
    { "CS",                  SFF8472_CONN_CS},
    { "SN",                  SFF8472_CONN_SN},
    { "MPO 2x12",            SFF8472_CONN_MPO_2X12},
    { "MPO 1x16",            SFF8472_CONN_MPO_1X16},
    { "Unknown",             SFF8472_CONN_UNKNOWN},
};

const char *
sff_connector_type_name (sff_connector_type_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e,
                      sff_connector_type_desc_map, 0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_connector_type_t'";
    }
}

const char *
sff_media_type_name (sff_media_type_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e, sff_media_type_map,
                      0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_media_type'";
    }
}

int
sff_media_type_value (const char *str,
                      sff_media_type_t *e, int substr)
{
    int i;
    AIM_REFERENCE (substr);
    if (aim_map_si_s (&i, str, sff_media_type_map,
                      0)) {
        /* Enum Found */
        *e = i;
        return 0;
    } else {
        return -1;
    }
}

const char *
sff_media_type_desc (sff_media_type_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e,
                      sff_media_type_desc_map, 0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_media_type'";
    }
}


aim_map_si_t sff_module_caps_map[] = {
    { "F_100", SFF_MODULE_CAPS_F_100 },
    { "F_1G", SFF_MODULE_CAPS_F_1G },
    { "F_10G", SFF_MODULE_CAPS_F_10G },
    { "F_25G", SFF_MODULE_CAPS_F_25G },
    { "F_40G", SFF_MODULE_CAPS_F_40G },
    { "F_100G", SFF_MODULE_CAPS_F_100G },
    { NULL, 0 }
};

aim_map_si_t sff_module_caps_desc_map[] = {
    { "None", SFF_MODULE_CAPS_F_100 },
    { "None", SFF_MODULE_CAPS_F_1G },
    { "None", SFF_MODULE_CAPS_F_10G },
    { "None", SFF_MODULE_CAPS_F_25G },
    { "None", SFF_MODULE_CAPS_F_40G },
    { "None", SFF_MODULE_CAPS_F_100G },
    { NULL, 0 }
};

const char *
sff_module_caps_name (sff_module_caps_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e, sff_module_caps_map,
                      0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_module_caps'";
    }
}

int
sff_module_caps_value (const char *str,
                       sff_module_caps_t *e, int substr)
{
    int i;
    AIM_REFERENCE (substr);
    if (aim_map_si_s (&i, str, sff_module_caps_map,
                      0)) {
        /* Enum Found */
        *e = i;
        return 0;
    } else {
        return -1;
    }
}

const char *
sff_module_caps_desc (sff_module_caps_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e,
                      sff_module_caps_desc_map, 0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_module_caps'";
    }
}

int
sff_module_caps_valid (sff_module_caps_t e)
{
    return aim_map_si_i (NULL, e, sff_module_caps_map,
                         0) ? 1 : 0;
}


aim_map_si_t sff_module_type_map[] = {
    { "100G_AOC", SFF_MODULE_TYPE_100G_AOC },
    { "100G_BASE_CR4", SFF_MODULE_TYPE_100G_BASE_CR4 },
    { "100G_BASE_SR4", SFF_MODULE_TYPE_100G_BASE_SR4 },
    { "100G_BASE_LR4", SFF_MODULE_TYPE_100G_BASE_LR4 },
    { "100G_BASE_ER4", SFF_MODULE_TYPE_100G_BASE_ER4 },
    { "100G_CWDM4", SFF_MODULE_TYPE_100G_CWDM4 },
    { "100G_PSM4", SFF_MODULE_TYPE_100G_PSM4 },
    { "100G_SWDM4", SFF_MODULE_TYPE_100G_SWDM4 },
    { "100G_PAM4_BIDI", SFF_MODULE_TYPE_100G_PAM4_BIDI },
    { "40G_BASE_CR4", SFF_MODULE_TYPE_40G_BASE_CR4 },
    { "40G_BASE_SR4", SFF_MODULE_TYPE_40G_BASE_SR4 },
    { "40G_BASE_LR4", SFF_MODULE_TYPE_40G_BASE_LR4 },
    { "40G_BASE_LM4", SFF_MODULE_TYPE_40G_BASE_LM4 },
    { "40G_BASE_ACTIVE", SFF_MODULE_TYPE_40G_BASE_ACTIVE },
    { "40G_BASE_CR", SFF_MODULE_TYPE_40G_BASE_CR },
    { "40G_BASE_SR2", SFF_MODULE_TYPE_40G_BASE_SR2 },
    { "40G_BASE_SM4", SFF_MODULE_TYPE_40G_BASE_SM4 },
    { "40G_BASE_ER4", SFF_MODULE_TYPE_40G_BASE_ER4 },
    { "40G_BASE_SWDM4", SFF_MODULE_TYPE_40G_BASE_SWDM4 },
    { "25G_BASE_CR", SFF_MODULE_TYPE_25G_BASE_CR },
    { "25G_BASE_SR", SFF_MODULE_TYPE_25G_BASE_SR },
    { "25G_BASE_LR", SFF_MODULE_TYPE_25G_BASE_LR },
    { "25G_BASE_AOC", SFF_MODULE_TYPE_25G_BASE_AOC },
    /* added 10GBase AOC, by tsihang, 2021-07-26. */
    { "10GBASE-AOC", SFF_MODULE_TYPE_10G_BASE_AOC },
    { "10G_BASE_SR", SFF_MODULE_TYPE_10G_BASE_SR },
    { "10G_BASE_LR", SFF_MODULE_TYPE_10G_BASE_LR },
    { "10G_BASE_LRM", SFF_MODULE_TYPE_10G_BASE_LRM },
    { "10G_BASE_ER", SFF_MODULE_TYPE_10G_BASE_ER },
    { "10G_BASE_CR", SFF_MODULE_TYPE_10G_BASE_CR },
    { "10G_BASE_SX", SFF_MODULE_TYPE_10G_BASE_SX },
    { "10G_BASE_LX", SFF_MODULE_TYPE_10G_BASE_LX },
    { "10G_BASE_ZR", SFF_MODULE_TYPE_10G_BASE_ZR },
    { "10G_BASE_SRL", SFF_MODULE_TYPE_10G_BASE_SRL },
    { "10G_BASE_T", SFF_MODULE_TYPE_10G_BASE_T },
    { "1G_BASE_SX", SFF_MODULE_TYPE_1G_BASE_SX },
    { "1G_BASE_LX", SFF_MODULE_TYPE_1G_BASE_LX },
    { "1G_BASE_ZX", SFF_MODULE_TYPE_1G_BASE_ZX },
    { "1G_BASE_CX", SFF_MODULE_TYPE_1G_BASE_CX },
    { "1G_BASE_T", SFF_MODULE_TYPE_1G_BASE_T },
    { "100_BASE_LX", SFF_MODULE_TYPE_100_BASE_LX },
    { "100_BASE_FX", SFF_MODULE_TYPE_100_BASE_FX },
    { "4X_MUX", SFF_MODULE_TYPE_4X_MUX },
    { NULL, 0 }
};

aim_map_si_t sff_module_type_desc_map[] = {
    { "100G-AOC", SFF_MODULE_TYPE_100G_AOC },
    { "100GBASE-CR4", SFF_MODULE_TYPE_100G_BASE_CR4 },
    { "100GBASE-SR4", SFF_MODULE_TYPE_100G_BASE_SR4 },
    { "100GBASE-LR4", SFF_MODULE_TYPE_100G_BASE_LR4 },
    { "100GBASE-ER4", SFF_MODULE_TYPE_100G_BASE_ER4 },
    { "100G-CWDM4", SFF_MODULE_TYPE_100G_CWDM4 },
    { "100G-PSM4", SFF_MODULE_TYPE_100G_PSM4 },
    { "100G-SWDM4", SFF_MODULE_TYPE_100G_SWDM4 },
    { "100G-PAM4-BIDI", SFF_MODULE_TYPE_100G_PAM4_BIDI },
    { "40GBASE-CR4", SFF_MODULE_TYPE_40G_BASE_CR4 },
    { "40GBASE-SR4", SFF_MODULE_TYPE_40G_BASE_SR4 },
    { "40GBASE-LR4", SFF_MODULE_TYPE_40G_BASE_LR4 },
    { "40GBASE-LM4", SFF_MODULE_TYPE_40G_BASE_LM4 },
    { "40GBASE-ACTIVE", SFF_MODULE_TYPE_40G_BASE_ACTIVE },
    { "40GBASE-CR", SFF_MODULE_TYPE_40G_BASE_CR },
    { "40GBASE-SR2", SFF_MODULE_TYPE_40G_BASE_SR2 },
    { "40GBASE-SM4", SFF_MODULE_TYPE_40G_BASE_SM4 },
    { "40GBASE-ER4", SFF_MODULE_TYPE_40G_BASE_ER4 },
    { "40GBASE-SWDM4", SFF_MODULE_TYPE_40G_BASE_SWDM4 },
    { "25GBASE-CR", SFF_MODULE_TYPE_25G_BASE_CR },
    { "25GBASE-SR", SFF_MODULE_TYPE_25G_BASE_SR },
    { "25GBASE-LR", SFF_MODULE_TYPE_25G_BASE_LR },
    { "25GBASE-AOC", SFF_MODULE_TYPE_25G_BASE_AOC },
    { "10GBASE-SR", SFF_MODULE_TYPE_10G_BASE_SR },
    { "10GBASE-LR", SFF_MODULE_TYPE_10G_BASE_LR },
    { "10GBASE-LRM", SFF_MODULE_TYPE_10G_BASE_LRM },
    { "10GBASE-ER", SFF_MODULE_TYPE_10G_BASE_ER },
    { "10GBASE-CR", SFF_MODULE_TYPE_10G_BASE_CR },
    { "10GBASE-SX", SFF_MODULE_TYPE_10G_BASE_SX },
    { "10GBASE-LX", SFF_MODULE_TYPE_10G_BASE_LX },
    { "10GBASE-ZR", SFF_MODULE_TYPE_10G_BASE_ZR },
    { "10GBASE-SRL", SFF_MODULE_TYPE_10G_BASE_SRL },
    { "10GBASE-T", SFF_MODULE_TYPE_10G_BASE_T },
    { "1GBASE-SX", SFF_MODULE_TYPE_1G_BASE_SX },
    { "1GBASE-LX", SFF_MODULE_TYPE_1G_BASE_LX },
    { "1GBASE-ZX", SFF_MODULE_TYPE_1G_BASE_ZX },
    { "1GBASE-CX", SFF_MODULE_TYPE_1G_BASE_CX },
    { "1GBASE-T", SFF_MODULE_TYPE_1G_BASE_T },
    { "100BASE-LX", SFF_MODULE_TYPE_100_BASE_LX },
    { "100BASE-FX", SFF_MODULE_TYPE_100_BASE_FX },
    { "4X-MUX", SFF_MODULE_TYPE_4X_MUX },
    { NULL, 0 }
};

const char *
sff_module_type_name (sff_module_type_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e, sff_module_type_map,
                      0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_module_type'";
    }
}

int
sff_module_type_value (const char *str,
                       sff_module_type_t *e, int substr)
{
    int i;
    AIM_REFERENCE (substr);
    if (aim_map_si_s (&i, str, sff_module_type_map,
                      0)) {
        /* Enum Found */
        *e = i;
        return 0;
    } else {
        return -1;
    }
}

const char *
sff_module_type_desc (sff_module_type_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e,
                      sff_module_type_desc_map, 0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_module_type'";
    }
}


aim_map_si_t sff_sfp_type_map[] = {
    { "SFP", SFF_SFP_TYPE_SFP },
    { "QSFP", SFF_SFP_TYPE_QSFP },
    { "QSFP+S", SFF_SFP_TYPE_QSFP_PLUS },
    { "QSFP28", SFF_SFP_TYPE_QSFP28 },
    { "SFP28", SFF_SFP_TYPE_SFP28 },
    { NULL, 0 }
};

aim_map_si_t sff_sfp_type_desc_map[] = {
    { "SFP", SFF_SFP_TYPE_SFP },
    { "QSFP", SFF_SFP_TYPE_QSFP },
    { "QSFP+", SFF_SFP_TYPE_QSFP_PLUS },
    { "QSFP28", SFF_SFP_TYPE_QSFP28 },
    { "SFP28", SFF_SFP_TYPE_SFP28 },
    { NULL, 0 }
};

const char *
sff_sfp_type_name (sff_sfp_type_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e, sff_sfp_type_map,
                      0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_sfp_type'";
    }
}

int
sff_sfp_type_value (const char *str,
                    sff_sfp_type_t *e, int substr)
{
    int i;
    AIM_REFERENCE (substr);
    if (aim_map_si_s (&i, str, sff_sfp_type_map, 0)) {
        /* Enum Found */
        *e = i;
        return 0;
    } else {
        return -1;
    }
}

const char *
sff_sfp_type_desc (sff_sfp_type_t e)
{
    const char *name;
    if (aim_map_si_i (&name, e, sff_sfp_type_desc_map,
                      0)) {
        return name;
    } else {
        return "-invalid value for enum type 'sff_sfp_type'";
    }
}

/* <auto.end.enum(ALL).source> */

