#
# @file CMakeLists.txt
# @date 2024/06/05
#
# Hang Tsi (tsihang@asterfusion.com)
#
# Belowing shows how to build bf-bsp-lts via CMake.
#
# cd bf-bsp-lts
# ./autogen.sh
# mkdir build && cd build
# cmake .. -DCMAKE_MODULE_PATH=`pwd`/../cmake -DCMAKE_INSTALL_PREFIX=$SDE_INSTALL
# make -j4 install
#
#

cmake_minimum_required(VERSION 3.5)

option(STANDALONE  "Build outside of pkgsrc" ON)
option(FORCE32            "Force a 32bit compile on 64bit" OFF)
option(THRIFT-DRIVER      "Build with thrift support" ON)
option(COVERAGE           "Build with gcov" OFF)
option(TOFINO             "Build for TOFINO" ON)
option(TOFINO2            "Build for TOFINO2" OFF)
option(ASTERFUSION-DIAGS  "Build Asterfusion Diags" OFF)

# Pass OS_NAME/OS_VERSION/SDE_VERSION by cmake CLI. Take belowing as an example:
# cmake .. -DCMAKE_MODULE_PATH=`pwd`/../cmake -DCMAKE_INSTALL_PREFIX=$SDE_INSTALL \
#          -DOS_NAME=(Debian|Ubuntu)                                     \
#          -DOS_VERSION=[(8|9|10|11|12),(14.04|16.04|18.04|20.04|22.04)] \
#          -DSDE_VERSION=(900|911|933|954|975|991|9112|9133)
#
message(STATUS "Supported -DOS_NAME=(Debian|Ubuntu)")
message(STATUS "Supported -DOS_VERSION=[(8|9|10|11|12),(14.04|16.04|18.04|20.04|22.04)]")
message(STATUS "Supported -DSDE_VERSION=(900|911|933|954|975|991|9112|9133)")
cmake_host_system_information(RESULT HOST_NAME QUERY HOSTNAME)
message(STATUS "${HOST_NAME}")

if(NOT DEFINED SDE_VERSION)
  # Default SDE_VERSION
  set(SDE_VERSION 9130)
endif()

if(NOT DEFINED OS_VERSION)
  # Default OS_VERSION
  set(OS_VERSION 9)
endif()

if(NOT DEFINED OS_NAME)
  # Default OS_NAME
  set(OS_NAME "Debian")
endif()

if(${SDE_VERSION} LESS 930)
  message(STATUS "SDE < 9.3.0")
elseif(${SDE_VERSION} LESS 990)
  message(STATUS "9.3.0 <= SDE < 9.9.0")
else()
  message(STATUS "SDE >= 9.9.0")
endif()
message(STATUS "Giving -DOS_NAME=${OS_NAME}")
message(STATUS "Giving -DOS_VERSION=${OS_VERSION}")
message(STATUS "Giving -DSDE_VERSION=${SDE_VERSION}")

# Please see drivers/include/bf_pltfm_types/bf_pltfm_types.h +34
add_definitions(-DOS_NAME=${OS_NAME})
add_definitions(-DOS_VERSION=${OS_VERSION})
add_definitions(-DSDE_VERSION=${SDE_VERSION})
# Unused
add_definitions(-DDEVICE_IS_ASIC)

if(${SDE_VERSION} LESS 990)
  set(bfutils_lib "bfutils")
  set(bfsys_lib "bfsys")
else()
  set(bfutils_lib "target_utils")
  set(bfsys_lib "target_sys")
endif()
message(STATUS "Linking ${bfutils_lib} ${bfsys_lib}")

if(FORCE32)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_TYPE "Release")

if(NOT EXTRA_CPPFLAGS)
  set(EXTRA_CPPFLAGS "" CACHE STRING "" FORCE)
else()
  message(STATUS "\nEXTRA_CPPFLAGS: ${EXTRA_CPPFLAGS}")
endif()

set(C_CXX_FLAGS "-Wall -Wextra -Werror -Wundef -Wshadow -Wno-missing-field-initializers -DBF_SYS_LOG_FORMAT_CHECK ${EXTRA_CPPFLAGS}")
if(COVERAGE)
  set(C_CXX_FLAGS "${C_CXX_FLAGS} --coverage")
endif()
if(FORCE32)
  set(CMAKE_C_FLAGS   "-m32")
  set(CMAKE_CXX_FLAGS "-m32")
endif()
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${C_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_CXX_FLAGS}")

link_directories(AFTER ${CMAKE_INSTALL_PREFIX}/lib)
include_directories(SYSTEM ${CMAKE_INSTALL_PREFIX}/include)

if (THRIFT-DRIVER)
  find_package(Thrift REQUIRED)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}     -DINC_PLTFM_UCLI -Wno-unused-parameter -Wno-shadow")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DINC_PLTFM_UCLI -Wno-unused-parameter -Wno-shadow")

if (THRIFT-DRIVER)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}     -DTHRIFT_ENABLED")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTHRIFT_ENABLED")
endif()

include_directories(platforms/include)
include_directories(platforms/asterfusion-bf/include)
include_directories(drivers/src)
include_directories(drivers/include)
include_directories(platforms/common/devices/include)

add_library(pltfm_o OBJECT
  drivers/src/bf_pltfm_mgr/pltfm_mgr_init.c
  drivers/src/bf_pltfm_mgr/pltfm_mgr_handlers.c
)

## Asterfusion PLAT Sources
set(ASTERFUSIONBF_PLTFM_SRCS
  platforms/asterfusion-bf/src/bf_pltfm_master_i2c/bf_pltfm_master_i2c.c
  platforms/asterfusion-bf/src/bf_pltfm_master_i2c/bf_pltfm_cgos_i2c.c
  platforms/asterfusion-bf/src/bf_pltfm_uart/bf_pltfm_uart.c
  platforms/asterfusion-bf/src/bf_pltfm_bmc_tty/bmc_tty.c
  platforms/asterfusion-bf/src/platform_mgr/platform.c
  platforms/asterfusion-bf/src/platform_mgr/platform_board.c
  platforms/asterfusion-bf/src/platform_mgr/platform_health_mntr.c
  platforms/asterfusion-bf/src/bf_pltfm_led/lq.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_led.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_led_x564p.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_led_x532p.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_led_x308p.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_led_x312p.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_led_x732q.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_led_hc36y24c.c
  platforms/asterfusion-bf/src/bf_pltfm_led/bf_pltfm_mav_cpld_led.c
  platforms/asterfusion-bf/src/bf_pltfm_cpld/bf_pltfm_cpld_ucli.c
  platforms/asterfusion-bf/src/bf_pltfm_spi/bf_pltfm_spi.c
)
## Asterfusion QSFP Sources
set(ASTERFUSIONBF_QSFP_SRCS
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_module.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_sub_module.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_sub_module_x564p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_sub_module_x532p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_sub_module_x308p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_sub_module_x312p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_sub_module_x732q.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_qsfp_sub_module_hc36y24c.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_sfp_module.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_sfp_module_x564p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_sfp_module_x532p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_sfp_module_x308p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_sfp_module_x312p.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_sfp_module_x732q.c
  platforms/asterfusion-bf/src/qsfp/bf_mav_sfp_module_hc36y24c.c
)
## Asterfusion CHSS MGT Sources
set(ASTERFUSIONBF_CHSS_MGMT_SRCS
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_intf.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.h
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_tmp.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_vrail.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_ps.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_fan.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_mac.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_ucli.c
)
## Asterfusion CP2112 Sources
set(ASTERFUSIONBF_CP2112_SRCS
  platforms/asterfusion-bf/src/bf_pltfm_cp2112/bf_pltfm_cp2112_intf.c
  platforms/asterfusion-bf/src/bf_pltfm_cp2112/bf_pltfm_cp2112_intf_ucli.c
)
add_library(asterfusionbf_driver_o OBJECT
  ${ASTERFUSIONBF_PLTFM_SRCS}
  ${ASTERFUSIONBF_QSFP_SRCS}
  ${ASTERFUSIONBF_CHSS_MGMT_SRCS}
  ${ASTERFUSIONBF_CP2112_SRCS}
)
add_library(asterfusionbf_driver SHARED $<TARGET_OBJECTS:asterfusionbf_driver_o>)
set_target_properties(asterfusionbf_driver PROPERTIES VERSION 0.0.0 SOVERSION 0)
target_link_libraries(asterfusionbf_driver ${bfsys_lib} curl usb-1.0 ${bfutils_lib} cgos)
add_library(pltfm_driver_o OBJECT
  drivers/src/bf_bd_cfg/bf_bd_cfg_intf.c
  drivers/src/bf_bd_cfg/bf_bd_cfg_intf_ucli.c
  drivers/src/bf_bd_cfg/bf_bd_cfg_porting.c
  drivers/src/bf_qsfp/bf_qsfp_cli.c
  drivers/src/bf_qsfp/bf_sfp_ucli.c
  drivers/src/bf_qsfp/bf_qsfp_comm.c
  drivers/src/bf_qsfp/bf_sfp_comm.c
  drivers/src/bf_qsfp/sff.c
  drivers/src/bf_qsfp/dom.c
  drivers/src/bf_qsfp/map.c
  drivers/src/bf_qsfp/nonstandard.c
  drivers/src/bf_qsfp/sff_db.c
  drivers/src/bf_qsfp/sff_enums.c
  drivers/src/bf_led/bf_led.c
  drivers/src/bf_port_mgmt/bf_pm_intf.c
  drivers/src/bf_port_mgmt/bf_pm_intf_tf2.c
  drivers/src/bf_port_mgmt/bf_pm_qsfp_mgmt.c
  drivers/src/bf_port_mgmt/bf_pm_sfp_mgmt.c
  drivers/src/bf_port_mgmt/bf_pm_priv.h
  drivers/src/bf_port_mgmt/bf_pm_porting.c
)
add_library(pltfm_driver SHARED $<TARGET_OBJECTS:pltfm_driver_o>)
set_target_properties(pltfm_driver PROPERTIES VERSION 0.0.0 SOVERSION 0)
target_link_libraries(pltfm_driver ${bfsys_lib} curl usb-1.0)
add_library(pltfm_mgr SHARED
  $<TARGET_OBJECTS:pltfm_o>
  $<TARGET_OBJECTS:pltfm_driver_o>
  $<TARGET_OBJECTS:asterfusionbf_driver_o>
)
set_target_properties(pltfm_mgr PROPERTIES VERSION 0.0.0 SOVERSION 0)

#thrift
if (THRIFT-DRIVER)
  add_subdirectory(platforms/asterfusion-bf/thrift)
  target_link_libraries(pltfm_mgr PUBLIC pltfm_mgr_thrift ${bfsys_lib} curl usb-1.0)
else()
  target_link_libraries(pltfm_mgr PUBLIC ${bfsys_lib} curl usb-1.0)
endif()

add_library(asterfusion_bin_srcs_o OBJECT
  platforms/asterfusion-bf/src/bf_pltfm_master_i2c/bf_pltfm_master_i2c.c
  platforms/asterfusion-bf/src/bf_pltfm_master_i2c/bf_pltfm_cgos_i2c.c
  platforms/asterfusion-bf/src/bf_pltfm_uart/bf_pltfm_uart.c
  platforms/asterfusion-bf/src/bf_pltfm_cp2112/bf_pltfm_cp2112_intf.c
  platforms/asterfusion-bf/src/bf_pltfm_bmc_tty/bmc_tty.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_intf.c
  platforms/asterfusion-bf/src/bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.c
  drivers/src/bf_bd_cfg/bf_bd_cfg_intf.c
)

add_executable(spi_i2c_util
  platforms/asterfusion-bf/tofino_spi_util/tofino_spi_if.c
  platforms/asterfusion-bf/tofino_spi_util/tofino_porting_spi.c
  $<TARGET_OBJECTS:asterfusion_bin_srcs_o>
)
target_link_libraries(spi_i2c_util ${bfsys_lib} usb-1.0 curl m ${bfutils_lib} cgos)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/spi_i2c_util DESTINATION bin)

add_executable(cp2112_util
  platforms/asterfusion-bf/cp2112_util/cp2112_util.c
  $<TARGET_OBJECTS:asterfusion_bin_srcs_o>
)
target_link_libraries(cp2112_util ${bfsys_lib} usb-1.0 curl m ${bfutils_lib} cgos)
target_compile_options(cp2112_util PRIVATE -Wno-implicit-function-declaration -Wno-sign-compare)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/cp2112_util DESTINATION bin)

add_executable(uart_util
  platforms/asterfusion-bf/uart_util/uart_util.c
  $<TARGET_OBJECTS:asterfusion_bin_srcs_o>
)
target_link_libraries(uart_util ${bfsys_lib} usb-1.0 curl m ${bfutils_lib} cgos)
target_compile_options(uart_util PRIVATE -Wno-implicit-function-declaration -Wno-sign-compare)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/uart_util DESTINATION bin)

add_executable(syseeprom_util
  platforms/asterfusion-bf/cp2112_util/syseeprom_util.c
  platforms/asterfusion-bf/cp2112_util/cp2112_util.c
  platforms/asterfusion-bf/cp2112_util/onie_tlvinfo.c
  $<TARGET_OBJECTS:asterfusion_bin_srcs_o>
)
set_target_properties(syseeprom_util PROPERTIES COMPILE_FLAGS -DNO_MAIN)
target_link_libraries(syseeprom_util ${bfsys_lib} usb-1.0 curl m ${bfutils_lib} cgos)
target_compile_options(syseeprom_util PRIVATE -Wno-error)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/syseeprom_util DESTINATION bin)

include_directories(platforms/asterfusion-bf/diags)
if(ASTERFUSION-DIAGS)
  add_library(accton_mav_diags SHARED
    platforms/asterfusion-bf/diags/mavericks/diag_server.c
    platforms/asterfusion-bf/diags/mavericks/diag_handler.c
    platforms/asterfusion-bf/diags/mavericks/diag_ucli.c
  )
  install(FILES platforms/asterfusion-bf/diags/mavericks/swutil DESTINATION bin)
  configure_file(platforms/asterfusion-bf/diags/mavericks/targets/tofino/mav_diag.conf.in tofino/mav_diag.conf)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tofino DESTINATION share/p4/targets)
endif()
install(PROGRAMS platforms/asterfusion-bf/utils/tofino_pci_bringup.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/tofino_i2c_wr_local.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/tofino_i2c_wr.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/tofino_i2c_rd_local.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/tofino_i2c_rd.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/firmwares/credo_firmware.bin DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/xt-cfgen.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/xt-clean.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/xt-diags.sh DESTINATION bin)
install(PROGRAMS platforms/asterfusion-bf/utils/xt-setup.sh DESTINATION bin)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/src/board-maps/
  DESTINATION share/platforms/board-maps/accton
  FILES_MATCHING
  PATTERN "board_lane_map*.json"
  PATTERN "*.py" EXCLUDE
  PATTERN "*.csv" EXCLUDE
)

if (THRIFT-DRIVER)
  include(PythonDependencies)

  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/platforms/asterfusion-bf/thrift/gen-py/pltfm_pm_rpc/ DESTINATION ${SDE_PYTHON_DEPENDENCIES_DIR}/pltfm_pm_rpc)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/platforms/asterfusion-bf/thrift/gen-py/pltfm_mgr_rpc/ DESTINATION ${SDE_PYTHON_DEPENDENCIES_DIR}/pltfm_mgr_rpc)
endif()

# platform generic headers
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/include/bf_pltfm_qsfp.h DESTINATION include/bf_pltfm)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/include/bf_pltfm_sfp.h DESTINATION include/bf_pltfm)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/include/bf_pltfm_led.h DESTINATION include/bf_pltfm)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/include/bf_pltfm_bd_cfg.h DESTINATION include/bf_pltfm)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/include/bf_pltfm_ext_phy.h DESTINATION include/bf_pltfm)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/include/bf_pltfm.h DESTINATION include/bf_pltfm)

# asterfusion platform headers
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/bf_pltfm_spi.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/bf_pltfm_cp2112_intf.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/bf_pltfm_chss_mgmt_intf.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/bf_pltfm_bmc_tty.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/bf_mav_led.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/bf_pltfm_master_i2c.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/bf_pltfm_uart.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/pltfm_types.h DESTINATION include/bf_pltfm/asterfusion-bf)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/platforms/asterfusion-bf/include/version.h DESTINATION include/bf_pltfm/asterfusion-bf)

# platform driver headers
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_bd_cfg/bf_bd_cfg_intf.h DESTINATION include/bf_bd_cfg)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_bd_cfg/bf_bd_cfg_porting.h DESTINATION include/bf_bd_cfg)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_bd_cfg/bf_bd_cfg_bd_map.h DESTINATION include/bf_bd_cfg)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_pltfm_types/bf_pltfm_types.h DESTINATION include/bf_pltfm_types)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_port_mgmt/bf_port_mgmt_intf.h DESTINATION include/bf_port_mgmt)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_port_mgmt/bf_port_mgmt_porting.h DESTINATION include/bf_port_mgmt)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_pltfm_mgr/pltfm_mgr_handlers.h DESTINATION include/bf_pltfm_mgr)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_led/bf_led.h DESTINATION include/bf_led)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/bf_qsfp.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/sff.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/bf_sfp.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/sff_standards.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/map.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/dom.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/sff_db.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/8436.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/8472.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/8636.h DESTINATION include/bf_qsfp)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/include/bf_qsfp/bf_sfp.h DESTINATION include/bf_qsfp)
