"""
Ophyd device for ADGermanium detector.

This module provides an Ophyd interface to the EPICS ADGermanium detector driver.
The GermaniumDetector class provides access to all the PVs defined in the 
Germanium.db database.
"""

from ophyd import Component as Cpt, Device, EpicsSignal


class GermaniumDetector(Device):
    """
    Ophyd device for ADGermanium detector.
    
    This device provides access to all configuration and control parameters
    for the Germanium detector from NSLS-II.
    
    Parameters
    ----------
    prefix : str
        The PV prefix for the detector (e.g., 'XF:28IDC-ES:1{Det:Ge1}')
    name : str
        A human-readable name for this detector
    """
    
    # Gain settings
    gain = Cpt(EpicsSignal, 'GAIN', 
               doc="Set gain (0=240keV, 1=120keV, 2=60keV, 3=30keV)")
    
    # Shaping time settings  
    shaping_time = Cpt(EpicsSignal, 'SHPT',
                      doc="Set shaping time (1=0.25us, 2=0.5us, 3=1us, 4=2us)")
    
    # Framing mode
    mode = Cpt(EpicsSignal, 'MODE',
              doc="Set framing mode (0=Framing, 1=Continuous)")
    
    # Time detector slope
    time_detector_slope = Cpt(EpicsSignal, 'TDS',
                             doc="Set time detector slope (0=1us, 1=2us, 2=3us, 3=4us, 4=6us, 5=9us, 6=12us)")
    
    # TDC mode
    tdc_mode = Cpt(EpicsSignal, 'TDM',
                  doc="Set TDC mode (0=Time of arrival, 1=Time over threshold)")
    
    # Pipeline and readout delays
    pipeline_delay = Cpt(EpicsSignal, 'PLDEL',
                        doc="Set pipeline delay")
    
    readout_delay = Cpt(EpicsSignal, 'RODEL',
                       doc="Set readout delay")
    
    # Test pulse controls
    test_pulse_enable = Cpt(EpicsSignal, 'TPENB',
                           doc="Enable test pulses (0=Off, 1=On)")
    
    test_pulse_amplitude = Cpt(EpicsSignal, 'TPAMP',
                              doc="Set test pulse amplitude")
    
    test_pulse_frequency = Cpt(EpicsSignal, 'TPFRQ',
                              doc="Set test pulse frequency")
    
    test_pulse_count = Cpt(EpicsSignal, 'TPCNT',
                          doc="Set test pulse count")
    
    # Input polarity
    polarity = Cpt(EpicsSignal, 'POL',
                  doc="Set input polarity (0=Negative, 1=Positive)")
    
    # Channel controls
    channel_enable = Cpt(EpicsSignal, 'CHEN',
                        doc="Channel enable array (per channel)")
    
    test_pulse_enable_per_channel = Cpt(EpicsSignal, 'TPEN',
                                       doc="Test pulse enable array (per channel)")
    
    # Monitor controls
    global_monitor = Cpt(EpicsSignal, 'GMON',
                        doc="Switch for channel monitors (0=Off, 1=Temperature, 2=Baseline, 3=Threshold, 4=Test pulse, 5=Channel monitor)")
    
    monitor_channel = Cpt(EpicsSignal, 'MONCH',
                         doc="Set channel which has monitor out enabled")
    
    leakage_or_pulse = Cpt(EpicsSignal, 'LOAO',
                          doc="Set channel monitor to leakage or pulse (0=Leakage, 1=Pulse)")
    
    # Acquisition control
    acquisition_control = Cpt(EpicsSignal, 'CNT',
                             doc="Acquisition control (0=Stop, 1=Start)")
    
    # Multi-fire suppression
    multi_fire_suppression = Cpt(EpicsSignal, 'MFS',
                                doc="Multi-fire suppression (0=Off, 1=250ns, 2=500ns, 3=1us, 4=2us)")
    
    # Pileup rejection
    pileup_rejection_enable = Cpt(EpicsSignal, 'PUEN',
                                 doc="Pileup rejection enable (0=Disable, 1=Enable)")
    
    # Threshold and trim arrays
    threshold_trim = Cpt(EpicsSignal, 'THTR',
                        doc="Array of NCHAN trim DAC values")
    
    pileup_threshold_trim = Cpt(EpicsSignal, 'PUTR',
                               doc="Array of NCHAN pileup threshold trim values")
    
    threshold = Cpt(EpicsSignal, 'THRSH',
                   doc="Threshold array")
    
    def __init__(self, prefix, *, name, **kwargs):
        """
        Initialize the GermaniumDetector.
        
        Parameters
        ----------
        prefix : str
            The PV prefix for the detector
        name : str
            A human-readable name for this detector
        **kwargs
            Additional keyword arguments passed to the parent class
        """
        super().__init__(prefix, name=name, **kwargs)
    
    def start_acquisition(self):
        """Start data acquisition."""
        self.acquisition_control.set(1)
    
    def stop_acquisition(self):
        """Stop data acquisition."""
        self.acquisition_control.set(0)
    
    def enable_test_pulses(self):
        """Enable test pulses globally."""
        self.test_pulse_enable.set(1)
    
    def disable_test_pulses(self):
        """Disable test pulses globally."""
        self.test_pulse_enable.set(0)
    
    def set_gain(self, gain_value):
        """
        Set the detector gain.
        
        Parameters
        ----------
        gain_value : int or str
            Gain setting: 0/'240keV', 1/'120keV', 2/'60keV', 3/'30keV'
        """
        if isinstance(gain_value, str):
            gain_map = {'240keV': 0, '120keV': 1, '60keV': 2, '30keV': 3}
            if gain_value not in gain_map:
                raise ValueError(f"Invalid gain value: {gain_value}. Must be one of {list(gain_map.keys())}")
            gain_value = gain_map[gain_value]
        
        if gain_value not in [0, 1, 2, 3]:
            raise ValueError("Gain value must be 0, 1, 2, or 3")
        
        self.gain.set(gain_value)
    
    def set_shaping_time(self, shaping_time_value):
        """
        Set the shaping time.
        
        Parameters
        ----------
        shaping_time_value : int or str
            Shaping time: 1/'0.25us', 2/'0.5us', 3/'1us', 4/'2us'
        """
        if isinstance(shaping_time_value, str):
            time_map = {'0.25us': 1, '0.5us': 2, '1us': 3, '2us': 4}
            if shaping_time_value not in time_map:
                raise ValueError(f"Invalid shaping time: {shaping_time_value}. Must be one of {list(time_map.keys())}")
            shaping_time_value = time_map[shaping_time_value]
        
        if shaping_time_value not in [1, 2, 3, 4]:
            raise ValueError("Shaping time value must be 1, 2, 3, or 4")
        
        self.shaping_time.set(shaping_time_value)
    
    def set_mode(self, mode_value):
        """
        Set the framing mode.
        
        Parameters
        ----------
        mode_value : int or str
            Mode: 0/'Framing', 1/'Continuous'
        """
        if isinstance(mode_value, str):
            mode_map = {'Framing': 0, 'Continuous': 1}
            if mode_value not in mode_map:
                raise ValueError(f"Invalid mode: {mode_value}. Must be one of {list(mode_map.keys())}")
            mode_value = mode_map[mode_value]
        
        if mode_value not in [0, 1]:
            raise ValueError("Mode value must be 0 or 1")
        
        self.mode.set(mode_value)
    
    def enable_pileup_rejection(self):
        """Enable pileup rejection."""
        self.pileup_rejection_enable.set(1)
    
    def disable_pileup_rejection(self):
        """Disable pileup rejection."""
        self.pileup_rejection_enable.set(0)
    
    def configure_for_measurement(self, gain='120keV', shaping_time='1us', 
                                 mode='Continuous', enable_pileup=True):
        """
        Configure the detector for a typical measurement.
        
        Parameters
        ----------
        gain : str, optional
            Gain setting, default '120keV'
        shaping_time : str, optional
            Shaping time, default '1us'  
        mode : str, optional
            Framing mode, default 'Continuous'
        enable_pileup : bool, optional
            Enable pileup rejection, default True
        """
        self.set_gain(gain)
        self.set_shaping_time(shaping_time)
        self.set_mode(mode)
        
        if enable_pileup:
            self.enable_pileup_rejection()
        else:
            self.disable_pileup_rejection()
    
    def get_status_summary(self):
        """
        Get a summary of the current detector configuration.
        
        Returns
        -------
        dict
            Dictionary containing current settings
        """
        gain_map = {0: '240keV', 1: '120keV', 2: '60keV', 3: '30keV'}
        shaping_map = {1: '0.25us', 2: '0.5us', 3: '1us', 4: '2us'}
        mode_map = {0: 'Framing', 1: 'Continuous'}
        
        return {
            'gain': gain_map.get(self.gain.get(), 'Unknown'),
            'shaping_time': shaping_map.get(self.shaping_time.get(), 'Unknown'),
            'mode': mode_map.get(self.mode.get(), 'Unknown'),
            'acquisition_active': bool(self.acquisition_control.get()),
            'test_pulses_enabled': bool(self.test_pulse_enable.get()),
            'pileup_rejection_enabled': bool(self.pileup_rejection_enable.get()),
            'polarity': 'Positive' if self.polarity.get() else 'Negative'
        }
