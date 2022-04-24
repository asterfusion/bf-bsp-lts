/*!
 * @file map.h
 * @date 2021/07/15
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#ifndef __AIM_MAP_H__
#define __AIM_MAP_H__

/**
 * Map a string to an integer.
 */
typedef struct aim_map_si_t {
    /** String value */
    const char *s;
    /** Integer value */
    int         i;
} aim_map_si_t;

/**
 * @brief Map a string to an integer.
 * @param [out] rv The mapped integer value.
 * @param s The string to map.
 * @param maps The map table.
 * @param count The number of entries in the map table.
 *
 * @returns 1 if the mapping was successful.
 * @returns 0 if the mapping failed.
 *
 * @note If count is 0, maps is assumed to be terminated with a null entry.
 */
int aim_map_si_s (int *rv, const char *s,
                  aim_map_si_t *maps, unsigned int count);

/**
 * @brief Map an integer to a string.
 * @param [out] rv The mapped string value.
 * @param i The integer to map.
 * @param maps The map table.
 * @param count The number of entries in the map table.
 *
 * @returns 1 if the mapping was successful.
 * @returns 0 if the mapping failed.
 *
 * @note If count is 0, maps is assumed to be terminated with a null entry.
 */
int aim_map_si_i (const char **rv, int i,
                  aim_map_si_t *maps,
                  unsigned int count);






#endif /* __AIM_MAP_H__ */

