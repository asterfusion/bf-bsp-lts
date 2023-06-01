/*!
 * @file map.c
 * @date 2021/07/15
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <bf_qsfp/map.h>

int
aim_map_si_s (int *rv, const char *s,
              aim_map_si_t *maps, unsigned int count)
{
    unsigned int idx;
    aim_map_si_t *p;

    if (s == NULL) {
        return 0;
    }

    for (p = maps, idx = 0; (count &&
                             (idx < count)) || p->s; idx++, p++) {
        if (!strcmp (p->s, s)) {
            if (rv) {
                *rv = p->i;
            }
            return 1;
        }
    }

    return 0;
}

int
aim_map_si_i (const char **rv, int i,
              aim_map_si_t *maps, unsigned int count)
{
    unsigned int idx;
    aim_map_si_t *p;
    for (p = maps, idx = 0; (count &&
                             (idx < count)) || p->s; idx++, p++) {
        if (i == p->i) {
            if (rv) {
                *rv = p->s;
            }
            return 1;
        }
    }
    return 0;
}


