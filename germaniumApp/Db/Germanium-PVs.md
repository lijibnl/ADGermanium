PV | Type | Addr | Value | Note
:-:|:-:|:-:|:-|:-
MCA    | NDArray | 1 | `$(NELM)*2048`
TDC    | NDArray | 1 | `$(NELM)*1024`
SPCT   | NDArray | 1 | `1*2048`
INTENS | NDArray | 1 | `1*2048`

- ## Config

PV | TYPE | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-|:-
GAIN | mbbo | 0 | 0: 240keV<br>1: 120keV<br>2: 60keV<br>3: 30keV | Set gain.
LEAK | mbbo | 0 | 0: Off<br>1: 2pA<br>2: 8pA |
SHPT | mbbo | 0 | 1: 0.25us<br>2: 0.5us<br>3: 1us<br>4: 2us | Set shaping time.
MODE | bo   | 0 | 0: Framing<br>1: Continuous | Set framing mode.
CONT | bo   | 0 | 0: OneShot<br>1: AutoCount
TDS  | mbbo | 0 | 0: 1us<br>1: 2us<br>2: 3us<br>3: 4us<br>4: 6us<br>5: 9us<br>6: 12us | Set time detector slope.
TDM  | bo   | 0 | 0: Time of arrival<br>1: Time over threshold | Set TDC mode.
PLDEL | longout | 0 | | Set pipeline delay.
RODEL | longout | 0 | | Set readout delay.

- ## Test pulse

PV | TYPE | ADDR | SIZE | VALUE | NOTE
:-:|:-:|:-:|:-|:-
TPENB | bo | 0 | | 0: Off<br>1: On | Enable test pulses.
POL   | bo | 0 | | 0: Negative<br>1: Positive | Set input polarity.
TPAMP | longout | 0 | |  | Set test pulse amplitude.
TPFRQ | longout | 0 | |  | Set test pulse frequency.
TPCNT | longout | 0 | |  | Set test pulse count.
CHEN  | waveformout | 0 | $(NELM) |  | Channel enable (per channel).
TPEN  | waveformout | 0 | $(NELM) |  | Test pulse enable (per channel).

- ## UDP

PV | TYPE | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-|:-
IPADDR | stringout | 0 |  |


- ## Count

PV | TYPE | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-|:-
TP  | ao | 0 | 
TP1 | ao | 0 | 
RUNNO | longout | 0 | 
GMON | mbbo | 0 | 0: Off<br>1: Temperature<br>2: Baseline<br>3: Threshold<br>4: Test pulse<br>5: Channel monitor | Set switch for channel monitors or others.
MONCH | longout | 0 |  | Set channel which has monitor out enabled.
CNT   | bo | 0 | 0: Stop<br>1: Start    | Acquisition control.

- ## Calibration

PV | TYPE | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-|:-
MFS   | mbbo | 0 | 0: Off<br>1: 250ns<br>2: 500ns<br>3: 1us<br>4: 2us | Multi-fire suppression.
LOAO  | bo   | 0 | 0: Leakage<br>1: Pulse | Set channel monitor to leakage or pulse.
EBLK  | | 0 | | Enable on-chip bias current generator.
PUEN  | bo | 0 | 0: Disable<br>1: Enable | Pileup Rejection enable.
THTR | waveform | 0 |  | Array of NCHAN trim DAC values.
PUTR | waveform | 0 |  | Array of NCHAN pileup threshold trim values.
THRSH | waveform | 0 |  | Threshold

- ## Environment

PV | TYPE | ADDR | VALUE | NOTE
:-:|:-:|:-:|:-|:-
HV     | ao | 0 |  |
HV_RBV | ai | 0 |  |
HV_CUR | ai | 0 |  |
Temp1  | ai | 0 |  |
Temp2  | ai | 0 |  |
Temp3  | ai | 0 |  |
ztmp   | ai | 0 |  |
