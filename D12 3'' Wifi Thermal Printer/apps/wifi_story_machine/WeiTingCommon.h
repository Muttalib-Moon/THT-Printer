#ifndef __WEITINGCOMMON_H
#define __WEITINGCOMMON_H
//#include "UserInterface.h"
typedef struct _DEVICE_REV
{
  void (*Initial)(void);
  void (*SaveData)(unsigned long  len,unsigned char * lodamp);
  unsigned long (*BuferGetEmptySize)(void);
  unsigned char (*CheckBufferEmptySize)(void);
  void (*DataProcesser)(void);
  void (*DataIn)(void);
  void (*DataOut)(unsigned char *Buffer,unsigned int  len);
  void (*RefreshRev)(void);
  u32 Pwrite; // 0: w=r
  u32 Pread;
  unsigned char Waitting;
  volatile unsigned char IDHandle;
  volatile unsigned long EP;//only for usb
  unsigned char *PBufferRev;
  volatile bool ConnectState;
  volatile unsigned long ReVRecordTotalLen;
}Device_Rev;




#endif
