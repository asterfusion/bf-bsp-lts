## Generic

This folder contains SONiC platform plugins written in accordance with:
https://github.com/Azure/SONiC/wiki/Porting-Guide

Expected plugins folder structure on the target device:

```bash
plugins/
├── eeprom.py
├── psuutil.py
├── sfputil.py
└── pltfm_mgr_rpc
    ├── __init__.py
    ├── pltfm_mgr_rpc.py
    └── ttypes.py
```

Also, please make sure that Python Thirft library is installed in both
SONiC host environment as well as pmon container:

```bash
pip install thrift==0.11.0
```

