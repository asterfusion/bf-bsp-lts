#!/usr/bin/env python
from __future__ import print_function

import codecs
import math
import os
import re
import string
import subprocess
import sys

try:
    import readline  # type: ignore
except ImportError:
    pass

from collections import OrderedDict
from distutils import spawn

try:
    from types import TracebackType
    from typing import Callable, Type
except ImportError:
    pass

# Platforms information
SUPPORTED_PLATFORMS = (
    X308PT,
    X312PT,
    X532PT,
    X564PT,
) = (
    "X308P-T",
    "X312P-T",
    "X532P-T",
    "X564P-T",
)
SELF_DEVELOPED_PLATFORMS = (
    X308PT,
    X532PT,
    X564PT,
)
S01_PLATFORMS = (X312PT,)
# Colors
RED = lambda s: "\033[91m{}\033[0m".format(s)  # type: Callable[[str], str]
GREEN = lambda s: "\033[92m{}\033[0m".format(s)  # type: Callable[[str], str]
YELLOW = lambda s: "\033[93m{}\033[0m".format(s)  # type: Callable[[str], str]
BLUE = lambda s: "\033[94m{}\033[0m".format(s)  # type: Callable[[str], str]
ERROR = RED("Error:")
SUCCESS = GREEN("Success:")
WARNING = YELLOW("Warning:")
NOTICE = BLUE("Notice:")
# Input function compatibility
PROMPT_INPUT = (
    input if sys.version_info.major == 3 else raw_input
)  # type: Callable[[object], str]
EEPROM_HDR = (
    "TLV Name            Code Len Value",
    "------------------- ---- --- -----",
)
EEPROM_DATA = (
    "Product Name        {:<4} {:<3} {}",
    "Part Number         {:<4} {:<3} {}",
    "Serial Number       {:<4} {:<3} {}",
    "Base MAC Address    {:<4} {:<3} {}",
    "Manufacture Date    {:<4} {:<3} {}",
    "Device Version      {:<4} {:<3} {}",
    "Label Revision      {:<4} {:<3} {}",
    "Platform Name       {:<4} {:<3} {}",
    "Onie Version        {:<4} {:<3} {}",
    "MAC Addresses       {:<4} {:<3} {}",
    "Manufacturer        {:<4} {:<3} {}",
    "Country Code        {:<4} {:<3} {}",
    "Vendor Name         {:<4} {:<3} {}",
    "Diag Version        {:<4} {:<3} {}",
    "Service Tag         {:<4} {:<3} {}",
    "Switch Vendor       {:<4} {:<3} {}",
    "Main Board Version  {:<4} {:<3} {}",
    "COME Version        {:<4} {:<3} {}",
    "GHC-0 Board Version {:<4} {:<3} {}",
    "GHC-1 Board Version {:<4} {:<3} {}",
    "CRC-32              {:<4} {:<3} {}",
)


class ViewPages:
    """This class is dedicated to creating the struct of viewpages
    And choices of each viewpage used by `CLIMode` instance.
    An example of the struct of a choice of a viewpage is given below.
    (
        "1",                                    `INDEX` of this choice
        {
            "description": "Quit from the CLI", `DESCRIPTION` of this choice
            "type": "function",                 `TYPE` of this choice
            "function": "exit",                 `TARGET` of this choice
            "platform": SUPPORTED_PLATFORMS      `PLATFORMS` of this choice
        },
    )

    1.`INDEX` is what user should input in CLI to select this choice.
    It could be anything. However, a numeric string is advertised.

    2.`DESCRIPTION` is something displayed on the pageview where user is on.
    It should be a string decripting what this choice can do.

    3.`TYPE` indicates how this choice works after selected.
    It must only be "function" or "viewpage" literally.

    4.`TARGET` holds the target of this choice.
    The key must be literally "function" or "viewpage" corresponding to `TYPE`.
    The value must be literally the name of the function to be executed
    Or the viewpage to which the user goes if selected.

    5.`PLATFORMS` indicates platforms by which the target function is supported.
    The value should be a tuple including all supported platforms.
    """

    HOME_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Respecify Platform",
                    "type": "function",
                    "function": "specify_current_platform",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Enter BMC Settings Page",
                    "type": "viewpage",
                    "viewpage": "BMC_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "4",
                {
                    "description": "Enter EEPROM Settings Page",
                    "type": "viewpage",
                    "viewpage": "EEPROM_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "5",
                {
                    "description": "Enter FAN Settings Page",
                    "type": "viewpage",
                    "viewpage": "FAN_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "6",
                {
                    "description": "Enter LED Settings Page",
                    "type": "viewpage",
                    "viewpage": "LED_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "7",
                {
                    "description": "Enter MISC Settings Page",
                    "type": "viewpage",
                    "viewpage": "MISC_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "8",
                {
                    "description": "Enter PSU Settings Page",
                    "type": "viewpage",
                    "viewpage": "PSU_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "9",
                {
                    "description": "Enter THERMAL Settings Page",
                    "type": "viewpage",
                    "viewpage": "THERMAL_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            # Shall not support operating on X312P-T's watchdogs until much later.
            # (
            #     "10",
            #     {
            #         "description": "Enter WATCHDOG Settings Page",
            #         "type": "viewpage",
            #         "viewpage": "WATCHDOG_PAGE",
            #         "platform": S01_PLATFORMS,
            #     },
            # ),
        )
    )
    BMC_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Read BMC FW Version",
                    "type": "function",
                    "function": "read_bmc_firmware_version",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "4",
                {
                    "description": "Read BMC RTC Time",
                    "type": "function",
                    "function": "read_bmc_rtc_time",
                    "platform": SELF_DEVELOPED_PLATFORMS,
                },
            ),
            (
                "5",
                {
                    "description": "Set BMC RTC Time",
                    "type": "function",
                    "function": "set_bmc_rtc_time",
                    "platform": SELF_DEVELOPED_PLATFORMS,
                },
            ),
            (
                "6",
                {
                    "description": "OTA Upgrade BMC FW",
                    "type": "function",
                    "function": "ota_upgrade_bmc_firmware",
                    "platform": SELF_DEVELOPED_PLATFORMS,
                },
            ),
            (
                "7",
                {
                    "description": "Reset BMC",
                    "type": "function",
                    "function": "reset_bmc",
                    "platform": SELF_DEVELOPED_PLATFORMS,
                },
            ),
        )
    )
    EEPROM_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Read All EEPROM Data",
                    "type": "function",
                    "function": "read_all_eeprom_data",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "4",
                {
                    "description": "Read Specific EEPROM Data",
                    "type": "function",
                    "function": "read_specific_eeprom_data",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "5",
                {
                    "description": "Write Specific EEPROM Data",
                    "type": "function",
                    "function": "write_specific_eeprom_data",
                    "platform": S01_PLATFORMS,
                },
            ),
        )
    )
    FAN_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Read All Fan Data",
                    "type": "function",
                    "function": "read_all_fan_data",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "4",
                {
                    "description": "Read Specific Fan Data",
                    "type": "function",
                    "function": "read_specific_fan_data",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "5",
                {
                    "description": "Set Auto Fan Control Status",
                    "type": "function",
                    "function": "set_auto_fan_control_status",
                    "platform": SELF_DEVELOPED_PLATFORMS,
                },
            ),
            (
                "6",
                {
                    "description": "Set Fan Speed Level",
                    "type": "function",
                    "function": "set_fan_speed_level",
                    "platform": (),
                },
            ),
        )
    )
    LED_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Set FAN LED Status",
                    "type": "function",
                    "function": "set_fan_led_status",
                    "platform": (),
                },
            ),
            (
                "4",
                {
                    "description": "Set Loc LED Status",
                    "type": "function",
                    "function": "set_loc_led_status",
                    "platform": (),
                },
            ),
        )
    )
    MISC_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Read All Peripheral Data",
                    "type": "function",
                    "function": "read_all_peripheral_data",
                    "platform": (),
                },
            ),
            (
                "4",
                {
                    "description": "Set COM-E<->CPLD I2C Channel",
                    "type": "function",
                    "function": "set_come_cpld_iic_channel",
                    "platform": SELF_DEVELOPED_PLATFORMS,
                },
            ),
            (
                "5",
                {
                    "description": "Reset CP2112",
                    "type": "function",
                    "function": "reset_cp2112",
                    "platform": SELF_DEVELOPED_PLATFORMS,
                },
            ),
        )
    )
    PSU_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Read PSU Data",
                    "type": "function",
                    "function": "read_psu_data",
                    "platform": (),
                },
            ),
            (
                "4",
                {
                    "description": "Read Payload Data",
                    "type": "function",
                    "function": "read_payload_data",
                    "platform": (),
                },
            ),
            (
                "5",
                {
                    "description": "Power Down Payload",
                    "type": "function",
                    "function": "power_down_payload",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "6",
                {
                    "description": "Power Reset Payload",
                    "type": "function",
                    "function": "power_reset_payload",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
        )
    )
    THERMAL_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Read Temperature Data",
                    "type": "function",
                    "function": "read_temperature_data",
                    "platform": (),
                },
            ),
        )
    )
    WATCHDOG_PAGE = OrderedDict(
        (
            (
                "1",
                {
                    "description": "Quit from the CLI",
                    "type": "function",
                    "function": "exit",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "2",
                {
                    "description": "Back to HOME Page",
                    "type": "viewpage",
                    "viewpage": "HOME_PAGE",
                    "platform": SUPPORTED_PLATFORMS,
                },
            ),
            (
                "3",
                {
                    "description": "Set BMC Watchdog",
                    "type": "function",
                    "function": "set_bmc_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "4",
                {
                    "description": "Start BMC Watchdog",
                    "type": "function",
                    "function": "start_bmc_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "5",
                {
                    "description": "Kick BMC Watchdog",
                    "type": "function",
                    "function": "kick_bmc_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "6",
                {
                    "description": "Stop BMC Watchdog",
                    "type": "function",
                    "function": "stop_bmc_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "7",
                {
                    "description": "Read BMC Watchdog Status",
                    "type": "function",
                    "function": "read_bmc_watchdog_status",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "8",
                {
                    "description": "Set I2C Watchdog",
                    "type": "function",
                    "function": "set_iic_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "9",
                {
                    "description": "Start I2C Watchdog",
                    "type": "function",
                    "function": "start_iic_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "10",
                {
                    "description": "Stop I2C Watchdog",
                    "type": "function",
                    "function": "stop_iic_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
            (
                "11",
                {
                    "description": "Read I2C Watchdog Status",
                    "type": "function",
                    "function": "read_iic_watchdog",
                    "platform": S01_PLATFORMS,
                },
            ),
        )
    )


class Functions(object):
    """This class is dedicated to providing functions referred by `ViewPages`."""

    @staticmethod
    def exit():
        sys.exit(0)

    @staticmethod
    def specify_current_platform():
        while True:
            print("Supported platforms:")
            for choice_index, supported_platform in enumerate(SUPPORTED_PLATFORMS):
                choice_index_str = BLUE("{})".format(choice_index + 1))
                print("{} {}".format(choice_index_str, supported_platform))
            try:
                index = int(PROMPT_INPUT("Specify current platform: "))
            except KeyboardInterrupt:
                sys.exit(0)
            except BaseException:
                Utilities.print_invalid_choice()
                continue
            if index > len(SUPPORTED_PLATFORMS):
                Utilities.print_invalid_choice()
                continue
            break
        Utilities.CURRENT_PLATFORM = SUPPORTED_PLATFORMS[index - 1]

    """BMC Functions"""

    @staticmethod
    def read_bmc_firmware_version():
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            with Utilities.UartUtil() as uart_util:
                result = Utilities.get_processed_uart_result(
                    uart_util(["0xd", "0xaa", "0xaa"])
                ).split(" ")[1:]
                print("BMC FW Version: {}\n".format(".".join(result)))
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            with Utilities.IICTool() as iic_tool:
                result = Utilities.get_processed_iic_result(
                    iic_tool(["0x3e", "0x11", "0xaa", "0xaa", "s"])
                ).split(" ")[1:]
                result = map(
                    lambda c: str(int(c))
                    if c.isdigit()
                    else codecs.decode(c, "hex").decode(errors="ignore"),
                    result,
                )
                print("BMC FW Version: {}\n".format(".".join(result)))

    @staticmethod
    def read_bmc_rtc_time():
        with Utilities.UartUtil() as uart_util:
            result = Utilities.get_processed_uart_result(
                uart_util(["0xe", "0x1"]),
                should_verify=False,
                should_warn=False,
            )
            print("BMC RTC Time: {}\n".format(result))

    @staticmethod
    def set_bmc_rtc_time():
        time_schema = "YYYY-MM-DD-HH-mm-SS"
        try:
            time_string = PROMPT_INPUT(
                "Input datetime string in format `{}`: ".format(time_schema)
            )
        except KeyboardInterrupt:
            print(NOTICE, "operation cancelled.\n")
            return
        if time_string == "":
            print(NOTICE, "operation cancelled.\n")
            return
        # Input schema validation
        if len(time_string) != len(time_schema):
            print(ERROR, "invalid datetime string format.")
            print(NOTICE, "operation cancelled.\n")
            return
        if not all(time_string[index] == "-" for index in (4, 7, 10, 13, 16)):
            print(ERROR, "invalid datetime string format.")
            print(NOTICE, "operation cancelled.\n")
            return
        if not all(char in string.digits for char in time_string.replace("-", "")):
            print(ERROR, "invalid datetime string format.")
            print(NOTICE, "operation cancelled.\n")
            return
        with Utilities.UartUtil() as uart_util:
            uart_util(["0xe", "0x2", time_string])
        print(
            NOTICE,
            "attempt to set BMC RTC time to {} has been made. "
            "Read the BMC RTC time to check if the operation succeeded.\n",
        )

    @staticmethod
    def ota_upgrade_bmc_firmware():
        firmware_path = ""
        try:
            firmware_path = PROMPT_INPUT("Input the path to the firmware: ")
        except KeyboardInterrupt:
            print(NOTICE, "operation cancelled.\n")
            return
        if firmware_path == "":
            print(NOTICE, "invalid path.")
            print(NOTICE, "operation cancelled.\n")
            return
        if not os.path.exists(firmware_path):
            print(ERROR, "{} does not exist.".format(firmware_path))
            print(NOTICE, "operation cancelled.\n")
            return
        print(WARNING, "This operation is dangerous! BMC firmware will be upgraded!")
        if not Utilities.think_twice_before_confirming():
            return
        with Utilities.UartUtil() as uart_util:
            print(uart_util(["bmc-ota", firmware_path], comm=True, comm_cmds=("Y",)))
        print(NOTICE, "attempt to upgrade BMC has been made.\n")

    @staticmethod
    def reset_bmc():
        print(WARNING, "This operation is dangerous! BMC and 88E6131 will be reset!")
        print(WARNING, "SSH sessions are expected to be unresponsive during resetting!")
        if not Utilities.think_twice_before_confirming():
            return
        with Utilities.UartUtil() as uart_util:
            uart_util(["0x9", "0xaa", "0xaa"])
        print(NOTICE, "attempt to reset BMC has been made.\n")

    """EEPROM Functions"""

    @staticmethod
    def read_all_eeprom_data():
        codes = list(map(hex, range(0x21, 0x35)))  # EEPROM data fields
        codes.append(hex(0xFE))  # EEPROM crc field
        print(NOTICE, "dumping eeprom data...")
        for line in EEPROM_HDR:
            print(line)
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            with Utilities.UartUtil() as uart_util:
                for index, code in enumerate(codes):
                    data = Utilities.get_processed_uart_result(
                        uart_util(["0x1", code, "0xaa"]),
                        should_verify=False,
                    )
                    if code == "0x24":
                        length = len(data.split(":"))
                    elif code == "0xfe":
                        length = 4
                    else:
                        length = len(data)
                    print(EEPROM_DATA[index].format(code, length, data))
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            with Utilities.IICTool() as iic_tool:
                for index, code in enumerate(codes):
                    result = iic_tool(["0x3e", "0x1", code, "0xaa", "s"])
                    if code == "0x24":
                        data = ":".join(
                            Utilities.get_processed_iic_result(
                                result,
                                should_decode=False,
                            ).split(" ")[1:]
                        ).upper()
                        length = len(data.split(":"))
                    elif code == "0xfe":
                        data = "0x{}".format(
                            "".join(
                                Utilities.get_processed_iic_result(
                                    result,
                                    should_decode=False,
                                ).split(" ")[1:]
                            ).upper()
                        )
                        length = 4
                    else:
                        data = Utilities.get_processed_iic_result(
                            result,
                            should_decode=True,
                        )
                        length = len(data)
                    print(EEPROM_DATA[index].format(code, length, data))
        print("")

    @staticmethod
    def read_specific_eeprom_data():
        codes = list(map(hex, range(0x21, 0x35)))  # EEPROM data fields
        codes.append(hex(0xFE))  # EEPROM crc field
        for line in EEPROM_HDR:
            print(line[:24])
        for index, line in enumerate(EEPROM_DATA):
            print(line[:25].format(codes[index]))
        try:
            input_codes = (
                PROMPT_INPUT(
                    "Available eeprom codes are listed above. "
                    "Input one or more codes seperated by space: "
                )
                .strip()
                .split(" ")
            )
        except KeyboardInterrupt:
            print(NOTICE, "operation cancelled.")
            return
        if all(code not in codes for code in input_codes):
            print(ERROR, "all code(s) invalid.")
            print(NOTICE, "operation cancelled.")
            return
        if any(code not in codes for code in input_codes):
            print(WARNING, "found and skipped invalid code(s).")
        input_codes = list(filter(lambda code: code in codes, input_codes))
        print(NOTICE, "dumping eeprom data...")
        for line in EEPROM_HDR:
            print(line)
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            with Utilities.UartUtil() as uart_util:
                for code in input_codes:
                    index = codes.index(code)
                    data = Utilities.get_processed_uart_result(
                        uart_util(["0x1", code, "0xaa"]),
                        should_verify=False,
                    )
                    if code == "0x24":
                        length = len(data.split(":"))
                    elif code == "0xfe":
                        length = 4
                    else:
                        length = len(data)
                    print(EEPROM_DATA[index].format(code, length, data))
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            with Utilities.IICTool() as iic_tool:
                for code in input_codes:
                    index = codes.index(code)
                    result = iic_tool(["0x3e", "0x1", code, "0xaa", "s"])
                    if code == "0x24":
                        data = ":".join(
                            Utilities.get_processed_iic_result(
                                result,
                                should_decode=False,
                            ).split(" ")[1:]
                        ).upper()
                        length = len(data.split(":"))
                    elif code == "0xfe":
                        data = "0x{}".format(
                            "".join(
                                Utilities.get_processed_iic_result(
                                    result,
                                    should_decode=False,
                                ).split(" ")[1:]
                            ).upper()
                        )
                        length = 4
                    else:
                        data = Utilities.get_processed_iic_result(
                            result,
                            should_decode=True,
                        )
                        length = len(data)
                    print(EEPROM_DATA[index].format(code, length, data))
        print("")

    @staticmethod
    def write_specific_eeprom_data():
        codes = list(map(hex, range(0x21, 0x35)))  # EEPROM data fields
        codes.append(hex(0xFE))  # EEPROM crc field
        for line in EEPROM_HDR:
            print(line[:24])
        for index, line in enumerate(EEPROM_DATA):
            print(line[:25].format(codes[index]))
        try:
            input_entries = PROMPT_INPUT(
                "Available eeprom codes are listed above. "
                "Input one or more entries seperated by space.\n"
                '(E.G: 0x21=X312P-48Y-T 0x24=60:EB:5A:00:E8:EC 0x25="12/31/2018 23:59:59"):\n'
            ).strip()
        except KeyboardInterrupt:
            print(NOTICE, "operation cancelled.")
            return
        pattern = re.compile(r"0x[0-9a-f]+=")
        indices = list(map(input_entries.index, pattern.findall(input_entries)))
        entries = [
            input_entries[c:].strip()
            if i + 1 == len(indices)
            else input_entries[c : indices[i + 1]].strip()
            for i, c in enumerate(indices)
        ]
        if all(entry[:4] not in codes for entry in entries):
            print(ERROR, "all code(s) invalid.")
            print(NOTICE, "operation cancelled.")
            return
        if any(entry[:4] not in codes for entry in entries):
            print(WARNING, "found and skipped invalid code(s).")
        print(
            WARNING,
            "This operation is dangerous! System EEPROM Data will be overwritten!",
        )
        if not Utilities.think_twice_before_confirming():
            return
        with Utilities.IICTool() as iic_tool:
            for entry in entries:
                code, data = entry.split("=", 1)
                while (data.startswith('"') and data.endswith('"')) or (
                    data.startswith("'") and data.endswith("'")
                ):  # Is data quoted with single or double quotes?
                    data = data.strip('"').strip("'").strip()
                if len(data) > 19:
                    print(
                        WARNING, "skipped code {} due to data overlength.".format(code)
                    )
                    continue
                if code == "0x24":
                    hex_data = Utilities.parse_mac_addr_into_hex_str_list(data)
                    if hex_data is None:
                        print(
                            WARNING,
                            "skipped code {} due to invalid format.".format(code),
                        )
                        continue
                else:
                    hex_data = Utilities.parse_ascii_str_into_hex_str_list(data)
                iic_tool(
                    ["0x3e", "0x2", code, hex(len(hex_data))] + hex_data + ["s"],
                    should_return=False,
                )
        print("")

    """FAN Functions"""

    @staticmethod
    def read_all_fan_data():
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            with Utilities.UartUtil() as uart_util:
                retry = 3
                result = ""
                while retry > 0:
                    retry -= 1
                    result = Utilities.get_processed_uart_result(
                        uart_util(["0x5", "0xaa", "0xaa"]),
                    )
                    if result != "":
                        break
                if retry == 0 and result == "":
                    print(ERROR, "unable to retrieve data from Uart1 after 3 retries.")
                    print(NOTICE, "please check if Uart1 is occupied by BSP.\n")
                    return
                data = result.split(" ")
                presence_mask = Utilities.parse_hex_char_into_binary_char(data[1])
                direction_mask = Utilities.parse_hex_char_into_binary_char(data[2])
                fan_total_num = int(len(data[3:]) / 2)
                fan_num_per_group = 2
                fan_group_num = int(fan_total_num / fan_num_per_group)
                print(
                    "{} fans divided into {} groups listed below:".format(
                        fan_total_num, fan_group_num
                    )
                )
                for fan_group_index in range(0, fan_group_num):
                    print("Fan Group {}:".format(fan_group_index + 1))
                    for fan_index_in_group in range(0, fan_num_per_group):
                        fan_index = (
                            fan_group_index * fan_num_per_group + fan_index_in_group
                        )
                        fan_presence = str(
                            presence_mask[8 - fan_group_num :][fan_group_index] == "1"
                        )
                        fan_status = "OK" if fan_presence else "Not OK"
                        fan_direction = (
                            "exhaust"
                            if direction_mask[8 - fan_group_num :][fan_group_index]
                            == "0"
                            else "intake"
                        )
                        fan_rpm = Utilities.parse_hex_num_into_decimal_num(
                            data[3:][fan_index * 2] + data[3:][fan_index * 2 + 1]
                        )
                        print(
                            "Fan{:<2}| "
                            "Presence: {:<5}"
                            "Status: {:<6}"
                            "Direction: {:<8}"
                            "RPM: {}".format(
                                fan_index + 1,
                                fan_presence,
                                fan_status,
                                fan_direction,
                                fan_rpm,
                            )
                        )
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            Utilities.print_unimplemented_function()
        print("")

    @staticmethod
    def read_specific_fan_data():
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            with Utilities.UartUtil() as uart_util:
                try:
                    input_fan_index_string = PROMPT_INPUT(
                        "Input 1-based index of fan(s) seperated by space: "
                    ).strip()
                except KeyboardInterrupt:
                    print(NOTICE, "operation cancelled.\n")
                    return
                if not all(
                    char in string.digits
                    for char in input_fan_index_string.replace(" ", "")
                ):
                    print(ERROR, "invalid fan fan index")
                    print(NOTICE, "operation cancelled.\n")
                    return
                input_fan_indices = input_fan_index_string.split(" ")
                if any(int(fan_index) < 1 for fan_index in input_fan_indices):
                    print(ERROR, "invalid fan fan index")
                    print(NOTICE, "operation cancelled.\n")
                    return
                retry = 3
                result = ""
                while retry > 0:
                    retry -= 1
                    result = Utilities.get_processed_uart_result(
                        uart_util(["0x5", "0xaa", "0xaa"]),
                    )
                    if result != "":
                        break
                if retry == 0 and result == "":
                    print(ERROR, "unable to retrieve data from Uart1 after 3 retries.")
                    print(NOTICE, "please check if Uart1 is occupied by BSP.\n")
                    return
                if any(
                    int(fan_index) > int(len(result.split(" ")[3:]) / 2)
                    for fan_index in input_fan_indices
                ):
                    print(ERROR, "invalid fan fan index")
                    print(NOTICE, "operation cancelled.\n")
                    return
                if Utilities.CURRENT_PLATFORM in (
                    X308PT,
                    X532PT,
                ):
                    fan_num_per_group = 2
                elif Utilities.CURRENT_PLATFORM in (X564PT,):
                    fan_num_per_group = 1
                else:
                    print(ERROR, "fan number per group is unknown.")
                    print(NOTICE, "operation cancelled.\n")
                    return
                for fan_index in input_fan_indices:
                    fan_group_index = math.ceil(int(fan_index) / fan_num_per_group)
                    fan_data_offset = int((fan_group_index % fan_num_per_group) * 2)
                    retry = 3
                    result = ""
                    while retry > 0:
                        retry -= 1
                        result = Utilities.get_processed_uart_result(
                            uart_util(["0x6", "0x{}".format(fan_group_index), "0x1"]),
                        )
                        if result != "":
                            break
                    if retry == 0 and result == "":
                        print(
                            ERROR, "unable to retrieve data from Uart1 after 3 retries."
                        )
                        print(NOTICE, "please check if Uart1 is occupied by BSP.\n")
                        return
                    fan_data = list(
                        map(
                            Utilities.parse_hex_char_into_binary_char,
                            result.split(" ")[1:],
                        )
                    )
                    fan_presence = str(fan_data[0][7] == "1")
                    fan_status = "OK" if fan_presence else "Not OK"
                    fan_warning = str(fan_data[0][6] == "1")
                    fan_direction = "exhaust" if fan_data[1][7] == "1" else "intake"
                    while retry > 0:
                        retry -= 1
                        result = Utilities.get_processed_uart_result(
                            uart_util(["0x6", "0x{}".format(fan_group_index), "0x0"]),
                        )
                        if result != "":
                            break
                    if retry == 0 and result == "":
                        print(
                            ERROR, "unable to retrieve data from Uart1 after 3 retries."
                        )
                        print(NOTICE, "please check if Uart1 is occupied by BSP.\n")
                        return
                    fan_rpm = Utilities.parse_hex_num_into_decimal_num(
                        "".join(
                            result.split(" ")[1:][fan_data_offset : fan_data_offset + 2]
                        )
                    )
                    print(
                        "Fan{:<2}| "
                        "Presence: {:<5}"
                        "Status: {:<6}"
                        "Warning: {:<6}"
                        "Direction: {:<8}"
                        "RPM: {}".format(
                            fan_index,
                            fan_presence,
                            fan_status,
                            fan_warning,
                            fan_direction,
                            fan_rpm,
                        )
                    )
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            Utilities.print_unimplemented_function()
        print("")

    @staticmethod
    def set_auto_fan_control_status():
        with Utilities.UartUtil() as uart_util:
            try:
                input_auto_fan_control = (
                    PROMPT_INPUT(
                        "Input on/off to turn on/off automatic fan speed controlling: "
                    )
                    .strip()
                    .lower()
                )
            except KeyboardInterrupt:
                print(NOTICE, "operation cancelled.\n")
                return
            if input_auto_fan_control not in ("on", "off"):
                print(ERROR, "unknown operation.")
                print(NOTICE, "operation cancelled.\n")
                return
            if input_auto_fan_control == "on":
                uart_util(["0x7", "0x1"])
            elif input_auto_fan_control == "off":
                uart_util(["0x7", "0x0"])
            print(
                WARNING,
                "attempt to set automatic fan speed controlling to {} has been made.\n".format(
                    input_auto_fan_control
                ),
            )

    @staticmethod
    def set_fan_speed_level():
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            Utilities.print_unimplemented_function()
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            Utilities.print_unimplemented_function()

    """LED Functions"""

    @staticmethod
    # To be implemented in next version
    def set_fan_led_status():
        Utilities.print_unimplemented_function()

    @staticmethod
    # To be implemented in next version
    def set_loc_led_status():
        Utilities.print_unimplemented_function()

    """MISC Functions"""

    @staticmethod
    def read_all_peripheral_data():
        # Gotta read BSP code carefully to find out which BMC FW versions
        # support quick peripheral reading before implement this function.
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            Utilities.print_unimplemented_function()
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            Utilities.print_unimplemented_function()

    @staticmethod
    def set_come_cpld_iic_channel():
        with Utilities.UartUtil() as uart_util:
            try:
                input_come_cpld_iic_channel = (
                    PROMPT_INPUT(
                        "Input cp2112/superio to set COM-E <-> CPLD channel to cp2112/superio: "
                    )
                    .strip()
                    .lower()
                )
            except KeyboardInterrupt:
                print(NOTICE, "operation cancelled.\n")
                return
            if input_come_cpld_iic_channel not in ("cp2112", "superio"):
                print(ERROR, "unknown channel.")
                print(NOTICE, "operation cancelled.\n")
            if input_come_cpld_iic_channel == "cp2112":
                uart_util(["0xf", "0x1", "0xaa"])
            elif input_come_cpld_iic_channel == "superio":
                uart_util(["0xf", "0x2", "0xaa"])
            print(
                WARNING,
                "attempt to set COM-E <-> CPLD channel to {} has been made.\n".format(
                    input_come_cpld_iic_channel
                ),
            )

    @staticmethod
    def reset_cp2112():
        print(WARNING, "This operation is dangerous! CP2112 will be reset!")
        if not Utilities.think_twice_before_confirming():
            return
        with Utilities.UartUtil() as uart_util:
            uart_util(["0x10", "0x1", "0xaa"])
            uart_util(["0x10", "0x2", "0xaa"])
        print(WARNING, "attempt to power reset CP2112 has been made.\n")

    @staticmethod
    def reset_pca9548():
        print(WARNING, "This operation is dangerous! PCA9548 will be reset!")
        if not Utilities.think_twice_before_confirming():
            return
        with Utilities.UartUtil() as uart_util:
            uart_util(["0x10", "0x3", "0xaa"])
            uart_util(["0x10", "0x4", "0xaa"])
        print(WARNING, "attempt to power reset PCA9548 has been made.\n")

    """PSU Functions"""

    @staticmethod
    def read_psu_data():
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            Utilities.print_unimplemented_function()
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            Utilities.print_unimplemented_function()

    @staticmethod
    def read_payload_data():
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            Utilities.print_unimplemented_function()
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            Utilities.print_unimplemented_function()

    @staticmethod
    def power_down_payload():
        print(WARNING, "This operation is dangerous! System will shut down!")
        if not Utilities.think_twice_before_confirming():
            return
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            with Utilities.UartUtil() as uart_util:
                uart_util(["0xa", "0xaa", "0xaa"])
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            with Utilities.IICTool() as iic_tool:
                iic_tool(["0x3e", "0xa", "0xaa", "0xaa", "s"])
        print(WARNING, "attempt to power down payload has been made.\n")

    @staticmethod
    def power_reset_payload():
        print(WARNING, "This operation is dangerous! System will be reset!")
        if not Utilities.think_twice_before_confirming():
            return
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            with Utilities.UartUtil() as uart_util:
                uart_util(["0xa", "0xa1", "0xaa"])
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            with Utilities.IICTool() as iic_tool:
                iic_tool(["0x3e", "0x9", "0xaa", "0xaa", "s"])
        print(WARNING, "attempt to power reset payload has been made.\n")

    """THERMAL Functions"""

    @staticmethod
    def read_temperature_data():
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            Utilities.print_unimplemented_function()
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            Utilities.print_unimplemented_function()

    """WATCHDOG Functions"""

    # TODO: implement watchdog functions for x312p-48y-t in the future
    # @staticmethod
    # def set_bmc_watchdog():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def start_bmc_watchdog():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def kick_bmc_watchdog():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def stop_bmc_watchdog():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def read_bmc_watchdog_status():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def set_iic_watchdog():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def start_iic_watchdog():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def stop_iic_watchdog():
    #     Utilities.print_unimplemented_function()

    # @staticmethod
    # def read_iic_watchdog():
    #     Utilities.print_unimplemented_function()


class UnsupportedOSError(Exception):
    """"""


class InsufficientPermissionsError(Exception):
    """"""


class InvalidUsageError(Exception):
    """"""


class UartUtilError(Exception):
    """"""


class IICToolError(Exception):
    """"""


class Utilities(object):
    # Initialize current platform
    CURRENT_PLATFORM = ""  # type: str  # type: ignore

    # Hex data of uart_util executable
    UART_UTIL_HEX = """"""

    @staticmethod
    def detect_current_platform():
        print(NOTICE, "trying to detect current platform...")
        config_filename = "platform.conf"
        et_cetra_dir = "/etc/"
        temporary_dir = "/tmp/"
        container = "syncd"
        device_dir = "/usr/share/sonic/device/"
        hwskus_dir = (
            "x86_64-asterfusion_cx308p_48y_t-r0/",
            "x86_64-asterfusion_cx312p_48y_t-r0/",
            "x86_64-asterfusion_cx532p_t-r0/",
            "x86_64-asterfusion_cx564p_t-r0/",
        )
        host_config = os.path.join(et_cetra_dir, config_filename)
        temp_config = os.path.join(temporary_dir, config_filename)
        hwsku_config = [
            os.path.join(device_dir, hwsku_dir, config_filename)
            for hwsku_dir in hwskus_dir
        ]
        try:
            with open(os.devnull, "a") as dev_null:
                subprocess.check_call(
                    (
                        "docker",
                        "cp",
                        "{}:{}".format(container, str(host_config)),
                        str(temp_config),
                    ),
                    stdout=dev_null,
                    stderr=dev_null,
                )
        except BaseException:
            pass
        platform = ""
        for config in [host_config, temp_config] + hwsku_config:
            if os.path.exists(config):
                with open(config, "r") as cfg:
                    for line in cfg:
                        if line.startswith("platform:"):
                            platform = line.replace("platform:", "").strip()
            if platform != "":
                break
        if os.path.exists(temp_config):
            os.remove(temp_config)
        if platform not in SUPPORTED_PLATFORMS:
            print(ERROR, "failed in detecting current platform.")
            Functions.specify_current_platform()
        else:
            Utilities.CURRENT_PLATFORM = platform

    @staticmethod
    def check_prerequisites():
        print(NOTICE, "checking prerequisites...")
        if "linux" not in sys.platform or getattr(os, "geteuid", None) is None:
            raise UnsupportedOSError(
                "unsupported operation system {}.".format(sys.platform)
            )
        if os.geteuid() != 0:  # type: ignore
            raise InsufficientPermissionsError("root permission denied.")
        if Utilities.CURRENT_PLATFORM in SELF_DEVELOPED_PLATFORMS:
            if spawn.find_executable("uart_util") is None:
                raise UartUtilError(
                    "uart_util executable not found. "
                    "Retrieve it from bsp installer package."
                )
        elif Utilities.CURRENT_PLATFORM in S01_PLATFORMS:
            if None in (
                spawn.find_executable("i2cdetect"),
                spawn.find_executable("i2cset"),
                spawn.find_executable("i2cdump"),
            ):
                raise IICToolError("i2ctools not installed. Install it first.")

    @staticmethod
    def get_processed_uart_result(
        result,  # type: str
        should_verify=True,  # type: bool
        should_warn=True,  # type: bool
    ):
        if should_verify:
            data = result.split(" ")
            length = Utilities.parse_hex_num_into_decimal_num(data[0])
            if length is None:
                print(ERROR, "received incorrect data: {}.".format(result))
                return ""
            if length + 1 != len(data):
                print(ERROR, "received incorrect data: {}.".format(result))
                return ""
            if "read failed" in result:
                print(ERROR, "received incorrect data: {}.".format(result))
                return ""
        else:
            if should_warn and "read failed" in result:
                print(WARNING, "received unverified data: {}.".format(result))
                return ""
            return result.strip()
        return result.strip()

    @staticmethod
    def get_processed_iic_result(
        result,  # type: str
        should_decode=False,  # type: bool
        should_verify=True,  # type: bool
        should_warn=True,  # type: bool
    ):
        result = " ".join(
            map(
                lambda line: line[4:52].strip(),
                result.split("\n")[1:],
            )
        )
        if should_verify:
            data = result.split(" ")
            length = Utilities.parse_hex_num_into_decimal_num(data[0])
            if length is None:
                print(ERROR, "received incorrect data: {}.".format(result))
                return ""
            if length + 1 != len(data):
                print(ERROR, "received incorrect data: {}.".format(result))
                return ""
            if "Error:" in result:
                print(ERROR, "received incorrect data: {}.".format(result))
                return ""
        else:
            if should_warn and "Error:" in result:
                print(WARNING, "received unverified data: {}.".format(result))
                return ""
            return result.strip()
        if should_decode:
            return codecs.decode("".join(result.split(" ")[1:]), "hex").decode(
                errors="ignore"
            )
        return result.strip()

    @staticmethod
    def think_twice_before_confirming():
        try:
            confirmation = PROMPT_INPUT("Think twice before confirming (y/n): ")
        except KeyboardInterrupt:
            print(NOTICE, "operation cancelled.\n")
            return False
        if confirmation.lower() != "y":
            print(NOTICE, "operation cancelled.\n")
            return False
        return True

    @staticmethod
    def parse_hex_num_into_decimal_num(
        hex_num,  # type: str
    ):
        try:
            return int(hex_num, 16)
        except BaseException:
            return

    @staticmethod
    def parse_hex_char_into_binary_char(
        hex_char,  # type: str
    ):
        if sys.version_info.major == 3:
            type_check = type(hex_char) == str
        else:
            type_check = isinstance(hex_char, basestring)  # type: ignore
        if not type_check:
            raise TypeError(
                "expect string type but received {}.".format(type(hex_char))
            )
        if len(hex_char) > 2:
            raise ValueError(
                "expect single hexadecimal character but received: {}.".format(hex_char)
            )
        if not all(char in string.hexdigits for char in hex_char):
            raise ValueError(
                "expect hexadecimal character but received: {}.".format(hex_char)
            )
        return "{:08b}".format(int(hex_char, 16))

    @staticmethod
    def parse_mac_addr_into_hex_str_list(
        mac_addr,  # type: str
    ):
        if sys.version_info.major == 3:
            type_check = type(mac_addr) == str
        else:
            type_check = isinstance(mac_addr, basestring)  # type: ignore
        if not type_check:
            raise TypeError(
                "expect string type but received: {}.".format(type(mac_addr))
            )
        if len(mac_addr) != 17 and len(mac_addr.split(":")) != 6:
            return
        if not all(mac_addr[index] == ":" for index in (2, 5, 8, 11, 14)):
            return
        if not all(char in string.hexdigits for char in mac_addr.replace(":", "")):
            return
        return list(map(lambda c: "0x{}".format(c), mac_addr.lower().split(":")))

    @staticmethod
    def parse_ascii_str_into_hex_str_list(
        ascii_str,  # type: str
    ):
        if sys.version_info.major == 3:
            type_check = type(ascii_str) == str
        else:
            type_check = isinstance(ascii_str, basestring)  # type: ignore
        if not type_check:
            raise TypeError(
                "expect string type but received: {}.".format(type(ascii_str))
            )
        hex_str_list = list(
            map(
                lambda c: "0x{}".format(
                    codecs.encode(c.encode("utf-8", errors="ignore"), "hex").decode(
                        errors="ignore"
                    )
                ),
                ascii_str,
            )
        )
        return hex_str_list

    @staticmethod
    def print_invalid_choice():
        print(ERROR, "invalid choice. Please check your input.")

    @staticmethod
    def print_unsupported_choice():
        print(ERROR, "unsupported choice on {}.".format(Utilities.CURRENT_PLATFORM))

    @staticmethod
    def print_unimplemented_function():
        print(ERROR, "unimplemented function on {}.".format(Utilities.CURRENT_PLATFORM))

    @staticmethod
    def print_cli_help_information():
        print("Usage: [python] {} [OPTION]...".format(sys.argv[0]))
        print("A python script helpful for communicating with BMC on X-T devices.")

        print("OPTION:")
        print("  -h, --help           show this help information and exit.")
        print("  -i, --interactive    enter interactive CLI mode.")

    class UartUtil(object):
        def __init__(self):
            self.uart_util = os.path.abspath(
                spawn.find_executable("uart_util")  # type: ignore
            )  # type: str  # type: ignore

        def __enter__(self):
            def inner(
                cmd,  # type: list[str]
                comm=False,  # type: bool
                comm_cmds=(),  # type: tuple[str, ...]
            ):
                # Currently this is only for bmc firmware ota upgrading
                if comm:
                    process = subprocess.Popen(
                        ["stdbuf", "-oL", self.uart_util, "/dev/ttyS1"] + cmd,
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE,
                        bufsize=1,
                    )
                    for comm_cmd in comm_cmds:
                        if process.stdin is not None:
                            process.stdin.write(comm_cmd.encode() + b"\n")
                            process.stdin.flush()
                    if process.stdout is not None:
                        for output in iter(process.stdout.readline, b""):
                            decoded_output = output.decode(errors="ignore")
                            if decoded_output.rstrip() == "":
                                decoded_output = "\n"
                            else:
                                decoded_output = decoded_output.rstrip()
                            print(decoded_output, end="\r")
                    print("")
                    return ""
                return (
                    subprocess.check_output([self.uart_util, "/dev/ttyS1"] + cmd)
                    .strip()
                    .decode(errors="ignore")
                )

            return inner

        def __exit__(
            self,
            exc_type,  # type: type
            exc_value,  # type: Type[Exception]
            exc_traceback,  # type: TracebackType
        ):
            pass

    class IICTool(object):
        def __init__(self):
            with open(os.devnull, "a") as dev_null:
                channels = (
                    subprocess.check_output(("i2cdetect", "-l"), stderr=dev_null)
                    .strip()
                    .decode(errors="ignore")
                    .split("\n")
                )
                if not any(
                    "CP2112" in channel or "sio_smbus" in channel
                    for channel in channels
                ):
                    raise IICToolError("all i2c channels unavailable")
                for channel in channels:
                    channel_num = channel[channel.find("i2c-") + 4]
                    try:
                        subprocess.check_call(
                            (
                                "i2cset",
                                "-y",
                                channel_num,
                                "0x3e",
                                "0x11",
                                "0xaa",
                                "0xaa",
                                "s",
                            ),
                            stdout=dev_null,
                            stderr=dev_null,
                        )
                        self.channel = channel_num
                        break
                    except BaseException:
                        pass
                else:
                    self.channel = ""

        def __enter__(self):
            if self.channel == "":
                raise IICToolError("all i2c channels unavailable")

            def inner(
                cmd,  # type: list[str]
                should_return=True,  # type: bool
            ):
                subprocess.check_call(["i2cset", "-y", self.channel] + cmd)
                if should_return:
                    return (
                        subprocess.check_output(
                            ("i2cdump", "-y", self.channel, "0x3e", "s", "0xff")
                        )
                        .strip()
                        .decode(errors="ignore")
                    )
                return ""

            return inner

        def __exit__(
            self,
            exc_type,  # type: type
            exc_value,  # type: Type[Exception]
            exc_traceback,  # type: TracebackType
        ):
            pass


class CLIMode(object):
    def __init__(self):
        Utilities.detect_current_platform()
        Utilities.check_prerequisites()
        self.current_page_name = "HOME_PAGE"
        print(NOTICE, "entered interactive CLI mode.")

    def print_viewpage(self):
        page_name = self.current_page_name.split("_")
        page_name[1] = page_name[1].title()
        page_name = " ".join(page_name)
        self.current_page = getattr(ViewPages, self.current_page_name)
        print(
            "Platform: {} | Viewpage: {}".format(
                BLUE(Utilities.CURRENT_PLATFORM), BLUE(page_name)
            )
        )
        print("Available choices:")
        for choice in self.current_page:
            choice_index = "{})".format(choice)
            choice_description = self.current_page.get(choice).get("description")
            choice_platform = self.current_page.get(choice).get("platform")
            choice_color = RED
            if Utilities.CURRENT_PLATFORM in choice_platform:
                choice_color = BLUE
            choice_index = choice_color(choice_index)
            print("{} {}".format(choice_index, choice_description))

    def serve_forever(self):
        while True:
            print("")
            self.print_viewpage()
            user_input = ""
            try:
                user_input = PROMPT_INPUT("Input your choice: ")
            except KeyboardInterrupt:
                Functions.exit()
            choice = self.current_page.get(user_input)  # type: ignore
            if choice is None:
                Utilities.print_invalid_choice()
                continue
            choice_platform = choice.get("platform")
            if Utilities.CURRENT_PLATFORM not in choice_platform:
                Utilities.print_unsupported_choice()
                continue
            choice_type = choice.get("type")
            choice_target = choice.get(choice_type)
            if choice_type == "function":
                choice_function = getattr(Functions, choice_target, None)
                if choice_function is None:
                    print(ERROR, "operation is not implemented yet.")
                    continue
                choice_function()
            elif choice_type == "viewpage":
                self.current_page = getattr(ViewPages, choice_target, None)
                if self.current_page is None:
                    print(ERROR, "viewpage is not introduced yet.")
                    continue
                self.current_page_name = choice_target


if __name__ == "__main__":
    if any(arg in sys.argv[1:] for arg in ("-h", "--help")):
        Utilities.print_cli_help_information()
    elif any(arg in sys.argv[1:] for arg in ("-i", "--interactive")):
        """temporarily directly enter interactive CLI mode."""
    else:
        """raise NotImplementedError("script mode not implemented yet.")"""
    CLIMode().serve_forever()
