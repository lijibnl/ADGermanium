"""
Example usage of the GermaniumDetector Ophyd device.

This script demonstrates how to create and use a GermaniumDetector device
for interacting with the ADGermanium EPICS IOC.
"""

from germanium_detector import GermaniumDetector
from ophyd import wait
import time


def main():
    """Example usage of the GermaniumDetector."""
    
    # Create a detector instance
    # Replace 'XF:28IDC-ES:1{Det:Ge1}' with your actual PV prefix
    detector = GermaniumDetector('XF:28IDC-ES:1{Det:Ge1}', name='germanium_det')
    
    # Wait for connection
    detector.wait_for_connection(timeout=5.0)
    print(f"Connected to detector: {detector.name}")
    
    # Get current status
    print("\nCurrent detector status:")
    status = detector.get_status_summary()
    for key, value in status.items():
        print(f"  {key}: {value}")
    
    # Configure the detector for a measurement
    print("\nConfiguring detector for measurement...")
    detector.configure_for_measurement(
        gain='120keV',
        shaping_time='1us',
        mode='Continuous',
        enable_pileup=True
    )
    
    # Set some test pulse parameters
    print("Setting test pulse parameters...")
    detector.test_pulse_amplitude.set(1000)
    detector.test_pulse_frequency.set(1000)  # Hz
    detector.test_pulse_count.set(100)
    
    # Enable test pulses
    detector.enable_test_pulses()
    print("Test pulses enabled")
    
    # Start acquisition
    print("Starting acquisition...")
    detector.start_acquisition()
    
    # Run for a few seconds
    time.sleep(3)
    
    # Stop acquisition
    print("Stopping acquisition...")
    detector.stop_acquisition()
    
    # Disable test pulses
    detector.disable_test_pulses()
    print("Test pulses disabled")
    
    # Get final status
    print("\nFinal detector status:")
    status = detector.get_status_summary()
    for key, value in status.items():
        print(f"  {key}: {value}")


if __name__ == "__main__":
    main()
