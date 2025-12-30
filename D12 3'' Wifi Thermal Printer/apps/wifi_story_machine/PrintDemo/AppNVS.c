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
#include "math.h"
extern void SaveWifiInfo(char *SSID,char *PASSWord);
extern void GetWifiSTAInfo(void);


#define CRC16_POLYNOMIAL 0x8005  // Polynomial used for CRC16
// Function to compute CRC16
unsigned int crc16(unsigned char *data, unsigned int length) 
{
    unsigned int crc = 0xFFFF;  // Initial value
    for (unsigned int i = 0; i < length; i++) 
	{
        crc ^= (data[i] << 8);  // XOR byte into the CRC
        for (unsigned int j = 8; j > 0; j--) 
		{
            if (crc & 0x8000)
			{
                crc = (crc << 1) ^ CRC16_POLYNOMIAL;  // If high bit is set, shift and XOR with polynomial
            }
			else 
			{
                crc <<= 1;  // Just shift left if no XOR
            }
        }
    }
    return crc;  // Return computed CRC
}

void CommandNVSWiFi(void)
{
	unsigned int i,SUM=0;
	if(command.ReadPos+64<=command.TotalLength)//if remainder data is enough
	{
		for(i=0;i<32;i++)
		{
			Nvs.SSID[i]=command.Buffer[command.ReadPos+i];
		}
		for(i=0;i<32;i++)
		{
			Nvs.SSIDPassword[i]=command.Buffer[command.ReadPos+i+32];
		}
		printf("SSID:%s;Password:%s\n",Nvs.SSID,Nvs.SSIDPassword);
		SaveWifiInfo(Nvs.SSID,Nvs.SSIDPassword);
		command.ReadPos+=64;
		command.DecodeStatus=CommandIdle;
		 WiFiStartManual();
		return;
	}
	ReturnModle();
}
void NVS_OperationTask(void *p)
{
	while(1)
	{
		vTaskDelay(pdMS_TO_TICKS(5000));
	}

}

