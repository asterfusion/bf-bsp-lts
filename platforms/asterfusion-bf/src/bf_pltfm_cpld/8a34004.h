/*!
 * @file 8a34004.h
 * @date 2026/07/08
 *
 * Hang Tsi (tsihang@asterfusion.com)
 *
 *
 */

#ifndef AFN_8A34004_REGS_H
#define AFN_8A34004_REGS_H

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define MAX_TOD		(4)
#define MAX_PLL		(8)

// DPLL0 ~ DPLL7 plus DPLL8 for SYSDPLL
#define AFN_8A34004_MAX_DPLL    (MAX_PLL + 1)
#define AFN_8A34004_MAX_TOD     (MAX_TOD)

#define MAX_ENTRIES_PER_DPLL    (16 + 1)
#define MAX_ENTRIES_PER_TOD     (24 + 1)

#define PAGE_ADDR_BASE                    0x0000
#define PAGE_ADDR                         0x00fc

#define HW_REVISION                       0x8180
#define REV_ID                            0x007a

#define HW_DPLL_0                         (0x8a00)
#define HW_DPLL_1                         (0x8b00)
#define HW_DPLL_2                         (0x8c00)
#define HW_DPLL_3                         (0x8d00)
#define HW_DPLL_4                         (0x8e00)
#define HW_DPLL_5                         (0x8f00)
#define HW_DPLL_6                         (0x9000)
#define HW_DPLL_7                         (0x9100)

#define HW_DPLL_TOD_SW_TRIG_ADDR__0       (0x080)
#define HW_DPLL_TOD_CTRL_1                (0x089)
#define HW_DPLL_TOD_CTRL_2                (0x08A)
#define HW_DPLL_TOD_OVR__0                (0x098)
#define HW_DPLL_TOD_OUT_0__0              (0x0B0)

#define HW_Q0_Q1_CH_SYNC_CTRL_0           (0xa740)
#define HW_Q0_Q1_CH_SYNC_CTRL_1           (0xa741)
#define HW_Q2_Q3_CH_SYNC_CTRL_0           (0xa742)
#define HW_Q2_Q3_CH_SYNC_CTRL_1           (0xa743)
#define HW_Q4_Q5_CH_SYNC_CTRL_0           (0xa744)
#define HW_Q4_Q5_CH_SYNC_CTRL_1           (0xa745)
#define HW_Q6_Q7_CH_SYNC_CTRL_0           (0xa746)
#define HW_Q6_Q7_CH_SYNC_CTRL_1           (0xa747)
#define HW_Q8_CH_SYNC_CTRL_0              (0xa748)
#define HW_Q8_CH_SYNC_CTRL_1              (0xa749)
#define HW_Q9_CH_SYNC_CTRL_0              (0xa74a)
#define HW_Q9_CH_SYNC_CTRL_1              (0xa74b)
#define HW_Q10_CH_SYNC_CTRL_0             (0xa74c)
#define HW_Q10_CH_SYNC_CTRL_1             (0xa74d)
#define HW_Q11_CH_SYNC_CTRL_0             (0xa74e)
#define HW_Q11_CH_SYNC_CTRL_1             (0xa74f)

#define SYNC_SOURCE_DPLL0_TOD_PPS	0x14
#define SYNC_SOURCE_DPLL1_TOD_PPS	0x15
#define SYNC_SOURCE_DPLL2_TOD_PPS	0x16
#define SYNC_SOURCE_DPLL3_TOD_PPS	0x17

#define SYNCTRL1_MASTER_SYNC_RST	BIT(7)
#define SYNCTRL1_MASTER_SYNC_TRIG	BIT(5)
#define SYNCTRL1_TOD_SYNC_TRIG		BIT(4)
#define SYNCTRL1_FBDIV_FRAME_SYNC_TRIG	BIT(3)
#define SYNCTRL1_FBDIV_SYNC_TRIG	BIT(2)
#define SYNCTRL1_Q1_DIV_SYNC_TRIG	BIT(1)
#define SYNCTRL1_Q0_DIV_SYNC_TRIG	BIT(0)

#define HW_Q8_CTRL_SPARE  (0xa7d4)
#define HW_Q11_CTRL_SPARE (0xa7ec)

/**
 * Select FOD5 as sync_trigger for Q8 divider.
 * Transition from logic zero to one
 * sets trigger to sync Q8 divider.
 *
 * Unused when FOD4 is driving Q8 divider (normal operation).
 */
#define Q9_TO_Q8_SYNC_TRIG  BIT(1)

/**
 * Enable FOD5 as driver for clock and sync for Q8 divider.
 * Enable fanout buffer for FOD5.
 *
 * Unused when FOD4 is driving Q8 divider (normal operation).
 */
#define Q9_TO_Q8_FANOUT_AND_CLOCK_SYNC_ENABLE_MASK  (BIT(0) | BIT(2))

/**
 * Select FOD6 as sync_trigger for Q11 divider.
 * Transition from logic zero to one
 * sets trigger to sync Q11 divider.
 *
 * Unused when FOD7 is driving Q11 divider (normal operation).
 */
#define Q10_TO_Q11_SYNC_TRIG  BIT(1)

/**
 * Enable FOD6 as driver for clock and sync for Q11 divider.
 * Enable fanout buffer for FOD6.
 *
 * Unused when FOD7 is driving Q11 divider (normal operation).
 */
#define Q10_TO_Q11_FANOUT_AND_CLOCK_SYNC_ENABLE_MASK  (BIT(0) | BIT(2))

#define RESET_CTRL                        0xc000
#define SM_RESET                          0x0012
#define SM_RESET_V520                     0x0013
#define SM_RESET_CMD                      0x5A

#define GENERAL_STATUS                    0xc014
#define BOOT_STATUS                       0x0000
#define HW_REV_ID                         0x000A
#define BOND_ID                           0x000B
#define HW_CSR_ID                         0x000C
#define HW_IRQ_ID                         0x000E
#define MAJ_REL                           0x0010
#define MIN_REL                           0x0011
#define HOTFIX_REL                        0x0012
#define PIPELINE_ID                       0x0014
#define BUILD_ID                          0x0018
#define JTAG_DEVICE_ID                    0x001c
#define PRODUCT_ID                        0x001e
#define OTP_SCSR_CONFIG_SELECT            0x0022

#define STATUS                            0xc03c
#define DPLL0_STATUS			  0x0018
#define DPLL1_STATUS			  0x0019
#define DPLL2_STATUS			  0x001a
#define DPLL3_STATUS			  0x001b
#define DPLL4_STATUS			  0x001c
#define DPLL5_STATUS			  0x001d
#define DPLL6_STATUS			  0x001e
#define DPLL7_STATUS			  0x001f
#define DPLL_SYS_STATUS                   0x0020
#define DPLL_SYS_APLL_STATUS              0x0021
#define DPLL0_FILTER_STATUS               0x0044
#define DPLL1_FILTER_STATUS               0x004c
#define DPLL2_FILTER_STATUS               0x0054
#define DPLL3_FILTER_STATUS               0x005c
#define DPLL4_FILTER_STATUS               0x0064
#define DPLL5_FILTER_STATUS               0x006c
#define DPLL6_FILTER_STATUS               0x0074
#define DPLL7_FILTER_STATUS               0x007c
#define DPLLSYS_FILTER_STATUS             0x0084
#define USER_GPIO0_TO_7_STATUS            0x008a
#define USER_GPIO8_TO_15_STATUS           0x008b

#define GPIO_USER_CONTROL                 0xc160
#define GPIO0_TO_7_OUT                    0x0000
#define GPIO8_TO_15_OUT                   0x0001
#define GPIO0_TO_7_OUT_V520               0x0002
#define GPIO8_TO_15_OUT_V520              0x0003

#define STICKY_STATUS_CLEAR               0xc164

#define GPIO_TOD_NOTIFICATION_CLEAR       0xc16c

#define ALERT_CFG                         0xc188

#define SYS_DPLL_XO                       0xc194

#define SYS_APLL                          0xc19c

#define INPUT_0                           0xc1b0
#define INPUT_1                           0xc1c0
#define INPUT_2                           0xc1d0
#define INPUT_3                           0xc200
#define INPUT_4                           0xc210
#define INPUT_5                           0xc220
#define INPUT_6                           0xc230
#define INPUT_7                           0xc240
#define INPUT_8                           0xc250
#define INPUT_9                           0xc260
#define INPUT_10                          0xc280
#define INPUT_11                          0xc290
#define INPUT_12                          0xc2a0
#define INPUT_13                          0xc2b0
#define INPUT_14                          0xc2c0
#define INPUT_15                          0xc2d0

#define REF_MON_0                         0xc2e0
#define REF_MON_1                         0xc2ec
#define REF_MON_2                         0xc300
#define REF_MON_3                         0xc30c
#define REF_MON_4                         0xc318
#define REF_MON_5                         0xc324
#define REF_MON_6                         0xc330
#define REF_MON_7                         0xc33c
#define REF_MON_8                         0xc348
#define REF_MON_9                         0xc354
#define REF_MON_10                        0xc360
#define REF_MON_11                        0xc36c
#define REF_MON_12                        0xc380
#define REF_MON_13                        0xc38c
#define REF_MON_14                        0xc398
#define REF_MON_15                        0xc3a4

#define DPLL_0                            0xc3b0
#define DPLL_CTRL_REG_0                   0x0002
#define DPLL_CTRL_REG_1                   0x0003
#define DPLL_CTRL_REG_2                   0x0004
#define DPLL_TOD_SYNC_CFG                 0x0031
#define DPLL_COMBO_SLAVE_CFG_0            0x0032
#define DPLL_COMBO_SLAVE_CFG_1            0x0033
#define DPLL_SLAVE_REF_CFG                0x0034
#define DPLL_REF_MODE                     0x0035
#define DPLL_PHASE_MEASUREMENT_CFG        0x0036
#define DPLL_MODE                         0x0037
#define DPLL_MODE_V520                    0x003B
#define DPLL_1                            0xc400
#define DPLL_2                            0xc438
#define DPLL_2_V520                       0xc43c
#define DPLL_3                            0xc480
#define DPLL_4                            0xc4b8
#define DPLL_4_V520                       0xc4bc
#define DPLL_5                            0xc500
#define DPLL_6                            0xc538
#define DPLL_6_V520                       0xc53c
#define DPLL_7                            0xc580
#define SYS_DPLL                          0xc5b8
#define SYS_DPLL_V520                     0xc5bc

#define DPLL_CTRL_0                       0xc600
#define DPLL_CTRL_DPLL_MANU_REF_CFG       0x0001
#define DPLL_CTRL_DPLL_FOD_FREQ           0x001c
#define DPLL_CTRL_COMBO_MASTER_CFG        0x003a
#define DPLL_CTRL_1                       0xc63c
#define DPLL_CTRL_2                       0xc680
#define DPLL_CTRL_3                       0xc6bc
#define DPLL_CTRL_4                       0xc700
#define DPLL_CTRL_5                       0xc73c
#define DPLL_CTRL_6                       0xc780
#define DPLL_CTRL_7                       0xc7bc
#define SYS_DPLL_CTRL                     0xc800

#define DPLL_PHASE_0                      0xc818
/* Signed 42-bit FFO in units of 2^(-53) */
#define DPLL_WR_PHASE                     0x0000
#define DPLL_PHASE_1                      0xc81c
#define DPLL_PHASE_2                      0xc820
#define DPLL_PHASE_3                      0xc824
#define DPLL_PHASE_4                      0xc828
#define DPLL_PHASE_5                      0xc82c
#define DPLL_PHASE_6                      0xc830
#define DPLL_PHASE_7                      0xc834

#define DPLL_FREQ_0                       0xc838
/* Signed 42-bit FFO in units of 2^(-53) */
#define DPLL_WR_FREQ                      0x0000
#define DPLL_FREQ_1                       0xc840
#define DPLL_FREQ_2                       0xc848
#define DPLL_FREQ_3                       0xc850
#define DPLL_FREQ_4                       0xc858
#define DPLL_FREQ_5                       0xc860
#define DPLL_FREQ_6                       0xc868
#define DPLL_FREQ_7                       0xc870

#define DPLL_PHASE_PULL_IN_0              0xc880
#define PULL_IN_OFFSET                    0x0000 /* Signed 32 bit */
#define PULL_IN_SLOPE_LIMIT               0x0004 /* Unsigned 24 bit */
#define PULL_IN_CTRL                      0x0007
#define DPLL_PHASE_PULL_IN_1              0xc888
#define DPLL_PHASE_PULL_IN_2              0xc890
#define DPLL_PHASE_PULL_IN_3              0xc898
#define DPLL_PHASE_PULL_IN_4              0xc8a0
#define DPLL_PHASE_PULL_IN_5              0xc8a8
#define DPLL_PHASE_PULL_IN_6              0xc8b0
#define DPLL_PHASE_PULL_IN_7              0xc8b8

#define GPIO_CFG                          0xc8c0
#define GPIO_CFG_GBL                      0x0000
#define GPIO_0                            0xc8c2
#define GPIO_DCO_INC_DEC                  0x0000
#define GPIO_OUT_CTRL_0                   0x0001
#define GPIO_OUT_CTRL_1                   0x0002
#define GPIO_TOD_TRIG                     0x0003
#define GPIO_DPLL_INDICATOR               0x0004
#define GPIO_LOS_INDICATOR                0x0005
#define GPIO_REF_INPUT_DSQ_0              0x0006
#define GPIO_REF_INPUT_DSQ_1              0x0007
#define GPIO_REF_INPUT_DSQ_2              0x0008
#define GPIO_REF_INPUT_DSQ_3              0x0009
#define GPIO_MAN_CLK_SEL_0                0x000a
#define GPIO_MAN_CLK_SEL_1                0x000b
#define GPIO_MAN_CLK_SEL_2                0x000c
#define GPIO_SLAVE                        0x000d
#define GPIO_ALERT_OUT_CFG                0x000e
#define GPIO_TOD_NOTIFICATION_CFG         0x000f
#define GPIO_CTRL                         0x0010
#define GPIO_CTRL_V520                    0x0011
#define GPIO_1                            0xc8d4
#define GPIO_2                            0xc8e6
#define GPIO_3                            0xc900
#define GPIO_4                            0xc912
#define GPIO_5                            0xc924
#define GPIO_6                            0xc936
#define GPIO_7                            0xc948
#define GPIO_8                            0xc95a
#define GPIO_9                            0xc980
#define GPIO_10                           0xc992
#define GPIO_11                           0xc9a4
#define GPIO_12                           0xc9b6
#define GPIO_13                           0xc9c8
#define GPIO_14                           0xc9da
#define GPIO_15                           0xca00

#define OUT_DIV_MUX                       0xca12
#define OUTPUT_0                          0xca14
#define OUTPUT_0_V520                     0xca20
/* FOD frequency output divider value */
#define OUT_DIV                           0x0000
#define OUT_DUTY_CYCLE_HIGH               0x0004
#define OUT_CTRL_0                        0x0008
#define OUT_CTRL_1                        0x0009
/* Phase adjustment in FOD cycles */
#define OUT_PHASE_ADJ                     0x000c
#define OUTPUT_1                          0xca24
#define OUTPUT_1_V520                     0xca30
#define OUTPUT_2                          0xca34
#define OUTPUT_2_V520                     0xca40
#define OUTPUT_3                          0xca44
#define OUTPUT_3_V520                     0xca50
#define OUTPUT_4                          0xca54
#define OUTPUT_4_V520                     0xca60
#define OUTPUT_5                          0xca64
#define OUTPUT_5_V520                     0xca80
#define OUTPUT_6                          0xca80
#define OUTPUT_6_V520                     0xca90
#define OUTPUT_7                          0xca90
#define OUTPUT_7_V520                     0xcaa0
#define OUTPUT_8                          0xcaa0
#define OUTPUT_8_V520                     0xcab0
#define OUTPUT_9                          0xcab0
#define OUTPUT_9_V520                     0xcac0
#define OUTPUT_10                         0xcac0
#define OUTPUT_10_V520                    0xcad0
#define OUTPUT_11                         0xcad0
#define OUTPUT_11_V520                    0xcae0

#define SERIAL                            0xcae0
#define SERIAL_V520                       0xcaf0

#define PWM_ENCODER_0                     0xcb00
#define PWM_ENCODER_1                     0xcb08
#define PWM_ENCODER_2                     0xcb10
#define PWM_ENCODER_3                     0xcb18
#define PWM_ENCODER_4                     0xcb20
#define PWM_ENCODER_5                     0xcb28
#define PWM_ENCODER_6                     0xcb30
#define PWM_ENCODER_7                     0xcb38
#define PWM_DECODER_0                     0xcb40
#define PWM_DECODER_1                     0xcb48
#define PWM_DECODER_1_V520                0xcb4a
#define PWM_DECODER_2                     0xcb50
#define PWM_DECODER_2_V520                0xcb54
#define PWM_DECODER_3                     0xcb58
#define PWM_DECODER_3_V520                0xcb5e
#define PWM_DECODER_4                     0xcb60
#define PWM_DECODER_4_V520                0xcb68
#define PWM_DECODER_5                     0xcb68
#define PWM_DECODER_5_V520                0xcb80
#define PWM_DECODER_6                     0xcb70
#define PWM_DECODER_6_V520                0xcb8a
#define PWM_DECODER_7                     0xcb80
#define PWM_DECODER_7_V520                0xcb94
#define PWM_DECODER_8                     0xcb88
#define PWM_DECODER_8_V520                0xcb9e
#define PWM_DECODER_9                     0xcb90
#define PWM_DECODER_9_V520                0xcba8
#define PWM_DECODER_10                    0xcb98
#define PWM_DECODER_10_V520               0xcbb2
#define PWM_DECODER_11                    0xcba0
#define PWM_DECODER_11_V520               0xcbbc
#define PWM_DECODER_12                    0xcba8
#define PWM_DECODER_12_V520               0xcbc6
#define PWM_DECODER_13                    0xcbb0
#define PWM_DECODER_13_V520               0xcbd0
#define PWM_DECODER_14                    0xcbb8
#define PWM_DECODER_14_V520               0xcbda
#define PWM_DECODER_15                    0xcbc0
#define PWM_DECODER_15_V520               0xcbe4
#define PWM_USER_DATA                     0xcbc8
#define PWM_USER_DATA_V520                0xcbf0

#define TOD_0                             0xcbcc
#define TOD_0_V520                        0xcc00
/* Enable TOD counter, output channel sync and even-PPS mode */
#define TOD_CFG                           0x0000
#define TOD_CFG_V520                      0x0001
#define TOD_1                             0xcbce
#define TOD_1_V520                        0xcc02
#define TOD_2                             0xcbd0
#define TOD_2_V520                        0xcc04
#define TOD_3                             0xcbd2
#define TOD_3_V520                        0xcc06

#define TOD_WRITE_0                       0xcc00
#define TOD_WRITE_0_V520                  0xcc10
/* 8-bit subns, 32-bit ns, 48-bit seconds */
#define TOD_WRITE                         0x0000
/* Counter increments after TOD write is completed */
#define TOD_WRITE_COUNTER                 0x000c
/* TOD write trigger configuration */
#define TOD_WRITE_SELECT_CFG_0            0x000d
/* TOD write trigger selection */
#define TOD_WRITE_CMD                     0x000f
#define TOD_WRITE_1                       0xcc10
#define TOD_WRITE_1_V520                  0xcc20
#define TOD_WRITE_2                       0xcc20
#define TOD_WRITE_2_V520                  0xcc30
#define TOD_WRITE_3                       0xcc30
#define TOD_WRITE_3_V520                  0xcc40

#define TOD_READ_PRIMARY_0                0xcc40
#define TOD_READ_PRIMARY_0_V520           0xcc50
/* 8-bit subns, 32-bit ns, 48-bit seconds */
#define TOD_READ_PRIMARY_BASE             0x0000
/* Counter increments after TOD write is completed */
#define TOD_READ_PRIMARY_COUNTER          0x000b
/* Read trigger configuration */
#define TOD_READ_PRIMARY_SEL_CFG_0        0x000c
/* Read trigger selection */
#define TOD_READ_PRIMARY_CMD              0x000e
#define TOD_READ_PRIMARY_CMD_V520         0x000f
#define TOD_READ_PRIMARY_1                0xcc50
#define TOD_READ_PRIMARY_1_V520           0xcc60
#define TOD_READ_PRIMARY_2                0xcc60
#define TOD_READ_PRIMARY_2_V520           0xcc80
#define TOD_READ_PRIMARY_3                0xcc80
#define TOD_READ_PRIMARY_3_V520           0xcc90

#define TOD_READ_SECONDARY_0              0xcc90
#define TOD_READ_SECONDARY_0_V520         0xcca0
/* 8-bit subns, 32-bit ns, 48-bit seconds */
#define TOD_READ_SECONDARY_BASE           0x0000
/* Counter increments after TOD write is completed */
#define TOD_READ_SECONDARY_COUNTER        0x000b
/* Read trigger configuration */
#define TOD_READ_SECONDARY_SEL_CFG_0      0x000c
/* Read trigger selection */
#define TOD_READ_SECONDARY_CMD            0x000e
#define TOD_READ_SECONDARY_CMD_V520       0x000f

#define TOD_READ_SECONDARY_1              0xcca0
#define TOD_READ_SECONDARY_1_V520         0xccb0
#define TOD_READ_SECONDARY_2              0xccb0
#define TOD_READ_SECONDARY_2_V520         0xccc0
#define TOD_READ_SECONDARY_3              0xccc0
#define TOD_READ_SECONDARY_3_V520         0xccd0

#define OUTPUT_TDC_CFG                    0xccd0
#define OUTPUT_TDC_CFG_V520               0xcce0
#define OUTPUT_TDC_0                      0xcd00
#define OUTPUT_TDC_1                      0xcd08
#define OUTPUT_TDC_2                      0xcd10
#define OUTPUT_TDC_3                      0xcd18
#define INPUT_TDC                         0xcd20

#define SCRATCH                           0xcf50
#define SCRATCH_V520                      0xcf4c

#define EEPROM                            0xcf68
#define EEPROM_V520                       0xcf64

#define OTP                               0xcf70

#define BYTE                              0xcf80

/* Bit definitions for the MAJ_REL register */
#define MAJOR_SHIFT                       (1)
#define MAJOR_MASK                        (0x7f)
#define PR_BUILD                          BIT(0)

/* Bit definitions for the USER_GPIO0_TO_7_STATUS register */
#define GPIO0_LEVEL                       BIT(0)
#define GPIO1_LEVEL                       BIT(1)
#define GPIO2_LEVEL                       BIT(2)
#define GPIO3_LEVEL                       BIT(3)
#define GPIO4_LEVEL                       BIT(4)
#define GPIO5_LEVEL                       BIT(5)
#define GPIO6_LEVEL                       BIT(6)
#define GPIO7_LEVEL                       BIT(7)

/* Bit definitions for the USER_GPIO8_TO_15_STATUS register */
#define GPIO8_LEVEL                       BIT(0)
#define GPIO9_LEVEL                       BIT(1)
#define GPIO10_LEVEL                      BIT(2)
#define GPIO11_LEVEL                      BIT(3)
#define GPIO12_LEVEL                      BIT(4)
#define GPIO13_LEVEL                      BIT(5)
#define GPIO14_LEVEL                      BIT(6)
#define GPIO15_LEVEL                      BIT(7)

/* Bit definitions for the GPIO0_TO_7_OUT register */
#define GPIO0_DRIVE_LEVEL                 BIT(0)
#define GPIO1_DRIVE_LEVEL                 BIT(1)
#define GPIO2_DRIVE_LEVEL                 BIT(2)
#define GPIO3_DRIVE_LEVEL                 BIT(3)
#define GPIO4_DRIVE_LEVEL                 BIT(4)
#define GPIO5_DRIVE_LEVEL                 BIT(5)
#define GPIO6_DRIVE_LEVEL                 BIT(6)
#define GPIO7_DRIVE_LEVEL                 BIT(7)

/* Bit definitions for the GPIO8_TO_15_OUT register */
#define GPIO8_DRIVE_LEVEL                 BIT(0)
#define GPIO9_DRIVE_LEVEL                 BIT(1)
#define GPIO10_DRIVE_LEVEL                BIT(2)
#define GPIO11_DRIVE_LEVEL                BIT(3)
#define GPIO12_DRIVE_LEVEL                BIT(4)
#define GPIO13_DRIVE_LEVEL                BIT(5)
#define GPIO14_DRIVE_LEVEL                BIT(6)
#define GPIO15_DRIVE_LEVEL                BIT(7)

/* Bit definitions for the DPLL_TOD_SYNC_CFG register */
#define TOD_SYNC_SOURCE_SHIFT             (1)
#define TOD_SYNC_SOURCE_MASK              (0x3)
#define TOD_SYNC_EN                       BIT(0)

/* Bit definitions for the DPLL_MODE register */
#define WRITE_TIMER_MODE                  BIT(6)
#define PLL_MODE_SHIFT                    (3)
#define PLL_MODE_MASK                     (0x7)
#define STATE_MODE_SHIFT                  (0)
#define STATE_MODE_MASK                   (0x7)

/* Bit definitions for the DPLL_MANU_REF_CFG register */
#define MANUAL_REFERENCE_SHIFT            (0)
#define MANUAL_REFERENCE_MASK             (0x1f)

/* Bit definitions for the GPIO_CFG_GBL register */
#define SUPPLY_MODE_SHIFT                 (0)
#define SUPPLY_MODE_MASK                  (0x3)

/* Bit definitions for the GPIO_DCO_INC_DEC register */
#define INCDEC_DPLL_INDEX_SHIFT           (0)
#define INCDEC_DPLL_INDEX_MASK            (0x7)

/* Bit definitions for the GPIO_OUT_CTRL_0 register */
#define CTRL_OUT_0                        BIT(0)
#define CTRL_OUT_1                        BIT(1)
#define CTRL_OUT_2                        BIT(2)
#define CTRL_OUT_3                        BIT(3)
#define CTRL_OUT_4                        BIT(4)
#define CTRL_OUT_5                        BIT(5)
#define CTRL_OUT_6                        BIT(6)
#define CTRL_OUT_7                        BIT(7)

/* Bit definitions for the GPIO_OUT_CTRL_1 register */
#define CTRL_OUT_8                        BIT(0)
#define CTRL_OUT_9                        BIT(1)
#define CTRL_OUT_10                       BIT(2)
#define CTRL_OUT_11                       BIT(3)
#define CTRL_OUT_12                       BIT(4)
#define CTRL_OUT_13                       BIT(5)
#define CTRL_OUT_14                       BIT(6)
#define CTRL_OUT_15                       BIT(7)

/* Bit definitions for the GPIO_TOD_TRIG register */
#define TOD_TRIG_0                        BIT(0)
#define TOD_TRIG_1                        BIT(1)
#define TOD_TRIG_2                        BIT(2)
#define TOD_TRIG_3                        BIT(3)

/* Bit definitions for the GPIO_DPLL_INDICATOR register */
#define IND_DPLL_INDEX_SHIFT              (0)
#define IND_DPLL_INDEX_MASK               (0x7)

/* Bit definitions for the GPIO_LOS_INDICATOR register */
#define REFMON_INDEX_SHIFT                (0)
#define REFMON_INDEX_MASK                 (0xf)
/* Active level of LOS indicator, 0=low 1=high */
#define ACTIVE_LEVEL                      BIT(4)

/* Bit definitions for the GPIO_REF_INPUT_DSQ_0 register */
#define DSQ_INP_0                         BIT(0)
#define DSQ_INP_1                         BIT(1)
#define DSQ_INP_2                         BIT(2)
#define DSQ_INP_3                         BIT(3)
#define DSQ_INP_4                         BIT(4)
#define DSQ_INP_5                         BIT(5)
#define DSQ_INP_6                         BIT(6)
#define DSQ_INP_7                         BIT(7)

/* Bit definitions for the GPIO_REF_INPUT_DSQ_1 register */
#define DSQ_INP_8                         BIT(0)
#define DSQ_INP_9                         BIT(1)
#define DSQ_INP_10                        BIT(2)
#define DSQ_INP_11                        BIT(3)
#define DSQ_INP_12                        BIT(4)
#define DSQ_INP_13                        BIT(5)
#define DSQ_INP_14                        BIT(6)
#define DSQ_INP_15                        BIT(7)

/* Bit definitions for the GPIO_REF_INPUT_DSQ_2 register */
#define DSQ_DPLL_0                        BIT(0)
#define DSQ_DPLL_1                        BIT(1)
#define DSQ_DPLL_2                        BIT(2)
#define DSQ_DPLL_3                        BIT(3)
#define DSQ_DPLL_4                        BIT(4)
#define DSQ_DPLL_5                        BIT(5)
#define DSQ_DPLL_6                        BIT(6)
#define DSQ_DPLL_7                        BIT(7)

/* Bit definitions for the GPIO_REF_INPUT_DSQ_3 register */
#define DSQ_DPLL_SYS                      BIT(0)
#define GPIO_DSQ_LEVEL                    BIT(1)

/* Bit definitions for the GPIO_TOD_NOTIFICATION_CFG register */
#define DPLL_TOD_SHIFT                    (0)
#define DPLL_TOD_MASK                     (0x3)
#define TOD_READ_SECONDARY                BIT(2)
#define GPIO_ASSERT_LEVEL                 BIT(3)

/* Bit definitions for the GPIO_CTRL register */
#define GPIO_FUNCTION_EN                  BIT(0)
#define GPIO_CMOS_OD_MODE                 BIT(1)
#define GPIO_CONTROL_DIR                  BIT(2)
#define GPIO_PU_PD_MODE                   BIT(3)
#define GPIO_FUNCTION_SHIFT               (4)
#define GPIO_FUNCTION_MASK                (0xf)

/* Bit definitions for the OUT_CTRL_1 register */
#define OUT_SYNC_DISABLE                  BIT(7)
#define SQUELCH_VALUE                     BIT(6)
#define SQUELCH_DISABLE                   BIT(5)
#define PAD_VDDO_SHIFT                    (2)
#define PAD_VDDO_MASK                     (0x7)
#define PAD_CMOSDRV_SHIFT                 (0)
#define PAD_CMOSDRV_MASK                  (0x3)

/* Bit definitions for the TOD_CFG register */
#define TOD_EVEN_PPS_MODE                 BIT(2)
#define TOD_OUT_SYNC_ENABLE               BIT(1)
#define TOD_ENABLE                        BIT(0)

/* Bit definitions for the TOD_WRITE_SELECT_CFG_0 register */
#define WR_PWM_DECODER_INDEX_SHIFT        (4)
#define WR_PWM_DECODER_INDEX_MASK         (0xf)
#define WR_REF_INDEX_SHIFT                (0)
#define WR_REF_INDEX_MASK                 (0xf)

/* Bit definitions for the TOD_WRITE_CMD register */
#define TOD_WRITE_SELECTION_SHIFT         (0)
#define TOD_WRITE_SELECTION_MASK          (0xf)
/* 4.8.7 */
#define TOD_WRITE_TYPE_SHIFT              (4)
#define TOD_WRITE_TYPE_MASK               (0x3)

/* Bit definitions for the TOD_READ_PRIMARY_SEL_CFG_0 register */
#define RD_PWM_DECODER_INDEX_SHIFT        (4)
#define RD_PWM_DECODER_INDEX_MASK         (0xf)
#define RD_REF_INDEX_SHIFT                (0)
#define RD_REF_INDEX_MASK                 (0xf)

/* Bit definitions for the TOD_READ_PRIMARY_CMD register */
#define TOD_READ_TRIGGER_MODE             BIT(4)
#define TOD_READ_TRIGGER_SHIFT            (0)
#define TOD_READ_TRIGGER_MASK             (0xf)
#define TOD_REF_INDEX_MASK                (0xf)

/* Bit definitions for the DPLL_CTRL_COMBO_MASTER_CFG register */
#define COMBO_MASTER_HOLD                 BIT(0)

/* Bit definitions for DPLL_SYS_STATUS register */
#define DPLL_SYS_STATE_MASK               (0xf)

/* Bit definitions for SYS_APLL_STATUS register */
#define SYS_APLL_LOSS_LOCK_LIVE_MASK       BIT(0)
#define SYS_APLL_LOSS_LOCK_LIVE_LOCKED     0
#define SYS_APLL_LOSS_LOCK_LIVE_UNLOCKED   1
#define SYS_APLL_STATE_MASK               (0x1)

/* Bit definitions for the DPLL0_STATUS register */
#define DPLL_STATE_MASK                   (0xf)
#define DPLL_STATE_SHIFT                  (0x0)

/* Values of DPLL_N.DPLL_MODE.STATE_MODE */
enum pll_state {
	PLL_STATE_MIN = 0,
	PLL_STATE_AUTO = PLL_STATE_MIN,
	PLL_STATE_FORCE_LOCK = 1,
	PLL_STATE_FORCE_FRUN = 2,
	PLL_STATE_FORCE_HOLDOVER = 3,
	PLL_STATE_MAX = PLL_STATE_FORCE_HOLDOVER,
};

/* Values of DPLL_N.DPLL_MODE.PLL_MODE */
enum pll_mode {
	PLL_MODE_MIN = 0,
	PLL_MODE_PLL = PLL_MODE_MIN,
	PLL_MODE_WRITE_PHASE = 1,
	PLL_MODE_WRITE_FREQUENCY = 2,
	PLL_MODE_GPIO_INC_DEC = 3,
	PLL_MODE_SYNTHESIS = 4,
	PLL_MODE_PHASE_MEASUREMENT = 5,
	PLL_MODE_DISABLED = 6,
	PLL_MODE_MAX = PLL_MODE_DISABLED,
};

/* Values of DPLL_CTRL_n.DPLL_MANU_REF_CFG.MANUAL_REFERENCE */
enum manual_reference {
	MANU_REF_MIN = 0,
	MANU_REF_CLK0 = MANU_REF_MIN,
	MANU_REF_CLK1,
	MANU_REF_CLK2,
	MANU_REF_CLK3,
	MANU_REF_CLK4,
	MANU_REF_CLK5,
	MANU_REF_CLK6,
	MANU_REF_CLK7,
	MANU_REF_CLK8,
	MANU_REF_CLK9,
	MANU_REF_CLK10,
	MANU_REF_CLK11,
	MANU_REF_CLK12,
	MANU_REF_CLK13,
	MANU_REF_CLK14,
	MANU_REF_CLK15,
	MANU_REF_WRITE_PHASE,
	MANU_REF_WRITE_FREQUENCY,
	MANU_REF_XO_DPLL,
	MANU_REF_MAX = MANU_REF_XO_DPLL,
};

enum hw_tod_write_trig_sel {
	HW_TOD_WR_TRIG_SEL_MIN = 0,
	HW_TOD_WR_TRIG_SEL_MSB = HW_TOD_WR_TRIG_SEL_MIN,
	HW_TOD_WR_TRIG_SEL_RESERVED = 1,
	HW_TOD_WR_TRIG_SEL_TOD_PPS = 2,
	HW_TOD_WR_TRIG_SEL_IRIGB_PPS = 3,
	HW_TOD_WR_TRIG_SEL_PWM_PPS = 4,
	HW_TOD_WR_TRIG_SEL_GPIO = 5,
	HW_TOD_WR_TRIG_SEL_FOD_SYNC = 6,
	WR_TRIG_SEL_MAX = HW_TOD_WR_TRIG_SEL_FOD_SYNC,
};

enum scsr_read_trig_sel {
	/* CANCEL CURRENT TOD READ; MODULE BECOMES IDLE - NO TRIGGER OCCURS */
	SCSR_TOD_READ_TRIG_SEL_DISABLE = 0,
	/* TRIGGER IMMEDIATELY */
	SCSR_TOD_READ_TRIG_SEL_IMMEDIATE = 1,
	/* TRIGGER ON RISING EDGE OF INTERNAL TOD PPS SIGNAL */
	SCSR_TOD_READ_TRIG_SEL_TODPPS = 2,
	/* TRGGER ON RISING EDGE OF SELECTED REFERENCE INPUT */
	SCSR_TOD_READ_TRIG_SEL_REFCLK = 3,
	/* TRIGGER ON RISING EDGE OF SELECTED PWM DECODER 1PPS OUTPUT */
	SCSR_TOD_READ_TRIG_SEL_PWMPPS = 4,
	SCSR_TOD_READ_TRIG_SEL_RESERVED = 5,
	/* TRIGGER WHEN WRITE FREQUENCY EVENT OCCURS  */
	SCSR_TOD_READ_TRIG_SEL_WRITEFREQUENCYEVENT = 6,
	/* TRIGGER ON SELECTED GPIO */
	SCSR_TOD_READ_TRIG_SEL_GPIO = 7,
	SCSR_TOD_READ_TRIG_SEL_MAX = SCSR_TOD_READ_TRIG_SEL_GPIO,
};

/* Values STATUS.DPLL_SYS_STATUS.DPLL_SYS_STATE */
enum dpll_state {
	DPLL_STATE_MIN = 0,
	DPLL_STATE_FREERUN = DPLL_STATE_MIN,
	DPLL_STATE_LOCKACQ = 1,
	DPLL_STATE_LOCKREC = 2,
	DPLL_STATE_LOCKED = 3,
	DPLL_STATE_HOLDOVER = 4,
	DPLL_STATE_OPEN_LOOP = 5,
	DPLL_STATE_DISABLED = 6,
	DPLL_STATE_MAX = DPLL_STATE_DISABLED,
};

/* 4.8.7 only */
enum scsr_tod_write_trig_sel {
	SCSR_TOD_WR_TRIG_SEL_DISABLE = 0,
	SCSR_TOD_WR_TRIG_SEL_IMMEDIATE = 1,
	SCSR_TOD_WR_TRIG_SEL_REFCLK = 2,
	SCSR_TOD_WR_TRIG_SEL_PWMPPS = 3,
	SCSR_TOD_WR_TRIG_SEL_TODPPS = 4,
	SCSR_TOD_WR_TRIG_SEL_SYNCFOD = 5,
	SCSR_TOD_WR_TRIG_SEL_GPIO = 6,
	SCSR_TOD_WR_TRIG_SEL_MAX = SCSR_TOD_WR_TRIG_SEL_GPIO,
};

/* 4.8.7 only */
enum scsr_tod_write_type_sel {
	SCSR_TOD_WR_TYPE_SEL_ABSOLUTE = 0,
	SCSR_TOD_WR_TYPE_SEL_DELTA_PLUS = 1,
	SCSR_TOD_WR_TYPE_SEL_DELTA_MINUS = 2,
	SCSR_TOD_WR_TYPE_SEL_MAX = SCSR_TOD_WR_TYPE_SEL_DELTA_MINUS,
};

// Register Access Permissions
typedef enum {
    REG_PERM_R  = 0x01, // Read Only
    REG_PERM_W  = 0x02, // Write Only
    REG_PERM_RW = 0x03  // Read and Write
} reg_perm_t;

// Unified Register Descriptor Structure
typedef struct {
    const char *name;       // Human-readable string identifier
    uint8_t     page;       // Upper 8 bits (Page Selector, e.g., 0xC0)
    uint8_t     base;       // Base address (Base address, e.g., 0x14)
    uint16_t    offset;     // Lower 8 bits (Register Window Offset, e.g., 0x10)
    uint8_t     width_bytes;// Number of continuous bytes to burst read/write
    reg_perm_t  perm;       // Software barrier flags
} ptp_reg_desc_t;
#define __ptp_reg_desc_init_val__ { NULL, 0, 0, 0, 0, REG_PERM_R }

// System-wide 8A34004 Functional Register Matrix Array Definition - General Registers
static const ptp_reg_desc_t g_8a34004_register_map_general[] = {
    { "RESET_CTRL.SM_RESET",            0xC0, 0x00, 0x12,  1, REG_PERM_RW },
    { "GENERAL_STATUS.BOOT_STATUS",     0xC0, 0x14, 0x00,  1, REG_PERM_RW },
    /* GLOBAL HARDWARE REVISION & ID ENTITIES */
    { "GENERAL_STATUS.PRODUCT_ID_L",    0xC0, 0x14, 0x1E,  1, REG_PERM_R  },
    { "GENERAL_STATUS.PRODUCT_ID_H",    0xC0, 0x14, 0x1F,  1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MAJOR",   0xC0, 0x14, 0x10,  1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MINOR",   0xC0, 0x14, 0x11,  1, REG_PERM_R  },
    { "HW_REVISION.REV_ID",             0x81, 0x80, 0x7A,  1, REG_PERM_R  }, // Product revision validation target
    { "STATUS.SYS_DPLL_STATUS",         0xC0, 0x3C, 0x20,  1, REG_PERM_RW },
    { "STATUS.SYS_APLL_STATUS",         0xC0, 0x3C, 0x21,  1, REG_PERM_RW },
    /* ASTERFUSION CUSTOM HOLDOVER PROTECTION FLAGGING */
    { "SCRATCH.SCRATCH3",               0xCF, 0x50, 0x0E,  1, REG_PERM_RW },

    /* PHYSICAL LINE SYNCE INPUT CLOCK ARRAYS ((M/N) FRACTION FORM) */
    { "INPUT_0.IN_FREQ",                0xC1, 0xB0, 0x00,  8, REG_PERM_RW }, /* OBSCLK 0: 6B M + 2B N */
    { "INPUT_1.IN_FREQ",                0xC1, 0xC0, 0x00,  8, REG_PERM_RW }, /* OBSCLK 1: 6B M + 2B N */

    { "DPLL_PHASE.EXEC_TRIGGER",        0xC8, 0x00, 0x0F,  1, REG_PERM_W  }, /* Global Phase trigger gate (Forces Pull-in) */
    { "DPLL_FREQ.EXEC_TRIGGER",         0xC8, 0x00, 0x3F,  1, REG_PERM_W  }, /* Global Frequency trigger gate */
    __ptp_reg_desc_init_val__
};

// System-wide 8A34004 Functional Register Matrix Array Definition - DPLL-Index-Based Registers
// End with NULL
static const ptp_reg_desc_t g_8a34004_register_map_dpll[AFN_8A34004_MAX_DPLL][MAX_ENTRIES_PER_DPLL] = {
    /* DPLL 0 */
    {
        { "DPLL0.FREQ",                     0xC8, 0x38, 0x00, 6, REG_PERM_RW },
        { "DPLL0.PHASE",                    0xC8, 0x18, 0x00, 4, REG_PERM_W  },
        { "DPLL0.TOD_SYNC_CFG",             0xC3, 0xB0, 0x31, 1, REG_PERM_RW },
        { "DPLL0.COMBO_SLAVE_CFG_0",        0xC3, 0xB0, 0x32, 1, REG_PERM_RW },
        { "DPLL0.COMBO_SLAVE_CFG_1",        0xC3, 0xB0, 0x33, 1, REG_PERM_RW },
        { "DPLL0.MODE",                     0xC3, 0xB0, 0x37, 1, REG_PERM_RW },
        { "DPLL0.STATUS",                   0xC0, 0x3C, 0x18, 1, REG_PERM_R  },
        { "DPLL0.REF_STAT",                 0xC0, 0x3C, 0x22, 1, REG_PERM_R  },
        { "DPLL0.PHASE_STATUS",             0xC0, 0x3C, 0xDC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 1 */
    {
        { "DPLL1.FREQ",                     0xC8, 0x40, 0x00, 6, REG_PERM_RW },
        { "DPLL1.PHASE",                    0xC8, 0x1C, 0x00, 4, REG_PERM_W  },
        { "DPLL1.TOD_SYNC_CFG",             0xC4, 0x00, 0x31, 1, REG_PERM_RW },
        { "DPLL1.COMBO_SLAVE_CFG_0",        0xC4, 0x00, 0x32, 1, REG_PERM_RW },
        { "DPLL1.COMBO_SLAVE_CFG_1",        0xC4, 0x00, 0x33, 1, REG_PERM_RW },
        { "DPLL1.MODE",                     0xC4, 0x00, 0x37, 1, REG_PERM_RW },
        { "DPLL1.STATUS",                   0xC0, 0x3C, 0x19, 1, REG_PERM_R  },
        { "DPLL1.REF_STAT",                 0xC0, 0x3C, 0x23, 1, REG_PERM_R  },
        { "DPLL1.PHASE_STATUS",             0xC0, 0x3C, 0xE4, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 2 */
    {
        { "DPLL2.FREQ",                     0xC8, 0x48, 0x00, 6, REG_PERM_RW },
        { "DPLL2.PHASE",                    0xC8, 0x20, 0x00, 4, REG_PERM_W  },
        { "DPLL2.TOD_SYNC_CFG",             0xC4, 0x38, 0x31, 1, REG_PERM_RW },
        { "DPLL2.COMBO_SLAVE_CFG_0",        0xC4, 0x38, 0x32, 1, REG_PERM_RW },
        { "DPLL2.COMBO_SLAVE_CFG_1",        0xC4, 0x38, 0x33, 1, REG_PERM_RW },
        { "DPLL2.MODE",                     0xC4, 0x38, 0x37, 1, REG_PERM_RW },
        { "DPLL2.STATUS",                   0xC0, 0x3C, 0x1A, 1, REG_PERM_R  },
        { "DPLL2.REF_STAT",                 0xC0, 0x3C, 0x24, 1, REG_PERM_R  },
        { "DPLL2.PHASE_STATUS",             0xC0, 0x3C, 0xEC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 3 */
    {
        { "DPLL3.FREQ",                     0xC8, 0x50, 0x00, 6, REG_PERM_RW },
        { "DPLL3.PHASE",                    0xC8, 0x24, 0x00, 4, REG_PERM_W  },
        { "DPLL3.TOD_SYNC_CFG",             0xC4, 0x80, 0x31, 1, REG_PERM_RW },
        { "DPLL3.COMBO_SLAVE_CFG_0",        0xC4, 0x80, 0x32, 1, REG_PERM_RW },
        { "DPLL3.COMBO_SLAVE_CFG_1",        0xC4, 0x80, 0x33, 1, REG_PERM_RW },
        { "DPLL3.MODE",                     0xC4, 0x80, 0x37, 1, REG_PERM_RW },
        { "DPLL3.STATUS",                   0xC0, 0x3C, 0x1B, 1, REG_PERM_R  },
        { "DPLL3.REF_STAT",                 0xC0, 0x3C, 0x25, 1, REG_PERM_R  },
        { "DPLL3.PHASE_STATUS",             0xC0, 0x3C, 0xF4, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 4 */
    {
        { "DPLL4.FREQ",                     0xC8, 0x58, 0x00, 6, REG_PERM_RW },
        { "DPLL4.PHASE",                    0xC8, 0x28, 0x00, 4, REG_PERM_W  },
        { "DPLL4.TOD_SYNC_CFG",             0xC4, 0xB8, 0x31, 1, REG_PERM_RW },
        { "DPLL4.COMBO_SLAVE_CFG_0",        0xC4, 0xB8, 0x32, 1, REG_PERM_RW },
        { "DPLL4.COMBO_SLAVE_CFG_1",        0xC4, 0xB8, 0x33, 1, REG_PERM_RW },
        { "DPLL4.MODE",                     0xC4, 0xB8, 0x37, 1, REG_PERM_RW },
        { "DPLL4.STATUS",                   0xC0, 0x3C, 0x1C, 1, REG_PERM_R  },
        { "DPLL4.REF_STAT",                 0xC0, 0x3C, 0x26, 1, REG_PERM_R  },
        { "DPLL4.PHASE_STATUS",             0xC0, 0x3C, 0xFC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 5 */
    {
        { "DPLL5.FREQ",                     0xC8, 0x60, 0x00, 6, REG_PERM_RW },
        { "DPLL5.PHASE",                    0xC8, 0x2C, 0x00, 4, REG_PERM_W  },
        { "DPLL5.TOD_SYNC_CFG",             0xC5, 0x00, 0x31, 1, REG_PERM_RW },
        { "DPLL5.COMBO_SLAVE_CFG_0",        0xC5, 0x00, 0x32, 1, REG_PERM_RW },
        { "DPLL5.COMBO_SLAVE_CFG_1",        0xC5, 0x00, 0x33, 1, REG_PERM_RW },
        { "DPLL5.MODE",                     0xC5, 0x00, 0x37, 1, REG_PERM_RW },
        { "DPLL5.STATUS",                   0xC0, 0x3C, 0x1D, 1, REG_PERM_R  },
        { "DPLL5.REF_STAT",                 0xC0, 0x3C, 0x27, 1, REG_PERM_R  },
        { "DPLL5.PHASE_STATUS",             0xC0, 0x3C, 0x104, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 6 */
    {
        { "DPLL6.FREQ",                     0xC8, 0x68, 0x00, 6, REG_PERM_RW },
        { "DPLL6.PHASE",                    0xC8, 0x30, 0x00, 4, REG_PERM_W  },
        { "DPLL6.TOD_SYNC_CFG",             0xC5, 0x38, 0x31, 1, REG_PERM_RW },
        { "DPLL6.COMBO_SLAVE_CFG_0",        0xC5, 0x38, 0x32, 1, REG_PERM_RW },
        { "DPLL6.COMBO_SLAVE_CFG_1",        0xC5, 0x38, 0x33, 1, REG_PERM_RW },
        { "DPLL6.MODE",                     0xC5, 0x38, 0x37, 1, REG_PERM_RW },
        { "DPLL6.STATUS",                   0xC0, 0x3C, 0x1E, 1, REG_PERM_R  },
        { "DPLL6.REF_STAT",                 0xC0, 0x3C, 0x28, 1, REG_PERM_R  },
        { "DPLL6.PHASE_STATUS",             0xC0, 0x3C, 0x10C, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 7 */
    {
        { "DPLL7.FREQ",                     0xC8, 0x70, 0x00, 6, REG_PERM_RW },
        { "DPLL7.PHASE",                    0xC8, 0x34, 0x00, 4, REG_PERM_W  },
        { "DPLL7.TOD_SYNC_CFG",             0xC5, 0x80, 0x31, 1, REG_PERM_RW },
        { "DPLL7.COMBO_SLAVE_CFG_0",        0xC5, 0x80, 0x32, 1, REG_PERM_RW },
        { "DPLL7.COMBO_SLAVE_CFG_1",        0xC5, 0x80, 0x33, 1, REG_PERM_RW },
        { "DPLL7.MODE",                     0xC5, 0x80, 0x37, 1, REG_PERM_RW },
        { "DPLL7.STATUS",                   0xC0, 0x3C, 0x1F, 1, REG_PERM_R  },
        { "DPLL7.REF_STAT",                 0xC0, 0x3C, 0x29, 1, REG_PERM_R  },
        { "DPLL7.PHASE_STATUS",             0xC0, 0x3C, 0x114, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    }
};

static const ptp_reg_desc_t g_8a34004_register_map_tod[AFN_8A34004_MAX_TOD][MAX_ENTRIES_PER_TOD] = {
    /* TOD 0 */
    {
        { "TOD0.READ_PRIMARY.SUB_NS",      0xCC, 0x40, 0x00,  1, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.NS",          0xCC, 0x40, 0x01,  4, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.SECONDS",     0xCC, 0x40, 0x05,  6, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.COUNTER",     0xCC, 0x40, 0x0B,  1, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x40, 0x0C,  1, REG_PERM_RW },
        { "TOD0.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x40, 0x0D,  1, REG_PERM_RW },
        { "TOD0.READ_PRIMARY.TRIGGER",     0xCC, 0x40, 0x0E,  1, REG_PERM_RW },
        { "TOD0.WRITE.SUB_NS",             0xCC, 0x00, 0x00,  1, REG_PERM_RW },
        { "TOD0.WRITE.NS",                 0xCC, 0x00, 0x01,  4, REG_PERM_RW },
        { "TOD0.WRITE.SECONDS",            0xCC, 0x00, 0x05,  6, REG_PERM_RW },
        { "TOD0.WRITE.COUNTER",            0xCC, 0x00, 0x0C,  1, REG_PERM_R  },
        { "TOD0.WRITE.SEL_CFG_0",          0xCC, 0x00, 0x0D,  2, REG_PERM_RW },
        { "TOD0.WRITE.TRIGGER",            0xCC, 0x00, 0x0F,  1, REG_PERM_RW },
        { "TOD0.READ_SECONDARY.SUB_NS",    0xCC, 0x90, 0x00,  1, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.NS",        0xCC, 0x90, 0x01,  4, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.SECONDS",   0xCC, 0x90, 0x05,  6, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.COUNTER",   0xCC, 0x90, 0x0B,  1, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.SEL_CFG_0", 0xCC, 0x90, 0x0C,  1, REG_PERM_RW },
        { "TOD0.READ_SECONDARY.SEL_CFG_1", 0xCC, 0x90, 0x0D,  1, REG_PERM_RW },
        { "TOD0.READ_SECONDARY.TRIGGER",   0xCC, 0x90, 0x0E,  1, REG_PERM_RW },
        { "TOD0.TOD_CFG",                  0xCB, 0xCC, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 1 */
    {
        { "TOD1.READ_PRIMARY.SUB_NS",      0xCC, 0x50, 0x00,  1, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.NS",          0xCC, 0x50, 0x01,  4, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.SECONDS",     0xCC, 0x50, 0x05,  6, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.COUNTER",     0xCC, 0x50, 0x0B,  1, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x50, 0x0C,  1, REG_PERM_RW },
        { "TOD1.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x50, 0x0D,  1, REG_PERM_RW },
        { "TOD1.READ_PRIMARY.TRIGGER",     0xCC, 0x50, 0x0E,  1, REG_PERM_RW },
        { "TOD1.WRITE.SUB_NS",             0xCC, 0x10, 0x00,  1, REG_PERM_RW },
        { "TOD1.WRITE.NS",                 0xCC, 0x10, 0x01,  4, REG_PERM_RW },
        { "TOD1.WRITE.SECONDS",            0xCC, 0x10, 0x05,  6, REG_PERM_RW },
        { "TOD1.WRITE.COUNTER",            0xCC, 0x10, 0x0C,  1, REG_PERM_R  },
        { "TOD1.WRITE.SEL_CFG_0",          0xCC, 0x10, 0x0D,  2, REG_PERM_RW },
        { "TOD1.WRITE.TRIGGER",            0xCC, 0x10, 0x0F,  1, REG_PERM_RW },
        { "TOD1.READ_SECONDARY.SUB_NS",    0xCC, 0xA0, 0x00,  1, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.NS",        0xCC, 0xA0, 0x01,  4, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.SECONDS",   0xCC, 0xA0, 0x05,  6, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.COUNTER",   0xCC, 0xA0, 0x0B,  1, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.SEL_CFG_0", 0xCC, 0xA0, 0x0C,  1, REG_PERM_RW },
        { "TOD1.READ_SECONDARY.SEL_CFG_1", 0xCC, 0xA0, 0x0D,  1, REG_PERM_RW },
        { "TOD1.READ_SECONDARY.TRIGGER",   0xCC, 0xA0, 0x0E,  1, REG_PERM_RW },
        { "TOD1.TOD_CFG",                  0xCB, 0xCE, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 2 */
    {
        { "TOD2.READ_PRIMARY.SUB_NS",      0xCC, 0x60, 0x00,  1, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.NS",          0xCC, 0x60, 0x01,  4, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.SECONDS",     0xCC, 0x60, 0x05,  6, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.COUNTER",     0xCC, 0x60, 0x0B,  1, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x60, 0x0C,  1, REG_PERM_RW },
        { "TOD2.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x60, 0x0D,  1, REG_PERM_RW },
        { "TOD2.READ_PRIMARY.TRIGGER",     0xCC, 0x60, 0x0E,  1, REG_PERM_RW },
        { "TOD2.WRITE.SUB_NS",             0xCC, 0x20, 0x00,  1, REG_PERM_W  },
        { "TOD2.WRITE.NS",                 0xCC, 0x20, 0x01,  4, REG_PERM_W  },
        { "TOD2.WRITE.SECONDS",            0xCC, 0x20, 0x05,  6, REG_PERM_W  },
        { "TOD2.WRITE.COUNTER",            0xCC, 0x20, 0x0C,  1, REG_PERM_R  },
        { "TOD2.WRITE.SEL_CFG_0",          0xCC, 0x20, 0x0D,  2, REG_PERM_RW },
        { "TOD2.WRITE.TRIGGER",            0xCC, 0x20, 0x0F,  1, REG_PERM_RW },
        { "TOD2.READ_SECONDARY.SUB_NS",    0xCC, 0xB0, 0x00,  1, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.NS",        0xCC, 0xB0, 0x01,  4, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.SECONDS",   0xCC, 0xB0, 0x05,  6, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.COUNTER",   0xCC, 0xB0, 0x0B,  1, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.SEL_CFG_0", 0xCC, 0xB0, 0x0C,  1, REG_PERM_RW },
        { "TOD2.READ_SECONDARY.SEL_CFG_1", 0xCC, 0xB0, 0x0D,  1, REG_PERM_RW },
        { "TOD2.READ_SECONDARY.TRIGGER",   0xCC, 0xB0, 0x0E,  1, REG_PERM_RW },
        { "TOD2.TOD_CFG",                  0xCB, 0xD0, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 3 */
    {
        { "TOD3.READ_PRIMARY.SUB_NS",      0xCC, 0x80, 0x00,  1, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.NS",          0xCC, 0x80, 0x01,  4, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.SECONDS",     0xCC, 0x80, 0x05,  6, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.COUNTER",     0xCC, 0x80, 0x0B,  1, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x80, 0x0C,  1, REG_PERM_RW },
        { "TOD3.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x80, 0x0D,  1, REG_PERM_RW },
        { "TOD3.READ_PRIMARY.TRIGGER",     0xCC, 0x80, 0x0E,  1, REG_PERM_RW },
        { "TOD3.WRITE.SUB_NS",             0xCC, 0x30, 0x00,  1, REG_PERM_W  },
        { "TOD3.WRITE.NS",                 0xCC, 0x30, 0x01,  4, REG_PERM_W  },
        { "TOD3.WRITE.SECONDS",            0xCC, 0x30, 0x05,  6, REG_PERM_W  },
        { "TOD3.WRITE.COUNTER",            0xCC, 0x30, 0x0C,  1, REG_PERM_R  },
        { "TOD3.WRITE.SEL_CFG_0",          0xCC, 0x30, 0x0D,  2, REG_PERM_RW },
        { "TOD3.WRITE.TRIGGER",            0xCC, 0x30, 0x0F,  1, REG_PERM_RW },
        { "TOD3.READ_SECONDARY.SUB_NS",    0xCC, 0xC0, 0x00,  1, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.NS",        0xCC, 0xC0, 0x01,  4, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.SECONDS",   0xCC, 0xC0, 0x05,  6, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.COUNTER",   0xCC, 0xC0, 0x0B,  1, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.SEL_CFG_0", 0xCC, 0xC0, 0x0C,  1, REG_PERM_RW },
        { "TOD3.READ_SECONDARY.SEL_CFG_1", 0xCC, 0xC0, 0x0D,  1, REG_PERM_RW },
        { "TOD3.READ_SECONDARY.TRIGGER",   0xCC, 0xC0, 0x0E,  1, REG_PERM_RW },
        { "TOD3.TOD_CFG",                  0xCB, 0xD2, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    }
};

#define BF_8A34004_REG_MAP_GENERAL_SIZE (sizeof(g_8a34004_register_map_general) / sizeof(ptp_reg_desc_t))
/* When base + offset exceeds 0xFF, the carry propagates into the page selector.
 * REGPAGE adds the overflow carry so that registers with large module offsets
 * (e.g. STATUS.DPLL0_PHASE_STATUS: base=0x3C + offset=0xDC = 0x118 -> page+1, addr=0x18)
 * resolve to the correct hardware page and address. */
#define REGADDR(regdesc) ((uint8_t)(((uint16_t)(regdesc).base + (regdesc).offset) & 0xFF))
#define REGPAGE(regdesc) ((uint8_t)((regdesc).page + (((uint16_t)(regdesc).base + (regdesc).offset) >> 8)))
#define REGWIDB(regdesc) ((regdesc).width_bytes)
// 1 LSB of the internal phase detector is exactly 1 UI * 2^-16.
// For the nominal internal clock operating around 3.84GHz, 1 LSB = 3.970588 picoseconds (or 0.003970588 ns).
#define DPLL_PHASE_RESOLUTION_NS 0.003970588

/**
 * @brief Decodes the DPLL status register byte into a human-readable string.
 *
 * @param dpll_index DPLL index (unused, but kept for signature consistency).
 * @param reg_byte Raw byte from STATUS.DPLLn_STATUS register.
 * @return const char* String representation of the DPLL state.
 */
static inline
const char* dpll_state_str(int dpll_index, enum dpll_state state)
{
    (void)dpll_index;
    switch (state) {
        case DPLL_STATE_FREERUN:     return "FREERUN ";
        case DPLL_STATE_LOCKACQ:     return "LOCKACQ ";
        case DPLL_STATE_LOCKREC:     return "LOCKREC ";
        case DPLL_STATE_LOCKED:      return "LOCKED  ";
        case DPLL_STATE_HOLDOVER:    return "HOLDOVER";
        case DPLL_STATE_OPEN_LOOP:   return "OPENLOOP";
        case DPLL_STATE_DISABLED:    return "DISABLED";
        default:                     return "N/A     ";
    }
}

/**
 * @brief Decodes the DPLL reference clock status byte into a human-readable string.
 *
 * @param reg_byte Raw byte from STATUS.DPLLn_REF_STAT register.
 * @return const char* String representation of the active reference clock.
 */
static inline
const char* dpll_refclk_str(uint8_t reg_byte)
{
    switch (reg_byte) {
        case 0x00: return "CLK0           ";
        case 0x01: return "CLK1           ";
        case 0x02: return "CLK2           ";
        case 0x03: return "CLK3           ";
        case 0x04: return "CLK4           ";
        case 0x05: return "CLK5           ";
        case 0x06: return "CLK6           ";
        case 0x07: return "CLK7           ";
        case 0x08: return "CLK8           ";
        case 0x09: return "CLK9           ";
        case 0x0A: return "CLK10          ";
        case 0x0B: return "CLK11          ";
        case 0x0C: return "CLK12          ";
        case 0x0D: return "CLK13          ";
        case 0x0E: return "CLK14          ";
        case 0x0F: return "CLK15          ";
        case 0x10: return "WRPHASE_INPUT  ";
        case 0x11: return "WRFREQ_INPUT   ";
        case 0x12: return "XO_DPLL        ";
        case 0x13: return "CLK19(DPLL0)   ";
        case 0x14: return "CLK20(DPLL1)   ";
        case 0x15: return "CLK21(DPLL2)   ";
        case 0x16: return "CLK22(DPLL3)   ";
        case 0x17: return "CLK23(DPLL4)   ";
        case 0x18: return "CLK24(DPLL5)   ";
        case 0x19: return "CLK25(DPLL6)   ";
        case 0x1A: return "CLK26(DPLL7)   ";
        case 0x1B: return "CLK27(SYSDPLL) ";
        case 0x1C: return "CLK28          ";
        case 0x1F: return "NOREF          ";
        default:   return "N/A            ";
    }
}

/**
 * @brief Decodes the operation mode from the DPLL mode register.
 *
 * @param pll_mode Extracted PLL_MODE field [5:3] from DPLL_n.DPLL_MODE (0=PLL … 6=OFF).
 * @return const char* String representation of the operation mode.
 */
static inline
const char* dpll_mode_op_str(enum pll_mode pll_mode)
{
    switch (pll_mode) {
        case PLL_MODE_PLL:
            return "PLL";
        case PLL_MODE_WRITE_PHASE:
            return "PHASE";
        case PLL_MODE_WRITE_FREQUENCY:
            return "FREQ";
        case PLL_MODE_GPIO_INC_DEC:
            return "GPIO";
        case PLL_MODE_SYNTHESIS:
            return "SYNTH";
        case PLL_MODE_PHASE_MEASUREMENT:
            return "PMEAS";
        case PLL_MODE_DISABLED:
            return "OFF";
        default:
            return "N/A";
    }
}

/**
 * @brief Decodes the state machine mode from the DPLL mode register.
 *
 * @param sm_mode  Extracted STATE_MODE field [2:0] from DPLL_n.DPLL_MODE (0=AUTO … 3=FORCE_HLDOVER).
 * @return const char* String representation of the state machine mode.
 */
static inline
const char* dpll_mode_sm_str(uint8_t sm_mode)
{
    switch (sm_mode) {
        case PLL_STATE_AUTO:
            return "AUTO         ";
        case PLL_STATE_FORCE_LOCK:
            return "FORCE_LOCK   ";
        case PLL_STATE_FORCE_FRUN:
            return "FORCE_FRUN   ";
        case PLL_STATE_FORCE_HOLDOVER:
            return "FORCE_HLDOVER";
        default:
            return "N/A          ";
    }
}

/* ─── 32-bit signed phase serialization helpers ──────────────────────────
 * The Renesas 8A34004 DPLL PHASE register is 4 bytes wide.
 * The BSP write in little-endian byte order (LSB at offset 0).
 *
 *   dpll_phase_le32_pack   / dpll_phase_le32_unpack   ── pack/unpack LE
 *   dpll_phase_be32_pack   / dpll_phase_be32_unpack   ── pack/unpack BE
 */

/** @brief Pack int32_t → 4-byte buffer, little-endian (LSB first). */
static inline
void dpll_phase_le32_pack(int32_t phase, uint8_t buf[4])
{
    buf[0] = (uint8_t)( phase        & 0xFF);
    buf[1] = (uint8_t)((phase >>  8) & 0xFF);
    buf[2] = (uint8_t)((phase >> 16) & 0xFF);
    buf[3] = (uint8_t)((phase >> 24) & 0xFF);
}

/**
 * @brief Unpacks a 4-byte little-endian buffer into a signed 32-bit integer.
 *
 * @param reg_bytes Pointer to a 4-byte little-endian array.
 * @return int32_t The decoded 32-bit signed integer.
 */
static inline
int32_t dpll_phase_le32_unpack(const uint8_t reg_bytes[4])
{
    uint32_t unsigned_32bit = 0;

    unsigned_32bit = ((uint32_t)reg_bytes[3] << 24) |
                     ((uint32_t)reg_bytes[2] << 16) |
                     ((uint32_t)reg_bytes[1] <<  8) |
                     ((uint32_t)reg_bytes[0]);

    return (int32_t)unsigned_32bit;
}

/** @brief Pack int32_t → 4-byte buffer, big-endian (MSB first). */
static inline
void dpll_phase_be32_pack(int32_t phase, uint8_t buf[4])
{
    buf[0] = (uint8_t)((phase >> 24) & 0xFF);
    buf[1] = (uint8_t)((phase >> 16) & 0xFF);
    buf[2] = (uint8_t)((phase >>  8) & 0xFF);
    buf[3] = (uint8_t)( phase        & 0xFF);
}

/** @brief Unpack 4-byte big-endian buffer → int32_t. */
static inline
int32_t dpll_phase_be32_unpack(const uint8_t reg_bytes[4])
{
    uint32_t unsigned_32bit = 0;

    unsigned_32bit = ((uint32_t)reg_bytes[0] << 24) |
                     ((uint32_t)reg_bytes[1] << 16) |
                     ((uint32_t)reg_bytes[2] <<  8) |
                     ((uint32_t)reg_bytes[3]);

    return (int32_t)unsigned_32bit;
}

/**
 * @brief Decodes the raw 5-byte data stream of STATUS.DPLLn_PHASE_STATUS into nanoseconds.
 *
 * @param dpll_index Target DPLL index.
 * @param reg_bytes Pointer to a 5-byte buffer containing raw data read
 *                  sequentially from the corresponding phase status registers.
 * @return double The decoded residual phase offset error value in nanoseconds (ns).
 */
static inline
double dpll_phase_status(int dpll_index, uint8_t *reg_bytes)
{
    int64_t raw_val_40bit = 0;
    (void)dpll_index;

    // 1. Construct the 40-bit integer from the 5-byte stream using Little-Endian order
    // reg_040h_buf[0] is D0 (LSB), reg_040h_buf[4] is D4 (MSB)
    raw_val_40bit = ((int64_t)reg_bytes[4] << 32) |
                    ((int64_t)reg_bytes[3] << 24) |
                    ((int64_t)reg_bytes[2] << 16) |
                    ((int64_t)reg_bytes[1] << 8)  |
                    ((int64_t)reg_bytes[0]);

    // 2. Perform manual 40-bit signed Two's Complement Sign-Extension to 64-bit int64_t.
    // Check if Bit 39 (the 40th bit, mask 0x8000000000) is set to 1 (indicating a negative value).
    if (raw_val_40bit & 0x0000008000000000LL) {
        // Sign-extend high 24 bits to 1s to properly register a negative integer in C
        raw_val_40bit |= 0xFFFFFF0000000000LL;
    } else {
        // Enforce pure 40-bit boundary limits for clear positive integers
        raw_val_40bit &= 0x000000FFFFFFFFFFLL;
    }

    // 3. Convert the signed scalar count into physical wall-time nanoseconds
    double phase_ns = (double)raw_val_40bit * DPLL_PHASE_RESOLUTION_NS;

    return phase_ns;
}

/**
 * @brief Parse the raw 8-byte data returned from page C1 of the 8A34004 chip,
 *        and calculate the actual physical layer observation clock frequency.
 *
 * @param input_index Target input channel index.
 * @param reg_bytes Pointer to an 8-byte little-endian array returned by the chip.
 * @return double The computed real frequency value in Hz.
 */
static inline
double decode_obsclk_freq(int input_index, uint8_t *reg_bytes)
{
    uint64_t m_val = 0;
    uint16_t n_val = 0;
    (void)input_index;

    m_val = ((uint64_t)reg_bytes[5] << 40) |
            ((uint64_t)reg_bytes[4] << 32) |
            ((uint64_t)reg_bytes[3] << 24) |
            ((uint64_t)reg_bytes[2] << 16) |
            ((uint64_t)reg_bytes[1] << 8)  |
            ((uint64_t)reg_bytes[0]);

    n_val = ((uint16_t)reg_bytes[7] << 8) | 
            ((uint16_t)reg_bytes[6]);

    if (n_val == 0) {
        n_val = 1;
    }
    //fprintf(stdout, "m_val = %lu n_val=%u\n", m_val, n_val);
    double freq_hz = (double)m_val / (double)n_val;

    return freq_hz;
}

/**
 * @brief Looks up a register in the general register map.
 *
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register_general(const char *name, ptp_reg_desc_t *out_desc)
{
    if (!name || !out_desc) return -1;

    for (size_t i = 0; i < BF_8A34004_REG_MAP_GENERAL_SIZE; i++) {
        const ptp_reg_desc_t *reg = &g_8a34004_register_map_general[i];
        if (reg->name == NULL)
            break;
        if (strcmp(reg->name, name) == 0) {
            *out_desc = *reg;
            return 0; // Target found
        }
    }
    return -1;
}

/**
 * @brief Looks up a register in a specific DPLL's register map.
 *
 * @param dpll_index DPLL index (0 to 7).
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register_dpll(int dpll_index, const char *name, ptp_reg_desc_t *out_desc)
{
    if (dpll_index > 7 || !name || !out_desc) return -1;

    for (size_t i = 0; i < MAX_ENTRIES_PER_DPLL; i++) {
        const ptp_reg_desc_t *reg = &g_8a34004_register_map_dpll[dpll_index][i];
        if (reg->name == NULL)
            break;
        if (strcmp(reg->name, name) == 0) {
            *out_desc = *reg;
            return 0; // Target found
        }
    }
    return -1;
}

/**
 * @brief Looks up a register in a specific TOD's register map.
 *
 * @param tod_index TOD index (0 to 3).
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register_tod(int tod_index, const char *name, ptp_reg_desc_t *out_desc)
{
    if (tod_index > 3 || !name || !out_desc) return -1;

    for (size_t i = 0; i < MAX_ENTRIES_PER_TOD; i++) {
        const ptp_reg_desc_t *reg = &g_8a34004_register_map_tod[tod_index][i];
        if (reg->name == NULL)
            break;
        if (strcmp(reg->name, name) == 0) {
            *out_desc = *reg;
            return 0; // Target found
        }
    }
    return -1;
}

/**
 * @brief Global lookup for a register across all maps (General, DPLL, TOD).
 *
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register(const char *name, ptp_reg_desc_t *out_desc)
{
    if (!name || !out_desc) return -1;

    if (bf_ptp_lookup_register_general(name, out_desc) == 0) return 0;

    for (int dpll_index = 0; dpll_index < AFN_8A34004_MAX_DPLL; dpll_index++) {
        if (bf_ptp_lookup_register_dpll(dpll_index, name, out_desc) == 0) return 0;
    }

    for (int tod_index = 0; tod_index < AFN_8A34004_MAX_TOD; tod_index++) {
        if (bf_ptp_lookup_register_tod(tod_index, name, out_desc) == 0) return 0;
    }

    return -1;
}

#endif // AFN_8A34004_REGS_H

