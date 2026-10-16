# ADGermanium

An EPICS areaDetector driver for the Germanium detector from NSLS-II.

Targeted platform: Linux x86-64.

## Protocol dependency

`GermaniumDetectorProtocol.cpp` and `GermaniumDetectorProtocol.hpp` are maintained in the separate [Germanium Communication Protocol repository](https://github.com/lijibnl/Germanium-Communication-Protocol). They are not tracked in ADGermanium; the build expects both files in `germaniumApp/src/`.

The protocol repository is currently under the `lijibnl` account and is planned for transfer to the NSLS2 organization.
