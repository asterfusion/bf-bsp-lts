"""
################################################################################
 # Copyright (c) 2015-2020 Barefoot Networks, Inc.
 # SPDX-License-Identifier: BSD-3-Clause
 #
 # $Id: $
 #
 ###############################################################################
"""

#!/usr/bin/python
# -*- coding: utf-8-sig -*-

import json
import csv
import sys
import os.path


"""
 Note: Convert *.xlsx into *.csv file before running this file.

 This python script takes a spreadsheet in CSV format (with certain
 known column headers) and outputs board-mapping in Json format.
 Output filename is baord_lane_map_<board_id>.json

 To run:
  python build_sd_map.py <csv-file-name.csv> <board-id in decimal> <1/0 to enable debug flag>"
  e.g., python build_sd_map.py Newport_p0a.csv 4033 1

"""

fdict = ['QSFPDD_Port', 'QSFPDD_Lane', 'Tile_Octal', 'Tile_TX_Lane', 'TX_PN_Swap', 'Tile_RX_Lane', 'RX_PN_Swap', 'TX_Main_Cu_0.5m_PAM4', \
         'TX_Pre1_Cu_0.5m_PAM4', 'TX_Pre2_Cu_0.5m_PAM4', 'TX_Post1_Cu_0.5m_PAM4', 'TX_Post2_Cu_0.5m_PAM4', 'TX_Main_Cu_1m_PAM4', \
         'TX_Pre1_Cu_1m_PAM4', 'TX_Pre2_Cu_1m_PAM4', 'TX_Post1_Cu_1m_PAM4', 'TX_Post2_Cu_1m_PAM4', 'TX_Main_Cu_1.5m_PAM4', \
         'TX_Pre1_Cu_1.5m_PAM4', 'TX_Pre2_Cu_1.5m_PAM4', 'TX_Post1_Cu_1.5m_PAM4', 'TX_Post2_Cu_1.5m_PAM4', 'TX_Main_Cu_2m_PAM4', \
         'TX_Pre1_Cu_2m_PAM4', 'TX_Pre2_Cu_2m_PAM4', 'TX_Post1_Cu_2m_PAM4', 'TX_Post2_Cu_2m_PAM4', 'TX_Main_Cu_2.5m_PAM4', \
         'TX_Pre1_Cu_2.5m_PAM4', 'TX_Pre2_Cu_2.5m_PAM4', 'TX_Post1_Cu_2.5m_PAM4', 'TX_Post2_Cu_2.5m_PAM4', 'TX_Main_Cu_Loop_0dB_PAM4', \
         'TX_Pre1_Cu_Loop_0dB_PAM4', 'TX_Pre2_Cu_Loop_0dB_PAM4', 'TX_Post1_Cu_Loop_0dB_PAM4', 'TX_Post2_Cu_Loop_0dB_PAM4', 'TX_Main_Opt_PAM4', \
         'TX_Pre1_Opt_PAM4', 'TX_Pre2_Opt_PAM4', 'TX_Post1_Opt_PAM4', 'TX_Post2_Opt_PAM4', 'TX_Main_Cu_0.5m_NRZ', 'TX_Pre1_Cu_0.5m_NRZ', 'TX_Pre2_Cu_0.5m_NRZ', \
         'TX_Post1_Cu_0.5m_NRZ', 'TX_Post2_Cu_0.5m_NRZ', 'TX_Main_Cu_1m_NRZ', 'TX_Pre1_Cu_1m_NRZ', 'TX_Pre2_Cu_1m_NRZ', 'TX_Post1_Cu_1m_NRZ', 'TX_Post2_Cu_1m_NRZ', \
         'TX_Main_Cu_1.5m_NRZ', 'TX_Pre1_Cu_1.5m_NRZ', 'TX_Pre2_Cu_1.5m_NRZ', 'TX_Post1_Cu_1.5m_NRZ', 'TX_Post2_Cu_1.5m_NRZ', 'TX_Main_Cu_2m_NRZ', 'TX_Pre1_Cu_2m_NRZ', \
         'TX_Pre2_Cu_2m_NRZ', 'TX_Post1_Cu_2m_NRZ', 'TX_Post2_Cu_2m_NRZ', 'TX_Main_Cu_2.5m_NRZ', 'TX_Pre1_Cu_2.5m_NRZ', 'TX_Pre2_Cu_2.5m_NRZ', 'TX_Post1_Cu_2.5m_NRZ', \
         'TX_Post2_Cu_2.5m_NRZ', 'TX_Main_Cu_Loop_0dB_NRZ', 'TX_Pre1_Cu_Loop_0dB_NRZ', 'TX_Pre2_Cu_Loop_0dB_NRZ', 'TX_Post1_Cu_Loop_0dB_NRZ', 'TX_Post2_Cu_Loop_0dB_NRZ',\
         'TX_Main_Opt_NRZ', 'TX_Pre1_Opt_NRZ', 'TX_Pre2_Opt_NRZ', 'TX_Post1_Opt_NRZ', 'TX_Post2_Opt_NRZ']

def bf_convert_cvs_to_json(file, board_id, enable_debug_flag, json_file):
    csv_rows = []
    print "Parsing " + str(file)
    with open(file) as csvfile:
        reader = csv.DictReader(csvfile)
        title = reader.fieldnames
        #print title
        for row in reader:
            #print row
            if row['QSFPDD_Port'] == '':
              row['QSFPDD_Port'] = last_qsfpdd_port
            else:
              last_qsfpdd_port = row['QSFPDD_Port']
            if row['JBAY Package Net Name'] == '':
              row['JBAY Package Net Name'] = last_mac_block
            else:
              row['JBAY Package Net Name'] = row['JBAY Package Net Name'].strip("ETH").strip("_\*")
              last_mac_block = row['JBAY Package Net Name']

            if row['Internal_port'] == '':
              row['Internal_port'] = last_internal_port
            else:
              last_internal_port = row['Internal_port']

            csv_rows.extend([{title[i]:row[title[i]] for i in range(len(title))}])
            #Use dictionary to filter the columns to generate the json
	    #csv_rows.extend([{fdict[i]:row[fdict[i]] for i in range(len(fdict))}])
        bf_write_json(csv_rows, board_id, enable_debug_flag, json_file, format)

#Convert csv data into json and write it
def bf_write_json(data, board_id, enable_debug, json_file, format):
    board_lane = "board_lane_map_entry"
    bd = "board_id"
    enlog = "enable_debug_log"
    bdata = '"{}": '.format(board_lane)
    bdid  = '"{}": '.format(bd) + '"{}"'.format(str(board_id))
    print bdid
    elog  = '"{}": '.format(enlog) + '"{}"'.format(str(enable_debug))
    print bdata, bdid, elog
    with open(json_file, "w") as f:
        f.write("{\n")
        f.write(bdid)
        f.write(",\n")
        f.write(elog)
        f.write(",\n")
        f.write(bdata)
        f.write("\n")
	f.write(json.dumps(data, sort_keys=True, indent=4, encoding="utf-8",ensure_ascii=True))
        f.write("\n}")

def bf_print_help():
    print "Usage: python build_sd_map.py <csv-file-name.csv> <board-id in decimal> <1/0 to enable debug flag>"

# Main
if __name__ == "__main__":
    if len(sys.argv) < 4:
	bf_print_help()
    else:
        jfname = 'board_lane_map_' + str(sys.argv[2]) + ".json" 
        print jfname
	# Args: cvsfile_name, board-id, 1/0 to enable debug log
	bf_convert_cvs_to_json( str(sys.argv[1]),  str(sys.argv[2]), str(sys.argv[3]), jfname) 
	print "Generated successfuly " + str(jfname)


