#include "typedef.h"

#include "app_config.h"  //a
#include "system/includes.h" //a
#include "wifi/wifi_connect.h" //a
#include "lwip.h"
#include "dhcp_srv/dhcp_srv.h"
#include "event/net_event.h"
#include "net/assign_macaddr.h"
#include "syscfg_id.h"
#include "lwip/sockets.h"
#include "wifi_app_task_Print.h"
#include "sock_api/sock_api.h"
#include "os/os_api.h"
#include "lwip/netdb.h"
#include "UsbPrinter.h"
#include "device/gpio.h"
#include "system/timer.h"
//#include "usb/device/hid.h"

#include "Printer/MyDriver.h"
#include "Printer/isr.h"
#include "Printer/UserInterface.h"



u8 USB_stateS=USB_NormalState;

extern Device_Rev *InputCOM_device;
extern u32 channel_release; 

u32 epS_usb;


struct   usb_device_t usb_deviceS_print;
usb_dev  usb_id_print;
u8   epBuffcym_USB[MAXP_SIZE_HIDOUT];
u32  RXLenCYM_USB=0;

//struct   usb_device_t usb_deviceS;
//usb_dev  usb_id;

u8 ep_buffer[MAXP_SIZE_HIDOUT];


extern u16 Page_CutPosition;
extern u16 Page_CutContinue;
extern bool Page_CutFlag;
extern u8 firstTimeFlag;
//bool angenl_Continue=true;
//u16 Page_CutPosition=0;
//u16 Page_CutContinue=0;
//bool Page_CutFlag=false;
extern bool data_usb_channel_busy_flag;
extern bool data_bt_channel_busy_flag;
extern bool data_wifi_channel_busy_flag;
extern bool data_le_channel_busy_flag;


extern u8 busy_timer_counter;

//#include "..\\apps\wifi_story_machine/user/UserInterface.h"
//#include "..\\user/McuIO.h"
//#include "..\\apps\wifi_story_machine/user/McuIO.h"





void USBDataOut(unsigned char *buffer,unsigned int len);
unsigned long USBBuferGetEmptySize(void);
static void Putbyte(char a)
{
	int msg[16],ret = 0;

	while ((JL_UART0->CON0 & BIT(15)) == 0)
	{
		ret = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);
		ADC_Read();
	}

	JL_UART0->BUF = a;
	while ((JL_UART0->CON0 & BIT(15)) == 0)
	{
	}
	JL_UART0->CON0 |= BIT(13);//clear pending
}

extern u8 sDeviceDescriptor[];

//extern void USBInitial(void);
Device_Rev USB_Rev=
{
	USBInitial,
	USBSaveData,
	USBBuferGetEmptySize,
	USBCheckBufferEmptySize,
	USBDataProcesser,
	USBDataIn,
	USBDataOut,
	USBRefreshRev,
	//0,	 // @Alke init same 初始化必须相等
	1,  //@20240319 SangStoneAngleAlke close
	0,
	0,
	0,
	0,
	0,
	0
};
Device_Rev *USB_RevC;
unsigned long TTU1=0,TTU2=0;

// @20230713 Alke USB回传在此应用中已经可以
//extern Device_Rev *USB_RevC;
unsigned char UsbDataBack[64*3] = {"Usb to PC!\r\n"};
unsigned char cUsbDataBack = 0;

struct netif_info netif_infoA;
extern void lwip_get_netif_info(u8 is_wireless, struct netif_info *info);

// @20240502 此处在纸张就绪后，仅发送一次信号给上位机。张耿确认，USB回传必须等待上位机读取数据后，才能成功上传。
// 系统定时器中断中，应也可以回传，重复回传回导致关机，估计是因为上位机没能及时读取数据。
// 旧程序：收到命令0x1b 0x90，返回改数据
void USBReturnPaper(void)
{
	u8 c8cc;

	c8cc = 0;
	UsbDataBack[c8cc++] = 0x1b;
	UsbDataBack[c8cc++] = 0x85;
	UsbDataBack[c8cc++] = 0;	//PaperfrontOK
	UsbDataBack[c8cc++] = 1;	//Printfront

	UsbDataBack[c8cc++] = 0;	// 2\3\4传感器
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = '0';
	UsbDataBack[c8cc++] = '1';
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;

	UsbDataBack[c8cc++] = 0x64;	//TherMalResister=100; 0x64
	UsbDataBack[c8cc++] = 0x00;
	UsbDataBack[c8cc++] = 0;	//ErrFlag88

	UsbDataBack[c8cc++] = 23;	//HeadDelayCount /*高速成列度*/
	UsbDataBack[c8cc++] = 0;

	UsbDataBack[c8cc++] = 1;	//0x01 /*高速陈列度*/
	UsbDataBack[c8cc++] = 0;	//

	UsbDataBack[c8cc++] = 0;	//0x01 /*高密陈列度*/
	UsbDataBack[c8cc++] = 0;

	UsbDataBack[c8cc++] = 0;	//Temp //传感器电压
	UsbDataBack[c8cc++] = 0;

	UsbDataBack[c8cc++] = 0x04;	//Temp //RightVoltagePaper
	UsbDataBack[c8cc++] = 0xc3;
	UsbDataBack[c8cc++] = 18;	// USB 握手参数

	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;
	UsbDataBack[c8cc++] = 0;

	USB_RevC->DataOut(UsbDataBack, 32); // sizeof(UsbDataBack) // ONLY 32 byteS for angenl software
}
// 0x1B 0X0B 0X05 THT 0X0B PID 0X0B IP 0X0B MCUID 0X0B NAME 0X0B SSID 0X0A // WIFI中询问时回复的信息，需要根据相关信息安装打印机驱动程序
void USBReturnWifi(unsigned char Step) // 最多64字节
{
	u8 flag = 0;
	u8 c8cc = 0;
	u8 c8d1 = 0;

	c8cc = 0;
	switch (Step)
	{
	default:
	case 1:
		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x12;
		UsbDataBack[c8cc++] = 'U';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'B';
		UsbDataBack[c8cc++] = ' ';
		UsbDataBack[c8cc++] = 'P';
		UsbDataBack[c8cc++] = 'r';
		UsbDataBack[c8cc++] = 'i';
		UsbDataBack[c8cc++] = 'n';
		UsbDataBack[c8cc++] = 't';
		UsbDataBack[c8cc++] = 'e';
		UsbDataBack[c8cc++] = 'r';
		UsbDataBack[c8cc++] = '!';
		UsbDataBack[c8cc++] = 0x0a;
		break;

	case 2:
		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x02;
		UsbDataBack[c8cc++] = 'W';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'F';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = ' ';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'D';
		UsbDataBack[c8cc++] = '+';
		UsbDataBack[c8cc++] = 'P';
		UsbDataBack[c8cc++] = 'W';
		UsbDataBack[c8cc++] = 'D';
		UsbDataBack[c8cc++] = '!';
		UsbDataBack[c8cc++] = 0x0a;
		break;

	case 3:
		lwip_get_netif_info(1, &netif_infoA);

		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x03;
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'P';
		UsbDataBack[c8cc++] = '"';

		flag = 0; // 非0数据，后面为0也得显示
		c8d1 = netif_infoA.ip&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag) // 首位非0，此时0也得显示
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;
		UsbDataBack[c8cc++] = '.';

		flag = 0;
		c8d1 = (netif_infoA.ip>>8)&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag)
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;
		UsbDataBack[c8cc++] = '.';

		flag = 0;
		c8d1 = (netif_infoA.ip>>16)&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag)
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;
		UsbDataBack[c8cc++] = '.';

		flag = 0;
		c8d1 = (netif_infoA.ip>>24)&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag)
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;

		UsbDataBack[c8cc++] = '"';
		UsbDataBack[c8cc++] = 0x0a; // '\n';
		break;

	case 4:
		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x04;
		UsbDataBack[c8cc++] = 'W';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'F';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = ' ';
		UsbDataBack[c8cc++] = 'R';
		UsbDataBack[c8cc++] = 'E';
		UsbDataBack[c8cc++] = 'A';
		UsbDataBack[c8cc++] = 'D';
		UsbDataBack[c8cc++] = 'Y';
		UsbDataBack[c8cc++] = '!';
		UsbDataBack[c8cc++] = 0x0a;
		break;

	case 5:
		lwip_get_netif_info(1, &netif_infoA);

		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x05;
		UsbDataBack[c8cc++] = 'U';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'B';
		UsbDataBack[c8cc++] = 0x0b;//'&';
		c8d1 = sDeviceDescriptor[11]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = sDeviceDescriptor[11]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = sDeviceDescriptor[10]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = sDeviceDescriptor[10]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		UsbDataBack[c8cc++] = 0x0b;//'&';

		flag = 0; // 非0数据，后面为0也得显示
		c8d1 = netif_infoA.ip&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag) // 首位非0，此时0也得显示
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;
		UsbDataBack[c8cc++] = '.';

		flag = 0;
		c8d1 = (netif_infoA.ip>>8)&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag)
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;
		UsbDataBack[c8cc++] = '.';

		flag = 0;
		c8d1 = (netif_infoA.ip>>16)&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag)
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;
		UsbDataBack[c8cc++] = '.';

		flag = 0;
		c8d1 = (netif_infoA.ip>>24)&0x000000ff;
		if(c8d1 > 99)
			{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
		c8d1 %= 100;
		if(c8d1 > 9)
			UsbDataBack[c8cc++] = 0x30+c8d1/10;
		else if(flag)
			UsbDataBack[c8cc++] = 0x30;
		c8d1 %= 10;
		UsbDataBack[c8cc++] = 0x30+c8d1;

		// @20231224 Alke use mac_address as ID number
		const u8 *mac_addr;
		mac_addr = bt_get_mac_addr();
		//printf("MAC:%02X%02X%02X%02X%02X%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		UsbDataBack[c8cc++] = 0x0b;//'&';

		c8d1 = mac_addr[5-0]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = mac_addr[5-0]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

		c8d1 = mac_addr[5-1]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = mac_addr[5-1]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

		c8d1 = mac_addr[5-2]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = mac_addr[5-2]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

		c8d1 = mac_addr[5-3]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = mac_addr[5-3]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

		c8d1 = mac_addr[5-4]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = mac_addr[5-4]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

		c8d1 = mac_addr[5-5]>>4;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
		c8d1 = mac_addr[5-5]&0x0f;
		UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

		UsbDataBack[c8cc++] = 0x0b;//'&';
		for(c8d1 = 0; c8d1 < 16; c8d1++) //64
		{
			if(TABLEName[c8d1])
				UsbDataBack[c8cc++] = TABLEName[c8d1];
			else
				c8d1 = 64; // close
		}

		UsbDataBack[c8cc++] = 0x0b;//'&';//0x0b;
		struct wifi_mode_info info;
		info.mode = STA_MODE;
		wifi_get_mode_cur_info(&info);
		c8d1 = 0;
		while(c8d1 < 33) // 64
		{
			if(info.ssid[c8d1])
			{
				UsbDataBack[c8cc++] = info.ssid[c8d1];
				c8d1++;
			}
			else
				break;
		}
		UsbDataBack[c8cc++] = 0x0a;
		break;

	case 6:	// NOT_FIND_SSID
		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x06;
		UsbDataBack[c8cc++] = 'N';
		UsbDataBack[c8cc++] = 'O';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = '_';
		UsbDataBack[c8cc++] = 'F';
		UsbDataBack[c8cc++] = 'O';
		UsbDataBack[c8cc++] = 'U';
		UsbDataBack[c8cc++] = 'N';
		UsbDataBack[c8cc++] = 'D';
		UsbDataBack[c8cc++] = '_';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'D';
		UsbDataBack[c8cc++] = ' ';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = 'r';
		UsbDataBack[c8cc++] = 'y';
		UsbDataBack[c8cc++] = ' ';
		UsbDataBack[c8cc++] = 'r';
		UsbDataBack[c8cc++] = 'e';
		UsbDataBack[c8cc++] = 's';
		UsbDataBack[c8cc++] = 't';
		UsbDataBack[c8cc++] = 'a';
		UsbDataBack[c8cc++] = 'r';
		UsbDataBack[c8cc++] = 't';
		UsbDataBack[c8cc++] = 'i';
		UsbDataBack[c8cc++] = 'n';
		UsbDataBack[c8cc++] = 'g';
		UsbDataBack[c8cc++] = ' ';
		UsbDataBack[c8cc++] = 't';
		UsbDataBack[c8cc++] = 'h';
		UsbDataBack[c8cc++] = 'e';
		UsbDataBack[c8cc++] = ' ';
		UsbDataBack[c8cc++] = 'p';
		UsbDataBack[c8cc++] = 'r';
		UsbDataBack[c8cc++] = 'i';
		UsbDataBack[c8cc++] = 'n';
		UsbDataBack[c8cc++] = 't';
		UsbDataBack[c8cc++] = 'e';
		UsbDataBack[c8cc++] = 'r';
		UsbDataBack[c8cc++] = 0x0a;
		break;

	case 7:	// ASSOCIAT_FAIL
		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x07;
		UsbDataBack[c8cc++] = 'A';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'O';
		UsbDataBack[c8cc++] = 'C';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'A';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = '_';
		UsbDataBack[c8cc++] = 'F';
		UsbDataBack[c8cc++] = 'A';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'L';
		UsbDataBack[c8cc++] = '!';
		UsbDataBack[c8cc++] = 0x0a;
		break;

	case 8:	// ASSOCIAT_TIMEOUT
		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x08;
		UsbDataBack[c8cc++] = 'A';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'S';
		UsbDataBack[c8cc++] = 'O';
		UsbDataBack[c8cc++] = 'C';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'A';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = '_';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = 'M';
		UsbDataBack[c8cc++] = 'E';
		UsbDataBack[c8cc++] = 'O';
		UsbDataBack[c8cc++] = 'U';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = '!';
		UsbDataBack[c8cc++] = 0x0a;
		break;

	case 9:	//DHCP_TIMEOUT
		UsbDataBack[c8cc++] = 0x1b;
		UsbDataBack[c8cc++] = 0x0b;
		UsbDataBack[c8cc++] = 0x09;
		UsbDataBack[c8cc++] = 'D';
		UsbDataBack[c8cc++] = 'H';
		UsbDataBack[c8cc++] = 'C';
		UsbDataBack[c8cc++] = 'P';
		UsbDataBack[c8cc++] = '_';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = 'I';
		UsbDataBack[c8cc++] = 'M';
		UsbDataBack[c8cc++] = 'E';
		UsbDataBack[c8cc++] = 'O';
		UsbDataBack[c8cc++] = 'U';
		UsbDataBack[c8cc++] = 'T';
		UsbDataBack[c8cc++] = '!';
		UsbDataBack[c8cc++] = 0x0a;
		break;
	}
	while(c8cc < 64)
		UsbDataBack[c8cc++] = 0;

	USB_RevC->DataOut(UsbDataBack, sizeof(UsbDataBack));
}
void WifiReturnSize(void)
{
	u8 c8cc = 0;
	u16 c16c1 = 0;
	u8 c8d1 = 0;

	c8cc = 0;
	UsbDataBack[c8cc++] = 0x1b;
	UsbDataBack[c8cc++] = 0x0b;
	UsbDataBack[c8cc++] = 0x60;

	if(MEP.SocketPread<=MEP.SocketPwrite)
		c16c1 = MEP.SocketPwrite-MEP.SocketPread;
	else
		c16c1 = MAXBufferReceiveSize-MEP.SocketPread+MEP.SocketPwrite;
	UsbDataBack[c8cc++] = c16c1>>8;
	UsbDataBack[c8cc++] = c16c1&0x00ff;

	UsbDataBack[c8cc++] = 0x0a;
//	while(c8cc < 64)
//		UsbDataBack[c8cc++] = 0;

	tcp_send_data_sang(UsbDataBack, 6); // sizeof(UsbDataBack)
}
void WifiReturnMsg(void)
{
	u8 flag = 0;
	u8 c8cc = 0;
	u8 c8d1 = 0;

	lwip_get_netif_info(1, &netif_infoA);

	c8cc = 0;
	UsbDataBack[c8cc++] = 0x1b;
	UsbDataBack[c8cc++] = 0x0b;
	UsbDataBack[c8cc++] = 0x05;
	UsbDataBack[c8cc++] = 'W';
	UsbDataBack[c8cc++] = 'I';
	UsbDataBack[c8cc++] = 'F';
	UsbDataBack[c8cc++] = 'I';
	UsbDataBack[c8cc++] = 0x0b;//'&';
	c8d1 = sDeviceDescriptor[11]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = sDeviceDescriptor[11]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = sDeviceDescriptor[10]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = sDeviceDescriptor[10]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	UsbDataBack[c8cc++] = 0x0b;//'&';

	flag = 0; // 非0数据，后面为0也得显示
	c8d1 = netif_infoA.ip&0x000000ff;
	if(c8d1 > 99)
		{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
	c8d1 %= 100;
	if(c8d1 > 9)
		UsbDataBack[c8cc++] = 0x30+c8d1/10;
	else if(flag) // 首位非0，此时0也得显示
		UsbDataBack[c8cc++] = 0x30;
	c8d1 %= 10;
	UsbDataBack[c8cc++] = 0x30+c8d1;
	UsbDataBack[c8cc++] = '.';

	flag = 0;
	c8d1 = (netif_infoA.ip>>8)&0x000000ff;
	if(c8d1 > 99)
		{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
	c8d1 %= 100;
	if(c8d1 > 9)
		UsbDataBack[c8cc++] = 0x30+c8d1/10;
	else if(flag)
		UsbDataBack[c8cc++] = 0x30;
	c8d1 %= 10;
	UsbDataBack[c8cc++] = 0x30+c8d1;
	UsbDataBack[c8cc++] = '.';

	flag = 0;
	c8d1 = (netif_infoA.ip>>16)&0x000000ff;
	if(c8d1 > 99)
		{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
	c8d1 %= 100;
	if(c8d1 > 9)
		UsbDataBack[c8cc++] = 0x30+c8d1/10;
	else if(flag)
		UsbDataBack[c8cc++] = 0x30;
	c8d1 %= 10;
	UsbDataBack[c8cc++] = 0x30+c8d1;
	UsbDataBack[c8cc++] = '.';

	flag = 0;
	c8d1 = (netif_infoA.ip>>24)&0x000000ff;
	if(c8d1 > 99)
		{UsbDataBack[c8cc++] = 0x30+c8d1/100; flag = 1;}
	c8d1 %= 100;
	if(c8d1 > 9)
		UsbDataBack[c8cc++] = 0x30+c8d1/10;
	else if(flag)
		UsbDataBack[c8cc++] = 0x30;
	c8d1 %= 10;
	UsbDataBack[c8cc++] = 0x30+c8d1;

	// @20231224 Alke use mac_address as ID number
	const u8 *mac_addr;
	mac_addr = bt_get_mac_addr();
	//printf("MAC:%02X%02X%02X%02X%02X%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	UsbDataBack[c8cc++] = 0x0b;//'&';

	c8d1 = mac_addr[5-0]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = mac_addr[5-0]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

	c8d1 = mac_addr[5-1]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = mac_addr[5-1]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

	c8d1 = mac_addr[5-2]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = mac_addr[5-2]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

	c8d1 = mac_addr[5-3]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = mac_addr[5-3]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

	c8d1 = mac_addr[5-4]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = mac_addr[5-4]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

	c8d1 = mac_addr[5-5]>>4;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));
	c8d1 = mac_addr[5-5]&0x0f;
	UsbDataBack[c8cc++] = ((c8d1<10)?(c8d1+'0'):(c8d1-10+'A'));

	UsbDataBack[c8cc++] = 0x0b;//'&';
	for(c8d1 = 0; c8d1 < 16; c8d1++)
	{
		if(TABLEName[c8d1])
			UsbDataBack[c8cc++] = TABLEName[c8d1];
		else
			c8d1 = 64;
	}

	UsbDataBack[c8cc++] = 0x0b;//'&';//0x0b;
	struct wifi_mode_info info;
	info.mode = STA_MODE;
	wifi_get_mode_cur_info(&info);
	c8d1 = 0;
	while(c8d1 < 33) //64
	{
		if(info.ssid[c8d1])
		{
			UsbDataBack[c8cc++] = info.ssid[c8d1];
			c8d1++;
		}
		else
			break;
	}


	UsbDataBack[c8cc++] = 0x0a;
	while(c8cc < 64)
		UsbDataBack[c8cc++] = 0;

	tcp_send_data_sang(UsbDataBack, sizeof(UsbDataBack));
}


void USBInitial(void)
{
	USB_RevC->PBufferRev=(unsigned char*)malloc(2000);
	task_create(USBPrinterTask, NULL, "USBPrinterTask");
}
void USBCheckState(void)
{

//  struct   usb_device_t usb_deviceS_print;
//  usb_dev  usb_id_print;

  if(GetUSBEmptyBuff(1)>BufferReceiveSizeEmpty)
  {
    if(USB_stateS==USB_BusyState)
    {

		USB_stateS=USB_NormalState;


		#if 1
			USB_stateS=USB_NormalState;

			USB_RevC->IDHandle =usb_device2id(&usb_deviceS_print);
			//usb_id_print =  usb_device2id(&usb_deviceS_print);
			RXLenCYM_USB = usb_g_bulk_read(USB_RevC->IDHandle, USB_RevC->EP, epBuffcym_USB, MAXP_SIZE_HIDOUT,0);
			//RXLenCYM_USB = usb_g_bulk_read(usb_id_print, epS_usb, epBuffcym_USB, MAXP_SIZE_HIDOUT,0);
			//USBDataIn(RXLenCYM_USB,epBuffcym_USB);
			USB_RevC->SaveData(RXLenCYM_USB,epBuffcym_USB);
			//printf("In usb check function:: USB_stateS:  %d    USBID: %d  \n",USB_stateS,USB_RevC->IDHandle) ;
		#endif
	}
  }
}

void USBSaveData(unsigned      long  sizebytes,unsigned char *BufferReceive)
{

	//unsigned int i;
	//	TTU2+=sizebytes;
	//printf("U0");
		if((USB_RevC->Pwrite+sizebytes)<=BufferReceiveSIZE)
		{
			memcpy(&USB_RevC->PBufferRev[USB_RevC->Pwrite], BufferReceive, sizebytes);
			if((USB_RevC->Pwrite+sizebytes)==BufferReceiveSIZE)
				USB_RevC->Pwrite=0;
			else
				USB_RevC->Pwrite=USB_RevC->Pwrite+sizebytes;
	//printf("U1");
		}
		else
		{
			memcpy(&USB_RevC->PBufferRev[USB_RevC->Pwrite], BufferReceive, BufferReceiveSIZE-USB_RevC->Pwrite);
			memcpy(USB_RevC->PBufferRev, &BufferReceive[BufferReceiveSIZE-USB_RevC->Pwrite], USB_RevC->Pwrite+sizebytes-BufferReceiveSIZE);
			//USB_RevC->Pwrite+=sizebytes-USBMAXBufferReceiveSize; //
			USB_RevC->Pwrite=USB_RevC->Pwrite+sizebytes-BufferReceiveSIZE;
			// @Alke @202310231940 w=w+s-m正确，原来是w+=s-m会出错
	//printf("U2");
		}
    //printf("In usb save data *********************************  \n");

}



unsigned long USBBuferGetEmptySize(void)//
{
	if(USB_RevC->Pread<=USB_RevC->Pwrite)
	{
		return USBMAXBufferReceiveSize+USB_RevC->Pread-USB_RevC->Pwrite;
	}
	else
	{
        return USB_RevC->Pread-USB_RevC->Pwrite;

	}
}
unsigned char USBCheckBufferEmptySize(void)
{
	//if(USB_RevC->BuferGetEmptySize()<USBRevBusyLevelValue)
	if(USBBuferGetEmptySize()<USBRevBusyLevelValue) // @Alke @20231018
	{
		return true;
	}
	return false;
}
void USBRefreshRev(void)
{
	if(USB_RevC->Waitting)
	{
		if(USB_RevC->BuferGetEmptySize()>USBRevRefreshLevelValue)
		{
//			USB_RevC->DataIn();
        #if DebugMsg
			printf("sang:USB-Busy clear..............................\r\n");
        #endif
		}
	}
}

// USB
void PrinterTask(unsigned char data)
{
	//InstructionParser(data, UsbData);//DealData(data);

}


// every 10ms run one time
void USBDataProcesser(void)
{
	static u16 c16ForLedLine=0;

	while(1)
	{
        //if(BufferPrintLineWrited() > BufferPrintLineFull) // Alke @20230629
        //    return;
		#if 0
		// @20240502 此处在纸张就绪后，仅发送一次信号给上位机。张耿确认，USB回传必须等待上位机读取数据后，才能成功上传。
		// 系统定时器中断中，应也可以回传，重复回传回导致关机，估计是因为上位机没能及时读取数据。
		if(0)//if((PaperState == sPaperFrontNormal)||(PaperState == sPaperBackNormal))
		{
			if(c16ForLedLine<110)	c16ForLedLine++;

			if(c16ForLedLine==100) // only send one time
			{
				USBReturnPaper();
			}
		}
		else
			c16ForLedLine = 0;
		#endif

		// R==W: 空
		if(USB_RevC->Pread==USB_RevC->Pwrite)	//空的返回
		{
			//copy_value(&USB_RevC->Pread, &cBufferReceiveRead);
			cBufferReceiveRead  =  USB_RevC->Pread;
			return;

		}
		else									// 有数据
		{

			PrinterTask(USB_RevC->PBufferRev[USB_RevC->Pread]);
			//CopyToBufferReceive_Read(BufferReceive, cBufferReceiveRead);
			//BufferReceive[cBufferReceiveRead] = USB_RevC->PBufferRev[USB_RevC->Pread];

			//memcpy(&BufferReceive[cBufferReceiveRead], &USB_RevC->PBufferRev[USB_RevC->Pread], sizeof(u8));
			//USB_RevC->ReVRecordTotalLen++;
			//CopyPBufferRevToBufferReceive(&BufferReceive);
			//copy_value(&USB_RevC->ReVRecordTotalLen, &BufferReceive);
			if(USB_RevC->Pread==(USBMAXBufferReceiveSize-1))
				{
				USB_RevC->Pread = 0;  //copy_value(&USB_RevC->Pread, &cBufferReceiveRead);
				cBufferReceiveRead = (u32)USB_RevC->Pread;
				}
			else
				{
				USB_RevC->Pread++; //copy_value(&USB_RevC->Pread, &cBufferReceiveRead);
				cBufferReceiveRead = (u32)USB_RevC->Pread;
				}
			//CopyToBufferReceive_Read( BufferReceive, cBufferReceiveRead);
			/*
			void copy_print_bufferData(void){

			copy_value(&USB_RevC->Pwrite, &cBufferReceiveWrite);
			CopyToBufferReceive( BufferReceive, cBufferReceiveWrite);
			CopyPBufferRevToBufferReceive( BufferReceive);
			*/

		}
	}
}
#if 0
void USBDataIn(u32 rx_len,u32 eep)
{


	//USB_RevC->Waitting=false;
	//u32 rx_len = usb_g_bulk_read(USB_RevC->IDHandle, USB_RevC->EP, ep_buffer, MAXP_SIZE_HIDOUT,0);
	rx_len = usb_g_bulk_read(usb_id_print, epS_usb, eep, MAXP_SIZE_HIDOUT,0);
	USB_RevC->SaveData(rx_len,eep);
}


#endif
void USBDataIn(void)
{

if(data_usb_channel_busy_flag != true){
	u8 ep_buffer[MAXP_SIZE_HIDOUT];

	USB_RevC->Waitting=false;
	u32 rx_len = usb_g_bulk_read(USB_RevC->IDHandle, USB_RevC->EP, ep_buffer, MAXP_SIZE_HIDOUT,0);
	USB_RevC->SaveData(rx_len,ep_buffer);
	}
}

void USBDataOut(unsigned char *buffer,unsigned int len)
{
    usb_g_bulk_write(USB_RevC->IDHandle, USB_RevC->EP, buffer,len);
}
void USB_rx_data(struct usb_device_t *usb_device, u32 ep)
{
   // printf("Write:%d,Read:%d",cBufferPrintLineWrite,cBufferPrintLineRead);


	 if(data_usb_channel_busy_flag ==false)
	 {
		u8	 epBuff_usbrx[MAXP_SIZE_HIDOUT];
	    
      
		USB_RevC->IDHandle=usb_device2id(usb_device);
		USB_RevC->EP=ep;
		RXLenCYM_USB = 0;  //

			InputCOM_device = USB_RevC;
			busy_timer_counter = 0;
		 	data_bt_channel_busy_flag = true;
		 	data_wifi_channel_busy_flag  = true;
	       	data_le_channel_busy_flag  = true;
		 if(USB_stateS == USB_NormalState){
		 	
	    		u32 GXM_len;
				USB_RevC->DataIn(); //u32 rx_len,u32 eep
		 	}
		 if(GetUSBEmptyBuff(1)<BufferReceiveSizeEmpty) //
	  	 {
	    	USB_stateS=USB_BusyState;
	    	return;
	  	 }

		 if(Data_Channel != UsbData &&  data_usb_channel_busy_flag != true)
	     {
	    	Data_Channel=UsbData;
		
			
		 }
         	
			
	 }
	 else{
		return;

	 	}
		//USB_RevC->Waitting=false;
}


void PrinterUSBStart(void *priv)
{
	USB_RevC=&USB_Rev;
	USB_RevC->Initial();
}

#define AWINLINE   __attribute__((always_inline))
#define TSEC SEC_USED(.volatile_ram_code)
bool angenl_Continue = true;
void USBPrinterTask(void *p)
{
	u8  mc8c0;
	u8  mc8c1;
	u8  mmmpid;
    static int res;
	static u8 FlagSeftCount = 0;
    int msg[16];
	int cc;

	//PoweronInit();
	//printf("Alke UsbPrinterTask \r\n");

	MCUINIT(); // add sample ch n // Alke HeadStb High/Low Valid


//	printf(TABLESoftWare);
//	printf("\r\n\r\n");

/*while(1)
{
	USB_RevC->RefreshRev();
	USB_RevC->DataProcesser();
	ADC_Read();
	res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);//JLFreeTask();
}*/
//printf("----------------------------------\r\n");

	AllDataInit();

	res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 50);

	#if  (BtTest==1)
	while(1)
	{
		DataParserTest();
		ADC_Read();
		res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);//JLFreeTask();
	}
	#endif



	#if MotorRunMode //开机马达运行条件
  if(JLflash.OpenMotorRun)
  {
//    GapEnb
    FlagPAPFeedSet
    TablePAPSpeed[0]=60000;
    #if  (HeadCoding==8||HeadCoding==9)
    {
      for(mc8c0=1;mc8c0<=PAPVolatileStep;mc8c0++)
        TablePAPSpeed[mc8c0]=TablePAPSpeed2000[mc8c0];
    }
    #else
    {
      for(mc8c0=1;mc8c0<=PAPVolatileStep;mc8c0++)
        TablePAPSpeed[mc8c0]=TablePAPSpeed825[mc8c0];
    }
    #endif
    while(GZRecorder==0x00){res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);ADC_Read();}//{JLFreeTask();}
    OpenMotorTime=1;
  }
  while(JLflash.OpenMotorRun)
  {
	ADC_Read();
    PAPCrtSet(1);
    DelayTime2ms(2);
    PAPCrtSet(3);
    DelayTime2ms(2);
    PAPMoveStep=1600;
    PAPFeedStart(0);
    //while(FlagPAPCrtFull&&JLflash.OpenMotorRun){res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);//JLFreeTask();}
while(PAPMoveStep&&JLflash.OpenMotorRun)	{res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);ADC_Read();}//{JLFreeTask();}
    PAPCrtSet(1);
    DelayTime2ms(2);
    PAPCrtSet(3);
    DelayTime2ms(2);
    PAPMoveStep=1600;
    PAPFeedStart(1);
    //while(FlagPAPCrtFull&&JLflash.OpenMotorRun){res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);//JLFreeTask();}
while(PAPMoveStep&&JLflash.OpenMotorRun)	{res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);ADC_Read();}//{JLFreeTask();}
    if(OpenMotorTime>120000)//空跑4分钟
    {
      OpenMotorTime=0;
      JLflash.OpenMotorRun=0;
      PAPMoveStep=30;
      syscfg_write(MemOMR,&JLflash.OpenMotorRun,1);
      break;
    }
  }
  TablePAPSpeed[0]=60000/6;
  for(mc8c0=1;mc8c0<=PAPVolatileStep;mc8c0++)
    TablePAPSpeed[mc8c0]=TablePAPSpeed1500[mc8c0];
  #endif // MotorRunMode

	PrintINIT();	// 先执行纸张装载
//printf("----------------------PrintINIT-------------------------\n");
	PaperTest();
//printf("----------------------PaperTest-------------------------\n");
	TestHDResister();
	ResisterToBuffer();
	ADC_Read();
	res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);//JLFreeTask();
//printf("----------------------ResisterToBuffer-------------------------\n");
	PWMSet(0);
//	printf("Printer-%d-%d\n", InGapPageBaseADC, PositionRubberRoller);
	//PrintSystem=1;//打印系统启动

	SystemCheck=1;
	//size_t wtsram=xPortGetPhysiceMemorySize();
	//printf("wtsram:%d\n",wtsram);
//printf("-------------------wtsram----------------------------\n");

/*
WriteBufferReceive('@'); // 写入打印自检页命令
WriteBufferReceive('S');
WriteBufferReceive('S');
WriteBufferReceive('P');
*/
	while (1)
	{
//printf("DataParser>>>");
		//ADC_Read();
		//USB_RevC->RefreshRev();
		//USB_RevC->DataProcesser();
		//res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);

		if(GZRecorder==0x00)
			{PaperTest();}

		if(SystemCheck==0){
				DataParser(Data_Channel);
	 	}
		else 
		{
		    //||NumPrint()>1
			if((NumBufferReceive()>1)&&NoPaperErr==0&&command.JumpFail==0&&Print_start_flag)
			{
				u32 get_val = NumBufferReceive();
				
				DataTimeout=1;
				SystemCheck=0;
				TimeForMotor=0;

                //#if UartReturn
                //    SendString("K3\r\n");
                //#endif // UartReturn
            }
            else if(Angenl_c123>0&&NoPaperErr==0&&command.JumpFail==0&&Print_start_flag&&(Data_Channel==BTData|| Data_Channel==LEData))
            {

              	//printf("\r\n In usbprinter task, bt data systemcheck:;;;;;;;;;  line 1121 prinyter task\r\n ");

                DataTimeout=1;
                SystemCheck=0;
                TimeForMotor=0;
                #if UartReturn
                SendString("K3\r\n");
                #endif // UartReturn
            }
            else if(Angenl_c123==0&&NoPaperErr==0&&command.JumpFail==0&&!Print_start_flag&&(Data_Channel==BTData|| Data_Channel==LEData)&&!angenl_Flag)
            {
                //printf("\r\n angenl96  \r\n ");
				//printf("\r\n In usbprinter task, bt data systemcheck333333333:;;;;;;;;; printer task line 1135 \r\n ");
                DataTimeout=1;
                SystemCheck=0;
                TimeForMotor=0;
                #if UartReturn
                SendString("K3\r\n");
                #endif // UartReturn
            }

			if(NoPaperErr)
			{
			    printf("\r\n angenl_NoPaperErr \r\n");
			    PAPMoveStep=0;
					 			busy_timer_counter = 1;  //Fahim Edit
								 channel_release = 0;   //Fahim Edit
			}

			if(FlagKeyPaper&&WhitePaper==0)
			{
			printf("line 1151");
				#if  (UartReturn==1)
					SendString("PaperPage\r\n");
        		#endif //UartReturn

				if(TimeSelfPage > (1000*60*5))
				{
					WriteBufferReceive('@',Data_Channel);
					WriteBufferReceive('<',Data_Channel);
					WriteBufferReceive(0x0d,Data_Channel);
					WriteBufferReceive(0x0a,Data_Channel);
					WriteBufferReceive(0x00,Data_Channel);
					WriteBufferReceive(0x00,Data_Channel);
					WriteBufferReceive('@',Data_Channel);
					WriteBufferReceive('>',Data_Channel);
					DelayTime2ms(5);
				}
				if(FlagKeyPaper){
					FlagKeyPaper--;
					 //printf("\r\n FlagKeyPaper: %d \r\n", FlagKeyPaper);
					}
				WhitePaper=1;
			}


			if(Angenl_c123>0&&(Data_Channel==BTData || Data_Channel==LEData))
        	{
          		//printf("\r\n angenl94  \r\n ");
            	//printf("\r\n TimeForMotor %d  \r\n ",TimeForMotor);

           		if(TimeForMotor > 500 || (PaperJamFlag&&(TimeForMotor > 50)))//超时信号到 if(TimeForMotor > 500 || (PaperJamFlag&&(TimeForMotor > 50)))
              	{
                  //res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);//JLFreeTask();
                  //printf("\r\n angenl92  \r\n ");
					//printf("\r\n line 1181In usbprinter task, bt data systemcheck:;;TimeForMotor > 500 || (PaperJamFlag&&(TimeForMotor > 50;;;;;;;  \r\n ");
                    DataTimeout=1;
                    SystemCheck=0;
                    TimeForMotor=0;
				}

						//printf ("\ngap searching3" );

			}
			else
			{
				//printf ("\ngap searching2" );
				if(TimeForMotor > 500 || (PaperJamFlag&&(TimeForMotor > 50)))//超时信号到
				{
					FlagSpeedUpdate = 1;
					FlagSelfPrint=0;
					command.ReJump=0;
					command.InPwrite=0;
					command.CommandType=NormalR; //回到正常模式
					#if 0//UartReturn
						for(mc8c0=0;mc8c0<=HeadNumber;mc8c0++)
						{
							printf("command.JumpPage[%d]=%x\r\n",mc8c0,command.JumpPage[mc8c0]);
						}
						printf("PrintRead %d\r\n",command.PrintRead);
						printf("PrintWrite %d\r\n",command.PrintWrite);
						DisplayData9999(cBufferReceiveWrite);
						DisplayData9999(USB_RevC->Pread);
						SendString("aFlag:");
						DisplayData9999(aFlag);
						SendString("K1\r\n");
        			#endif // UartReturn
					TimeForMotor=0;
					PaperTearLine=1;
					PWMSet(0);
		            if(Data_Channel==BTData || Data_Channel==LEData)
		            {
		                angenl_Continue=true;
						printf ("\n gap angenl_Continue : %d" ,angenl_Continue );
		            }
		            else
		            {
		                angenl_Continue=false;
		            }
					printf ("\ngap searching1");

					if(NoPaperErr==0&&((((!Page[cPageRead].HeadUsed)&&(Page[cPageRead].HeadCount < PositionCut510)&&FlagModeGap))||(angenl_Continue)&&(!Last_page_flag)))//||FlagModeContinuous
					{
						#if UartReturn
							SendString("K2\r\n");
						#endif // UartReturn
						if(FlagSmallPage)
						{
						   // printf("FlagSmallPage \n");
							FlagCutPageStart=1;
							cCutPageStart=1;   //PAPVolatileStep-1;//CXYCXY20210910
							FlagCut8Finish=0;
						}
						else
						{
							//printf("Not in FlagSmallPage \n");
							FlagCutPageStart=0;
							cCutPageStart=0; // cCutPageStart=0 计数器不累加
							FlagCut8Finish=1;
						}
						PrintSystem=0;

						// 应该是打印结束，超时后走到撕纸位置
		  				#if (Categories==1)
			  				//printf("In category 1 \n");
							//SendString("Categories-1");
			  				// 撕纸位置调整：原始+20；@20231005 应弥龙要求改为30；@20231008弥龙说陆凤反应首个打印位置有偏差，再次改回来
							//PAPMoveStep = PositionCut510-PositionRubberRoller+30;//+20; @20231005Alke 20-->24	//WT612系列
							PAPMoveStep = PositionCut510-PositionRubberRoller+StepsOnemm*5;//+20;	//+20; @20231005Alke 20-->24	//WT612系列
				            if(Data_Channel==BTData || Data_Channel==LEData)
				            {
				                if(!Last_page_flag)
				                {
				                    Last_page_flag=true;
				                    PAPMoveStep = PositionCut510-PositionRubberRoller+StepsOnemm*5;//20	//WT612系列
				                }
				            }
				            else
				            {
				                PAPMoveStep = PositionCut510-PositionRubberRoller+StepsOnemm*5;//20	//WT612系列
				            }



          				#elif (Categories==2)
		 					//printf("In category 2 \n");
				            //SendString("Categories-2");
				            PAPMoveStep = PositionCut510-PositionRubberRoller-StepsOnemm*2;//-16;//TP350系列

	            			if(Data_Channel==BTData || Data_Channel==LEData)
				            {
				                if(!Last_page_flag)
				                {
				                    Last_page_flag=true;
				                    PAPMoveStep = PositionCut510-PositionRubberRoller-StepsOnemm*2;//20	//WT612系列
				                }
				            }
				            else
				            {
				                PAPMoveStep = PositionCut510-PositionRubberRoller-StepsOnemm*2;//20	//WT612系列
				            }

          				#elif (Categories==3)
						    //printf("In category 3 \n");
				            //SendString("Categories-3");
						  	//printf("In category 3 \n");
				            PAPMoveStep = PositionCut510-PositionRubberRoller+StepsOnemm;//TP511系列

							if (Data_Channel == BTData || Data_Channel==LEData)
							{
								if (!Last_page_flag)
								{
									Last_page_flag = true;
									PAPMoveStep 	= PositionCut510 - PositionRubberRoller + StepsOnemm; //TP511系列
									//printf ("In Cat3: Last_page_flag: %d,	Data_Channel: %d", Last_page_flag, Data_Channel);
								}
							}
							else
							{
								PAPMoveStep = PositionCut510 - PositionRubberRoller + StepsOnemm; //TP511系列
								//printf ("In Cat3: Last_page_flag: %d,	Data_Channel: %d", Last_page_flag, Data_Channel);
							}

						#endif // Categories


						#if (PWRMotoHead==0)
							HeadMotorPowerOff
							printf("\n HeadMotorPowerOff");

							/***************************/
							
								 Page_CutContinue=0;
								 Print_start_flag=true;
								 //angenl_Flag=false;
								 //PrintWritePage_Count=1;
							     //pcount=0;
								 Last_page_flag=false;
								 Page_counter=0;
								 //printf("\r\n angenl309 \r\n");
								 command.CommandType=NormalR;
                                 command.InPwrite=0;
                                 command.PageWrite=1;
                                 command.PageRead=0;
                                 Print_line_counter=0;
                                 command.HeadWrite=1;
                                 command.HeadRead=0;
                                 command.PrintWrite=1;
                                 command.PrintRead=1;
                                 command.InZlibeWrite=0;
								// motor_StopFlag=true;
								 firstTimeFlag=1;
                               //  c123=0;
                                 Angenl_c123=0;
                                 FlagPrintStep=0;
								 busy_timer_counter = 1;
								 channel_release = 0; 
								
							/***************************/
						if(Data_Channel != 1)  //fahim edit
						{
							Data_Channel = 1;
							InputCOM_device = USB_RevC;
							
						}
							//printer_increm++;
						#endif

						HeadTime[1] = 1800 / 6;
						HeadTime[2] = 1800 / 6;

						for (mc8c0 = 1; mc8c0 <= PAPVolatileStep; mc8c0++)
							mTablePAPSpeed[mc8c0] = TablePAPSpeed[mc8c0];

						for (mc8c0 = 1; mc8c0 <= PAPVolatileStep; mc8c0++)
							TablePAPSpeed[mc8c0] = TablePAPSpeed1500[mc8c0];

						PAPCrtSet (3);
						cPAPSpeed = 0;
						PAPFeedStart (PAPFeed);
						
						//printf("before printing, flag stats:  PrintSystem: %d, FlagPrintStep: %d , PAPMoveStep: %d\n",PrintSystem,FlagPrintStep,PAPMoveStep);
						while (PAPMoveStep);
							/*{
								PAPMoveStep--;
								if(PAPMoveStep<0)
								{
									PAPMoveStep=0;
									break;
								}
							}*/

                       
						PrintSystem 	= 1;
						FlagPrintStep	= 0;
						FlagInGap		= 0;
						cGapCount		= 0;
						
						//printf("After printing, flag stats:  PrintSystem: %d, FlagPrintStep: %d , PAPMoveStep: %d\n",PrintSystem,FlagPrintStep,PAPMoveStep);
						for (mc8c0 = 1; mc8c0 <= PAPVolatileStep; mc8c0++)
							TablePAPSpeed[mc8c0] = mTablePAPSpeed[mc8c0];

						MotorDis


						if (FlagModeGap) //缝隙纸
						{
							for (mc8c1 = 0; mc8c1 < PageNumber; mc8c1++)
							{
								if (Page[mc8c1].HeadCym)
								{
									if (FlagSmallPage)
									{
										Page[mc8c1].HeadCount -= (PositionInSensor - 2);
										Page[mc8c1].cymHeadCount -= (PositionInSensor - 2);
									}
								}

								if (Page[mc8c1].EndCym)
								{
									if (FlagSmallPage)
									{
										Page[mc8c1].EndCount -= (PositionInSensor - 2);
										Page[mc8c1].cymEndCount -= (PositionInSensor - 2);
									}
								}
							}
						}
						else if (FlagModeContinuous)
						{
							ContinuousStep			= 0;
						}
					}

					if (PaperJamFlag == 2)
					{
						u8 nbuff[]	=
											{
												"SecondTest"
											};
						FlagSelfPrint	= 1;
						//printf("In Usb printer paperjamflag =2 \n");
						PaperEnb NoPaperErr = 0;

					//					GapEnb
					//					BMDisable
						FeedScanBackRestore (1);
						PaperJam (1, &nbuff);
						PaperJamFlag = 3;
					}
					else if (PaperJamFlag == 3)
					{
						u8 nbuff[] =
										{
											"ThirdTest"
										};
						FlagSelfPrint = 1;
						printf("In Usb printer paperjamflag =3 \n");
						PaperEnb NoPaperErr = 0;

						//					GapEnb
						//					BMDisable
						FeedScanBackRestore (1);
						PaperJam (2, &nbuff);
						PaperJamFlag = 0;
					}
				}
			}
		}

		//printf("DataParser	 2>>>");
		//USB_RevC->RefreshRev();
		//USB_RevC->DataProcesser();
		res = __os_taskq_pend (msg, ARRAY_SIZE (msg), 1);
	}
}


#if 0
{
	static int res;
	static u8 FlagSeftCount = 0;
    int msg[16];
	int cc;

	PoweronInit();
	res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 50);

	// 判断是否需要打印自检页
	if(KeyLineRecorder==0xff)	FlagSeftCount = 1;

	PrinterInit();
	res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 500); // 500 需要等待较长时间，防止解析/打印自检页时，有其他命令插入干扰。
	ADC_Read();

	while(1)
	{
		USB_RevC->RefreshRev();
		USB_RevC->DataProcesser();
		res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);
//		ADC_Read();

		if(FlagSeftCount)
		{
			if(1)//if((PaperState==sPaperFrontNormal)||(PaperState==sPaperBackNormal))
			{
				//if(FlagSeftCount<10)	FlagSeftCount++;
				//else if(DataFromChannel == 0)
//				if(DataFromChannel == 0)
				{
					FlagSeftCount = 0;
//					DataFromChannel = 1;

					USB_RevC->Pwrite = 0;
					USB_RevC->Pread = 0;

					USB_RevC->PBufferRev[USB_RevC->Pwrite++] = 0x1b;
					USB_RevC->PBufferRev[USB_RevC->Pwrite++] = 0x3a;
					USB_RevC->PBufferRev[USB_RevC->Pwrite++] = 0x00;
					USB_RevC->PBufferRev[USB_RevC->Pwrite++] = 0x00;
					USB_RevC->PBufferRev[USB_RevC->Pwrite++] = 0x50;
				}
			}
			else
				FlagSeftCount = 1;
		}
    }
}
#endif

late_initcall(PrinterUSBStart);




