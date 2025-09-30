# Germanium PV Definitions

# 1. PVs

**Doesn't include (most) RBVs yet**.

-  Data

PV | Type | Addr | Value | Note
:-:|:-:|:-:|:-|:-
MCA    | NDArray | 1 | `$(NELM)*4096`| Spectra.
TDC    | NDArray | 1 | `$(NELM)*1024` | 
SPCT   | NDArray | 1 | `1*4096` | 
INTENS | NDArray | 1 | `1*$(NELM)` | 

- ## Config

PV | TYPE | OP | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-:|:-|:-
MODE | bo   | Register | 0 | 0: Framing<br>1: Continuous | Set framing mode.
MODE_RBV | bi   | Register | 0 | 0: Framing<br>1: Continuous | Framing mode.
CONT | bo   | ? |0 | 0: OneShot<br>1: AutoCount | Set count mode.
CONT_RBV | bi   | ? |0 | 0: OneShot<br>1: AutoCount | Count mode.
EBLK  | mbbo | MARS | 0 | 0: Off<br>1: 2pA<br>2: 8pA | Enable on-chip bias current generator.
GAIN | mbbo | MARS | 0 | 0: 240keV<br>1: 120keV<br>2: 60keV<br>3: 30keV | Set gain.
<sameas EBLK???>LEAK | mbbo | ? | 0 | 0: Off<br>1: 2pA<br>2: 8pA | PV doesn't exist?
LOAO  | bo   | MARS | 0 | 0: Leakage<br>1: Pulse | Set channel monitor to leakage or pulse.
MFS   | mbbo | MARS | 0 | 0: Off<br>1: 250ns<br>2: 500ns<br>3: 1us<br>4: 2us | Multi-fire suppression.
OFFS | | No | 0 | |  Energy calibration offset. Used in spctx calculation.
PLDEL | longout | No | 0 | | Set pipeline delay.
PUEN  | bo | MARS | 0 | 0: Disable<br>1: Enable | Pileup Rejection enable.
??? PUTF | waveform | MARS | 0 | $(NELM) | Array of pileup rejection values.
PUTR | waveform | MARS |0 | $(NELM) | Array of NCHAN pileup threshold trim values.
RODEL | longout | No | 0 | | Set readout delay.
SHPT | mbbo | MARS | 0 | 1: 0.25us<br>2: 0.5us<br>3: 1us<br>4: 2us | Set shaping time.
SLP | ao | No | 0 | Used in spctx calculation.
TDS  | mbbo | MARS | 0 | 0: 1us<br>1: 2us<br>2: 3us<br>3: 4us<br>4: 6us<br>5: 9us<br>6: 12us | Set time detector slope.
TDM  | bo   | MARS | 0 | 0: Time of arrival<br>1: Time over threshold | Set TDC mode.
THRSH | waveform | MARS | 0 | $(NCHIPS) | Threshold
THTR | waveform | MARS | 0 | $(NELM) | Array of NCHAN trim DAC values.
ADC0_SKEW | longout | Register | 0 | | Clock skew of ADC 0.
ADC1_SKEW | longout | Register | 0 | | Clock skew of ADC 1.
ADC2_SKEW | longout | Register | 0 | | Clock skew of ADC 2.

- ## Test pulse

PV | TYPE | OP | ADDR | SIZE | VALUE | NOTE
:-:|:-:|:-:|:-:|:-|:-|:-
CHEN  | waveformout | MARS | 0 | $(NELM) |  | Channel enable (per channel).
CHEN_SET | mbbo | MARS | 0 | | 0: enable channel $(MONCH)<br>1: enable all channels<br>2: disable channel $(MONCH)<br>3: disable all channels
POL   | bo | MARS | 0 | | 0: Negative<br>1: Positive | Set input polarity.
TPAMP | longout | MARS | 0 | |  | Set test pulse amplitude.
TPCNT | longout | MARS | 0 | |  | Set test pulse count.
TSEN  | waveformout | MARS | 0 | $(NELM) |  | Test pulse enable (per channel).
TSEN_SET | mbbo | MARS | 0 | | 0: enable channel $(MONCH)<br>1: enable all channels<br>2: disable channel $(MONCH)<br>3: disable all channels
TPENB | bo | MARS | 0 | | 0: Off<br>1: On | Enable test pulses. Writes to MARS_CALPULSE. Why MARS-CFG?
TPFRQ | longout | MARS | 0 | |  | Set test pulse frequency. Wirtes to CALPULSE_WIDTH. Why MARS-CFG?

- ## UDP

PV | TYPE | OP | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-:|:-|:-
IPADDR | stringout | Register | 0 |  |
IPADDR_RBV | stringin | Register | 0 |  |
FNAM | stringin | Soft channel | 0 | | Name of data file.
FSIZ | stringin | Soft channel | 0 | | Maximum size of data file.
DIR | stringin | Soft channel | 0 | | Directory of data file.


- ## Count

PV | TYPE | OP | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-:|:-|:-
CNT   | bo | Register | 0 | 0: Stop<br>1: Start    | Acquisition control.
CNT_RBV   | bi | Register | 0 | 0: Stop<br>1: Start    | Acquisition status.
GMON | mbbo | MARS | 0 | 0: Off<br>1: Temperature<br>2: Baseline<br>3: Threshold<br>4: Test pulse<br>5: Channel monitor | Set switch for channel monitors or others.
PR1 | | ? | 0 | | Integer preset for count.
RUNNO | longout | Register | 0 |  |
RUNNO_RBV | longin | Register | 0 |  |
MONCH | longout | MARS | 0 |  | Set channel which has monitor out enabled.
TP  | longout | Register | 0 |  | Count time.
TP  | longin | Register | 0 |  | Count time readback value.
TP1 | ao | ? | 0 |  |


- ## Environment

PV | TYPE | OP |  ADDR | VALUE | NOTE
:-:|:-:|:-:|:-:|:-|:-
HV     | ao | Peripheral | 0 |  |
HV_RBV | ai | Peripheral | 0 |  |
HV_CUR | ai | Peripheral | 0 |  |
Temp1  | ai | Peripheral | 0 |  |
Temp2  | ai | Peripheral | 0 |  |
Temp3  | ai | Peripheral | 0 |  |
ztmp   | ai | Peripheral | 0 |  |


## 2. Fields and PVs not exposed on Phoebus screens

- ### Fields in `det1`

  - ACKS**
  - ACKT**
  - AMSG
  - ASG**
  - BKPT**
  - CHAN: not used
  - DISA**
  - DISP**
  - DISV**
  - DLY: delay before count starts
  - DLY1: delay before count starts, for autocount
  - EBLK: exposed, but only 1 value?
  - EVNT**
  - LCNT**
  - OFFS: energy calibration offset
  - PHAS** grep pscal->t
  - PLDEL: MARS pipeline delay
  - PR1: integer preset for count. Not exposed.
  - PUEN
  - PUTF: pileup rejection registers. Not currently used but needed.
  - PUTR
  - RAT1: display rate (screen data update rate in autocount mode?)
  - RATE: displayrate (screen data update rate in acquisition mode?)
  - RODEL: Display hold time between updates / readout delay
  - SLP: energy calibrations slopes
  - T: not used?
  - TP1: Preset count in autocount mode. Is already exposed?
  - TSE: ?

  **: from EPICS base

  
- ### Independent PVs - not used

  - P1_CUR
  - P2_CUR
  - P1
  - P2


## 3. Reference

```
$ dbpr det1 2
ACKS: NO_ALARM      ACKT: YES           AMSG:               ASG :               
BKPT: 00            CALF:               CHAN: 0             CHEN: PTR (nil)     
CHIP: 0             CNT : Done          CONT: OneShot       COUT: CONSTANT      
COUTP: CONSTANT     DESC:               DISA: 0             DISP: 0             
DISS: NO_ALARM      DISV: 1             DLY : 0             DLY1: 0             
DTYP: NSLS detector EBLK: 2pA           EGU : counts        EVNT:               
EXSIZE: 4096        EYSIZE: 384         FLNK: CONSTANT      FNAM:               
FREQ: 1000000       FVER: 18            GAIN: 240keV        GMON: Off           
INP : VME_IO #C0 S1 @384                INTENS: PTR (nil)   IPADDR: 10.66.211.64
LCNT: 0             MCA : PTR (nil)     MODE: Framing       MONCH: 0            
NAME: det1          NAMSG:              NCH : 384           NCHIPS: 12          
NELM: 384           NSEV: NO_ALARM      NSTA: NO_ALARM      OFFS: PTR (nil)     
OUT : VME_IO #C0 S0 @                   PACT: 0             PCNT: Done          
PHAS: 0             PINI: YES           PLDEL: 72           POL : Positive      
PR1 : 1000000       PREC: 0             PRIO: LOW           PUEN: Disable       
PUTF: 0             PUTR: PTR (nil)     RAT1: 0             RATE: 2             
RODEL: 15           RPRO: 0             RUNNO: 0            SCAN: Passive       
SDIS: CONSTANT      SEVR: NO_ALARM      SHPT: 0.5us         SLP : PTR (nil)     
SPCT: PTR (nil)     SPCTX: PTR (nil)    STAT: NO_ALARM      T   : 0             
TDC : PTR (nil)     TDM : Time of arrival                   TDS : 1us           
TIME: 2019-02-14 05:12:06.937185478     TP  : 1             TP1 : 1             
TPAMP: 102          TPCNT: 0            TPENB: Off          TPFRQ: 0            
TPRO: 0             TSE : 0             TSEL: CONSTANT      TSEN: PTR (nil)     
TXSIZE: 1024        TYSIZE: 384         UDF : 0             UDFS: INVALID       
VAL : 0             VERS: 1   
```
