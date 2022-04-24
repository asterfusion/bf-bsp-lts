Barefoot Networks Mavericks/Montara Platforms Software
======================================================
The <bf-platforms/platforms/accton-bf/>
  Package contains source code for the drivers and agents for Barefoot Networks
  Platforms, Mavericks and Montara. It is organized as follow.


  src/

    Driver source files made up of following directories.

    bf_pltfm_chss_mgmt:
      Chassis Mgmt API (e.g. fans, temp sensors, power-mgmt, eeprom)
    bf_pltfm_cp2112:
      i2c bus access API
    bf_pltfm_led:
      System and port LED Mgmt API
    bf_pltfm_qsfp:
      QSFP access, RESET, INTERRUPT, PRESENCE and LPMODE
    bf_pltfm_rtmr:
      Port retimer Mgmt API
    bf_pltfm_rptr:
      Port repeater Mgmt API
    bf_pltfm_si5342:
      APIs for configuring clock sync device si5342
    bf_pltfm_mgr:
      Platform initialization
    bf_pltfm_slave_i2c:
      API to access Barefoot ASIC over i2c bus
    bf_pltfm_spi:
      API to access PCIe serdes EEPROM over SPI interface of Barefoot ASIC

  include/
    Header files with forward declarations of APIs exported by modules
    under src/

  thrift/
    Thrift IDL to expose APIs over thrift

  tcl_server/
    tcl server implementation

  diags/
    manufacturing data plane diagnostic test suite 

  ptf-tests/
    Example scripts that demonstrate invoking of API over thrift in python

Dependencies
============
The <bf-platforms/platforms/accton-bf> package expects a few dependencies like 
libusb and libcurl to be pre-installed before it can be successfully built. All 
the necessary packages can be installed in the following manner
can be installed in the following manner

    cd $SDE/bf-platforms/platforms/accton-bf
    ./install_pltfm_deps.sh

Artifacts installed
===================
Here're the artifacts that get installed for <bf-platforms>

Build artifacts:

    header files for driver API to  $BSP_INSTALL/include/bf_pltfm/accton-bf/

    libacctonbf_driver.[a,la,so] to $BSP_INSTALL/lib/
      driver library to manage the accton-bf platform components

Thrift mode additional build artifacts:

    libplatform_thrift.[a,la,so] to $BSP_INSTALL/lib/
      library that provides thrift API for platform API

