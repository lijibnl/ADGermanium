# Germanium Detector: Simplified Ophyd Interface

This directory contains a simplified Ophyd interface for the Germanium detector that exposes **only** the PVs listed in `PVs-from-Phoebus.md` plus essential NDArrays for data acquisition.

## PV Mapping

All PVs from `PVs-from-Phoebus.md` are mapped to Ophyd attributes:

| Phoebus PV | Ophyd Attribute | Type | Description |
|------------|----------------|------|-------------|
| GAIN | `det.gain` | EpicsSignal | Detector gain setting |
| SHPT | `det.shpt` | EpicsSignal | Shaping time |
| MODE | `det.mode` | EpicsSignalWithRBV | Acquisition mode |
| CONT | `det.cont` | EpicsSignalWithRBV | Continuous mode |
| TP | `det.tp` | EpicsSignalWithRBV | Count time |
| TP1 | `det.tp1` | EpicsSignal | Auto count time |
| RUNNO | `det.runno` | EpicsSignalWithRBV | Run number |
| RUNNO_RBV | `det.runno.readback` | - | Run number readback |
| MFS | `det.mfs` | EpicsSignal | Multi-fire suppression |
| LOAO | `det.loao` | EpicsSignal | Leakage/Pulse mode |
| EBLK | `det.eblk` | EpicsSignal | Bias current enable |
| PUEN | `det.puen` | EpicsSignal | Pileup rejection enable |
| HV | `det.hv` | EpicsSignalWithRBV | High voltage |
| HV_RBV | `det.hv.readback` | - | High voltage readback |
| HV_CURR | `det.hv_curr` | EpicsSignalRO | HV current readback |
| TDS | `det.tds` | EpicsSignal | TDC slope |
| TDM | `det.tdm` | EpicsSignal | TDC mode |
| TPENB | `det.tpenb` | EpicsSignal | Test pulse enable |
| POL | `det.pol` | EpicsSignal | Input polarity |
| IPADDR | `det.ipaddr` | EpicsSignalWithRBV | IP address |
| IPADDR_RBV | `det.ipaddr.readback` | - | IP address readback |
| FSIZ | `det.fsiz` | EpicsSignalWithRBV | File size |
| FNAM | `det.fnam` | EpicsSignalWithRBV | File name |

## NDArray Data

Essential data arrays for spectrum and timing data:

| NDArray | Ophyd Attribute | Description |
|---------|----------------|-------------|
| MCA | `det.mca` | Multi-channel spectra data |
| TDC | `det.tdc` | Time-to-digital converter data |
| SPCT | `det.spct` | Single channel spectrum |
| INTENS | `det.intens` | Intensity per channel |

## Usage

### Basic Setup
```python
from germanium_ophyd import create_germanium_detector

# Create detector instance
det = create_germanium_detector("Det:", "germanium_det")

# Configure basic parameters
det.gain.put(2)           # GAIN: 60keV range
det.shpt.put(3)           # SHPT: 1us shaping time
det.mode.put(0)           # MODE: framing mode
det.cont.put(0)           # CONT: one-shot mode
det.tp.put(120.0)         # TP: 2 minute acquisition
det.runno.put(12345)      # RUNNO: run number
```

### MARS ASIC Configuration
```python
# Configure MARS ASIC parameters
det.mfs.put(2)            # MFS: 500ns multi-fire suppression
det.loao.put(1)           # LOAO: pulse mode
det.eblk.put(1)           # EBLK: enable bias current
det.puen.put(1)           # PUEN: enable pileup rejection
```

### High Voltage and TDC
```python
# High voltage settings
det.hv.put(3000.0)        # HV: 3000V
hv_readback = det.hv.readback.get()    # HV_RBV
current = det.hv_curr.get()            # HV_CURR

# TDC configuration
det.tds.put(1)            # TDS: TDC slope
det.tdm.put(0)            # TDM: time of arrival mode
```

### Test Pulse and Polarity
```python
# Test pulse configuration
det.tpenb.put(1)          # TPENB: enable test pulse
det.pol.put(1)            # POL: positive polarity
```

### Network and File I/O
```python
# Network configuration
det.ipaddr.put("192.168.1.100")      # IPADDR
ip_readback = det.ipaddr.readback.get()  # IPADDR_RBV

# File settings
det.fsiz.put(500*1024*1024)           # FSIZ: 500MB max file size
det.fnam.put("run_001.dat")           # FNAM: output filename
```

### Data Acquisition
```python
# Access NDArray data
spectrum_data = det.spectrum_data     # MCA data
tdc_data = det.tdc_data              # TDC data
intensity_data = det.intensity_data   # Intensity per channel

# Direct NDArray access
mca_array = det.mca                   # MCA NDArray object
tdc_array = det.tdc                   # TDC NDArray object
spct_array = det.spct                 # Single channel spectrum
intens_array = det.intens             # Intensity array
```

## Files

- `germanium_ophyd.py` - Simplified Ophyd device class with only Phoebus PVs + NDArrays
- `phoebus_pv_simple.py` - Complete demonstration of all Phoebus PVs
- `detector_staging.py` - Staging examples for different experiment types
- `validate_pv_mapping.py` - Validation script for PV mappings
- `germanium_ophyd_full.py` - Previous full implementation (backup)

## Key Features

1. **Minimal Interface**: Only exposes PVs from Phoebus + essential NDArrays
2. **Standard EPICS Format**: All PVs follow `$(P)$(R)XXX` convention
3. **Type Safety**: Proper EpicsSignal types (Signal, SignalWithRBV, SignalRO)
4. **Bluesky Ready**: Compatible with Bluesky data acquisition framework
5. **Data Access**: Direct access to MCA, TDC, and intensity data

## Example Complete Workflow

```python
from germanium_ophyd import create_germanium_detector

# Create detector
det = create_germanium_detector("Det:", "ge_det")

# Configure for gamma spectroscopy
det.gain.put(2)           # 60keV range
det.shpt.put(3)           # 1us shaping
det.mode.put(0)           # framing mode
det.puen.put(1)           # enable pileup rejection
det.hv.put(3000.0)        # 3000V bias
det.tp.put(300.0)         # 5 minute acquisition

# Set run parameters
det.runno.put(20250918)
det.fnam.put("spectroscopy_run_20250918.dat")

# Start acquisition (implementation dependent)
det.trigger()

# Monitor and get data
while det.acquiring:
    current = det.hv_curr.get()
    print(f"HV Current: {current}")
    time.sleep(1)

# Get final data
spectrum = det.spectrum_data
print(f"Acquired spectrum with {spectrum.shape} channels")
```

This simplified interface provides direct, efficient access to all the PVs used in Phoebus while maintaining the essential data acquisition capabilities through NDArrays.
