#========================================
# Enable test using test pulses
#========================================
dbpf $(PREFIX)AcquireTime         1
dbpf $(PREFIX)TestPulseAmplitude  10000
dbpf $(PREFIX)TestPulseFrequency  10000
dbpf $(PREFIX)TestPulseCount      999999
dbpf $(PREFIX)TsenAll             1
dbpf $(PREFIX)TestPulseEnable     0
dbpf $(PREFIX)TestPulseEnable     1
#========================================

