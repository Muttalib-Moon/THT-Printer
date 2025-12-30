/*
******************************************************************************
* @file 	  AppDriverCommand.c
* @author	  Sang
* @date 	  2024-12
* @brief	  Printer command layer (application layer)
*
*			  This file provides firmware functions to manage the following
*			  functionalities:
*			  + data parsing instruction
*/
#include "AppDriverGeneric.h"//JL lib dependency
#include "AppDriver.h"
#include "queue.h"
#include "string.h"
#include "zlib.h"
#include "AsciiLib.h"
#include "../../common/ble/include/le_common.h"

#define MAX_BT_NAME_LEN  32
extern char edr_name[MAX_BT_NAME_LEN];
extern u8 sDeviceDescriptor[];
extern volatile u16 levelPapType;
volatile PDataPacket PrintBLink=NULL;//print linked list
unsigned long LineCount=0;
Command  command={NULL,0,0,0,CommandIdle,830,19,LablePaper,80,0,0,0,false,false,0,NULL,32,{0},0};//cpmmand processer buffer
const CommandHead CMDHead[CommandTotalNum]=//command list
{
{4,{0x40,0x53,0x4d,0x53}},//speed command--->CommandSpeed=0
{4,{0x40,0x53,0x7E,0x7E}},//density command-->CommandDensity=1
{4,{0x40,0x53,0x50,0x50}},//paper types--->CommandPaperType=2
{2,{"@<"}},//page start command--------->CommandPageStart=3
{2,{"#<"}},//line start command--------->CommandLineStart=4
{2,{"#>"}},//line end command----------->CommandLineEnd=5
{2,{"@>"}},//page end command------------>CommandPageEnd=6
{strlen("ANGENL"),{"ANGENL"}},//zip command----->CommandZipStart=7
{strlen("NVS-WiFi"),{"NVS-WiFi"}},//zip command-----commandNVSStart=8
{4, {0x40, 0x53, 0x42, 0x54}},   // @SBT // BT name
{4, {0x40, 0x53, 0x49, 0x44}},   // @SID // PID set
};
void ReturnModle(void)
{
	command.LineDecodeOneceFinshed=true;
	command.ReturnFlag=true;
	command.RemainderLength=command.TotalLength-command.ReadPos;
}
static unsigned int CalculateAccSteps(unsigned int Speed)
{

	for(unsigned int i=0;i<31;i++)
		if(Speed>=TableSpeedUS[i])return i;
	return 30;
}
void CommandSetSpeed(void)
{
	if(command.ReadPos+2<command.TotalLength)//if remainder data is enough
	{
		command.Speed=((unsigned int)command.Buffer[command.ReadPos])*256+command.Buffer[command.ReadPos+1];
		command.ReadPos+=2;
		command.AccSteps=CalculateAccSteps(command.Speed);

		//printf("Speed-->%d-Acc:%d\n",command.Speed,command.AccSteps);
		command.DecodeStatus=CommandIdle;//

		return;
	}
	ReturnModle();
}
void CommandSetDensity(void)
{
	if(command.ReadPos+1<command.TotalLength)//if remainder data is enough
	{
		command.density=command.Buffer[command.ReadPos++];
		if(HeadStbLowValid==1)
        {
            if(command.density<96)
            command.density=96;
        }
		else
        {
          if(command.density<45)
            command.density=45;
		}
		printf("density-->%d\n",command.density);
		command.DecodeStatus=CommandIdle;//
		return;
	}
	ReturnModle();
}
void CommandSetPaperType(void)
{
	if(command.ReadPos+1<command.TotalLength)//if remainder data is enough
	{
		command.PaperType=command.Buffer[command.ReadPos++];
		printf("PaperType----------------->%d\n",command.PaperType);
		command.DecodeStatus=CommandIdle;//
		return;
	}
	ReturnModle();
}

void CommandSetPageStart(void)
{
	command.DecodeStatus=CommandIdle;
	command.PageEndFlag=false;
	LineCount=0;
}
void WritePrintLinkedList(void)
{
	LinkedListData LLData;
	LLData.PageNum=command.PageNum;
	LLData.len= PrintlineBytes;//108
	LLData.data=(unsigned char*)malloc(LLData.len*sizeof(unsigned char));
	memcpy(LLData.data,command.DotsMatric,LLData.len);
	_malloc(&PrintBLink,LLData);//save data in linked list
	free(LLData.data);
}
void WritePrintLinkedListB(unsigned char *Buffer,unsigned int Len)
{
	LinkedListData LLData;
	LLData.PageNum=command.PageNum;
	LLData.len=Len;
	LLData.data=(unsigned char*)malloc(LLData.len*sizeof(unsigned char));
	memcpy(LLData.data,Buffer,LLData.len);
	_malloc(&PrintBLink,LLData);//save data in linked list
	free(LLData.data);
}


void CommandSetLineStart(void)
{
	static unsigned char DotValue=0;
	const unsigned char Cmp[]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
	command.LineDecodeOneceFinshed=false;												//set one line data decode finished flag-->false
	while(!command.LineDecodeOneceFinshed)												//wait line decode end
	{
		switch(command.LineDeCodeWrite)													//decode location
		{
			case UserDefineCommandHead:													//decode command head
				if(command.ReadPos+1<command.TotalLength)								//if remainder data is enough
				{
					if(command.Buffer[command.ReadPos]&0x80)							//repeat dots flag
					{
						command.LineDeCodeWrite=1;										//repeat dots calculate goes to the subsequent case
						DotValue=0;														//default value 0
						if(command.Buffer[command.ReadPos]&0x7f)						//gray realistic value
						{
							DotValue=1;													//set value-->1
						}
					}
					else																//on dot section case
					{
						if(command.Buffer[command.ReadPos]&0x7f&&command.Dots<576)		//gray judge
						{
							command.DotsMatric[command.Dots/8]|=Cmp[command.Dots%8];	//fill dots matric buffer
						}
						command.Dots++;													//record dots
					//	printf("1Dots:%d-%d-1\n",command.Dots,DotValue);
						if(command.Dots>=LineDecodeDots)								//line dots decodes finish ->left 16 bits empty
						{
							command.DecodeStatus=CommandIdle;
							command.LineDecodeOneceFinshed=true;
							WritePrintLinkedList();
							LineCount++;
							//printf("L2:%d-Dots:%d\n",LineCount,command.Dots);
						}
					}
					command.ReadPos++;
				}
				else
					ReturnModle();
				break;
			case UserDefineCommandData:															//successive section case
				if(command.ReadPos+1<command.TotalLength)										//if remainder data is enough
				{
					unsigned int LineDots=(unsigned int)command.Buffer[command.ReadPos++]+1;	//overlapped data -->successive dots count
					//if(command.Dots+LineDots>864)
						//printf("Dots:%d\n",command.Dots+LineDots);
					if(DotValue)
					{
						for(unsigned int i=0;i<LineDots&&(command.Dots+i)<576;i++)
						{
							command.DotsMatric[(command.Dots+i)/8]|=Cmp[(command.Dots+i)%8];	//fill dots matric buffer
						}
					}
					command.Dots+=LineDots;
					if(command.Dots>=LineDecodeDots)
					{
						command.DecodeStatus=CommandIdle;
						command.LineDecodeOneceFinshed=true;
						WritePrintLinkedList();
						LineCount++;
						//printf("L1:%d-Dots:%d\n",LineCount,command.Dots);
					}
					command.LineDeCodeWrite=0;// enter case decode data
				}
				else
					ReturnModle();
				break;

		}
	}
}







void CommandSetLineEnd(void)
{
	command.DecodeStatus=CommandIdle;
}
void CommandSetPageEnd(void)
{
	command.DecodeStatus=CommandIdle;
	command.PageEndFlag=true;
	command.PageNum++;//pages unique ID

	LinkedListData LLDataTempA;							//temporary buffer for one line
	_copy_PageNum(&PrintBLink,&LLDataTempA);			//read current page number

	printf("P:%d-%d\r\n",command.PageNum,LLDataTempA.PageNum);
}


void CommandSetBTName(void)
{
    unsigned int i;
    if(command.ReadPos + MAX_BT_NAME_LEN <= command.TotalLength)
    {
        memset(edr_name, 0, sizeof(edr_name));
        for(i = 0; i < MAX_BT_NAME_LEN - 1; i++)
        {
            edr_name[i] = command.Buffer[command.ReadPos + i];
        }
        edr_name[MAX_BT_NAME_LEN - 1] = '\0';
        const u8 *mac_addr = bt_get_mac_addr();
        char mac_suffix[6];
        sprintf(mac_suffix, "-%02X%02X%02X", mac_addr[5], mac_addr[4], mac_addr[3]);
        strncat(edr_name, mac_suffix, sizeof(edr_name) - strlen(edr_name) - 1);

        printf("BT Name:%s\n", edr_name);
        syscfg_write(CFG_BT_NAME, edr_name, MAX_BT_NAME_LEN);

        bt_ble_adv_enable(0);               // Stop advertising
        bt_ble_init();                       // Reinitialize with new name
        bt_ble_adv_enable(1);               // Restart advertising

        command.ReadPos += MAX_BT_NAME_LEN;
        command.DecodeStatus = CommandIdle;
        return;
    }
    ReturnModle();
}


void CommandSetPID(void)
{
    if (command.ReadPos + 2 < command.TotalLength) // need NL, NH
   {
        u8 pid_low = command.Buffer[command.ReadPos++];   // NL
        u8 pid_high = command.Buffer[command.ReadPos++];  // NH

        u16 pid = (pid_high << 8) | pid_low;

        printf("Setting USB PID to:: 0x%04X\n", pid);
        sDeviceDescriptor[10]  = pid & 0xFF;        // LSB
        sDeviceDescriptor[11] = (pid >> 8) & 0xFF; // MSB
        int ret = syscfg_write(CFG_USB_PID, &pid, sizeof(pid));

        printf("PID save ret=%d\n", ret);
        command.DecodeStatus = CommandIdle;
        return;
    }

    ReturnModle(); // Not enough data
}


static void IdleProces(void)
{
	for(int i=0;i<CommandTotalNum;i++)
	{
		if(command.ReadPos+CMDHead[i].len<command.TotalLength)//if remainder data is enough
		{
			if(!memcmp(command.Buffer+command.ReadPos,CMDHead[i].CommandContent,CMDHead[i].len))//compare command head
			{
				//printf("Find command:%d>\r\n",i);
				command.ReadPos+=CMDHead[i].len;//shift read
				command.DecodeStatus=i;			//DecodeStatus ID
				if(command.DecodeStatus==CommandLineStart)
				{
					command.LineDeCodeWrite=0;//decode function switch
					command.Dots=0;//16;
					memset(command.DotsMatric,0,PrintlineBytes);
				}
				return;
			}
		}
	}
	if(command.ReadPos+MAxHeadLength<=command.TotalLength)//if remainder data is enough
		command.ReadPos++;
	else
		ReturnModle();

}
static void CommandZipSize(void)
{
	if(command.ReadPos+2<=command.TotalLength)//if remainder data is enough
	{
		command.ZipSize=((unsigned int)command.Buffer[command.ReadPos])*256+command.Buffer[command.ReadPos+1];
		command.ReadPos+=2;
		//printf("Size:%d\n",command.ZipSize);
		command.DecodeStatus=CommandZipData;//
		return;
	}
	ReturnModle();
}

void ZipDecodeEntrance(void)
{
		command.DecodeStatus=CommandIdle;
		ReturnModle();
		if(false==ZipDecode(command.zipData,command.ZipSize))
		{
			command.DecodeStatus=CommandIdle;
		}
		//printf("X:%d-%d\r",command.TotalLength-command.ReadPos,command.DecodeStatus);
		free(command.zipData);
		command.zipData=NULL;

}
/*
static void CommandZipCheckSUM(void)
{
	unsigned long check_sum=0;
	if(command.ReadPos+4<=command.TotalLength)//if remainder data is enough
	{
		check_sum = (((unsigned long)command.Buffer[command.ReadPos])     << 24) |
            (((unsigned long)command.Buffer[command.ReadPos + 1]) << 16) |
            (((unsigned long)command.Buffer[command.ReadPos + 2]) <<  8) |
            ((unsigned long)command.Buffer[command.ReadPos + 3]);
		//printf("check_sum:%02x-%02x-%02x-%02x\r",command.Buffer[command.ReadPos],command.Buffer[command.ReadPos+1],command.Buffer[command.ReadPos+2],command.Buffer[command.ReadPos+3]);
		//printf("check_sum:%d,SUM:%d-%d\n",check_sum,SUM,command.Buffer[command.ReadPos]);
		if(SUM!=check_sum)
			printf("check_sum:%d,SUM:%d\n",check_sum,SUM);
		ZipDecodeEntrance();//
		command.ReadPos+=4;
		return;
	}
	ReturnModle();
}
*/
static void CommandZipFetchData(void)
{
	static unsigned int Write=0;
	if(command.zipData==NULL)
	{
		command.zipData=(unsigned char*)malloc(command.ZipSize*sizeof(unsigned char)); //memory for Zip Command
	}
	while(Write<command.ZipSize)
	{
		if(command.ReadPos+1<=command.TotalLength)//if remainder data is enough
		{
			command.zipData[Write++]=command.Buffer[command.ReadPos++];
			if(Write>=command.ZipSize)
			{
				//command.DecodeStatus=CommandZipCheck;
				ZipDecodeEntrance();
				Write=0;
				return;
			}
			continue;
		}
		ReturnModle();
		return;
	}

}
static void PrintDataProcesser(void)
{
	while(!command.ReturnFlag)// if status is return modle ,then exit the function
	{
		switch(command.DecodeStatus)
		{
			case CommandSpeed:CommandSetSpeed();break;
			case CommandDensity:CommandSetDensity();break;
			case CommandPaperType:CommandSetPaperType();break;
			case CommandPageStart:CommandSetPageStart();break;
			case CommandLineStart:CommandSetLineStart();break;
			case CommandLineEnd:CommandSetLineEnd();break;
			case CommandPageEnd:CommandSetPageEnd();break;
			case CommandZipStart:CommandZipSize();break;
			case CommandZipData:CommandZipFetchData();break;
			case commandNVSWiFi:CommandNVSWiFi();break;
			case commandSetBTName:CommandSetBTName();break;
            case commandSetPID:CommandSetPID();break;
			default:IdleProces();break;
		}
	}
}

void DataManagement(unsigned int len,unsigned char *data)
{
	_realloc(len,data,&command);//write process  memory
	PrintDataProcesser();//data process
}
#define DecodeMaxBytes 1000
//static uint8_t out[DecodeMaxBytes];
#define ZIP_HEAP_SIZE	20000
//static uint8_t zip_heap[ZIP_HEAP_SIZE];
static size_t zip_heap_offset = 0;
static uint8_t out[DecodeMaxBytes] __attribute__((aligned(4)));
static uint8_t zip_heap[ZIP_HEAP_SIZE] __attribute__((aligned(4)));
void *zip_alloc(void *opaque, unsigned int items, unsigned int size)
{
    size_t bytes = items * size;
    zip_heap_offset = (zip_heap_offset + 3) & ~3;
    if (zip_heap_offset + bytes > ZIP_HEAP_SIZE) {
        printf("memory scarce\r\n");
        return NULL;
    }
    void *p = &zip_heap[zip_heap_offset];
    zip_heap_offset += bytes;
    return p;
}
void zip_free(void *opaque, void *address)
{
    ;
}
void zip_reset_heap(void)
{
    zip_heap_offset = 0;
}

bool ZipDecode(unsigned char *ZipData,unsigned int len)
{
	unsigned long TotalL=0;
    DataPacket Dp;
    z_stream strm;
    int ret,J,k=0;
	unsigned long CurrentPos=0;
	unsigned char ZipTimes=0;
	ZIPAgain:
    // initialize data stream
   //printf(">",len);
    memset(&strm, 0, sizeof(strm));
	zip_reset_heap();
    strm.zalloc = zip_alloc;
    strm.zfree  = zip_free;
    strm.opaque = Z_NULL;
    ret = inflateInit2(&strm, -12); //   Deflate format
    if (ret != Z_OK)
	{
        printf("inflateInit2 failed with error: %d\n", ret);
        return false;
    }
	strm.avail_in=len;			//zip data len
	strm.next_in = ZipData; 	//pointer to the zip data
	// decode loop
	do
	{
		vTaskDelay(pdMS_TO_TICKS(10));
		wdt_clear();
		//printf("@",len);
		k++;
		strm.avail_out = DecodeMaxBytes; //set max size once
		strm.next_out = out;
    	ret = inflate(&strm, Z_NO_FLUSH);
		switch (ret)
		{
                case Z_NEED_DICT:
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
					malloc_stats();
                    printf("inflate failed with error: %d-%d-%d\n", ret,len,k);
                    inflateEnd(&strm);
					for(J=0;J<len;J++)
					{
						printf("%02X ",ZipData[J]);
					}
					/*
					while(1)
					{
						vTaskDelay(pdMS_TO_TICKS(100));

						ZipTimes++;
						if(ZipTimes>3)
							break;
						goto ZIPAgain;
					}
					*/
                    return false;
		}
		CurrentPos=strm.total_out-CurrentPos;
		TotalL+=CurrentPos;
        #if 0
		DataManagement(CurrentPos,out);
		#else
       while(PrintDataManagement(CurrentPos,out)==false)
        {
            os_time_dly(10);
        }
        #endif
		CurrentPos=strm.total_out;
		//printf("#",len);
	}
	while (strm.avail_out == 0);
    inflateEnd(&strm); // release resource
   // printf("ok:%d\r",_malloc_size(PWiFiPrinterDP));
	return true;
}

bool PrintDataManagement(unsigned int len,unsigned char *data)
{
	//taskENTER_CRITICAL(); //enter into the critical area
	unsigned int NodeCount=_malloc_size(PrintBLink);//calculate node quantity
	if(NodeCount<PrintLinkedListCount)
	{
		_realloc(len,data,&command);//write process  memory
		//taskEXIT_CRITICAL(); //enter into the critical area
		PrintDataProcesser();//data process
		return true;
	}
	//taskEXIT_CRITICAL(); //enter into the critical area
	return false;
}
//this function uses to run one empty page
void WriteEmptyPage(unsigned int Dots)
{
	unsigned char Buffer[1000];
	unsigned char D832[]={0x23,0x3C,0x80,0xFF,0x80,0xFF,0x80,0xFF,0x80,0x3F,0x23,0x3E};
	unsigned int Pwrite=0;
	memcpy(Buffer,"@<0000",strlen("@<0000"));
	Pwrite+=strlen("@<0000");
	for(int i=0;i<30;i++)
	{
		if(Dots==832)
		{
			memcpy(Buffer+Pwrite,D832,sizeof(D832));
			Pwrite+=sizeof(D832);
		}
	}
	memcpy(Buffer+Pwrite,"@>\r\n",strlen("@>\r\n"));
	Pwrite+=strlen("@>\r\n");
	WiFiPrinterReceiveDataTask(Buffer,Pwrite);
	printf("WriteEmptyPage:%d",Pwrite);
}
extern void WiFiStartManual(void);
extern TickType_t printTimeout;

void Print(void)
{
	CoverOperation();																									 // this function manage the cover operation
	EventBits_t bits = xEventGroupWaitBits(Printer_event_group, PrinterReady_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(100)); // waiting for the printer to be ready
	if (!(bits & PrinterReady_BIT) || SMP.Status == Running)
		return;										   // if printer cover detected and finding paper edge finished,and step motor stopped then enter into printing process.
	unsigned int NodeCount = _malloc_size(PrintBLink); // calculate node quantity of print buffer
    //printf("nodcount :%d ------------------>>>>\n\n\n\n",NodeCount);
    unsigned int OldNodeCount=NodeCount;

	if (NodeCount > PrintLinkedListSP){			   //PrintLinkedListSP bleStartPrint
		if (Gap.paperPosition == CutPos)
		{
        #if defined(MACHIN_TP876)
			StepMP SMPPrintPos = {8, 0, 0, false, Backward, Stopped, ModeFixSteps, 40, false, false, false, false, false}; // 154 //9
        #elif defined(MACHIN_TP870)
        StepMP SMPPrintPos = {8, 0, 0, false, Backward, Stopped, ModeFixSteps, 90, false, false, false, false, false}; // 154 //9
        #endif // defined

			StepMotorStart(SMPPrintPos);
			while (SMP.Status == Running)
				vTaskDelay(pdMS_TO_TICKS(100)); // waiting for step motor stopped
			pdMS_TO_TICKS(200);
			StepMP SMPPrintPos1 = {8, 0, 0, false, Forward, Stopped, ModeFixSteps, 1, false, false, false, false, false};//4//4
			StepMotorStart(SMPPrintPos1);
			while (SMP.Status == Running)
				vTaskDelay(pdMS_TO_TICKS(100)); // waiting for step motor stopped
			Gap.paperPosition = PrintPos;
			pdMS_TO_TICKS(200);
		}

		// StepMP SMPCurrent={command.AccSteps,0,0,false,Forward,Stopped,ModeContinue,0,false,true,false,true,true};//print parameters
        #if defined(MACHIN_TP876)
		StepMP SMPCurrent = {levelPapType==1?8:  levelPapType==2?8:8, 0, 0, false, Forward, Stopped, ModeContinue, 0, false, true, false, true, true}; // 10
		#elif defined(MACHIN_TP870)
        StepMP SMPCurrent = {levelPapType==1?8:  levelPapType==2?5:5, 0, 0, false, Forward, Stopped, ModeContinue, 0, false, true, false, true, true}; // print parameters//15//  for tp 870 15 according to milon vai
		#endif
		StepMotorStart(SMPCurrent);																				  // start motor to print
		for (; SMP.Status == Running;)
			vTaskDelay(pdMS_TO_TICKS(100)); // waiting for the print finished.
		if (Page.SensorError)
		{
			SensorErrDuringPrintProcess(); // sensor error return to task
			return;
		}
		vTaskDelay(pdMS_TO_TICKS(200));	  // stable delay
		if (Page.EliminateRedundanctData) // if program cann't eliminate the redundant data at the gap then executes this process
		{
			printf("enter into the moving redundant data process...");
			RedundantDataProcessing(true);
		}
		Page.ContinuePrint = false;


		if (command.PageEndFlag==false)
		{
			u8 keeptrying = 0;
			u8 keeptryingWithData=0;
			do
			{
				keeptrying++;
				keeptryingWithData=0;
                if (command.PageEndFlag==true)break;
			again:
				vTaskDelay(pdMS_TO_TICKS(800));		  // stable delay 800
				NodeCount = _malloc_size(PrintBLink); // calculate node quantity of print buffer

				if(OldNodeCount!=NodeCount)
                {
                    OldNodeCount=NodeCount;
                    keeptryingWithData=0;
                }
                printTimeout = xTaskGetTickCount();

                printf("NodeCount: %d > PrintLinkedListSP: %d keeptry:%d  keeptryingWithData: %d\n", NodeCount, PrintLinkedListSP, keeptrying ,keeptryingWithData);
				if (NodeCount > 0 && NodeCount < 400)
				{
					keeptryingWithData++;
					if(keeptryingWithData>15  || command.PageEndFlag==true )break;
					goto again;
				}
			} while (NodeCount == 0 && keeptrying <= 8);
		}else
		{
			vTaskDelay(pdMS_TO_TICKS(800));	  // stable delay 800
			NodeCount = _malloc_size(PrintBLink); // calculate node quantity of print buffer
		}
		if (NodeCount > PrintLinkedListSP)
		{
			printf("Print continue>>>  ");
			//printf("NodeCount: %d > PrintLinkedListSP: %d \n", NodeCount, PrintLinkedListSP);
			Page.ContinuePrint = true;
			return; // continue to print
		}
		printf("step motor runs to cut position... & NodeCount %d > PrintLinkedListSP %d \n", NodeCount, PrintLinkedListSP);
        #if defined(MACHIN_TP876)
		StepMP SMPCut = {8, 0, 0, false, Forward, Stopped, ModeFixSteps, 20, false, false, false, false};// tp876 165 according to milon vai
		#elif defined(MACHIN_TP870)
        StepMP SMPCut = {8, 0, 0, false, Forward, Stopped, ModeFixSteps, levelPapType >0 ? 62: 62, false, false, false, false};// tp876 165 according to milon vai
		#endif // defined
		StepMotorStart(SMPCut);
		while (SMP.Status == Running)
			vTaskDelay(pdMS_TO_TICKS(100)); // waiting for step motor stopped
		Gap.paperPosition = CutPos;
		// WiFiStartManual();
	}
}
/**
 * @brief PrintTask.
 * @param p:Task Parameter Pass.
 * @return none
 */
void PrintTask(void *p)
{
    int msg[16];
	int Count=0;
	printf("PrintTask...>>>>>>>>>>>>>>>>>>\n");
    while(1)
    {
    	Print();									    //print data
		__os_taskq_pend(msg, ARRAY_SIZE(msg), 100);
    }
}

