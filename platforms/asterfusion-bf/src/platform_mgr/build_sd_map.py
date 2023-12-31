################################################################################
 # Copyright (c) 2015-2020 Barefoot Networks, Inc.
 # SPDX-License-Identifier: BSD-3-Clause
 #
 # $Id: $
 #
 ###############################################################################

#!/usr/bin/python

"""
 CSV compiler for board map files (.xls) generated by HW group.

 This python script takes a spreadsheet in CSV format (with certain
 known column headers) and outputs a C-language structure initialized
 with the values from the .csv file.

 The (current) structure output follows a typedef defined in pltfm_bd_map.h

    typedef struct bd_map_ent_t {
      uint32_t connector;
      uint32_t channel;
      uint32_t mac_block;
      uint32_t mac_ch;
      uint32_t tx_lane;
      uint32_t tx_pn_swap;
      uint32_t rx_lane;
      uint32_t rx_pn_swap;
      uint32_t tx_eq_pre[MAX_QSFP_TYPES];
      uint32_t tx_eq_post[MAX_QSFP_TYPES];
      uint32_t tx_eq_attn[MAX_QSFP_TYPES];
      uint32_t has_repeater;
      uint32_t eg_rptr_nbr;
      uint32_t eg_rptr_i2c_addr;
      uint32_t eg_rptr_port;
      uint32_t eg_rptr_eq_bst1[MAX_QSFP_TYPES];
      uint32_t eg_rptr_eq_bst2[MAX_QSFP_TYPES];
      uint32_t eg_rptr_eq_bw[MAX_QSFP_TYPES];
      uint32_t eg_rptr_eq_bst_bypass[MAX_QSFP_TYPES];
      uint32_t eg_rptr_vod[MAX_QSFP_TYPES];
      uint32_t eg_rptr_regF[MAX_QSFP_TYPES];
      uint32_t eg_rptr_high_gain[MAX_QSFP_TYPES];
      uint32_t ig_rptr_nbr;
      uint32_t ig_rptr_i2c_addr;
      uint32_t ig_rptr_port;
      uint32_t ig_rptr_eq_bst1[MAX_QSFP_TYPES];
      uint32_t ig_rptr_eq_bst2[MAX_QSFP_TYPES];
      uint32_t ig_rptr_eq_bw[MAX_QSFP_TYPES];
      uint32_t ig_rptr_eq_bst_bypass[MAX_QSFP_TYPES];
      uint32_t ig_rptr_vod[MAX_QSFP_TYPES];
      uint32_t ig_rptr_regF[MAX_QSFP_TYPES];
      uint32_t ig_rptr_high_gain[MAX_QSFP_TYPES];
    
      /* Re-timer params for Mavericks P0C board */
      uint32_t has_rtmr;
      uint32_t rtmr_nbr;
      uint32_t rtmr_i2c_addr;
      uint32_t rtmr_host_side_lane;
      uint32_t rtmr_host_side_tx_pn_swap;
      uint32_t rtmr_host_side_rx_pn_swap;
      uint32_t rtmr_line_side_lane;
      uint32_t rtmr_line_side_tx_pn_swap;
      uint32_t rtmr_line_side_rx_pn_swap;
      uint32_t rtmr_tx_main_opt = 0;
      uint32_t rtmr_tx_post_1_opt;
      uint32_t rtmr_tx_precursor_opt;

      /* Internal port for Montara P0C board */
      uint32_t is_internal_port;
    } bd_map_ent_t;

 To run:

  python build_sd_map.py <csv-file-path> <structure-name-suffix> 

 You can redirect the output directly to the desired file.
 For example, the Mavericks P0B board map was generated by the following,

  python build_sd_map.py ./Mavericks_P0B_Port_Mapping-2.csv mavericks_P0B >pltfm_bd_map_mavericks_P0B.h

"""

import csv
import sys

import os.path
import hashlib
import copy
import re

from operator import mul
from types import StringTypes

qsfp_type_names = ["_Cu_0.5m","_Cu_1m","_Cu_2m","_Cu_3m","_Cu_Loop_0dB","_Opt"]

def print_member(member_name, member, flag):      
    print "    ." + member_name + " = {", 
    for qsfp_id in range(len(qsfp_type_names)):
	if flag == 1:
    	    print str(member[qsfp_id]) + ",",
	else:
    	    print "0" + ",",
    print "},"  

#
# Build a dictionary of ports
#
def csv_parse_bd_map( path, map_name ):

    cur_fp = 0
    cur_quad = 0
    qsfp_ch = 0
    quad = 0
    internal_port = 0
    rptr = 0
    pre = [0, 0, 0, 0, 0, 0]
    post = [0, 0, 0, 0, 0, 0]
    attn = [0, 0, 0, 0, 0, 0]
    eg_rptr_eq_bst1 = [0, 0, 0, 0, 0, 0]
    eg_rptr_eq_bst2 = [0, 0, 0, 0, 0, 0]
    eg_rptr_eq_bw = [0, 0, 0, 0, 0, 0]
    eg_rptr_eq_bst_bypass = [0, 0, 0, 0, 0, 0]
    eg_rptr_vod = [0, 0, 0, 0, 0, 0]
    eg_rptr_regF = [0, 0, 0, 0, 0, 0]
    eg_rptr_high_gain = [0, 0, 0, 0, 0, 0]
    ig_rptr_eq_bst1 = [0, 0, 0, 0, 0, 0]
    ig_rptr_eq_bst2 = [0, 0, 0, 0, 0, 0]
    ig_rptr_eq_bw = [0, 0, 0, 0, 0, 0]
    ig_rptr_eq_bst_bypass = [0, 0, 0, 0, 0, 0]
    ig_rptr_vod = [0, 0, 0, 0, 0, 0]
    ig_rptr_regF = [0, 0, 0, 0, 0, 0]
    ig_rptr_high_gain = [0, 0, 0, 0, 0, 0]

    with open(path, "rb") as csv_file:
        csv_reader = csv.DictReader(csv_file)
        row_num = 0

        print "bd_map_ent_t " + map_name + "_bd_map[] = {"

        for row in csv_reader:
            if row["QSFP_Port"] != "":
                cur_fp = int(row["QSFP_Port"])
            qsfp_ch    = int(row["QSFP_Lane"])
            if row["Quad"] != "":
                quad   = int(row["Quad"])
            tx_lane    = int(row["TX_Lane"])
            if re.match("Y\s*",row["TX_PN_Swap"]):
                tx_pn_swap = 1
            else:
                tx_pn_swap = 0
            rx_lane    = int(row["RX_Lane"])
            if re.match("Y\s*",row["RX_PN_Swap"]):
                rx_pn_swap = 1
            else:
                rx_pn_swap = 0
    	    for qsfp_id in range(len(qsfp_type_names)):
            	pre[qsfp_id] = int(row["TX_Pre" + qsfp_type_names[qsfp_id]])            
            	post[qsfp_id] = int(row["TX_Post" + qsfp_type_names[qsfp_id]])            
            	attn[qsfp_id] = int(row["TX_Attn" + qsfp_type_names[qsfp_id]])
            #if re.match("No\s*",row["Internal_port"]):
            #    internal_port = 0
            #else:
            #    internal_port = 1
            if re.match("No\s*",row["Repeater"]):
                rptr = 0
            else:
                rptr = 1
            if rptr == 1:
		if row["Egress_Repeater"] != "":
                    eg_rptr_nbr = int(row["Egress_Repeater"])
  		if row["Egress_Repeater_I2C_Addr"] != "":
                    eg_rptr_i2c_addr = int(row["Egress_Repeater_I2C_Addr"], 16)
                eg_rptr_port = int(row["Egress_Repeater_Lane"])
		for qsfp_id in range(len(qsfp_type_names)):
                    eg_rptr_eq_bst1[qsfp_id] = int(row["Egress_EQ_BST1" + qsfp_type_names[qsfp_id]])
                    eg_rptr_eq_bst2[qsfp_id] = int(row["Egress_EQ_BST2" + qsfp_type_names[qsfp_id]])
                    eg_rptr_eq_bw[qsfp_id] = int(row["Egress_EQ_BW" + qsfp_type_names[qsfp_id]])
                    eg_rptr_eq_bst_bypass[qsfp_id] = int(row["Egress_EQ_BST_BYPASS" + qsfp_type_names[qsfp_id]])
                    eg_rptr_vod[qsfp_id] = int(row["Egress_VOD" + qsfp_type_names[qsfp_id]])
                    eg_rptr_regF[qsfp_id] = int(row["Egress_RegF" + qsfp_type_names[qsfp_id]], 16)
                    eg_rptr_high_gain[qsfp_id] = int(row["Egress_EQ_HG" + qsfp_type_names[qsfp_id]])

		if row["Ingress_Repeater"] != "":
                    ig_rptr_nbr = int(row["Ingress_Repeater"])
		if row["Ingress_Repeater_I2C_Addr"] != "":
                    ig_rptr_i2c_addr = int(row["Ingress_Repeater_I2C_Addr"], 16)
                ig_rptr_port = int(row["Ingress_Repeater_Lane"])
		for qsfp_id in range(len(qsfp_type_names)):
                    ig_rptr_eq_bst1[qsfp_id] = int(row["Ingress_EQ_BST1" + qsfp_type_names[qsfp_id]])
                    ig_rptr_eq_bst2[qsfp_id] = int(row["Ingress_EQ_BST2" + qsfp_type_names[qsfp_id]])
                    ig_rptr_eq_bw[qsfp_id] = int(row["Ingress_EQ_BW" + qsfp_type_names[qsfp_id]])
                    ig_rptr_eq_bst_bypass[qsfp_id] = int(row["Ingress_EQ_BST_BYPASS" + qsfp_type_names[qsfp_id]])
                    ig_rptr_vod[qsfp_id] = int(row["Ingress_VOD" + qsfp_type_names[qsfp_id]])
                    ig_rptr_regF[qsfp_id] = int(row["Ingress_RegF" + qsfp_type_names[qsfp_id]], 16)
                    ig_rptr_high_gain[qsfp_id] = int(row["Ingress_EQ_HG" + qsfp_type_names[qsfp_id]])

            try:
                if re.match("No\s*",row["Retimer"]):
                    rtmr = 0
                else:
                    rtmr = 1
            except (KeyError):
                rtmr = 0
            if rtmr == 1:
                if row["Retimer_Number"] != "":
                    rtmr_nbr = int(row["Retimer_Number"])
                if row["Retimer_I2C_Addr"] != "":
                    rtmr_i2c_addr = int(row["Retimer_I2C_Addr"], 16)
                rtmr_host_side_lane = int(row["Retimer_Host_Side_Lane"])
                if re.match("Y\s*", row["Retimer_Host_Side_TX_PN_Swap"]):
                    rtmr_host_side_tx_pn_swap = 1
                else:
                    rtmr_host_side_tx_pn_swap = 0
                if re.match("Y\s*", row["Retimer_Host_Side_RX_PN_Swap"]):
                    rtmr_host_side_rx_pn_swap = 1
                else:
                    rtmr_host_side_rx_pn_swap = 0
                rtmr_line_side_lane = int(row["Retimer_Line_Side_Lane"])
                if re.match("Y\s*", row["Retimer_Line_Side_TX_PN_Swap"]):
                    rtmr_line_side_tx_pn_swap = 1
                else:
                    rtmr_line_side_tx_pn_swap = 0
                if re.match("Y\s*", row["Retimer_Line_Side_RX_PN_Swap"]):
                    rtmr_line_side_rx_pn_swap = 1
                else:
                    rtmr_line_side_rx_pn_swap = 0
                if row["Retimer_TX_Main_Opt"] != "":
                    rtmr_tx_main_opt = int(row["Retimer_TX_Main_Opt"],16)
                if row["Retimer_TX_Post_1_Opt"] != "":
                    rtmr_tx_post_1_opt = int(row["Retimer_TX_Post_1_Opt"],16)
                if row["Retimer_TX_Precursor_Opt"] != "":
                    rtmr_tx_precursor_opt = int(row["Retimer_TX_Precursor_Opt"],16)
            print "//"
            print "// QSFP Port: " + str(cur_fp) + " : Channel: " + str(qsfp_ch) + " : Types :",
	    for i in qsfp_type_names:
	       	print "QSFP" + i + ",",
	    print ""
            print "//"
            print "  { "
            print "    .connector = " + str(cur_fp) + ","
            print "    .channel = " + str(qsfp_ch) + ","
            print "    .mac_block = " + str(quad) + ","
            print "    .mac_ch = " + str(qsfp_ch) + ","
            print "    .tx_lane = " + str(tx_lane) + ","
            print "    .tx_pn_swap = " + str(tx_pn_swap) + ","
            print "    .rx_lane = " + str(rx_lane) + ","
            print "    .rx_pn_swap = " + str(rx_pn_swap) + ","

	    print_member("tx_eq_pre", pre, 1)
	    print_member("tx_eq_post", post, 1)
	    print_member("tx_eq_attn", attn, 1)
		
            print "    .is_internal_port= " + str(internal_port) + ","
            print "    .has_repeater= " + str(rptr) + ","
            if rptr == 1:
                # egress repeater settings
                print "    .eg_rptr_nbr= " + str(eg_rptr_nbr) + ","
                print "    .eg_rptr_i2c_addr= " + str(eg_rptr_i2c_addr) + ","
                print "    .eg_rptr_port= " + str(eg_rptr_port) + ","
		print_member("eg_rptr_eq_bst1", eg_rptr_eq_bst1, 1)
		print_member("eg_rptr_eq_bst2", eg_rptr_eq_bst2, 1)
		print_member("eg_rptr_eq_bw", eg_rptr_eq_bw, 1)
		print_member("eg_rptr_eq_bst_bypass", eg_rptr_eq_bst_bypass, 1)
		print_member("eg_rptr_vod", eg_rptr_vod, 1)
		print_member("eg_rptr_regF", eg_rptr_regF, 1)
		print_member("eg_rptr_high_gain", eg_rptr_high_gain, 1)

                # ingress repeater settings
                print "    .ig_rptr_nbr= " + str(ig_rptr_nbr) + ","
                print "    .ig_rptr_i2c_addr= " + str(ig_rptr_i2c_addr) + ","
                print "    .ig_rptr_port= " + str(ig_rptr_port) + ","
		print_member("ig_rptr_eq_bst1", ig_rptr_eq_bst1, 1)
		print_member("ig_rptr_eq_bst2", ig_rptr_eq_bst2, 1)
		print_member("ig_rptr_eq_bw", ig_rptr_eq_bw, 1)
		print_member("ig_rptr_eq_bst_bypass", ig_rptr_eq_bst_bypass, 1)
		print_member("ig_rptr_vod", ig_rptr_vod, 1)
		print_member("ig_rptr_regF", ig_rptr_regF, 1)
		print_member("ig_rptr_high_gain", ig_rptr_high_gain, 1)
            else:
                # egress repeater settings
                print "    .eg_rptr_nbr= 0,"
                print "    .eg_rptr_i2c_addr= 0,"
                print "    .eg_rptr_port= 0,"
		print_member("eg_rptr_eq_bst1", eg_rptr_eq_bst1, 0)
		print_member("eg_rptr_eq_bst2", eg_rptr_eq_bst2, 0)
		print_member("eg_rptr_eq_bw", eg_rptr_eq_bw, 0)
		print_member("eg_rptr_eq_bst_bypass", eg_rptr_eq_bst_bypass, 0)
		print_member("eg_rptr_vod", eg_rptr_vod, 0)
		print_member("eg_rptr_regF", eg_rptr_regF, 0)
		print_member("eg_rptr_high_gain", eg_rptr_high_gain, 0)

                # ingress repeater settings
                print "    .ig_rptr_nbr= 0,"
                print "    .ig_rptr_i2c_addr= 0,"
                print "    .ig_rptr_port= 0,"
		print_member("ig_rptr_eq_bst1", ig_rptr_eq_bst1, 0)
		print_member("ig_rptr_eq_bst2", ig_rptr_eq_bst2, 0)
		print_member("ig_rptr_eq_bw", ig_rptr_eq_bw, 0)
		print_member("ig_rptr_eq_bst_bypass", ig_rptr_eq_bst_bypass, 0)
		print_member("ig_rptr_vod", ig_rptr_vod, 0)
		print_member("ig_rptr_regF", ig_rptr_regF, 0)
		print_member("ig_rptr_high_gain", ig_rptr_high_gain, 0)

            print "    .has_rtmr= " + str(rtmr) + ","
            if rtmr == 1:
                print "    .rtmr_nbr= " + str(rtmr_nbr) + ","
                print "    .rtmr_i2c_addr= " + str(rtmr_i2c_addr) + ","
                print "    .rtmr_host_side_lane= " + str(rtmr_host_side_lane) + ","
                print "    .rtmr_host_side_tx_pn_swap= " + str(rtmr_host_side_tx_pn_swap) + ","
                print "    .rtmr_host_side_rx_pn_swap= " + str(rtmr_host_side_rx_pn_swap) + ","
                print "    .rtmr_line_side_lane= " + str(rtmr_line_side_lane) + ","
                print "    .rtmr_line_side_tx_pn_swap= " + str(rtmr_line_side_tx_pn_swap) + ","
                print "    .rtmr_line_side_rx_pn_swap= " + str(rtmr_line_side_rx_pn_swap) + ","
                print "    .rtmr_tx_main_opt= " + str(rtmr_tx_main_opt) + ","
                print "    .rtmr_tx_post_1_opt= " + str(rtmr_tx_post_1_opt) + ","
                print "    .rtmr_tx_precursor_opt= " + str(rtmr_tx_precursor_opt) + ","
            else:
                print "    .rtmr_nbr= 0,"
                print "    .rtmr_i2c_addr= 0,"
                print "    .rtmr_host_side_lane= 0,"
                print "    .rtmr_host_side_tx_pn_swap= 0,"
                print "    .rtmr_host_side_rx_pn_swap= 0,"
                print "    .rtmr_line_side_lane= 0,"
                print "    .rtmr_line_side_tx_pn_swap= 0,"
                print "    .rtmr_line_side_rx_pn_swap= 0,"
                print "    .rtmr_tx_main_opt= 0,"
                print "    .rtmr_tx_post_1_opt= 0,"
                print "    .rtmr_tx_precursor_opt= 0,"
            print "  },"
        print "};"

    csv_file.close()


# Main
if __name__ == "__main__":

    csv_parse_bd_map( str(sys.argv[1]), str(sys.argv[2]) )

