"""
Ophyd device definitions for Germanium detector based on areaDetector.

This module provides Ophyd device classes f    hv = ADCpt(EpicsSignalWithRBV, 'HV')      # High voltage
    hv_cur = ADCpt(EpicsSignalRO, 'HV_CURR')  # HV current
    temp1 = ADCpt(EpicsSignalRO, 'Temp1')     # Temperature 1the Germanium detector,
including both control PVs and NDArray data components.
"""

from ophyd import (
    Device, EpicsSignal, EpicsSignalRO, EpicsSignalWithRBV,
    Component as Cpt, DynamicDeviceComponent as DDC,
    ADComponent as ADCpt, AreaDetector, ImagePlugin,
    DetectorBase, Kind, FormattedComponent as FCpt
)
from ophyd.areadetector import NDArrayBase
from ophyd.areadetector.plugins import PluginBase
import numpy as np
from typing import Optional


class GermaniumSpectrumPlugin(PluginBase):
    """Plugin for Germanium spectrum data (MCA, TDC, etc.)"""
    _default_suffix = 'Spectrum1:'
    _suffix_re = r'Spectrum\d+:'
    _html_docs = ['NDPluginSpectrum.html']
    _plugin_type = 'NDPluginSpectrum'

    # Spectrum data arrays
    spectrum_data = ADCpt(EpicsSignal, 'ArrayData')
    spectrum_size = ADCpt(EpicsSignalRO, 'ArraySize_RBV')
    spectrum_counter = ADCpt(EpicsSignalRO, 'ArrayCounter_RBV')


class GermaniumNDArray(NDArrayBase):
    """Custom NDArray component for Germanium detector data"""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._array_data = None
    
    def get_array(self):
        """Get the current array data"""
        return self._array_data
    
    def read(self):
        """Read the current array data and metadata"""
        ret = super().read()
        # Add custom read logic if needed
        return ret


class GermaniumDetector(AreaDetector):
    """
    Germanium detector device class based on areaDetector.
    
    This class provides access to both control PVs and data arrays
    for the Germanium detector system.
    """
    
    # ==== Control PVs - Config ====
    
    # Acquisition modes
    mode = ADCpt(EpicsSignalWithRBV, 'MODE')  # 0: Framing, 1: Continuous
    cont = ADCpt(EpicsSignalWithRBV, 'CONT')  # 0: OneShot, 1: AutoCount
    
    # MARS ASIC configuration
    eblk = ADCpt(EpicsSignal, 'EBLK')         # Bias current: 1: 2pA
    gain = ADCpt(EpicsSignal, 'GAIN')         # 0: 240keV, 1: 120keV, 2: 60keV, 3: 30keV
    loao = ADCpt(EpicsSignal, 'LOAO')         # 0: Leakage, 1: Pulse
    mfs = ADCpt(EpicsSignal, 'MFS')           # Multi-fire suppression
    puen = ADCpt(EpicsSignal, 'PUEN')         # Pileup rejection enable
    shpt = ADCpt(EpicsSignal, 'SHPT')         # Shaping time
    tds = ADCpt(EpicsSignal, 'TDS')           # Time detector slope
    tdm = ADCpt(EpicsSignal, 'TDM')           # TDC mode
    
    # Delays and timing
    pldel = ADCpt(EpicsSignal, 'PLDEL')       # Pipeline delay
    rodel = ADCpt(EpicsSignal, 'RODEL')       # Readout delay
    
    # Calibration
    slp = ADCpt(EpicsSignal, 'SLP')           # Energy calibration slope
    offs = ADCpt(EpicsSignal, 'OFFS')         # Energy calibration offset
    
    # Arrays - per channel/chip settings
    chen = ADCpt(EpicsSignal, 'CHEN')         # Channel enable array
    putr = ADCpt(EpicsSignal, 'PUTR')         # Pileup threshold trim array
    thrsh = ADCpt(EpicsSignal, 'THRSH')       # Threshold array (per chip)
    thtr = ADCpt(EpicsSignal, 'THTR')        # Trim DAC values array
    tsen = ADCpt(EpicsSignal, 'TSEN')         # Test pulse enable array
    
    # ==== Control PVs - Test Pulse ====
    
    chen_set = ADCpt(EpicsSignal, 'CHEN_SET') # Channel enable control
    pol = ADCpt(EpicsSignal, 'POL')           # Input polarity
    tpamp = ADCpt(EpicsSignal, 'TPAMP')       # Test pulse amplitude
    tpcnt = ADCpt(EpicsSignal, 'TPCNT')       # Test pulse count
    tsen_set = ADCpt(EpicsSignal, 'TSEN_SET') # Test pulse enable control
    tpenb = ADCpt(EpicsSignal, 'TPENB')       # Test pulse enable
    tpfrq = ADCpt(EpicsSignal, 'TPFRQ')       # Test pulse frequency
    
    # ==== Control PVs - UDP ====
    
    ipaddr = ADCpt(EpicsSignalWithRBV, 'IPADDR')  # IP address
    
    # ==== Control PVs - File I/O ====
    
    fsiz = ADCpt(EpicsSignalWithRBV, 'FSIZ')     # File size
    fnam = ADCpt(EpicsSignalWithRBV, 'FNAM')     # File name
    
    # ==== Control PVs - Count ====
    
    cnt = ADCpt(EpicsSignalWithRBV, 'CNT')    # Acquisition control
    gmon = ADCpt(EpicsSignal, 'GMON')         # Global monitor mode
    runno = ADCpt(EpicsSignalWithRBV, 'RUNNO') # Run number
    monch = ADCpt(EpicsSignal, 'MONCH')       # Monitor channel
    tp = ADCpt(EpicsSignalWithRBV, 'TP')      # Count time
    tp1 = ADCpt(EpicsSignal, 'TP1')           # Auto count time
    
    # ==== Control PVs - Environment ====
    
    hv = ADCpt(EpicsSignalWithRBV, 'HV')      # High voltage
    hv_cur = ADCpt(EpicsSignalRO, 'HV_CUR')   # HV current
    temp1 = ADCpt(EpicsSignalRO, 'Temp1')     # Temperature 1
    temp2 = ADCpt(EpicsSignalRO, 'Temp2')     # Temperature 2
    temp3 = ADCpt(EpicsSignalRO, 'Temp3')     # Temperature 3
    ztmp = ADCpt(EpicsSignalRO, 'ztmp')       # Zynq temperature
    
    # ==== Data Arrays (NDArrays) ====
    
    # Spectrum data - MCA format
    mca = ADCpt(GermaniumNDArray, 'MCA')      # Spectra data: $(NELM)*4096
    
    # Time-to-Digital Converter data
    tdc = ADCpt(GermaniumNDArray, 'TDC')      # TDC data: $(NELM)*1024
    
    # Selected channel spectrum
    spct = ADCpt(GermaniumNDArray, 'SPCT')    # Single channel spectrum: 1*4096
    
    # Intensity data per channel
    intens = ADCpt(GermaniumNDArray, 'INTENS') # Intensity array: 1*$(NELM)
    
    # ==== Standard areaDetector plugins ====
    
    # Image plugin for 2D visualization
    image = ADCpt(ImagePlugin, 'image1:')
    
    # Custom spectrum plugin
    spectrum = ADCpt(GermaniumSpectrumPlugin, 'spectrum1:', 
                    read_attrs=['spectrum_data', 'spectrum_counter'])
    
    # ==== Additional properties ====
    
    def __init__(self, prefix, *, name, **kwargs):
        super().__init__(prefix, name=name, **kwargs)
        
        # Set up read attributes - what gets read during a scan
        self.read_attrs = [
            'mca', 'tdc', 'spct', 'intens',  # Data arrays
            'cnt', 'runno', 'tp',             # Status info
            'temp1', 'temp2', 'temp3', 'ztmp' # Environment
        ]
        
        # Configuration attributes - saved with metadata
        self.configuration_attrs = [
            'mode', 'cont', 'gain', 'shpt', 'puen', 'mfs',  # Detector config
            'tds', 'tdm', 'pldel', 'rodel',                 # Timing
            'slp', 'offs',                                   # Calibration
            'ipaddr', 'hv', 'monch',                        # System config
            'fsiz', 'fnam'                                   # File I/O config
        ]
        
        # Set kind attributes for different use cases
        self.mca.kind = Kind.normal
        self.tdc.kind = Kind.normal
        self.spct.kind = Kind.normal
        self.intens.kind = Kind.normal
        
        # Status and configuration
        self.cnt.kind = Kind.normal
        self.runno.kind = Kind.config
        self.mode.kind = Kind.config
        self.gain.kind = Kind.config
        
        # Environment monitoring
        self.temp1.kind = Kind.normal
        self.temp2.kind = Kind.normal
        self.temp3.kind = Kind.normal
        self.hv.kind = Kind.config
    
    def stage(self):
        """Stage the detector for acquisition"""
        # Set up acquisition parameters
        super().stage()
        
        # Custom staging logic if needed
        return [self]
    
    def unstage(self):
        """Unstage the detector after acquisition"""
        super().unstage()
        return [self]
    
    def trigger(self):
        """Trigger acquisition"""
        # Start acquisition
        return self.cnt.set(1)
    
    def stop(self):
        """Stop acquisition"""
        return self.cnt.set(0)
    
    @property
    def spectrum_data(self):
        """Get the current MCA spectrum data"""
        return self.mca.get_array()
    
    @property
    def tdc_data(self):
        """Get the current TDC data"""
        return self.tdc.get_array()
    
    @property
    def intensity_data(self):
        """Get the current intensity data per channel"""
        return self.intens.get_array()
    
    def get_channel_spectrum(self, channel: Optional[int] = None):
        """
        Get spectrum for a specific channel or the currently selected one.
        
        Parameters
        ----------
        channel : int, optional
            Channel number. If None, uses currently selected monitor channel.
            
        Returns
        -------
        numpy.ndarray
            Spectrum data for the specified channel
        """
        if channel is not None:
            # Set monitor channel and get spectrum
            self.monch.put(channel)
        
        return self.spct.get_array()
    
    def set_acquisition_time(self, time_seconds: float):
        """Set acquisition time in seconds"""
        return self.tp.set(time_seconds)
    
    def set_gain(self, gain_setting: int):
        """
        Set gain setting.
        
        Parameters
        ----------
        gain_setting : int
            0: 240keV, 1: 120keV, 2: 60keV, 3: 30keV
        """
        if gain_setting not in [0, 1, 2, 3]:
            raise ValueError("Gain setting must be 0, 1, 2, or 3")
        return self.gain.set(gain_setting)
    
    def set_shaping_time(self, shpt_setting: int):
        """
        Set shaping time.
        
        Parameters
        ----------
        shpt_setting : int
            1: 0.25us, 2: 0.5us, 3: 1us, 4: 2us
        """
        if shpt_setting not in [1, 2, 3, 4]:
            raise ValueError("Shaping time must be 1, 2, 3, or 4")
        return self.shpt.set(shpt_setting)
    
    def set_multi_fire_suppression(self, mfs_setting: int):
        """
        Set multi-fire suppression.
        
        Parameters
        ----------
        mfs_setting : int
            0: Off, 1: 250ns, 2: 500ns, 3: 1us, 4: 2us
        """
        if mfs_setting not in [0, 1, 2, 3, 4]:
            raise ValueError("MFS setting must be 0, 1, 2, 3, or 4")
        return self.mfs.set(mfs_setting)
    
    def set_tdc_slope(self, tds_setting: int):
        """
        Set time detector slope.
        
        Parameters
        ----------
        tds_setting : int
            0: 1us, 1: 2us, 2: 3us, 3: 4us, 4: 6us, 5: 9us, 6: 12us
        """
        if tds_setting not in range(0, 7):
            raise ValueError("TDS setting must be 0-6")
        return self.tds.set(tds_setting)
    
    def set_tdc_mode(self, tdm_setting: int):
        """
        Set TDC mode.
        
        Parameters
        ----------
        tdm_setting : int
            0: Time of arrival, 1: Time over threshold
        """
        if tdm_setting not in [0, 1]:
            raise ValueError("TDM setting must be 0 or 1")
        return self.tdm.set(tdm_setting)
    
    def set_channel_monitor_mode(self, loao_setting: int):
        """
        Set channel monitor to leakage or pulse.
        
        Parameters
        ----------
        loao_setting : int
            0: Leakage, 1: Pulse
        """
        if loao_setting not in [0, 1]:
            raise ValueError("LOAO setting must be 0 or 1")
        return self.loao.set(loao_setting)
    
    def set_global_monitor(self, gmon_setting: int):
        """
        Set global monitor mode.
        
        Parameters
        ----------
        gmon_setting : int
            0: Off, 1: Temperature, 2: Baseline, 3: Threshold, 
            4: Test pulse, 5: Channel monitor
        """
        if gmon_setting not in range(0, 6):
            raise ValueError("GMON setting must be 0-5")
        return self.gmon.set(gmon_setting)
    
    def enable_pileup_rejection(self, enable: bool = True):
        """
        Enable or disable pileup rejection.
        
        Parameters
        ----------
        enable : bool
            True to enable, False to disable
        """
        return self.puen.set(1 if enable else 0)
    
    def enable_bias_current(self, enable: bool = True):
        """
        Enable or disable on-chip bias current generator.
        
        Parameters
        ----------
        enable : bool
            True to enable (2pA), False to disable
        """
        return self.eblk.set(1 if enable else 0)
    
    def set_input_polarity(self, positive: bool = True):
        """
        Set input polarity.
        
        Parameters
        ----------
        positive : bool
            True for positive, False for negative
        """
        return self.pol.set(1 if positive else 0)
    
    def set_acquisition_mode(self, continuous: bool = False):
        """
        Set acquisition mode.
        
        Parameters
        ----------
        continuous : bool
            True for continuous, False for framing
        """
        return self.mode.set(1 if continuous else 0)
    
    def set_count_mode(self, auto_count: bool = False):
        """
        Set count mode.
        
        Parameters
        ----------
        auto_count : bool
            True for AutoCount, False for OneShot
        """
        return self.cont.set(1 if auto_count else 0)
    
    def set_monitor_channel(self, channel: int):
        """
        Set which channel has monitor output enabled.
        
        Parameters
        ----------
        channel : int
            Channel number to monitor
        """
        return self.monch.set(channel)
    
    def set_pipeline_delay(self, delay: int):
        """
        Set MARS pipeline delay.
        
        Parameters
        ----------
        delay : int
            Pipeline delay value
        """
        return self.pldel.set(delay)
    
    def set_readout_delay(self, delay: int):
        """
        Set readout delay.
        
        Parameters
        ----------
        delay : int
            Readout delay value
        """
        return self.rodel.set(delay)
    
    def set_high_voltage(self, voltage: float):
        """
        Set high voltage.
        
        Parameters
        ----------
        voltage : float
            High voltage value
        """
        return self.hv.set(voltage)
    
    def set_run_number(self, run_number: int):
        """
        Set run number.
        
        Parameters
        ----------
        run_number : int
            Run number for data collection
        """
        return self.runno.set(run_number)
    
    def configure_channels(self, enabled_channels: list = None, 
                          test_pulse_channels: list = None):
        """
        Configure channel enable and test pulse settings.
        
        Parameters
        ----------
        enabled_channels : list, optional
            List of channel numbers to enable. If None, no change.
        test_pulse_channels : list, optional
            List of channel numbers to enable test pulse. If None, no change.
        """
        if enabled_channels is not None:
            # This would need array handling - implementation depends on 
            # how the waveform records are set up
            pass  # TODO: Implement array setting
            
        if test_pulse_channels is not None:
            # This would need array handling
            pass  # TODO: Implement array setting
    
    def quick_enable_all_channels(self):
        """Quick command to enable all channels."""
        return self.chen_set.set(1)  # 1: enable all channels
    
    def quick_disable_all_channels(self):
        """Quick command to disable all channels."""
        return self.chen_set.set(3)  # 3: disable all channels
    
    def quick_enable_channel(self, channel: int):
        """
        Quick command to enable a specific channel.
        
        Parameters
        ----------
        channel : int
            Channel number to enable
        """
        self.set_monitor_channel(channel)
        return self.chen_set.set(0)  # 0: enable channel $(MONCH)
    
    def quick_disable_channel(self, channel: int):
        """
        Quick command to disable a specific channel.
        
        Parameters
        ----------
        channel : int
            Channel number to disable
        """
        self.set_monitor_channel(channel)
        return self.chen_set.set(2)  # 2: disable channel $(MONCH)
    
    def configure_detector(self, gain: int = 1, shaping_time: int = 2,
                          pileup_rejection: bool = True, 
                          multi_fire_suppression: int = 1,
                          tdc_mode: int = 0, bias_current: bool = True,
                          positive_polarity: bool = True):
        """
        Configure detector with common settings.
        
        Parameters
        ----------
        gain : int, default 1
            Gain setting (0: 240keV, 1: 120keV, 2: 60keV, 3: 30keV)
        shaping_time : int, default 2
            Shaping time (1: 0.25us, 2: 0.5us, 3: 1us, 4: 2us)
        pileup_rejection : bool, default True
            Enable pileup rejection
        multi_fire_suppression : int, default 1
            MFS setting (0: Off, 1: 250ns, 2: 500ns, 3: 1us, 4: 2us)
        tdc_mode : int, default 0
            TDC mode (0: Time of arrival, 1: Time over threshold)
        bias_current : bool, default True
            Enable bias current generator
        positive_polarity : bool, default True
            Input polarity
        """
        self.set_gain(gain)
        self.set_shaping_time(shaping_time)
        self.enable_pileup_rejection(pileup_rejection)
        self.set_multi_fire_suppression(multi_fire_suppression)
        self.set_tdc_mode(tdc_mode)
        self.enable_bias_current(bias_current)
        self.set_input_polarity(positive_polarity)
        
        print(f"Detector {self.name} configured:")
        print(f"  Gain: {gain} ({'240keV' if gain==0 else '120keV' if gain==1 else '60keV' if gain==2 else '30keV'})")
        print(f"  Shaping time: {shaping_time} ({'0.25us' if shaping_time==1 else '0.5us' if shaping_time==2 else '1us' if shaping_time==3 else '2us'})")
        print(f"  Pileup rejection: {'Enabled' if pileup_rejection else 'Disabled'}")
        print(f"  Multi-fire suppression: {multi_fire_suppression}")
        print(f"  TDC mode: {'Time of arrival' if tdc_mode==0 else 'Time over threshold'}")
        print(f"  Bias current: {'Enabled' if bias_current else 'Disabled'}")
        print(f"  Polarity: {'Positive' if positive_polarity else 'Negative'}")
    
    def get_detector_status(self):
        """
        Get current detector configuration and status.
        
        Returns
        -------
        dict
            Dictionary containing current detector settings
        """
        status = {
            'acquisition_active': bool(self.cnt.get()),
            'gain': self.gain.get(),
            'shaping_time': self.shpt.get(),
            'pileup_rejection': bool(self.puen.get()),
            'multi_fire_suppression': self.mfs.get(),
            'tdc_mode': self.tdm.get(),
            'bias_current': bool(self.eblk.get()),
            'polarity': bool(self.pol.get()),
            'acquisition_time': self.tp.get(),
            'run_number': self.runno.get(),
            'high_voltage': self.hv.get(),
            'monitor_channel': self.monch.get(),
            'temperatures': {
                'temp1': self.temp1.get(),
                'temp2': self.temp2.get(),
                'temp3': self.temp3.get(),
                'zynq': self.ztmp.get()
            },
            'ip_address': self.ipaddr.get()
        }
        return status
    
    def enable_test_pulse(self, amplitude: int, frequency: int, enable: bool = True):
        """
        Configure and enable/disable test pulse.
        
        Parameters
        ----------
        amplitude : int
            Test pulse amplitude
        frequency : int
            Test pulse frequency
        enable : bool
            Enable or disable test pulse
        """
        self.tpamp.put(amplitude)
        self.tpfrq.put(frequency)
        self.tpenb.put(1 if enable else 0)
    
    def set_file_name(self, filename: str):
        """
        Set the output filename for data acquisition.
        
        Parameters
        ----------
        filename : str
            Name of the output file
        """
        return self.fnam.set(filename)
    
    def set_file_size(self, size: int):
        """
        Set the maximum file size for data acquisition.
        
        Parameters
        ----------
        size : int
            Maximum file size in bytes
        """
        return self.fsiz.set(size)
    
    def get_file_info(self):
        """
        Get current file configuration.
        
        Returns
        -------
        dict
            Dictionary containing filename and file size settings
        """
        return {
            'filename': self.fnam.get(),
            'file_size': self.fsiz.get()
        }


# Factory function for easy instantiation
def create_germanium_detector(prefix: str, name: str) -> GermaniumDetector:
    """
    Create a Germanium detector instance.
    
    Parameters
    ----------
    prefix : str
        EPICS PV prefix for the detector
    name : str
        Name for the detector instance
        
    Returns
    -------
    GermaniumDetector
        Configured detector instance
    """
    return GermaniumDetector(prefix, name=name)


# Example usage
if __name__ == "__main__":
    # Create detector instance
    det = create_germanium_detector("Det:", "germanium_det")
    
    # Quick configuration with common settings
    det.configure_detector(
        gain=1,                      # 120keV range
        shaping_time=2,              # 0.5us
        pileup_rejection=True,       # Enable pileup rejection
        multi_fire_suppression=2,    # 500ns
        tdc_mode=0,                  # Time of arrival
        bias_current=True,           # Enable bias current
        positive_polarity=True       # Positive polarity
    )
    
    # Individual settings
    det.set_acquisition_time(10.0)           # 10 seconds
    det.set_high_voltage(2000.0)             # 2000V
    det.set_monitor_channel(5)               # Monitor channel 5
    det.set_global_monitor(5)                # Channel monitor mode
    det.set_run_number(1001)                 # Run number
    
    # Channel management
    det.quick_enable_all_channels()          # Enable all channels
    det.quick_disable_channel(10)            # Disable specific channel
    
    # Test pulse configuration
    det.enable_test_pulse(
        amplitude=100, 
        frequency=1000, 
        enable=True
    )
    
    # Start acquisition
    det.trigger()
    
    # Get detector status
    status = det.get_detector_status()
    print("\nDetector Status:")
    for key, value in status.items():
        if key == 'temperatures':
            print(f"  {key}:")
            for temp_name, temp_val in value.items():
                print(f"    {temp_name}: {temp_val}°C")
        else:
            print(f"  {key}: {value}")
    
    # Get data
    spectrum = det.spectrum_data
    tdc_data = det.tdc_data
    
    print(f"\nData acquisition ready for {det.name}")
    print(f"Spectrum shape: {spectrum.shape if spectrum is not None else 'No data'}")
    print(f"TDC data shape: {tdc_data.shape if tdc_data is not None else 'No data'}")
