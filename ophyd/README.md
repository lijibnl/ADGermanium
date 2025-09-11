# ADGermanium Ophyd Interface

This directory contains Ophyd device classes for interfacing with the ADGermanium EPICS IOC.

## Installation

Make sure you have ophyd installed:

```bash
pip install ophyd
```

## Usage

### Basic Usage

```python
from germanium_detector import GermaniumDetector

# Create detector instance (replace with your PV prefix)
detector = GermaniumDetector('XF:28IDC-ES:1{Det:Ge1}', name='germanium_det')

# Wait for connection
detector.wait_for_connection()

# Configure for measurement
detector.configure_for_measurement(
    gain='120keV',
    shaping_time='1us', 
    mode='Continuous',
    enable_pileup=True
)

# Start acquisition
detector.start_acquisition()
```

### Available Parameters

The `GermaniumDetector` class provides access to all PVs defined in the Germanium.db database:

#### Configuration Parameters
- `gain`: Detector gain (240keV, 120keV, 60keV, 30keV)
- `shaping_time`: Signal shaping time (0.25us, 0.5us, 1us, 2us)  
- `mode`: Framing mode (Framing, Continuous)
- `time_detector_slope`: Time detector slope (1us to 12us)
- `tdc_mode`: TDC mode (Time of arrival, Time over threshold)
- `polarity`: Input polarity (Negative, Positive)

#### Delay Settings
- `pipeline_delay`: Pipeline delay
- `readout_delay`: Readout delay

#### Test Pulse Controls
- `test_pulse_enable`: Global test pulse enable/disable
- `test_pulse_amplitude`: Test pulse amplitude
- `test_pulse_frequency`: Test pulse frequency
- `test_pulse_count`: Test pulse count
- `test_pulse_enable_per_channel`: Per-channel test pulse enable array

#### Channel Controls
- `channel_enable`: Per-channel enable array
- `monitor_channel`: Channel for monitor output
- `global_monitor`: Monitor type selection
- `leakage_or_pulse`: Monitor mode (leakage/pulse)

#### Acquisition Controls
- `acquisition_control`: Start/stop acquisition
- `multi_fire_suppression`: Multi-fire suppression timing
- `pileup_rejection_enable`: Enable/disable pileup rejection

#### Threshold Arrays
- `threshold_trim`: Trim DAC values array
- `pileup_threshold_trim`: Pileup threshold trim array  
- `threshold`: Threshold values array

### Convenience Methods

The class provides several convenience methods:

- `start_acquisition()` / `stop_acquisition()`
- `enable_test_pulses()` / `disable_test_pulses()`
- `set_gain(value)` - accepts numeric or string values
- `set_shaping_time(value)` - accepts numeric or string values
- `set_mode(value)` - accepts numeric or string values
- `configure_for_measurement()` - one-step configuration
- `get_status_summary()` - get current settings summary

### Example

See `example_usage.py` for a complete example showing how to:
- Connect to the detector
- Configure parameters
- Enable test pulses
- Start/stop acquisition
- Monitor status

## PV Naming Convention

The detector expects PVs with the prefix you provide, followed by the parameter name:
- `$(PREFIX)GAIN` - Gain setting
- `$(PREFIX)SHPT` - Shaping time
- `$(PREFIX)MODE` - Framing mode
- etc.

Make sure your EPICS IOC is running with the correct PV prefix before using this interface.

## Notes

- All array-type parameters (like `channel_enable`, `threshold_trim`) expect the appropriate array sizes as defined in your IOC configuration
- The detector supports up to 16384 channels based on the waveform record definitions
- Time values in parameter names refer to the actual timing settings, not array indices
