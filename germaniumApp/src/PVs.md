det1:HV_RBV
det1:HV_CUR
det1:P1_CUR
det1:P2_CUR
det1:HV
det1:P1
det1:P2
det1:TIME_ELAPSED
det1:TIME_LEFT




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

- Data

PV | Note
:-:|:-
MCA |
TDC |
SPCT |
INTENS |

Config

PV | Note
:-:|:-
GAIN | 240keV/...
SHPT | 0.5us/...
MODE | Framing/Continuous
CONT | OneShot/ AutoCount
TDS | 1us/...
TDM | Time of arrival/...

- ## Test pulse

PV | Note
:-:|:-
TPENB
POL
TPAMP
TPFRQ
TPCNT
CHEN

- ## UDP

PV | Note
:-:|:-
IPADDR


- ## Count

PV | Note
:-:|:-
TP
TP1
RUNNO
GMON | Off/Temperature/...
MONCH
CNT

- ## Calibration

PV | Note
:-:|:-
MFS
LOAO
EBLK
PUEN
THRHD

- ## Environment

PV | Note
:-:|:-
HV
HV_RBV
HV_CUR
Temp1
Temp2
Temp3
ztmp