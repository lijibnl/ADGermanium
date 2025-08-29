/*
 * pl.h - PicoLogic register definitions for Mars_DDM detector system
 * 
 * Original file from https://github.com/lijibnl/Mars_DDM/blob/main/zDDMApp/src/pl.h
 * Modified to remove EVENT_FIFO_* and DMA_IRQ_* registers as requested
 */

#ifndef __PL__
#define __PL__

// PL registers
#define MARS_CONF_LOAD      0
#define LEDS                1
#define MARS_CONFIG         2
#define VERSIONREG          3
#define MARS_CALPULSE       4
#define MARS_PIPE_DELAY     5
#define DETECTOR_TYPE       6
#define MARS_RDOUT_ENB      8 
#define EVENT_TIME_CNTR     9
#define SIM_EVT_SEL         10
#define SIM_EVENT_RATE      11
#define ADC_SPI             12
#define CALPULSE_CNT        16
#define CALPULSE_RATE       17
#define CALPULSE_WIDTH      18
#define CALPULSE_MODE       19
#define TD_CAL              20
#define DMA_CONTROL         32
#define DMA_STAT            33
#define DMA_BASEADDR        34
#define DMA_BURSTLEN        35
#define DMA_BUFLEN          36
#define DMA_CURADDR         37
#define DMA_THROTTLE        38
#define UDP_IP_ADDR         40
#define TRIG                52
#define COUNT_TIME_LO       53
#define COUNT_TIME_HI       54
#define FRAME_NO            55
#define COUNT_MODE          56

#define LOADS               80
//#define CMD_REG_READ    0
//#define CMD_REG_WRITE   1
//#define CMD_START_DMA   2
//
//#define FIFODATAREG     EVENT_FIFO_DATA
//#define FIFORDCNTREG    EVENT_FIFO_CNT
//#define FIFOCNTRLREG    EVENT_FIFO_CNTRL
//
//#define FRAMEACTIVEREG  52
//#define FRAMENUMREG     54
//#define FRAMELENREG     53

#endif
