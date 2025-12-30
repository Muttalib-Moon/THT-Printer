  /*
  ******************************************************************************
  * @file    	AppDriverCommunication.c
  * @author  	Sang 2024
  * @date		2024-12
  * @brief   Encapsulate JieLi hardware layers
  *
  *
  */
#include "AppDriverGeneric.h"//JL lib dependency
#include "AppDriver.h"
#include "stdio.H"

extern void BT_disable(void);
extern void BT_Enable(void);
extern void StartPWM(void);
SemaphoreHandle_t xMutex;
QueueHandle_t xQueueNVS;
EventGroupHandle_t Printer_event_group;
SemaphoreHandle_t xMutex;
static u8 heatTime= 16;  //default 16=1000
unsigned int ADCValue[ADCMemoryLimit]={0}; //ADC value queue



// Add these with your other global variables
uint16_t gap_min = 0;
uint16_t gap_max = 0;

#define GAP_BUF_SIZE 16
uint16_t d16min[GAP_BUF_SIZE] = {0}; // will be initialized to 0x3FF at runtime
uint16_t d16max[GAP_BUF_SIZE] = {0}; // default 0
static uint16_t sumLow = 0, sumHigh = 0;
static uint8_t minCount = 0, maxCount = 0;
uint16_t threshold = 500;


const unsigned long  TableSpeedUS[31] =//Speed table for acceleration and deceleration
{//attention!!! you can modify the value of this speed table ,but the maximum steps must be set to 31 steps, otherwise error will be occured during the printing!
	10000,3000,2519,2375,1917,1717,1558,1429,1322,1232,1156,1091,1035,987,945,908,875,847,822,800,781,764,750,738,727,718,712,706,702,700,700,

	//10000,3000,2519,2175,1917,1717,1558,1429,1322,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,1200,
};
StepMP SMP={8,0,0,false,Forward,Stopped,ModeFixSteps,500,false,false,false,false};//step motor running parameter
GAP Gap={0};
PrintPage Page={0,2400,0};
unsigned long TotalLen=0;
 bool WiFiLastPackage=false;
NVS Nvs={105};
unsigned char HandleCover=false;
u8 HeadStbLowValid = 0; // 0=not detected, 1=low active, 2=high active


static void GPIOSetOut(unsigned int IO)
{
	gpio_set_die(IO, 1);//set IO digital model
	gpio_set_pull_up(IO, 1);//set IO pull up
	gpio_set_pull_down(IO, 0);//close IO pull down
	gpio_set_direction(IO, 0);//Set IO output
}


bool gapCalculated=false;
bool inited = false;
volatile u16 levelPapType=0;


/* Utility to initialize min/max arrays once */
static inline void InitSharedGapArrays(void)
{
	if (!inited)
	{
		for (int i = 0; i < GAP_BUF_SIZE; i++)
		{
			d16min[i] = 0x03FF;
			d16max[i] = 0;
		}
		sumLow = sumHigh = 0;
		minCount = maxCount = 0;
		//threshold = 500;
		inited = true;
	}
}


/**
@30% also work 510
@40% at 510ohm ok perfect 111/810
@max 50% can be achived at 510
@
*/
//#define ADC_DUTY_CYCLE 40

#define ADC_DUTY_CYCLE_DEFAULT 10
#define DUTY_COUNT 7
const uint8_t DutySequence[DUTY_COUNT] = {50, 45, 40, 35, 30, 55, 60};

volatile uint8_t ADC_DUTY_CYCLE = ADC_DUTY_CYCLE_DEFAULT;
volatile uint8_t DutyIndex = 0;

void SetupGapEmitterPWM(void)
{

    gpio_output_channle(IO_PORTC_02, CH2_T2_PWM_OUT);
    JL_TIMER2->CON = 0;
    JL_TIMER2->PRD = 240;

    if (!CoverState) //&& ADCValue[PAPER_R_ADC] < 500)
    {
        JL_TIMER2->PWM = JL_TIMER2->PRD * ADC_DUTY_CYCLE / 100;
        JL_TIMER2->CON |= BIT(8);  // PWM_EN
        JL_TIMER2->CON |= BIT(0) | BIT(3); // Timer start + mode
    }
    else
    {
        JL_TIMER2->CON &= ~BIT(0);
        JL_TIMER2->CON &= ~BIT(8);
        gpio_clear_output_channle(IO_PORTC_02, CH2_T2_PWM_OUT);
        gpio_direction_output(IO_PORTC_02,1);
    }

    //printf("[PWM Setup] DutyCycle=%d%%\n", ADC_DUTY_CYCLE);  // Debug
}



/**
 * @brief  GPIO configration.
 */
static void GPIO_Configuration(void)
{
	///Stepper Motor
	GPIOSetOut(MOTOR_A_P_GPIOIO);     //MTA+	    PA6(GPIO)
	GPIOSetOut(MOTOR_A_N_GPIOIO);     //MTA-	    PA5(GPIO)
	GPIOSetOut(MOTOR_B_P_GPIOIO);     //MTB+	    PA3(GPIO)
	GPIOSetOut(MOTOR_B_N_GPIOIO);     //MTB-	    PA4(GPIO)
	GPIOSetOut(MOTOR_EN_GPIOIO);      //MTEN	    PA0(GPIO)

	MOTOR_DIS;
	MOTOR_A_P_HIGH;
    MOTOR_A_N_LOW;
    MOTOR_B_P_HIGH;
    MOTOR_B_N_LOW;

	///Print Head
  /*gpio_set_die(IO_PORTC_00, 1);       //set ADC mode
	gpio_set_pull_up(IO_PORTC_00, 1);   //close pull up
	gpio_set_pull_down(IO_PORTC_00, 0); //close pull down
	gpio_set_direction(IO_PORTC_00, 0); //input mode*/
	int level = gpio_read(IO_PORTC_00);
    printf("GPIO PC00 state------------------------->>>>>>>>>>>>>>>>>>>>>>>>>%d\r\n", level); // 0 = Low, 1 = High
    //GPIOSetOut(HeadstrobeIO);   //STB    PC0
	GPIOSetOut(HeadLatchIO);   //LATCh  PA1
	GPIOSetOut(HeadDataIO);    //DATA   PC8
    GPIOSetOut(HeadClkIO);     //SCK    PC9

	HeadstrobeDisable;	//Strobe IO set 0
	//HeadstrobeEnable	;

	///Power Enable
	GPIOSetOut(PowerLockIO);//PwEnb power lock IO
	LockPower;	//lock working power

	///Cover sensor
	gpio_set_die(CoverIO, 1);
	gpio_set_pull_up(CoverIO, 1);
	gpio_set_pull_down(CoverIO, 0);
	gpio_set_direction(CoverIO, 1);

	///Gap Sensor
    GPIOSetOut(GAP_T1_GPIOIO);     //GapT1	PC0(GPIO)
	GAP_T1_HIGH;

	GPIOSetOut(GAP_T2_GPIOIO);     //GapT2	PC2(GPIO)
	//GAP_T2_LOW;//Upper IR Led ON

    ///Paper Sensor
	GPIOSetOut(PAPER_T_GPIOIO);    //PaperT	PA10(GPIO)
    PAPER_T_HIGH; //lower IR Led OFF

    ///Led
    GPIOSetOut(LED_PAPER_GPIOIO);       //Led Pap	PA7(GPIO)
	GPIOSetOut(CHARGING_D801_GPIOIO);   //D801	    PD1(GPIO)
	GPIOSetOut(LEDErrGpioIO);
	GPIOSetOut(LEDLineGpioIO);
    LEDErrTurnOff;
    //LEDLineTurnOn;
    CHARGING_D801_HIGH;

    ///ON-OFF Button
    gpio_set_die(IO_PORTB_01, 1);       //set ADC mode
	gpio_set_pull_up(IO_PORTB_01, 1);   //close pull up
	gpio_set_pull_down(IO_PORTB_01, 0); //close pull down
	gpio_set_direction(IO_PORTB_01, 1); //input mode

	///------------ADC----------------
	//KeyADC(PA8)->Channel 1
	gpio_set_die(IO_PORTA_08, 0);       //set ADC mode
	gpio_set_pull_up(IO_PORTA_08, 0);   //close pull up
	gpio_set_pull_down(IO_PORTA_08, 0); //close pull down
	gpio_set_direction(IO_PORTA_08, 1); //input mode

	//TMO(PA10)->Channel 2
	gpio_set_die(IO_PORTA_10, 0);       //set ADC mode
	gpio_set_pull_up(IO_PORTA_10, 1);   //Set pull up
	gpio_set_pull_down(IO_PORTA_10,+ 0); //close pull down
	gpio_set_direction(IO_PORTA_10, 1); //input mode

	//PaperR(PB7)->Channel 5
	gpio_set_die(IO_PORTB_07, 0);
	gpio_set_pull_up(IO_PORTB_07, 0);
	gpio_set_pull_down(IO_PORTB_07, 0);
	gpio_set_direction(IO_PORTB_07, 1);

	//GapR(PC1)->Channel 7
	gpio_set_die(IO_PORTC_01, 0);
	gpio_set_pull_up(IO_PORTC_01, 0);
	gpio_set_pull_down(IO_PORTC_01, 0);
	gpio_set_direction(IO_PORTC_01, 1);

    // PC0 strobe   PWM IO
	if (gpio_read(IO_PORTC_00))
	{
		HeadStbLowValid = 1;
		gpio_set_die(IO_PORTC_00, 1);		// set IO digital model
		gpio_set_pull_down(IO_PORTC_00, 0); // Close IO pull up
		gpio_set_pull_up(IO_PORTC_00, 1);	// Set pull down
		gpio_set_direction(IO_PORTC_00, 0); // Set IO output
		printf("STB Active HIGH\n");
		gpio_direction_output(IO_PORTC_00,1);
	}
	else
	{
		HeadStbLowValid = 2;
		gpio_set_die(IO_PORTC_00, 1);		// set IO digital model
		gpio_set_pull_down(IO_PORTC_00, 1); // Set IO pull up
		gpio_set_pull_up(IO_PORTC_00, 0);	// Close pull down
		gpio_set_direction(IO_PORTC_00, 0); // Set IO output
		printf("STB Active LOW\n");
        gpio_direction_output(IO_PORTC_00,0);
	}

	USB_PID_Init();
}

/**
 * @brief timer 1 interrupt callback function
 */

 void timer1(void)
{
    TimerIntervalCON =0;
    TimerIntervalCNT =0;
    TimerIntervalCON |= BIT(14);
    TimerIntervalCON |= 0x02 << 4;
    TimerIntervalPRD  = 50*10/3;
    TimerIntervalCON |= BIT(0);

}
/*#define MAXPWMCount 9	   // This value is the definition of maximum value of the PWM pulses ,when define MAXPWMCount 4 the total heat time is 400us,because the PWM width is 100us
unsigned int PWMCount = 0; // this value is the PWM counter in every motor step
static ___interrupt USIRAM void Timer1CallBackMotor(void)
{
	if (TimerIntervalCON & BIT(15))//interrupt bit
	{
        TimerIntervalCON |= BIT(14);//clear interrupt bit

		PortDriverPowerPulseAvert;//invert pulse
		USBPrinterReveiveDataTask();				 //USB receive data stream manegement
		EdrPrinterReceiveDataTask();
		BLEPrinterReceiveDataTask();


		if(PWMCount>=MAXPWMCount)
        {
            PWMCount = 0;
            PortDriverPowerPulseAvert;//invert pulse
        }
		if (PWMCount == 4)
		{

			TimerPWMCON &= BIT(0);									 // Close PWM
			gpio_clear_output_channle(HeadstrobeIO, CH3_T3_PWM_OUT); // if the STB IO has hardware protection circuit,this code is nor necessary.
		}
		else if(PWMCount== 0)
        {
            StartPWM();
        }
        PWMCount++;
    }
}*/
#define MAXPWMCount 4	   // This value is the definition of maximum value of the PWM pulses ,when define MAXPWMCount 4 the total heat time is 400us,because the PWM width is 100us
static int PWMCount = 0;
static int STBState = 0;
static u8 comChek;
static ___interrupt USIRAM void Timer1CallBackMotor(void)
{
    if (TimerIntervalCON & BIT(15))
    {
        TimerIntervalCON |= BIT(14);  // clear interrupt flag

//        PortDriverPowerPulseAvert;

        if(comChek>=80){
            comChek=0;
            USBPrinterReveiveDataTask();
            EdrPrinterReceiveDataTask();
            BLEPrinterReceiveDataTask();
        }

        comChek++;
        if (SMP.HeadPrint == true)
        {
            PWMCount++;

            if (STBState == 1)
            {
                if ( PWMCount >=10)
                {
                    STBState = 0;
                    PWMCount = 0;

                    TimerPWMCON &= BIT(0);
                    gpio_clear_output_channle(HeadstrobeIO, CH3_T3_PWM_OUT);
                    if(HeadStbLowValid==1)HeadstrobeEnable;
                    else if(HeadStbLowValid==2) HeadstrobeDisable;
                }
            }
            else
            {
                if (PWMCount >= heatTime)  // OFF duration
                {
                    STBState = 1;
                    PWMCount = 0;
                    TimerPWMCON |= BIT(0);
                    gpio_output_channle(HeadstrobeIO, CH3_T3_PWM_OUT);
                }
            }
        }
        else
        {
            STBState = 0;
            PWMCount = 0;
           // TimerPWMCON &= BIT(0);
            //gpio_clear_output_channle(HeadstrobeIO, CH3_T3_PWM_OUT);
            if(HeadStbLowValid==1)HeadstrobeEnable;
            else if(HeadStbLowValid==2) HeadstrobeDisable;
        }
    }
}

void SensorStatusISR(void)
{
    uint16_t adc = ADCValue[PAPER_R_ADC];

	static int ADCPaperEdgeCount=0;
	if(CoverState)
	{
		SMP.Suspend=true;
		Page.SensorError=true;
		printf("SensorStatusISR 1\r");
	}
	if(adc<100&&Forward==SMP.Dir)
	{
		ADCPaperEdgeCount++;
		if(ADCPaperEdgeCount>200)
		{
			SMP.Suspend=true;
			Page.SensorError=true;
			printf("SensorStatusISR 2\r");
		}
	}
	else
	{
		ADCPaperEdgeCount=0;
	}
}


void SoftSPI_Send(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    int8_t   j;
    //printf("Len: %d\n",len);
    if(len<108){
        len=72;
    }else{
     len=len;
    }
    HeadLatchHigh;

    for (i = 0; i < len; i++)
    {
        for (j = 7; j >= 0; j--)
        {
            HeadClkLow;

            if (buf[i] & (1 << j))
            {
                HeadDataHigh;
            }
            else
            {
                HeadDataLow;
            }

            HeadClkHigh;
        }
    }

    HeadLatchLow;
    HeadLatchLow;
    HeadLatchHigh;
}
void PrinterDataIn(unsigned char *data,unsigned char len)
{
	if(SMP.HeadPrint==false)return;
//	_dev_write_spi(data,len);//SPI write data
	SoftSPI_Send(data,len);

	HeadLatchIn;			//latch data in
}
void RedundantDataProcessing(bool waiting)//use this function should be carefully as the waiting parameter would be reset the chip at some sitituations
{
	unsigned int Count=0;
	LinkedListData LLDataTempA;
	while(1)
	{
		if(CopyPageNumISR(&PrintBLink,&LLDataTempA))//delete data packet and judge remainder data page number,current page print finished
		{
			//printf("b:%d-%d>",Page.PageNum,LLDataTempA.PageNum);
			if(Page.PageNum==LLDataTempA.PageNum)//if data page changes,delete processing ends
			{
				//printf("b:%d-%d>",Page.PageNum,LLDataTempA.PageNum);
				break;
			}
			Count++;
			if(Count>25&&!waiting)//due to the remainder time isn't enought to redundant colossal data at once as to set a limitation
			{
				//printf("CX:%d",Count);
				break;
			}
			if(waiting)// this only occured in a task ,if interrupt executes the delay ,chip will be reseted immediately.
			{
				unsigned int NodeCount=_malloc_size(PrintBLink);//calculate node quantity of print buffer
				if(NodeCount) continue;
				vTaskDelay(pdMS_TO_TICKS(200));
			}
		}
		else//this situation occurs when data is the last page or the print temporary buffer isn't big enough to hold the redundant data
		{
			if(!waiting)
			{
				//printf("C:%d",Count);
				SMP.Suspend=true;//redundant data can't be removed during the gap moving ,set step motor into the stop processing
				Page.EliminateRedundanctData=true;//tell the subquent code to handle this situation
				//printf("A3\r");
			}
			break;

		}
	}
}
void RedundantDataProcessingLastPage(void)
{
	unsigned int Count=0;
	LinkedListData LLDataTempA;
	while(1)
	{
		if(CopyPageNumISR(&PrintBLink,&LLDataTempA))//delete data packet and judge remainder data page number,current page print finished
		{
			if(Page.PageNum==LLDataTempA.PageNum)//if data page changes,delete processing ends
			{
				Count++;
				if(Count>25)//due to the remainder time isn't enought to redundant colossal data at once as to set a limitation
				{
					//printf("LX:%d",Count);
					break;
				}
			}
			else
			{
				break;
			}

		}
		else
		{
			break;
		}
	}
}

//brief:This function is a line counter for authentic label size which calculated by the gap.
void AuthenticPageMangement(void)
{
	Page.RemainderSteps--;//reminder steps in current page,subtract this value untile it become zero
	if(!Page.RemainderSteps)//change page
	{
		Gap.CountLable++;
		Page.PageNum++;//change print page
		Page.RemainderSteps=2400;//if subsequent is invalid ,use maximum value
		if(Gap.LabelLength)	//if lable length is valid then set it as the primary value of the countdown variable
		{
			Page.RemainderSteps=Gap.LabelLength;//the next page length
		}
		printf("A:%d-%d\n",Page.PageNum,Gap.LabelLength);
	}
}
//brief:This function aims to reconcile the page data with label size
void PageDataCalculation(void)
{
	bool CompensateEmptyLine=false;	//empty line output flag
	bool RedundantData=false;		//delete redundant data flag

	LinkedListData LLDataTempA;							//temporary buffer for one line
	if(_copy_PageNum(&PrintBLink,&LLDataTempA))			//read current page number
	{
		if(Gap.LockPage==false)//true:this is the last authentic paper page
		{
			if(Page.PageNum==LLDataTempA.PageNum)			//judge page number,if page number remains unchange
			{
				AuthenticPageMangement();//calculate steps by anthentic lable heighth
				if(Page.RemainderSteps<16)
				{
					RedundantDataProcessingLastPage();
					CompensateEmptyLine=true;
				}
			}
			else//print page changes,there are two situations ,print page number changed and print page data page changed
			{
				//printf("A:%d-%d>",Page.PageNum,LLDataTempA.PageNum);
				CompensateEmptyLine=true;//both situations need empty line output
				if(Page.PageNum>LLDataTempA.PageNum)//print page number changed,this situation means there are redundant data->actually in this situation,label gap is under the print head
				{
					//printf("@:%d",LLDataTempA.PageNum);
					RedundantDataProcessing(false);//remove the redundant data processing,don't wait,The parameter can't be set to true when function in ISR, otherwise the chip will be reset
				}
				else//this situation means the print page heighth is shorter than the label's height,we need to print empty line untile the "Page.PageNum" catches the "LLDataTempA.PageNum"
				{
					//printf(">:%d",LLDataTempA.PageNum);
					AuthenticPageMangement();//lable reminder steps count++
				}
			}
		}
	}
	else//this situation occurs when the page data's heighth is shorter than the lable heighth as there are no print data left none data read out(one page printting)
	{
		AuthenticPageMangement();
	}
	//----------------motor suspend condition--------------------------------------------------
	unsigned int NodeCount=_malloc_size(PrintBLink);//calculate parkets quantity
	//printf("%d",NodeCount);
	if(NodeCount<=SMP.ACCSteps&&Page.RemainderSteps<SMP.ACCSteps)//if remainder steps lower than accelerate steps then go to decelerate process
	{
		SMP.Suspend=true;// set step motor suspend flag ,then it goes to decelerate process atomatically
		//printf("A4\r");
	}
	else if(NodeCount<=SMP.ACCSteps&&command.PageEndFlag==false)//if the handle processing is faster than the print data in then stop the motor !
	{
		SMP.Suspend=true;
		//printf("A5:%d\r",NodeCount);
	}

	if(!CompensateEmptyLine&&NodeCount)
	{
		LinkedListData LLDataTemp;
		CopyMemoryISR(&PrintBLink,&LLDataTemp);				//read one linked list data
		PrinterDataIn(LLDataTemp.data,LLDataTemp.len);		//print one line data--->The second parameter represent the bytes one line-4inches:108;3inches:80,2inches:64
		free(LLDataTemp.data);								//release the temporary buffer
	}
	else
	{
		unsigned char Empty[108]={0};
		PrinterDataIn(Empty,108);	//in order to keep procedure consistency
	}
	//printf("%d",Page.RemainderSteps);
}
 //@brief stop step motor and close the periheral IOs
void StopStepMotor(void)
{
	//StepMotordisable;
    MOTOR_DIS;
    TimerPWMCON &= BIT(0);   //Close PWM
	gpio_clear_output_channle(HeadstrobeIO, CH3_T3_PWM_OUT);
	//HeadstrobeDisable;				//close PWM IO
	if(HeadStbLowValid==1)HeadstrobeEnable;
    else if(HeadStbLowValid==2) HeadstrobeDisable;

	unsigned char DataIn[108]={0};
	PrinterDataIn(DataIn,108);
	printf("S:%d!\n",_malloc_size(PrintBLink));
	SMP.Status=Stopped;				//step motor status
}
//@brief:This function serves as the core algorithm for dynamic page adjustment based on the gap, which occurs only when the label gap crosses the gap sensor.one page only execuate once!
void PageCompensationByGap(void)
{
	unsigned int EStimateValueA,EStimateValueB,RemainderSteps;
	if(Gap.LabelLength<Nvs.PrintPosAndGapValue&&Gap.LabelLength)//Authentic page heighth is shorter than the "PrintPosAndGapValue"
	{
		unsigned int Temp=Nvs.PrintPosAndGapValue-7;
		unsigned int LableHeight;
		LableHeight=Gap.LabelLength;
		bool DragFlag=false;
		for(unsigned int i=1;i<5;i++)//Estimate process
		{
			if(Temp>i*LableHeight)
			{
				EStimateValueA=Temp-i*LableHeight; //estimateA: left steps in the page
				//printf("&%d:%d-%d",i,EStimateValueA,Page.RemainderSteps);
				if(EStimateValueA+40>Page.RemainderSteps&&EStimateValueA<Page.RemainderSteps+40)
				{
					if(Page.RemainderSteps<20)//this situation means the print header is above the gap,don't need to protract the print;the print process is complicated any situation may occured
					{
						if(Page.RemainderSteps>EStimateValueA)
							Page.RemainderSteps=EStimateValueA;
						//printf("1:%d",Page.RemainderSteps);
					}
					else
					{
						Page.RemainderSteps=EStimateValueA;
						//printf("2:%d",Page.RemainderSteps);
					}
					if(!Page.RemainderSteps)Page.RemainderSteps=1;
					break;
				}
			}
			else//special situation print above the lable gap
			{

				EStimateValueA=i*LableHeight-Temp; //estimateA: left steps in the page
				//printf("#%d:%d-%d",i,EStimateValueA,Page.RemainderSteps);
				if(EStimateValueA+40>Page.RemainderSteps&&EStimateValueA<Page.RemainderSteps+40)
				{
					//printf("X%d:%d-%d",i,Gap.LabelLength,Page.RemainderSteps);
					if(Gap.LabelLength>Page.RemainderSteps&&Page.RemainderSteps<20)
					{
						Gap.LabelLength-=Page.RemainderSteps;
						Page.RemainderSteps=1;
						AuthenticPageMangement();
					}
					else
					{
						Page.RemainderSteps=Temp;
					}
					if(!Page.RemainderSteps)Page.RemainderSteps=1;
					//printf("Y:%d",Page.RemainderSteps);
				}
				else
				{
					DragFlag=true;
				}
				//printf(">>");
				break;
			}
		}
		if(DragFlag)
		{
			//printf("D:%d",Page.RemainderSteps);
			for(int j=1;j<5;j++)
			{
				if(Temp<j*Gap.LabelLength)
				{
					if(j==1)
						Page.RemainderSteps=1;
					else
						Page.RemainderSteps=Temp-(j-1)*Gap.LabelLength;
					break;
				}
			}
			if(!Page.RemainderSteps)Page.RemainderSteps=1;
			//printf("E:%d",Page.RemainderSteps);
		}
	}
	else
	{
		if(Gap.LabelLength)
		{
			//printf("R:%d\n",Page.RemainderSteps);
			if(Page.RemainderSteps>Nvs.PrintPosAndGapValue/2)//there is a special situation when the label's heighth approximate  to the "PrintPosAndGapValue"
				Page.RemainderSteps=Nvs.PrintPosAndGapValue;
		}
		else// only occured at the first page when the page size is super bigger than "PrintPosAndGapValue"
		{
			Page.RemainderSteps=Nvs.PrintPosAndGapValue;
		}
		//printf("X:%d\n",Page.RemainderSteps);
	}

}
void HandleLastAuthenticPageGap(void)
{
	Gap.CountLastAuthenticPageGap++;
	Gap.LabelGapLength=Gap.StepsCount-Gap.StepsGapPos;
	if(Gap.LabelGapLength>8*10)//if gap heighth exceed 10mm,some labels are lost, stop the motor and handle this issue manually
	{

		if(Gap.CountLastAuthenticPageGap>270&&Gap.LockPage==false)
		{
			Gap.LockPage=true;
			SMP.Suspend=true;		//motor enter into dec process
			Page.SensorError=true;//set status sensor detects errors
			//printf("G:%d",Gap.LabelGapLength);
			//Page.PageNum++;
			LinkedListData LLDataTempA;							//temporary buffer for one line
			if(_copy_PageNum(&PrintBLink,&LLDataTempA)==true)			//read current page number
			{
				printf("X:%d-%d\r\n",Page.PageNum,LLDataTempA.PageNum);
				if(Page.PageNum!=LLDataTempA.PageNum)
					Page.PageNum=LLDataTempA.PageNum;
			}
		}
	}
}

static uint16_t rawBuf[GAP_BUF_SIZE]; // circular buffer (raw order)
static uint8_t rawIndex = 0;

static void insertSorted(uint16_t *arr, uint16_t *sum, uint8_t *count, uint16_t val, bool lowSide)
{
	// Remove oldest if buffer full
	if (*count == GAP_BUF_SIZE)
	{
		*sum -= rawBuf[rawIndex];
	}
	else
	{
		(*count)++;
	}

	// Insert into raw circular buffer
	rawBuf[rawIndex] = val;
	rawIndex = (rawIndex + 1) % GAP_BUF_SIZE;

	*sum += val;

	// Rebuild sorted array from raw buffer
	memcpy(arr, rawBuf, (*count) * sizeof(uint16_t));
	// Sort (ascending)
	for (int i = 0; i < *count - 1; i++)
	{
		for (int j = i + 1; j < *count; j++)
		{
			if ((lowSide && arr[j] < arr[i]) || (!lowSide && arr[j] > arr[i]))
			{
				uint16_t tmp = arr[i];
				arr[i] = arr[j];
				arr[j] = tmp;
			}
		}
	}
}

void GapCalculate(void)
{
    InitSharedGapArrays();
    uint16_t adc = ADCValue[PAPER_R_ADC];
    //printf("Adc %d\n",adc);
	Gap.StepsCount++;// record Gap steps

	if(gapCalculated==false){

        if (adc < 0x3FF) // valid ADC//1023
        {
            if (adc < threshold)
            {
                insertSorted(d16min, &sumLow, &minCount, adc, true);
            }
            else
            {
                insertSorted(d16max, &sumHigh, &maxCount, adc, false);
            }
             //   printf("%d",adc);
        }

        if (minCount >= 16 && maxCount >= 16)
        {
            uint32_t sum = 0;

            for (int i = 3; i < 15; i++)
            {
                sum += d16min[i];
            }

            for (int i = 3; i < 15; i++)
            {
                sum += d16max[i];
            }

            threshold = sum / 24;
            gap_min = d16min[0];
            gap_max = d16max[0];
            gapCalculated=true;
            inited= false;
           InitSharedGapArrays();
            printf("gapCalculated threshold %d------------->\n",threshold);
        }
	}
	if(adc < threshold)
	{
		Gap.CountGap=(Gap.CountGap<<1)|0x01;
	}
	else
	{
		Gap.CountGap>>=1;
	}
	//printf("GAP ADC:%d-%02X\n",adc,Gap.CountGap);
	if(Gap.CountGap==0x07&&Gap.GapIn==false)//07
	{
		if(Gap.StepsGapPos)//gap data exist
		{
			Gap.LabelLength=Gap.StepsCount-Gap.StepsGapPos;//total length of one label include the gap height
			Gap.StepsGapPos=Gap.StepsCount;//record steps
			//printf("P:%d\n",Gap.LabelLength);
			if(Gap.LableLengthCP)
				if(Gap.LabelLength+5<Gap.LableLengthCP||Gap.LabelLength>Gap.LableLengthCP+5)
				{
					Gap.LabelLength=Gap.LableLengthCP;
				}
		}
		Gap.StepsGapPos=Gap.StepsCount;//record steps
		//printf("P:%d\n",Gap.LabelLength);
		Gap.GapIn=true;
		Gap.CountLastAuthenticPageGap=0;
		Gap.LockPage=false;
	}
	if(Gap.GapIn)
	{
		if(Gap.CountGap==0x01)//01
		{
			Gap.LabelGapLength=Gap.StepsCount-Gap.StepsGapPos;
			Gap.GapIn=false;
			Gap.LockPage=false;
			printf("Gap.LabelGapLength :%d\n",Gap.LabelGapLength);
			if(SMP.CalculateCompensate)
				PageCompensationByGap();///print page compensate algorithm
			if(Gap.LabelGapLength>8*10)//if gap heighth exceed 10mm,some labels are lost, stop the motor and handle this issue manually
			{
				SMP.Suspend=true;		//motor enter into dec process
				Page.SensorError=true;//set status sensor detects errors
				//printf("G:%d",Gap.LabelGapLength);
			}
		}
		else
		{
			HandleLastAuthenticPageGap();
		}
	}
}
void ThermometerADJ(void)
{
	/*if(ADCValue[TEMPERATURE_ADC]<512)//50 degree
	{
		unsigned long DensityADJ;
		DensityADJ=command.density;
		if(command.density>(512-ADCValue[TEMPERATURE_ADC])/3)
			DensityADJ=command.density-(512-ADCValue[TEMPERATURE_ADC])/3;
		if(DensityADJ<35)
			DensityADJ=35;
		TimerPWMPWM= TimerPWMPRD*((unsigned long)DensityADJ)/100;//set density duty ration
	}*/
}
void FindFaperEdgeISR(void)
{
	static unsigned int PaperSensor=0xff;
    uint16_t adc = ADCValue[PAPER_R_ADC];

	if(adc>500){
            printf("FindFaperEdgeISR.................................\n\n\n");
            Gap.CountPaper=(Gap.CountPaper<<1)|0x01;
	}
	else
		Gap.CountPaper>>=1;

  //  printf("SMP.CurrentSteps   %d   SMP.StepsFixedValue %d  \n\n\n",SMP.CurrentSteps ,SMP.StepsFixedValue);
	if(Gap.CountPaper==0x0f||SMP.CurrentSteps>SMP.StepsFixedValue)
	{
		SMP.Suspend=true;
		//printf("A7\r");
	}
}

/**
 * @brief step motor phase switching
 */
static void PhaseSwitching(void)
{
	static unsigned char PhaseSerial=0;//phase flag of the step motor
	switch(PhaseSerial%4)//phase switching
	{
		case 0:Step1IO;break;
		case 1:Step2IO;break;
		case 2:Step3IO;break;
		default:Step4IO;break;
	}
	Forward==SMP.Dir?PhaseSerial++:PhaseSerial--;//move direction
	SMP.Mode==ModeContinue?ContinueMode(&SMP):FixedStepsMode(&SMP);//successive steps or fixed steps
	if(Forward==SMP.Dir&&SMP.RecordGap)//when move forward and find gap variable set then execute find gap function
	{
		GapCalculate();
	}
}

void StartPWM(void)
{
    u32 c32c1 = 0;
    if (HeadStbLowValid == 0)
        return;
        PWMCount = 0;
    gpio_output_channle(HeadstrobeIO, CH3_T3_PWM_OUT);
    if (HeadStbLowValid == 1)//new
    {
        c32c1 = command.density;
        if (c32c1 > 100)
            c32c1 = 100;
        c32c1 = 100 - c32c1;
        TimerPWMPWM = TimerPWMPRD * 1/ 100;   // set density duty
        TimerPWMCON |= BIT(0);
    }
    else if (HeadStbLowValid == 2)//old
    {
        //TimerPWMPWM = TimerPWMPRD * ((unsigned long)command.density * 2 / 3) / 100;
        TimerPWMPWM= TimerPWMPRD*95/100;
        TimerPWMCON |= BIT(0);
    }
}


//call back function for timer4 interrupt->this timer uses to control the step motor
static ___interrupt USIRAM void Timer4CallBackMotor(void)
{
	 //BaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();// Enter into critical area
	//  local_irq_disable();
    if (TimerStepMotorCON & BIT(15))//interrupt bit
	{
        TimerStepMotorCON |= BIT(14);//clear interrupt bit
        if(SMP.StepMotorFinished)//if decelerate finished then stop step motor operation
        {
        	StopStepMotor();//stop step motor
        }
		else
		{
			if(SMP.FindPaperEdge)//subsequent code is the find paper edge algorithm
			{
				FindFaperEdgeISR();//this function uses to find the paper edge
			}
			else if(SMP.HeadPrint==true)//subsequent code is the head print algorithm
			{
				PageDataCalculation();//page data calculation and print head  data output process
				ThermometerADJ();//when temperature increases decreases PWM ratio duty
			}
	        PhaseSwitching();// phase switching ,period time reload,lable size calculate process
	        SensorStatusISR();//check papersensor and cover status
		}
    }
	// __local_irq_enable();
	// taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);// exit critical area
}
void TwinkleLedErr(void)
{
	LEDErrTurnOn;vTaskDelay(pdMS_TO_TICKS(500));
	LEDErrTurnOff;vTaskDelay(pdMS_TO_TICKS(500));
}
void SensorErrDuringPrintProcess(void)
{
	while(CoverState)
	{
		TwinkleLedErr();
	}
}
bool StepMotorStatus(void)
{
	if(SMP.Status==Running) return 1;
	return 0;
}

//step motor starting entrance
void StepMotorStart(StepMP CurrentSMP)
{
	if(SMP.Status==Running) return;//if step motor running return to task idle
	Page.EliminateRedundanctData=false;//this variable will be set when the page's heighth supasses the label height a lot.
	Page.SensorError=false;
	Gap.GapIn=false;
	Gap.LockPage=false;
	SMP=CurrentSMP;
	SMP.Status=Running;
	if(SMP.Mode==ModeFixSteps)
	{
		if(SMP.StepsFixedValue<SMP.ACCSteps*2)
		{
			SMP.ACCSteps=SMP.StepsFixedValue/2;
		}
	}
	if(SMP.HeadPrint==true)
	{
		//gpio_output_channle(HeadstrobeIO, CH3_T3_PWM_OUT);
		//TimerPWMPWM= TimerPWMPRD*((unsigned long)command.density*2/3)/100;//set density duty ration
        //TimerPWMPWM= TimerPWMPRD*90/100;//set density duty ration

		//TimerPWMCON |= BIT(0);// tim3

        //heatTime = ceil(TableSpeedUS[SMP.ACCSteps] / 75);
       // printf("Heat time ...................%d",heatTime);

		StartPWM();
		if(Page.ContinuePrint==false){
			if(Gap.LabelLength)//if page length is valid use this page length as the pageheight;This only happened when is label height is shorter than GAP-Print gap
			{
				Page.RemainderSteps=Gap.LabelLength;//
			}
			else
				Page.RemainderSteps=2400;
		}
		printf("Set page height Page.RemainderSteps:%d",Page.RemainderSteps);
	}else{
		if(HeadStbLowValid==1)HeadstrobeEnable;
		else if(HeadStbLowValid==2) HeadstrobeDisable;
	}

	Gap.CountLable=0;
	Gap.StepsCount=0;
	Gap.StepsGapPos=0;

	TimerIntervalCON|=BIT(0);//tim1
	MOTOR_EN;
	vTaskDelay(pdMS_TO_TICKS(1));		//delay100ms
	TimerStepMotorCON|=BIT(0);//tim4

	printf("step motor runing...\n");
}
void FindFaperEdgeStart(void)//find paper edge configuration and operation queue
{

    printf("FindFaperEdgeStart.................................\n\n\n");
    uint16_t adc = ADCValue[PAPER_R_ADC];

	if(adc>500)//paper on the edge sensor
	{
		printf("Paper is on the edge sensor..\n");
		/*StepMP StepMPFindEdge={8,0,0,false,Backward,Stopped,ModeContinue,20,false,false,true,false};//1200 is the storage parameter
		StepMotorStart(StepMPFindEdge);
		while(SMP.Status==Running) vTaskDelay(pdMS_TO_TICKS(100));//waiting for step motor stopped
		StepMP MPCutPos={8,0,0,false,Forward,Stopped,ModeFixSteps,20,false,false,false,false};
		StepMotorStart(MPCutPos);
		while(SMP.Status==Running) vTaskDelay(pdMS_TO_TICKS(100));//waiting for step motor stopped*/
		printf("Find paper edge!\n");
		//vTaskDelay(pdMS_TO_TICKS(100));
	}
	else
	{
		printf("No paper in the printer..\n");
		while(CoverState) vTaskDelay(pdMS_TO_TICKS(100));//wait for cover open
	}
}
void FindGapOperation(void)
{

	GAP GapT={0};
	Gap.LableLengthCP=0;
	printf("Find gap..\n");
	Gap=GapT;//when cover open then set default value
	/*StepMP StepMPFindEdge={8,0,0,false,Forward,Stopped,ModeFixSteps,20,false,false,false,true,false};
	StepMotorStart(StepMPFindEdge);
	while(SMP.Status==Running) vTaskDelay(pdMS_TO_TICKS(100));//waiting for step motor stopped
	printf("Find gap end\n");
	vTaskDelay(pdMS_TO_TICKS(100));
    #if defined(MACHIN_TP876)
    StepMP StepMPFindEdgeA = {8,0,0,false,Backward,Stopped,ModeFixSteps,20,false,false,false,false,false};
    #elif defined(MACHIN_TP870)
    StepMP StepMPFindEdgeA = {8,0,0,false,Backward,Stopped,ModeFixSteps,20,false,false,false,false,false};
    #endif
	StepMotorStart(StepMPFindEdgeA);
	while(SMP.Status==Running) vTaskDelay(pdMS_TO_TICKS(100));//waiting for step motor stopped
	vTaskDelay(pdMS_TO_TICKS(100));*/


	if(Gap.LabelLength)
	{
		printf("find label paper size:%d\n",Gap.LabelLength);
		//10*30 = 98<100
		//30*40 = 295<300 big gap
		//30*40= 265<300 small gap
		//75*45= 425<450
		// 4 inc
        levelPapType=Gap.LabelLength<=200 ?1:    Gap.LabelLength<=450 && Gap.LabelLength>=200?2:0 ;
	}
	else
	{
        levelPapType=0;
		printf("find label paper failed LabelLength:%d;Gap:%d\n",Gap.LabelLength,Gap.LabelGapLength);
	}
	        printf("paper type.......................................; %d \n",levelPapType);

    vTaskDelay(pdMS_TO_TICKS(1000));
	//xEventGroupSetBits(Printer_event_group, PrinterReady_BIT);
	Gap.paperPosition=CutPos;
	Gap.StepsCount=0;	//reseek the gap!!
	Gap.LableLengthCP=0;
	if(Gap.LabelLength)//if found the label length when power on or cover close operation
	{
		Gap.LableLengthCP=Gap.LabelLength;
		printf("lable size calculate succesful!-%d\n",Gap.LableLengthCP);
	}
}
void CoverOperation(void)
{
	static bool CoverCloseOperation=true;
	if(CoverState)
	{
		xEventGroupClearBits(Printer_event_group,PrinterReady_BIT);
		printf("cover opens,waiting cover closed!-->\n");
		while(CoverState) TwinkleLedErr();
		CoverCloseOperation=true;
        gapCalculated=false;
		printf("cover closes..\n");

	}
	else
	{
		if(CoverCloseOperation)//only execuate when the power on situation
		{
			CoverCloseOperation=false;
			while(SMP.Status==Running) vTaskDelay(pdMS_TO_TICKS(200));
			FindFaperEdgeStart();//when power on and the cover is closed, then execute the find edge algorithm
			FindGapOperation();//when the label heighth shorter than the distance between gap and print position, this function is working
		}
		xEventGroupSetBits(Printer_event_group, PrinterReady_BIT);//cover closed and find paper edge finished
	}
}
extern void SaveWifiInfo(char *SSID,char *PASSWord);
extern void disableTCPTask(void);
extern void enablePrintTask(void);

static TickType_t bootTick = 0;
void SystemPowerOnInit(void)
{
    bootTick = xTaskGetTickCount();  // record power-on time
}



void KeyOperation(void)
{
	unsigned int Times = 0;
	uint16_t ADCKeyVal; // adc value
	bool buttonDetected = false;
	bool isSelfTest = false;

	// Read initial ADC value to determine which button
	ADCKeyVal = ADCValue[KEY_BOARD_ADC];

	if (ADCKeyVal > 130 && ADCKeyVal < 400) {
		isSelfTest = true;
		buttonDetected = true;
		printf("Self Test button detected\n");
	} else if (ADCKeyVal > 800) {
		isSelfTest = false;
		buttonDetected = true;
		printf("OTA button detected\n");
	}

	if (!buttonDetected) {
		return; // No valid button press
	}

	// For OTA button (ADC > 800), we need to adjust the loop condition
	if (!isSelfTest) {
		/// OTA button
		do
		{
			ADCKeyVal = ADCValue[KEY_BOARD_ADC]; // read key adc value
			vTaskDelay(pdMS_TO_TICKS(150));
			Times++;

			if (Times > 10)
			{
				if (!SwitchState)
					break;
				else
					LEDErrTurnOn;
			}
		} while (ADCKeyVal > 800); // Keep checking OTA button is still pressed (high ADC)
	} else {
		/// Self Test button
		do
		{
			ADCKeyVal = ADCValue[KEY_BOARD_ADC]; // read key adc value
			vTaskDelay(pdMS_TO_TICKS(150));
			Times++;

			if (Times > 10)
			{
				if (!SwitchState)
					break;
				else
					LEDErrTurnOn;
			}
		} while (ADCKeyVal > 130); // Original condition for button release
	}

	/// OTA Mode
	if (Times > 10 && !SwitchState && !isSelfTest)
	{
		printf("\n\n\n\n\n\nNetUpdata_demo_Task------->>>->>>\n\n\n");
		disableTCPTask(); // disable tcp server
		while (!SwitchState)
		{
			LEDErrTurnOff;
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		net_ota_single_back_init();
        printf("Net updata starting ------->>>->>>");
		while (1)
		{
			// Green LED OTA mode: blink 3 times @ 3Hz, then 3s close, loop
			for(int i = 0; i < 3; i++) {
				//LEDLineTurnOn;
				LED_PAPER_HIGH;
				vTaskDelay(pdMS_TO_TICKS(167));
				//LEDLineTurnOff;
				LED_PAPER_LOW;
				vTaskDelay(pdMS_TO_TICKS(167));
			}
			vTaskDelay(pdMS_TO_TICKS(3000));

			if (!SwitchState)
			{
				printf("power off from OTA\n");
				PowerOff;
			}
		}
	}

	/// Self Test Mode
	else if (Times > 4 && SwitchState && isSelfTest)
    {
      /*TickType_t currentTime = xTaskGetTickCount();
        TickType_t elapsedTime = currentTime - bootTick;
        if (elapsedTime > pdMS_TO_TICKS(5 * 60 * 1000))
        {
            printf("5 minutes Up not to self print. elapsed: %lu ticks\n", elapsedTime);
            return;
        }*/
        LEDErrTurnOff;
        printf("self print\n");
        SelfPrint();
	}
}



void OffHandle(void)
{
	static u8 powerStateCounter;
    static TickType_t pressStartTime;

	if (!SwitchState)

	{
	    //printf("Power state:: %d",SwitchState);
		if (((PowerLockGpio & PowerLockIOBit) >> 8) == 0x01 && powerStateCounter == 0)
		{
			powerStateCounter++; // set counter to 1
		}
		if (powerStateCounter == 2)
			powerStateCounter++;
		if (((PowerLockGpio & PowerLockIOBit) >> 8) == 0x00 && powerStateCounter == 3)
		{
			if ((xTaskGetTickCount() - pressStartTime) >= pdMS_TO_TICKS(3000))
			{
				PowerOff;
				//LEDLineTurnOff;
				CHARGING_D801_LOW;
				for (int i = 0; i < 3 ; i++)
				{
					//LEDErrTurnOn;
					CHARGING_D801_HIGH;
					vTaskDelay(pdMS_TO_TICKS(100));
					//LEDErrTurnOff;
					CHARGING_D801_LOW;
					vTaskDelay(pdMS_TO_TICKS(100));
				}
				printf("power off\n");
			}
		}
	}
	else
	{
		powerStateCounter = 2; // reset counter
		pressStartTime = xTaskGetTickCount();
	}
}
void PeripheralApparatusTask(void *p)//this task manage cover,paper..
{
    while(1)
    {
        OffHandle();
		KeyOperation();
		SetupGapEmitterPWM();
		vTaskDelay(pdMS_TO_TICKS(10));
    }
}
 void DataStreamControlTask(void *p)
 {
	 while(1)
	 {

		//USBPrinterReveiveDataTask();				 //USB receive data stream manegement
		 EdrPrinterReceiveDataTask();				//bluetooth data flow
	     BLEPrinterReceiveDataTask();

		 vTaskDelay(pdMS_TO_TICKS(10));
	 }
 }

 extern bool UserCPasswordR;
 extern unsigned char WiFiConnectStatus;

 char pcWriteBuffer[512];
  void IdleTask(void *p)
 {
	static char Delay=0;
	for(int i=0;i<10;i++)
	{
		if(UserCPasswordR==false)
			vTaskDelay(pdMS_TO_TICKS(500));
		if(WiFiConnectStatus==true)
			break;
	}
	task_create(PrintTask, NULL, "PrintTask");//communication data management task
	while(1)
	{
		//unsigned int NodeCount=_malloc_size(PrintBLink);//calculate parkets quantity
        // printf("NodeCount:%d-------->",NodeCount);
	// malloc_stats();
		//vTaskList(pcWriteBuffer);
		//printf("%s\n", pcWriteBuffer);
		//ADCValue[ADCPaperEdge];
		//printf("sang:%d,sang1:%d,sang2:%d\r\n",sang,sang1,sang2);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
 }
  /*
 void NetUpdata_demo_Task(void *p)
 {
	gpio_set_die(IO_PORTB_05,1);		//digital function
    gpio_set_pull_up(IO_PORTB_05,1);	//pull up enable
    gpio_set_pull_down(IO_PORTB_05, 0);	//pull down disable
    gpio_set_direction(IO_PORTB_05, 1);	//input
	while(1)
	{
		if(!gpio_read(IO_PORTB_05))
		{
			printf("Net updata starting -------->");
			//Net updata one backup updata demo,If this updata fails, use JL updata device to restore.
			//net_ota_single_back_init();
			while(1)
			{
				vTaskDelay(pdMS_TO_TICKS(1500));
			}
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
 }
 */
 /*void Timer2Start(void)
 {
	printf("timer1 configuration---------------\n");
	JL_TIMER2->CON = 0;				//clear all value to default
	JL_TIMER2->CNT = 0;
	JL_TIMER2->CON |= BIT(14);		//clear interrupt bit
	JL_TIMER2->CON |= 0x02 << 4; 	//lsb_clk fequency divider 16
	JL_TIMER2->PRD = 10000*10/3;		//period value 1ms,formula  PRD= us*10/3--10ms
	JL_TIMER2->CON|=BIT(0);
 }*/


extern void WiFiDataLinkLastPackage(void);
extern void FreeLastTempPackage(void);
/*static ___interrupt USIRAM void Timer2CallBackMotor(void)
{
	static unsigned int k=0;
	if (JL_TIMER2->CON & BIT(15))//interrupt bit
	{
        JL_TIMER2->CON |= BIT(14);	*/			//clear interrupt bit

		/*
		if(WiFiLastPackage)
		{
			WiFiDataLinkLastPackage();
			sang1++;
			WiFiLastPackage=false;
		}
		*/
    //}
//}

 void SPIUserDefine(void)
 {

 }
 //@brief Entrance APPPrintDemoMain
void APPPrintDemoMain(void)
{
	GPIO_Configuration();//GPIO configuration
	vTaskDelay(pdMS_TO_TICKS(1000));
	printf("APPPrintDemoMain entrance---------->\r\n");
	Printer_event_group=xEventGroupCreate();//Initial events groups
    xQueueNVS = xQueueCreate(10, sizeof(char[128]));//NVS
	_dev_open("timer4",0);//step motor timer
	_dev_open("timer1",0);//step motor timer
    _dev_open("timer3pwm",0);//step motor timer
	request_irq(IRQ_TIMER4_IDX, 3, Timer4CallBackMotor, 0);//register interrupt to callback function
	request_irq(IRQ_TIMER1_IDX, 3, Timer1CallBackMotor, 0);//register interrupt to callback function
	//request_irq(IRQ_TIMER2_IDX, 3, Timer2CallBackMotor, 0);//register interrupt to callback function
	//Timer2Start();
	timer1();
	//SetupGapEmitterPWM();
	adc_init();  //use JL API to initialize ADC
	StopStepMotor();//clear print buffer,set motor status to stop
	task_create(DataManagementTask, NULL, "DataManagementTask");
	task_create(PeripheralApparatusTask, NULL, "PeripheralApparatusTask");//peripheral device status management
	task_create(NVS_OperationTask, NULL, "NVS_OperationTask");//NVS operation task
	task_create(IdleTask, NULL, "IdleTask");
}
late_initcall(APPPrintDemoMain);
