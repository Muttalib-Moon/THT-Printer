#ifndef __WIFI_APP_TASK_PRINT_H
#define __WIFI_APP_TASK_PRINT_H



#include "os/os_api.h"
#include "app_config.h"
#include "circular_buf.h"
#include "WeiTingCommon.h"

//#define MAXBufferReceiveSize	30000//50000//20000//50000 Working
#define MAX_RECV_BUF_SIZE   		200
#define Wifi_RevRefreshLevelValue 	900

#define SERVER_TCP_PORT 			9100
#define TCPIPRevBusyLevelValue		MAX_RECV_BUF_SIZE*5


extern Device_Rev Wifi_Rev;

extern Device_Rev *Wifi_RevC;

struct MyEthernetP
{
     unsigned long IP;
	 int SockID;
	 bool ConnectState;
	 bool TCPIPTaskRunning;
	 bool SocketTaskRunning;
	 unsigned long SocketPwrite;
	 unsigned long SocketPread;
	// unsigned char SocketRevBuffer[MAXBufferReceiveSize];

};

//extern void Printer_Tcp_Recv_handler(void);
//extern void Printer_Tcp_Sock_Accpet(void);

extern struct MyEthernetP MEP;
extern void wifiCheckState(void);
void MyApp_Monitor(void *p);
void MyTCIPPrinterTask(void *p);
void MyTCIPSocketTask(void *p);
void WifiPrintTask(void *p);
void SocketSaveData(uint16_t     sizebytes,unsigned char *BufferReceive);
unsigned long GetWifiEmptySize(void);
unsigned long getBufferDataLength(void);

void CheckBufferEmptySize(void);
int tcp_send_data_sang(const void *buf, u32 len);




#endif
