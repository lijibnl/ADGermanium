# Build ADGermanium module and IOC

ADGermanium relies on the following EPICS modules:

- ADCore
- ASYN
- BUSY
- CALC
- SSCAN

ADCore must be built with the following support (in `CONFIG_SITE`):

```
# HDF5
WITH_HDF5=YES
HDF5_EXTERNAL=YES
HDF5=/usr
HDF5_LIB=/usr/lib64
HDF5_INCLUDE=/usr/include

# Other file types
WITH_TIFF=YES
WITH_JPEG=YES
WITH_NETCDF=NO
WITH_NEXUS=NO

# PVA
WITH_PVA=YES

# RHEL9 might install libxml2 in a directory different from 
# where areaDetector expects.
USR_CXXFLAGS += -I/usr/include/libxml2
USR_CFLAGS += -I/usr/include/libxml2
```

