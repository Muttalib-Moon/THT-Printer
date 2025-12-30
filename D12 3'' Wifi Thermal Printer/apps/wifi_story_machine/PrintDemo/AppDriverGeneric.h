
#ifdef __cplusplus
extern "C" {
#endif


#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "asm/power_interface.h"
#include "event/bt_event.h"
#include "event/event.h"
#include "syscfg/syscfg_id.h"
#include "app_config.h"
#include "os/os_api.h"

//#include "system/malloc.h"
#include "bt_common.h"
#include "btstack_typedef.h"
#include "string.h"
#include "device/gpio.h"
#include "asm/adc_api.h"

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"
#include "portable.h"


//global variable

//functions in AppDriverBase.c
void SysReset(void);
void Debug(const char* format, ...);

//declaration of functions
void USBPrinterReveiveDataTask(void);
//-----Macro of LED
#define LEDErrGpioIO	            IO_PORT_USB_DPB
#define	LEDErrTurnOff		        gpio_direction_output(IO_PORT_USB_DPB,0)
#define	LEDErrTurnOn		        gpio_direction_output(IO_PORT_USB_DPB,1)



#define LEDLineGpio		            JL_PORTA->OUT
#define LEDLineGpioIO	            IO_PORTA_09
#define LEDLineGpioIOBit	        BIT(9)
#define	LEDLineTurnOff		      LEDLineGpio&=~LEDLineGpioIOBit
#define	LEDLineTurnOn		      LEDLineGpio|=LEDLineGpioIOBit

//LedPap
#define LED_PAPER_GPIO              JL_PORTA->OUT
#define LED_PAPER_GPIOIO            IO_PORTA_07
#define LED_PAPER_GPIOBIT           BIT(7)
#define LED_PAPER_HIGH              LED_PAPER_GPIO |=  LED_PAPER_GPIOBIT
#define LED_PAPER_LOW               LED_PAPER_GPIO &= ~LED_PAPER_GPIOBIT

// CHARGING D801
#define CHARGING_D801_GPIOIO        IO_PORT_USB_DMB
#define CHARGING_D801_HIGH          gpio_direction_output(CHARGING_D801_GPIOIO,1)
#define CHARGING_D801_LOW           gpio_direction_output(CHARGING_D801_GPIOIO,0)

//-----------------------------Macro of step motor-----------------------------
#define Forward			0
#define Backward		1

#define Stopped			0
#define Running			1

#define ModeContinue		0
#define ModeFixSteps		1

#define UserDefineCommandHead		0
#define UserDefineCommandData		1


#define USIRAM SEC_USED(.volatile_ram_code)
#define TimerStepMotorCON	JL_TIMER4->CON
#define TimerStepMotorPRD	JL_TIMER4->PRD
#define TimerStepMotorCNT	JL_TIMER4->CNT


#define TimerIntervalCON	JL_TIMER1->CON
#define TimerIntervalPRD	JL_TIMER1->PRD
#define TimerIntervalCNT	JL_TIMER1->CNT


//Macro of step motor IO

// MTA+
#define MOTOR_A_P_GPIO             JL_PORTA->OUT
#define MOTOR_A_P_GPIOIO           IO_PORTA_06
#define MOTOR_A_P_GPIOBIT          BIT(6)
#define MOTOR_A_P_LOW              MOTOR_A_P_GPIO &= ~MOTOR_A_P_GPIOBIT
#define MOTOR_A_P_HIGH             MOTOR_A_P_GPIO |=  MOTOR_A_P_GPIOBIT

// MTA-
#define MOTOR_A_N_GPIO             JL_PORTA->OUT
#define MOTOR_A_N_GPIOIO           IO_PORTA_05
#define MOTOR_A_N_GPIOBIT          BIT(5)
#define MOTOR_A_N_LOW              MOTOR_A_N_GPIO &= ~MOTOR_A_N_GPIOBIT
#define MOTOR_A_N_HIGH             MOTOR_A_N_GPIO |=  MOTOR_A_N_GPIOBIT

// MTB+
#define MOTOR_B_P_GPIO             JL_PORTA->OUT
#define MOTOR_B_P_GPIOIO           IO_PORTA_03
#define MOTOR_B_P_GPIOBIT          BIT(3)
#define MOTOR_B_P_LOW              MOTOR_B_P_GPIO &= ~MOTOR_B_P_GPIOBIT
#define MOTOR_B_P_HIGH             MOTOR_B_P_GPIO |=  MOTOR_B_P_GPIOBIT

// MTB-
#define MOTOR_B_N_GPIO             JL_PORTA->OUT
#define MOTOR_B_N_GPIOIO           IO_PORTA_04
#define MOTOR_B_N_GPIOBIT          BIT(4)
#define MOTOR_B_N_LOW              MOTOR_B_N_GPIO &= ~MOTOR_B_N_GPIOBIT
#define MOTOR_B_N_HIGH             MOTOR_B_N_GPIO |=  MOTOR_B_N_GPIOBIT

// Motor Enable (MTEN)
#define MOTOR_EN_GPIO              JL_PORTA->OUT
#define MOTOR_EN_GPIOIO            IO_PORTA_00
#define MOTOR_EN_GPIOBIT           BIT(0)

#define  MOTOR_DIS              MOTOR_EN_GPIO &= ~MOTOR_EN_GPIOBIT;    TimerStepMotorCON&=~BIT(0);
#define  MOTOR_EN               MOTOR_EN_GPIO |=  MOTOR_EN_GPIOBIT;        TimerStepMotorCON|=BIT(0);



#define Step1IO			MOTOR_A_P_HIGH;   MOTOR_A_N_LOW;    MOTOR_B_P_HIGH;   MOTOR_B_N_LOW;
#define Step2IO			MOTOR_A_P_LOW;    MOTOR_A_N_HIGH;   MOTOR_B_P_HIGH;   MOTOR_B_N_LOW;
#define Step3IO			MOTOR_A_P_LOW;    MOTOR_A_N_HIGH;   MOTOR_B_P_LOW;    MOTOR_B_N_HIGH;
#define Step4IO			MOTOR_A_P_HIGH;   MOTOR_A_N_LOW;    MOTOR_B_P_LOW;    MOTOR_B_N_HIGH;


typedef struct StepMotorRunningParameter
{
	unsigned int  ACCSteps;		//accelerate steps
	unsigned long CurrentSteps; //record Current steps
	unsigned int CurrentDecSteps;//record deceleration steps
	bool Suspend;				//manipulate running stablility of the step motor
	unsigned char Dir;			//step motor runs direction
	unsigned char Status;		//Step motor status,0:stopped,1:running
	unsigned char Mode;			//step motor control mode 0:continue 1:fix steps
	unsigned int StepsFixedValue;//if the mode is fix steps this value is the steps step motor runs
	bool StepMotorFinished;		//this is a indicatator for interrup function
	bool HeadPrint;				//head print flag, true:print false:only move step motor
	bool FindPaperEdge;					//find paper edge function	true:step motor find paper edge; false: this operation is forbidden
	bool RecordGap;					//true:when motor runs forward the gap informatrion will be recorded and calculated
	//
	bool CalculateCompensate;	//This value uses in the powerOn or cover close operation



}StepMP;

typedef struct PRINTPAGE
{
	unsigned int CurrentLine;		//Current printing line in one page
	int RemainderSteps;	//current page remainder steps
	unsigned long PageNum;			//this value is current printing page
	bool EliminateRedundanctData;	//this value used when the redundant data can't be removed during the Gap moving,the motor will stopped and delete the left page data
	bool SensorError;
	bool ContinuePrint;
}PrintPage;
#define PrintPos	0
#define CutPos		1

typedef struct GapAndPaper
{
	unsigned int LabelLength;		//label length steps calculates by sensor
	unsigned long StepsCount;		//
	unsigned long StepsGapPos; 		//record the gap in location
	unsigned int LabelGapLength;	//label gap length
	unsigned char CountGap;			//Gap Status bit
	bool GapIn;						//label gap on sensor flag
	unsigned char CountPaper;		//count paper sensor status
	unsigned char paperPosition;	//print position:0 cut paper position:1
	unsigned int LableLengthCP;		//record the power on page length when power on
	unsigned int CountLable;
	unsigned int CountLastAuthenticPageGap;
	unsigned char LockPage;




}GAP;

extern GAP Gap;

//ADC channel
enum ADCChannelName
{
	KEY_BOARD_ADC=0,
    TEMPERATURE_ADC,    //Thermometer 25 degree:750 -->50 degree,10k=ADC 512;-->60 degree,7.5K=ADC 438
    PAPER_R_ADC,
	GAP_R_ADC,
	ADCMemoryLimit
};

//----------------------------------Macro of thermal printer head----------------------------------
//PWM timer3 macro
#define    TimerPWMCON JL_TIMER3->CON
#define	   TimerPWMPRD JL_TIMER3->PRD
#define	   TimerPWMPWM JL_TIMER3->PWM
#define	   TimerPWMCNT JL_TIMER3->CNT
///Print Head

//Strobe
#define HeadstrobeGpio	            JL_PORTC->OUT
#define HeadstrobeIO	            IO_PORTC_00
#define HeadstrobeIOBit	            BIT(0)
#define HeadstrobeEnable			HeadstrobeGpio|=HeadstrobeIOBit
#define HeadstrobeDisable			HeadstrobeGpio&=~HeadstrobeIOBit

//Latch
#define HeadLatchGpio	            JL_PORTA->OUT
#define HeadLatchIO		            IO_PORTA_01
#define HeadLatchIOBit	            BIT(1)
#define HeadLatchHigh				HeadLatchGpio|=HeadLatchIOBit
#define HeadLatchLow				HeadLatchGpio&=~HeadLatchIOBit
#define HeadLatchIn					HeadLatchGpio|=HeadLatchIOBit;HeadLatchGpio&=~HeadLatchIOBit;HeadLatchGpio&=~HeadLatchIOBit;HeadLatchGpio|=HeadLatchIOBit;

//Clock
#define HeadClkGpio		            JL_PORTC->OUT
#define HeadClkIO		            IO_PORTC_09
#define HeadClkIOBit	            BIT(9)
#define HeadClkHigh					HeadClkGpio|=HeadClkIOBit
#define HeadClkLow					HeadClkGpio&=~HeadClkIOBit

//Data
#define HeadDataGpio	            JL_PORTC->OUT
#define HeadDataIO		            IO_PORTC_08
#define HeadDataIOBit	            BIT(8)
#define HeadDataHigh				HeadDataGpio|=HeadDataIOBit
#define HeadDataLow					HeadDataGpio&=~HeadDataIOBit

///Power Enable IO
#define PowerLockGpio	            JL_PORTA->OUT
#define PowerLockIO		            IO_PORTA_02
#define PowerLockIOBit	            BIT(2)
#define LockPower                   PowerLockGpio |= PowerLockIOBit
#define PowerOff                    PowerLockGpio &= ~PowerLockIOBit

///ON/OFF Button
#define SwitchPin                   IO_PORTB_01
#define SwitchState                 gpio_read(SwitchPin)

/// Cover sensor
#define CoverGpio	                JL_PORTB->OUT
#define CoverIO		                IO_PORTB_06
#define CoverIOBit	                BIT(6)
#define	CoverState					gpio_read(CoverIO)

///GapT1
#define GAP_T1_GPIO                 JL_PORTB->OUT
#define GAP_T1_GPIOIO               IO_PORTB_05
#define GAP_T1_GPIOBIT              BIT(5)

#define GAP_T1_HIGH                 GAP_T1_GPIO |=  GAP_T1_GPIOBIT
#define GAP_T1_LOW                  GAP_T1_GPIO &= ~GAP_T1_GPIOBIT

///GapT2
#define GAP_T2_GPIO                 JL_PORTC->OUT
#define GAP_T2_GPIOIO               IO_PORTC_02
#define GAP_T2_GPIOBIT              BIT(2)

#define GAP_T2_HIGH                 GAP_T2_GPIO |=  GAP_T2_GPIOBIT
#define GAP_T2_LOW                  GAP_T2_GPIO &= ~GAP_T2_GPIOBIT

///PaperT
#define PAPER_T_GPIO                JL_PORTC->OUT
#define PAPER_T_GPIOIO              IO_PORTC_07
#define PAPER_T_GPIOBIT             BIT(7)

#define PAPER_T_LOW                 PAPER_T_GPIO &= ~PAPER_T_GPIOBIT
#define PAPER_T_HIGH                PAPER_T_GPIO |=  PAPER_T_GPIOBIT

/*
//power lock IO
#define PowerLockGpio	JL_PORTC->OUT
#define PowerLockIO		IO_PORTC_08
#define PowerLockIOBit	(0x0001<<8)

// Power lock macro
#define SwitchPin IO_PORTB_01
#define LockPower PowerLockGpio |= PowerLockIOBit
#define PowerOff PowerLockGpio &= ~PowerLockIOBit
#define SwitchState gpio_read(SwitchPin)

#define LockPower				PowerLockGpio|=PowerLockIOBit
#define PowerOff				PowerLockGpio&=~PowerLockIOBit
// Cover sensor
#define CoverGpio	JL_PORTB->OUT
#define CoverIO		IO_PORTB_08
#define CoverIOBit	(0x0001<<08)
#define	CoverState					gpio_read(CoverIO)	//B8/C0  GZ when the cover closes then IO become low level

//power control for paper edge sensor
#define PowerPaperEdgeGpio	JL_PORTC->OUT
#define PowerPaperEdgeIO	IO_PORTC_05
#define PowerPaperEdgeIOBit	(0x0001<<5)

#define PowerPaperEdgeOff				PowerPaperEdgeGpio|=PowerPaperEdgeIOBit
#define PowerPaperEdgeOn				PowerPaperEdgeGpio&=~PowerPaperEdgeIOBit

//power control for paper edge sensor
#define GapEmitterTopGpio	JL_PORTA->OUT
#define GapEmitterTopIO	IO_PORTA_01
#define GapEmitterTopIOBit	(0x0001<<1)

#define GapEmitterTopOff			GapEmitterTopGpio|=GapEmitterTopIOBit
#define GapEmitterTopOn				GapEmitterTopGpio&=~GapEmitterTopIOBit


//power control for paper edge sensor
#define GapEmitterBottomGpio	JL_PORTC->OUT
#define GapEmitterBottomIO		IO_PORTC_06
#define GapEmitterTopIOBit		(0x0001<<6)

#define GapEmitterBottomOff			GapEmitterBottomGpio|=GapEmitterTopIOBit
#define GapEmitterBottomOn			GapEmitterBottomGpio&=~GapEmitterTopIOBit
*/
extern PrintPage Page;
extern StepMP SMP;
extern const unsigned long  TableSpeedUS[];
void ContinueMode(StepMP *SMP);
void FixedStepsMode(StepMP *SMP);
void USBPrinterReveiveDataTask(void);
void CoverOperation(void);
void RedundantDataProcessing(bool waiting);
void SensorErrDuringPrintProcess(void);
void WriteEmptyPage(unsigned int Dots);
void WritePrintLinkedListB(unsigned char *Buffer,unsigned int Len);
void SelfPrint(void);
void CommandSetPageStart(void);
void CommandSetPageEnd(void);

//NVS structure
typedef struct NoneVolatileStorage
{
	unsigned int PrintPosAndGapValue;//storage the gap distance between gap sensor and print location. unite:steps default value 240
	unsigned int USBPID;
	char EDR_BLEName[32];
	char SSID[32];
	char SSIDPassword[32];
	char ManufactureName[32];
	char ModelName[32];
	unsigned int Dots;

}NVS;
void CommandNVSWiFi(void);
#define GAP_BUF_SIZE 16
extern NVS Nvs;
extern uint16_t threshold;
extern uint16_t d16min[GAP_BUF_SIZE];
extern uint16_t d16max[GAP_BUF_SIZE];
extern u8 HeadStbLowValid;
extern bool gapCalculated;
extern uint16_t gap_min;
extern uint16_t gap_max;




#ifdef __cplusplus
}
#endif


