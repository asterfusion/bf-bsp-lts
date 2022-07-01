/*!
 * @file bf_pltfm_sfp_module.h
 * @date 2020/05/31
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#ifndef _BF_MAV_SFP_MODULE_H
#define _BF_MAV_SFP_MODULE_H

struct opt_ctx_t {
    int (*read) (uint32_t module, uint8_t offset,
                 uint8_t len, uint8_t *buf);
    int (*write) (uint32_t module, uint8_t offset,
                  uint8_t len, uint8_t *buf);
    /* Tx Disable control */
    int (*txdis) (uint32_t module, bool disable,
                  void *st);
    /* LOS */
    int (*losrd) (uint32_t module, bool *los,
                  void *st);
    /* Present Mask */
    int (*presrd) (uint32_t *pres_l, uint32_t *pres_h);

    void (*lock)
    ();      /* internal lock to lock shared resources,
                          * such as master i2c to avoid resource competion with other applications. */
    void (*unlock)
    ();    /* internal lock to lock shared resources,
                          * such as master i2c to avoid resource competion with other applications. */
    int (*select) (uint32_t module);
    int (*unselect) (uint32_t module);

    void *sfp_ctx;  /* You can define your own sfp_ctx here. */
    void *xsfp_ctx;
};


int bf_pltfm_get_sfp_ctx (struct sfp_ctx_t
                          **sfp_ctx);


#endif
