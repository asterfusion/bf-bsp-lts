/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2014 Asterfusion Technology Corporation.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 *
 *
 ***********************************************************/

/************************************************************
 * BAREFOOT NETWORKS
 *
 * source modified from its original contents
 ***********************************************************/

#ifndef __BF_PLTFM_BMC_TTY_H__
#define __BF_PLTFM_BMC_TTY_H__

int bmc_send_command (char *cmd,
                      unsigned long udelay);
int bmc_send_command_with_output (int timeout,
                                  char *cmd,
                                  char *output,
                                  size_t outlen);
int bmc_file_read_int (int *value, char *file,
                       int base);
int bmc_i2c_readb (uint8_t bus, uint8_t devaddr,
                   uint8_t addr);
int bmc_i2c_writeb (uint8_t bus, uint8_t devaddr,
                    uint8_t addr, uint8_t value);
int bmc_i2c_readw (uint8_t bus, uint8_t devaddr,
                   uint8_t addr);
int bmc_i2c_readraw (
    uint8_t bus, uint8_t devaddr, uint8_t addr,
    char *data, int data_size);

#endif /* __BF_PLTFM_BMC_TTY_H__ */
