#include "system/app_core.h"

#include "app_config.h"



#include "a2dp_media_codec.h"

#include "event/bt_event.h"

#include "syscfg/syscfg_id.h"

#include "asm/rf_coexistence_config.h"
#include "WeiTingCommon.h"

#define BTMAXBufferReceiveSize			10000
#define BTRevBusyLevelValue				2500
#define BTRevRefreshLevelValue			400


extern Device_Rev *BT_RevC;
extern Device_Rev BT_Rev;


void PrinterBTStart(void);

void PrinterInstructionSet(unsigned char);

void BTPrinterTask(void *p);





