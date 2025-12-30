/* application layer
******************************************************************************
* @file 	  AppDriver.h
* @author	  Sang
* @date 	  2024-12
* @brief	  declare user defined macro and data structure
*			  This file is independent of CPU type, associated with FreeRTOS system.
*
*
*
*
*
*/

//#define     MACHIN_TP876// small
#define     MACHIN_TP870// big box

//macro for bit operation
#define BIT31   	0x80000000
#define BIT30   	0x40000000
#define BIT29   	0x20000000
#define BIT28   	0x10000000
#define BIT27   	0x08000000
#define BIT26   	0x04000000
#define BIT25   	0x02000000
#define BIT24   	0x01000000
#define BIT23   	0x00800000
#define BIT22   	0x00400000
#define BIT21   	0x00200000
#define BIT20   	0x00100000
#define BIT19   	0x00080000
#define BIT18   	0x00040000
#define BIT17   	0x00020000
#define BIT16   	0x00010000
#define BIT15  	 	0x00008000
#define BIT14   	0x00004000
#define BIT13   	0x00002000
#define BIT12   	0x00001000
#define BIT11   	0x00000800
#define BIT10   	0x00000400
#define BIT9     	0x00000200
#define BIT8     	0x00000100
#define BIT7     	0x00000080
#define BIT6     	0x00000040
#define BIT5     	0x00000020
#define BIT4     	0x00000010
#define BIT3     	0x00000008
#define BIT2     	0x00000004
#define BIT1     	0x00000002
#define BIT0     	0x00000001

#define Wifi_Obtain_IP 			BIT0		//cover status
#define FindEdgeFinished_BIT 	BIT1
#define PrinterReady_BIT 		BIT2



//Macro for some functions
#define DebugEnable				//debug information switch

#define LineDecodeDots				576//560//848				//compatible with windows driver 832 dots --->16 --832 16
//Macro for some data
#define MaxLinkedListCount			500 //500
#define RestartRxCount				50 // 200

#define PrintLinkedListCount		400			//limits
#define PrintLinkedListSP			30			//start print linked list count 30



typedef enum CommandStatus
{
	CommandSpeed,		//command speed 0
	CommandDensity,		//command dengsity 1
	CommandPaperType,	//command paper type 2
	CommandPageStart,	//command page start 3
	CommandLineStart,	//command line start 4
	CommandLineEnd,		//commandline end 5
	CommandPageEnd,		//command page end 6
	CommandZipStart,	//command zip command 7
	commandNVSWiFi,		//NVS WiFi command head 8
	commandSetBTName, // set the BT name
	commandSetPID,    // set PID
	CommandTotalNum,    //---command head end-----
	CommandZipCheck,
	CommandZipData,	    //zip data
	CommandNVSPID,		//NVS command data PID
	CommandIdle,		//idle modle ->find command head

}CMDStatus;




typedef enum PaperType
{
	LablePaper=0,
	BlackPaper,
	SuccessivePaper,
}PType;

typedef enum CommStatus
{
	CommIdle,
	CommSuspended,
}CStatus;

#ifdef __cplusplus
extern "C" {
#endif
//queue data for communication
typedef struct LinkedData
{
	unsigned int len;    		//data packet len
	unsigned char *data;		//data packet
	unsigned long PageNum;//max 4294967295‌

}LinkedListData;

// Linked List Structure for Communication data packet
typedef struct DATAPACKET
{
	LinkedListData LLData;
	struct DATAPACKET* next;   	//pointer to the next node

}DataPacket,*PDataPacket;

//USB data packet,The control of the USB data stream occurs outside of the interrupt callback.
typedef struct USBDataPacket
{
	PDataPacket DP;	//pointer to the current linked list head
	CStatus Status;			//USB device ID
}USBDP;

#define MAxHeadLength 8
typedef struct WTCommandHead
{
	unsigned char len;	//pointer to the current linked list head
	unsigned char CommandContent[MAxHeadLength];//USB device ID
}CommandHead;

#define PrintlineBytes		72//108
//attention:subsequent structure is crucial,don't modify the existed variables,but you can add your new command at the tail of this structure
typedef struct WTCommand
{
	unsigned char *Buffer;				//print data temporary buffer--------->don't modify
	unsigned int TotalLength;			//print data temporary buffer lenght--------->don't modify
	unsigned int RemainderLength;		//record remainder data length--------->don't modify
	unsigned int ReadPos;				//record read position--------->don't modify
	unsigned char DecodeStatus;			//decode status
	unsigned int Speed;					//print speed
	unsigned int AccSteps;				//this value transfer from Speed
	unsigned char PaperType;			//paper type
	unsigned char density;				//strobe duty ration
	unsigned int PageLength;			//page length
	unsigned int ZipLength;				//Zip document length
	unsigned int LineDeCodeWrite;		//decode print data position
	bool ReturnFlag;					//this value set1 data process will be suspend and return to task immediately--------->don't modify
	bool LineDecodeOneceFinshed;
	//---subsequent variable are associated with zip command
	unsigned int ZipSize;
	unsigned char *zipData;
	unsigned int Dots;					//dots in one line,this is a temporary variable to record the fill position
	//----subsequent variable are associated with line print command
	unsigned char DotsMatric[PrintlineBytes*2];
	unsigned long PageNum;		//record the page ID
	//---------add your new command variables below ,don't modify the up variables in order to avert unexpectable errors----------------
	bool PageEndFlag;



}Command;

extern Command command;



//linked list API
void _malloc(DataPacket** head,LinkedListData DataTable);
unsigned char _copy_memery(DataPacket**,DataPacket *);//free one packet and return one packet data,return false when none data left
unsigned char _copy_PageNum(DataPacket** head,DataPacket *DP);

void _free(DataPacket**);
unsigned int _malloc_size(DataPacket*);
void _realloc(unsigned int len,unsigned char *data,Command *command);
void _dev_open(char*,unsigned long);	//API open Perisheral device such as SPI timer
void _dev_write_spi(unsigned char *data,unsigned char len);//API write SPI2
//unsigned int LinkedListCount(DataPacket* head);
unsigned long PageNumCurrent(DataPacket** DataType);
bool CopyMemoryISR(DataPacket** DataType,LinkedListData *LLData);
bool CopyPageNumISR(DataPacket** DataType,LinkedListData *LLData);

bool PrintDataManagement(unsigned int len,unsigned char *data);


//bool PrintDataManagement(unsigned int len,unsigned char *data);
bool ZipDecode(unsigned char *ZipData,unsigned int len);
void StepMotorStart(StepMP CurrentSMP);

void DataManagementTask(void *p);
void PrintTask(void *p);
void WiFiPrinterReceiveDataTask(unsigned char *data,unsigned int len);
void EdrPrinterReceiveDataISR(unsigned char *data,unsigned int len);
void NVS_OperationTask(void *p);

//declaration global variable
extern USBDP USBdp;
extern PDataPacket PWiFiPrinterDP;//Wifi data paket structure
extern PDataPacket PEdrDP;
extern volatile PDataPacket PrintBLink;//print linked list
extern	EventGroupHandle_t Printer_event_group;
extern	QueueHandle_t xQueueNVS;


extern unsigned int sang;
extern unsigned int sang1;
extern unsigned int sang2;
extern bool WiFiLastPackage;
#ifdef __cplusplus
}
#endif


