"""
Ophyd device definitions for Germanium detector based on areaDetector.

This module provides Ophyd device classes for the Germanium detector,
exposing only the PVs listed in PVs-from-Phoebus.md plus essential NDArrays.
"""

from ophyd import (
    Device, EpicsSignal, EpicsSignalRO, EpicsSignalWithRBV,
    Component as Cpt, ADComponent as ADCpt, AreaDetector, 
    DetectorBase, Kind
)
from ophyd.areadetector import NDArrayBase
import numpy as np
from typing import Optional


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
        return ret


class GermaniumDetector(AreaDetector):
    """
    Germanium detector device class based on areaDetector.
    
    Exposes only the PVs listed in PVs-from-Phoebus.md plus essential NDArrays.
    """
    
    # ==== PVs from PVs-from-Phoebus.md ====
    
    # Basic configuration
    gain = ADCpt(EpicsSignal, 'GAIN')                    # Detector gain setting
    shpt = ADCpt(EpicsSignal, 'SHPT')                    # Shaping time  
    mode = ADCpt(EpicsSignalWithRBV, 'MODE')             # Acquisition mode
    cont = ADCpt(EpicsSignalWithRBV, 'CONT')             # Continuous mode
    
    # Timing and run control
    tp = ADCpt(EpicsSignalWithRBV, 'TP')                 # Count time
    tp1 = ADCpt(EpicsSignal, 'TP1')                      # Auto count time
    runno = ADCpt(EpicsSignalWithRBV, 'RUNNO')           # Run number (includes RUNNO_RBV)
    
    # MARS ASIC configuration
    mfs = ADCpt(EpicsSignal, 'MFS')                      # Multi-fire suppression
    loao = ADCpt(EpicsSignal, 'LOAO')                    # Leakage/Pulse mode
    eblk = ADCpt(EpicsSignal, 'EBLK')                    # Bias current enable
    puen = ADCpt(EpicsSignal, 'PUEN')                    # Pileup rejection enable
    
    # High voltage
    hv = ADCpt(EpicsSignalWithRBV, 'HV')                 # High voltage (includes HV_RBV)
    hv_curr = ADCpt(EpicsSignalRO, 'HV_CURR')            # HV current readback
    
    # TDC configuration
    tds = ADCpt(EpicsSignal, 'TDS')                      # TDC slope
    tdm = ADCpt(EpicsSignal, 'TDM')                      # TDC mode
    
    # Test pulse
    tpenb = ADCpt(EpicsSignal, 'TPENB')                  # Test pulse enable
    pol = ADCpt(EpicsSignal, 'POL')                      # Input polarity
    
    # Network and file I/O
    ipaddr = ADCpt(EpicsSignalWithRBV, 'IPADDR')         # IP address (includes IPADDR_RBV)
    fsiz = ADCpt(EpicsSignalWithRBV, 'FSIZ')             # File size
    fnam = ADCpt(EpicsSignalWithRBV, 'FNAM')             # File name
    
    # ==== Essential NDArrays ====
    
    # Spectrum data - MCA format
    mca = ADCpt(GermaniumNDArray, 'MCA')                 # Multi-channel spectra data
    
    # Time-to-Digital Converter data  
    tdc = ADCpt(GermaniumNDArray, 'TDC')                 # TDC data
    
    # Selected channel spectrum
    spct = ADCpt(GermaniumNDArray, 'SPCT')               # Single channel spectrum
    
    # Intensity data per channel
    intens = ADCpt(GermaniumNDArray, 'INTENS')           # Intensity array
    
    def __init__(self, prefix, *, name, **kwargs):
        super().__init__(prefix, name=name, **kwargs)
        
        # Set up read attributes - what gets read during a scan
        self.read_attrs = [
            'mca', 'tdc', 'spct', 'intens',  # Data arrays
        ]
        
        # Configuration attributes - saved with metadata
        self.configuration_attrs = [
            'gain', 'shpt', 'mode', 'cont', 'tp', 'runno',  # Basic config
            'mfs', 'loao', 'eblk', 'puen',                  # MARS config
            'hv', 'tds', 'tdm', 'tpenb', 'pol',             # Hardware config
            'ipaddr', 'fsiz', 'fnam'                        # Network/file config
        ]
        
        # Set kind attributes for different use cases
        self.mca.kind = Kind.normal
        self.tdc.kind = Kind.normal
        self.spct.kind = Kind.normal
        self.intens.kind = Kind.normal
        
        # Configuration parameters
        self.gain.kind = Kind.config
        self.mode.kind = Kind.config
        self.runno.kind = Kind.config
        self.hv.kind = Kind.config
    
    def trigger(self):
        """Start acquisition"""
        # Note: Actual trigger mechanism depends on the specific PV for starting acquisition
        return super().trigger()
    
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


# Factory function for easy instantiation
def create_germanium_detector(prefix: str, name: str) -> GermaniumDetector:
    """
    Create a Germanium detector instance.
    
    Parameters
    ----------
    prefix : str
        EPICS PV prefix for the detector (e.g., "Det:")
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
    
    print(f"Germanium detector '{det.name}' created with prefix '{det.prefix}'")
    print("\nAvailable PVs from Phoebus:")
    
    # List all the Phoebus PVs
    phoebus_pvs = [
        ('gain', 'GAIN'), ('shpt', 'SHPT'), ('mode', 'MODE'), ('cont', 'CONT'),
        ('tp', 'TP'), ('tp1', 'TP1'), ('runno', 'RUNNO/RUNNO_RBV'),
        ('mfs', 'MFS'), ('loao', 'LOAO'), ('eblk', 'EBLK'), ('puen', 'PUEN'),
        ('hv', 'HV/HV_RBV'), ('hv_curr', 'HV_CURR'), ('tds', 'TDS'), ('tdm', 'TDM'),
        ('tpenb', 'TPENB'), ('pol', 'POL'), ('ipaddr', 'IPADDR/IPADDR_RBV'),
        ('fsiz', 'FSIZ'), ('fnam', 'FNAM')
    ]
    
    for attr, pv in phoebus_pvs:
        print(f"  det.{attr:10} -> {det.prefix}{pv}")
    
    print("\nAvailable NDArrays:")
    ndarrays = [
        ('mca', 'MCA - Multi-channel spectra'),
        ('tdc', 'TDC - Time-to-digital converter data'), 
        ('spct', 'SPCT - Single channel spectrum'),
        ('intens', 'INTENS - Intensity per channel')
    ]
    
    for attr, desc in ndarrays:
        print(f"  det.{attr:10} -> {desc}")
