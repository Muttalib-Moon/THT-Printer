/*
******************************************************************************
* @file 	  AppDriverMain.c
* @author	  Sang 2024
* @date 	  2024-12
* @brief	  Software structure and algorithm layer(application layer)
*			  This module is independent of the CPU type and is only dependent on FreeRTOS.
*
*			  This file provides firmware functions to manage the following
*			  functionalities:
*			  + Data Packet Linked List Management
*			  +
*			  +
*
*
*/
#include "AppDriverGeneric.h" //JL lib dependency
#include "AppDriver.h"
//---------------test value---------------//
unsigned int sang  =  0;
unsigned int sang1 =  0;
unsigned int sang2 =  0;
//---------------test value end---------------//

extern bool SSP_Enable;
extern void BT_disable(void);
extern void BT_Enable(void);
USBDP USBdp = {NULL, CommIdle};	   // usb communication
PDataPacket PWiFiPrinterDP = NULL; // Wifi data paket structure
PDataPacket PEdrDP = NULL;		   // Edr bluetooth data paket structure
PDataPacket PBLEDP = NULL;		   // Edr bluetooth data paket structure
TickType_t printTimeout;

/**
 *@ printing method
 *C_Type_USB 1
 *C_Type_SSP 2
 *C_Type_BLE 3
 *C_Type_wifi 4
 */
typedef enum
{
	IDLE = 0,
	USB,
	SPP,
	BLE,
	WIFI
} printMethod;
printMethod method = IDLE;

/**
 * @brief User define USB printer Interrupt Service Routine call back function
 * @param UsbDevice: usb device pointer;Ep:end pointer number
 * @return none
 */
void USBPrinterReveiveDataIsr(struct usb_device_t *UsbDevice, u32 Ep)
{
	unsigned char Buffer[MAXP_SIZE_HIDOUT]; // receive buffer
	LinkedListData LLData;
	// BaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();// Enter into critical area
	unsigned int NodeCount = _malloc_size(USBdp.DP); // calculate node quantity
	u16 temp=(method==IDLE || method == USB)? MaxLinkedListCount:RestartRxCount;
	//printf("DATA from USB %d",temp);
	if (NodeCount >  temp)				 // limit the data stream
	{
		USBdp.Status = CommSuspended; // communication suspend
		return;
	}
	unsigned int Len = usb_g_bulk_read(0, HID_EP_OUT, Buffer, MAXP_SIZE_HIDOUT, 0);
	LLData.len = Len;
	LLData.data = (unsigned char *)malloc(Len * sizeof(unsigned char));
	memcpy(LLData.data, Buffer, LLData.len);
	_malloc(&USBdp.DP, LLData); // save data in linked list
	free(LLData.data);
	// taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);// exit critical area
}
/**
 * @brief User define USB printer data receive function in task
 * @param UsbDevice: usb device pointer;Ep:end pointer number
 * @return none
 */
void USBPrinterReveiveDataTask(void)
{
	if (CommSuspended == USBdp.Status) // suspend->check linked list
	{
		unsigned char Buffer[MAXP_SIZE_HIDOUT]; // receive buffer
		LinkedListData LLData;
		unsigned int NodeCount = _malloc_size(USBdp.DP); // calculate node quantity
		if (NodeCount < RestartRxCount)					 // limit the data stream
		{
			// taskENTER_CRITICAL(); //enter into the critical area
			// local_irq_disable();
			unsigned int Len = usb_g_bulk_read(0, HID_EP_OUT, Buffer, MAXP_SIZE_HIDOUT, 0);
			LLData.len = Len;
			LLData.data = (unsigned char *)malloc(Len * sizeof(unsigned char));
			memcpy(LLData.data, Buffer, LLData.len);
			_malloc(&USBdp.DP, LLData); // save data in linked list
			USBdp.Status = CommIdle;
			free(LLData.data);
			// local_irq_enable();
			// taskEXIT_CRITICAL();// exit critical area
		}
	}
}
extern void Wifi_Mute_Wait(void);
extern void Wifi_Mute_Post(void);
extern bool WiFiLastPackage;
void WiFiPrinterReceiveDataTask(unsigned char *data, unsigned int len)
{
	LinkedListData LLData;
	for (unsigned int i = 0; i < len; i += 64)
	{
		if (i + 64 < len)
			LLData.len = 64;
		else
			LLData.len = len - i;
		LLData.data = (unsigned char *)malloc(LLData.len * sizeof(unsigned char));
		memcpy(LLData.data, data + i, LLData.len);
		_malloc(&PWiFiPrinterDP, LLData); // save data in linked list
		free(LLData.data);
	}
	unsigned int CountSlow = 0;
	unsigned int NodeCount;
	while (1)
	{
		NodeCount = _malloc_size(PWiFiPrinterDP);
		u16 temp=(method==IDLE || method == WIFI)? 600:50;
		//printf("DATA from WIFI. %d",temp);
		if (NodeCount >= temp) // the data is full
		{
			vTaskDelay(pdMS_TO_TICKS(20));
		}
		else
		{
			vTaskDelay(pdMS_TO_TICKS(5));
			return;
		}
	}
}
extern int transport_spp_flow_enable(u8 en);
bool EdrBusy = false;
void EdrPrinterReceiveDataISR(unsigned char *data, unsigned int len)
{
	// printf("sozib len %d\n\n\n\n",len );
	LinkedListData LLData;
	unsigned int NodeCount;
	for (int i = 0; i < len; i += 64)
	{
		if (i + 64 < len)
			LLData.len = 64;
		else
			LLData.len = len - i;
		LLData.data = (unsigned char *)malloc(LLData.len * sizeof(unsigned char));
		memcpy(LLData.data, data + i, LLData.len);
		_malloc(&PEdrDP, LLData);
		free(LLData.data);
	}
	NodeCount = _malloc_size(PEdrDP);
	//printf("B:%d-%d\r\n", NodeCount, len);
	u16 temp=(method==IDLE || method == SPP)? 400:50;
	//printf("DATA from SPP. %d",temp);
	if (NodeCount > temp && EdrBusy == false)
	{
		transport_spp_flow_enable(1);
		EdrBusy = true;
	}
	vTaskDelay(pdMS_TO_TICKS(5));
}
void EdrPrinterReceiveDataTask(void)
{
	// taskENTER_CRITICAL(); //enter into the critical area
	unsigned int NodeCount = _malloc_size(PEdrDP);
	if (NodeCount < 50 && EdrBusy)
	{
		transport_spp_flow_enable(0);
		EdrBusy = false;
		printf(">");
	}
	// taskEXIT_CRITICAL();// exit critical area
}

/*
 void BLEPrinterReceiveDataISR(unsigned char *data,unsigned int len)
{
	LinkedListData LLData;
	LLData.len=len;
	LLData.data=(unsigned char*)malloc(LLData.len*sizeof(unsigned char));
	memcpy(LLData.data,data,LLData.len);
	_malloc(&PBLEDP,LLData);//save data in linked list
	free(LLData.data);
   // unsigned int NodeCount=_malloc_size(PBLEDP);
   // printf("BLE=%d",NodeCount);

}
*/
extern void att_server_flow_enable(u8 enable);
extern void att_server_flow_hold(u16 con_handle, u8 hold);
bool BleBusy = false;
extern u16 HCI_le_Printer;
#define rcvLen 64
bool bleStartPrint = false;
void BLEPrinterReceiveDataISR(unsigned char *data, unsigned int len)
{
	LinkedListData LLData;
	unsigned int NodeCount;
	for (int i = 0; i < len; i += rcvLen)
	{
		if (i + rcvLen < len)
			LLData.len = rcvLen;
		else
			LLData.len = len - i;

		LLData.data = (unsigned char *)malloc(LLData.len * sizeof(unsigned char));
		memcpy(LLData.data, data + i, LLData.len);
		_malloc(&PBLEDP, LLData);
		free(LLData.data);
	}

	NodeCount = _malloc_size(PBLEDP);
	// printf("BLE Nodes: %d, Len: %d\n", NodeCount, len);
	u16 temp=(method==IDLE || method == BLE)? 400:50;
	//printf("DATA from BLE. %d",temp);
	if (NodeCount > temp)
	{
		// printf("BLE Flow: HOLD - Nodes: %d\n", NodeCount);
		// att_server_flow_enable(1);
		att_server_flow_hold(HCI_le_Printer, 1);
		BleBusy = true;
	} // else {
	  //    BleBusy = false;
	// att_server_flow_hold(HCI_le_Printer, 0);
	//  printf("BleBusy  : %d=========================================================>>>>>>>>>>>\n", BleBusy);
	//}
	vTaskDelay(pdMS_TO_TICKS(5));
}

void BLEPrinterReceiveDataTask(void)
{
	unsigned int NodeCount = _malloc_size(PBLEDP);

	if (NodeCount < 50 && BleBusy)
	{
		// printf("BLE Flow: HOLD - Nodes: %d\n", NodeCount);
		// att_server_flow_enable(1);
		att_server_flow_hold(HCI_le_Printer, 0);
		BleBusy = false;
	}
}

// @brief CopyMemoryISR,this function uses in interrupt function-->print timer exclusive function and copies only one packet
bool CopyMemoryISR(DataPacket **DataType, LinkedListData *LLData)
{
	DataPacket Dp;
	if (_copy_memery(DataType, &Dp)) // free packet and obtain data
	{
		LLData->PageNum = Dp.LLData.PageNum;
		LLData->len = Dp.LLData.len;												 // data length
		LLData->data = (unsigned char *)malloc(LLData->len * sizeof(unsigned char)); // allocate memory
		memcpy(LLData->data, Dp.LLData.data, LLData->len);							 // abtain memory
		_free(DataType);															 // free the data packet
		free(Dp.LLData.data);														 // free temporary buffer
		return true;
	}
	return false;
}

bool CopyPageNumISR(DataPacket **DataType, LinkedListData *LLData)
{

	DataPacket Dp;
	if (_copy_memery(DataType, &Dp)) // free packet and obtain data
	{
		LLData->PageNum = Dp.LLData.PageNum;
		_free(DataType);	  // free the data packet
		free(Dp.LLData.data); // free temporary buffer
		return true;
	}
	return false;
}
unsigned long PageNumCurrent(DataPacket **DataType)
{
	DataPacket Dp;
	if (_copy_memery(DataType, &Dp)) // free packet and obtain data
	{
		return Dp.LLData.PageNum;
		;
	}
	return 0;
}
#define  C_Type_Idle  0
#define  C_Type_USB   1
#define  C_Type_SSP   2
#define  C_Type_BLE   3
#define  C_Type_Net   4

// This function uses in task
static void CopyMemoryTask(DataPacket **DataType, char C_Type)
{
	unsigned int count = 0;
	DataPacket Dp;

	static TickType_t ledLastToggle = 0;
    static uint8_t ledState = 0;
    static uint8_t ledBlinkCount = 0;
    static uint8_t ledCurrentBlink = 0; // current blink index
    static bool ledActive = false;
    static TickType_t ledPauseStart = 0;
    static bool ledInPause = false;

	while (1)
	{
	    TickType_t now = xTaskGetTickCount();

        if (method != IDLE && ledBlinkCount > 0)
        {
            if (!ledInPause)
            {
                if ((now - ledLastToggle) >= pdMS_TO_TICKS(250)) // 2Hz toggle
                {
                    ledState ^= 1;
                    if (ledState)
                    {
                        LEDLineTurnOn;
                    }
                    else
                    {
                        LEDLineTurnOff;
                    }
                    ledLastToggle = now;
                    if (!ledState)
                    {
                        ledCurrentBlink++;
                        if (ledCurrentBlink >= ledBlinkCount)
                        {
                            ledInPause = true;
                            ledPauseStart = now;
                            ledCurrentBlink = 0;
                        }
                    }
                }
            }
            else
            {
                LEDLineTurnOff;
                if ((now - ledPauseStart) >= pdMS_TO_TICKS(2000))
                {
                    ledInPause = false;
                    ledLastToggle = now;
                }
            }
        }
        else
        {
            ledActive?LEDLineTurnOn:NULL;
            ledActive = false;
            ledInPause = false;
            ledCurrentBlink = 0;
            ledState = 0;
        }


		if (_copy_memery(DataType, &Dp)) // free packet and obtain data
		{

			if (method == IDLE)
			{
				if (C_Type == C_Type_USB)
				{
					method = USB;
				}
				else if (C_Type == C_Type_SSP)
				{
					method = SPP;
				}
				else if (C_Type == C_Type_BLE)
				{
					method = BLE;
				}
				else if (C_Type == C_Type_Net)
				{
					method = WIFI;
				}
				printf("Return to %s: \r\n", method == USB ? "USB" : method == SPP ? "SPP": method == BLE   ? "BLE": "WIFI");

                switch (method)
                {
                    case USB:  ledBlinkCount = 1; break;
                    case WIFI: ledBlinkCount = 2; break;
                    case SPP:  ledBlinkCount = 3; break;
                    case BLE:  ledBlinkCount = 4; break;
                    default:   ledBlinkCount = 0; break;
                }
                ledCurrentBlink = 0;
                ledState = 0;
                ledActive = true;
                ledInPause = false;
                ledLastToggle = xTaskGetTickCount();
            }
            else
            {
                printTimeout = xTaskGetTickCount();
            }

            // handle print data
            if (PrintDataManagement(Dp.LLData.len, Dp.LLData.data) == false)
            {
                free(Dp.LLData.data);
                return; // print memory limitation
            }

            _free(DataType);          // free the data packet
            free(Dp.LLData.data);     // free temporary buffer

            if (count++ > 200)
            {
                printf("rest------>\r\n");
                return;
            }
        }
        else
        {
            if ((xTaskGetTickCount() - printTimeout) >= 1000 && method > IDLE)
            {
                method = IDLE;
                printf("Time out return to IDLE\r\n");
            }
            return;
        }
    }
}

#if 0
void SuspendWreatheTask(char C_Type)
{
	if(SSP_Enable&&C_Type!=C_Type_SSP)
	{
		printf("BT Disable <<<<<<<<<<<<>\r");
		BT_disable();
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

//This function uses in task
static void CopyUSBDataToProcess(DataPacket** DataType,char C_Type)
{
	bool DataIn=false;
	DataPacket Dp;
	while(1)
	{
	//	taskENTER_CRITICAL(); //enter into the critical area
		if(_copy_memery(DataType,&Dp))									//free packet and obtain data
		{
		//	taskEXIT_CRITICAL();// exit critical area
			DataIn=true;
			SuspendWreatheTask(C_Type);
			if(PrintDataManagement(Dp.LLData.len,Dp.LLData.data)==false) 	//handle print data
			{
				free(Dp.LLData.data);									  	//free temporary buffer
				return;														//print memory limitation
			}
			_free(DataType);												//free the data packet-->data link point to the next package
			free(Dp.LLData.data);											//free temporary buffer
		}
		else
		{
		//	taskEXIT_CRITICAL();// exit critical area
			if(SSP_Enable==false&&DataIn==false)
			{
				printf("BT enable =====>\r");
				BT_Enable();
			}
			return;
		}
	}
}
extern void Wifi_Mute_Wait(void);
extern void Wifi_Mute_Post(void);

extern void DataManagement(unsigned int len,unsigned char *data);
#endif
DataPacket LastDp;
void WiFiDataLinkLastPackage(void)
{
	if (_copy_memery(&PWiFiPrinterDP, &LastDp)) // free packet and obtain data
	{
		_free(&PWiFiPrinterDP); // free the data packet-->data link point to the next package
	}
}
void FreeLastTempPackage(void)
{
	free(LastDp.LLData.data);
}
static void CopyMemoryTaskWiFi(DataPacket **DataType, char C_Type)
{
	unsigned int count = 0;
	DataPacket Dp;
	while (1)
	{
		if (_malloc_size(PWiFiPrinterDP) == 1)
		{
			printf("--->\r\n");
			WiFiLastPackage = true;
			while (1)
			{
				vTaskDelay(pdMS_TO_TICKS(1));
				if (WiFiLastPackage == false)
					break;
			}
			DataManagement(LastDp.LLData.len, LastDp.LLData.data);
			FreeLastTempPackage();
		}
		else
		{
			if (_copy_memery(DataType, &Dp)) // free packet and obtain data
			{
				if (PrintDataManagement(Dp.LLData.len, Dp.LLData.data) == false) // handle print data
				{
					free(Dp.LLData.data); // free temporary buffer
					return;				  // print memory limitation
				}
				_free(DataType);	  // free the data packet-->data link point to the next package
				free(Dp.LLData.data); // free temporary buffer
				if (count++ > 100)
					return;
			}
			else
			{
				return;
			}
		}
	}
}

/**
 * @brief DataManagementTask.
 */
void DataManagementTask(void *p)
{
	while (1)
	{
		switch (method)
		{
		case IDLE:
		{
			if(method==IDLE)CopyMemoryTask(&USBdp.DP, C_Type_USB);		 // read usb communication linked list data and send to process
			if(method==IDLE)CopyMemoryTask(&PEdrDP, C_Type_SSP);		 // bluetooth
			if(method==IDLE)CopyMemoryTask(&PBLEDP, C_Type_BLE);		 // bluetooth
			if(method==IDLE)CopyMemoryTask(&PWiFiPrinterDP, C_Type_Net); // wifi printer fetch data
		}
		break;
		case USB:
		{
			CopyMemoryTask(&USBdp.DP, C_Type_USB); // read usb communication linked list data and send to process
		}
		break;

		case WIFI:
		{
			CopyMemoryTask(&PWiFiPrinterDP, C_Type_Net); // wifi printer fetch data
		}
		break;
		case SPP:
		{
			CopyMemoryTask(&PEdrDP, C_Type_SSP); // bluetooth
		}
		break;
		case BLE:
		{
			CopyMemoryTask(&PBLEDP, C_Type_BLE); // bluetooth
		}
		break;
		default:
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
