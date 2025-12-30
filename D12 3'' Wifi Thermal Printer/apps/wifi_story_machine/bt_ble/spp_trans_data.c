#include "app_config.h"
#include "spp_user.h"
#include "bt_common.h"
#include "btstack/avctp_user.h"
#include "system/timer.h"
#include "system/sys_time.h"


/// \cond DO_NOT_DOCUMENT

#if 1
#define log_info          printf
#define log_info_hexdump  printf_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

#define TEST_SPP_DATA_RATE        1

#if TEST_SPP_DATA_RATE
#define SPP_TIMER_MS            (2)
#define TEST_SPP_SEND_SIZE      660
static u32 test_data_count;
static u8 spp_test_start;
#endif
/// \endcond

/**
 * \name
 * \{
 */
#define SPP_DATA_RECIEVT_FLOW              1
#define FLOW_SEND_CREDITS_NUM              1
#define FLOW_SEND_CREDITS_TRIGGER_NUM      1
/* \} name */

/// \cond DO_NOT_DOCUMENT
int transport_spp_flow_enable(u8 en);
void rfcomm_change_credits_setting(u16 init_credits, u8 base);
int rfcomm_send_cretits_by_profile(u16 rfcomm_cid, u16 credit, u8 auto_flag);

static struct spp_operation_t *spp_api = NULL;
static u8 spp_state;
u16 spp_channel;
/// \endcond

/* ----------------------------------------------------------------------------*/
/**

 */
/* ----------------------------------------------------------------------------*/
int transport_spp_send_data(u8 *data, u16 len)
{
    if (spp_api) {
         log_info("spp_api_tx(%d) \n", len);
        /* log_info_hexdump(data, len); */
        clear_sniff_cnt();
        return spp_api->send_data(NULL, data, len);
    }
    return SPP_USER_ERR_SEND_FAIL;
}
int transport_spp_send_dataSang(u8 *data, u16 len) // @20230713 sang
{
        clear_sniff_cnt();
        int Ret=spp_api->send_data(NULL, data, len);
        os_time_dly(len*5);
        return Ret;

}
/* ----------------------------------------------------------------------------*/
/**
 * @brief �?查spp是否允许发�?�数�?
 * @param[in]  len: 准备发�?�的数据长度
 * @return 1: 允许发�??
 * @return 0: 发�?�忙�?
 */
/* ----------------------------------------------------------------------------*/
int transport_spp_send_data_check(u16 len)
{
    if (spp_api) {
        if (spp_api->busy_state()) {
            return 0;
        }
    }
    return 1;
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief spp传输状�?�回�?
 * @param[in]  state: 连接状�??
 */
/* ----------------------------------------------------------------------------*/
static void transport_spp_state_cbk(u8 state)
{
    spp_state = state;

    switch (state) {
    case SPP_USER_ST_CONNECT:
        printf("SPP_USER_ST_CONNECT ~~~\n");
        break;
    case SPP_USER_ST_DISCONN:
        log_info("SPP_USER_ST_DISCONN ~~~\n");
        spp_channel = 0;
        break;
    default:
        break;
    }
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief spp传输唤醒回调
 */
/* ----------------------------------------------------------------------------*/
static void transport_spp_send_wakeup(void)
{
    /* putchar('W'); */
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief spp数据接收回调
 * @param[in]  priv: spp_channel
 * @param[in]  buf: 接收到的数据指针
 * @param[in]  len: 接收到的数据长度
 */
/* ----------------------------------------------------------------------------*/
//extern void transport_spp_recieve_cbk(void *priv, u8 *buf, u16 len);
/*
extern void transport_spp_recieve_cbk(void *priv, u8 *buf, u16 len);

{
    spp_channel = (u16)priv;
    printf("sang:spp_api_rx(%d) \n", len);

    //log_info_hexdump(buf, len);

    clear_sniff_cnt();

#if TEST_SPP_DATA_RATE
    if ((buf[0] == 'A') && (buf[1] == 'F')) {
        spp_test_start = 1;//start
    } else if ((buf[0] == 'A') && (buf[1] == 'A')) {
        spp_test_start = 0;//stop
    }

    //loop send data for test
    if (transport_spp_send_data_check(len)) {
        transport_spp_send_data(buf, len);
    }
#endif
}

*/
#if 1
   extern void transport_spp_recieve_cbk(void *priv, u8 *buf, u16 len);
#else
static void transport_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    spp_channel = (u16)priv;

	if(Data_Channel !=BTData)
      Data_Channel=BTData;
    if(BTstateS==BTFree)
    {
      BTSaveData(len,buf);		//save BT data   SaveData(len , buf);
    }
	/*
	if(BT_RevC->CheckBufferEmptySize()&&BT_RevC->Waitting==false)
	{

		BT_RevC->Waitting=true;
		transport_spp_flow_enable(1);// disable data in
		BTstateS=BTBusy;
    #if DebugMsg
		printf("sang:BT(SPP) busy set:%d..............................\r\n",BT_RevC->BuferGetEmptySize());
    #endif
	}*/
	    if(GetUSBEmptyBuff(BTData)<(BufferReceiveSizeEmpty))
    {
       transport_spp_flow_enable(1);
       BTstateS=BTBusy;
    }
    clear_sniff_cnt();

}
#endif
#if 0
static  void transport_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    spp_channel = (u16)priv;
    #if    (BtReceive==1)
    log_info("spp_api_rx(%d) \n", len);
    #endif
    //log_info_hexdump(buf, len);
    if(Data_Channel!=BTData)
      Data_Channel=BTData;
    if(BTstateS==BTFree)
    {
      //SaveData(len , buf, BTData);
    }
    if(GetUSBEmptyBuff()<(BufferReceiveSizeEmpty+500))
    {
       transport_spp_flow_enable(1);
       BTstateS=BTBusy;
    }
    clear_sniff_cnt();

#if TEST_SPP_DATA_RATE
    if ((buf[0] == 'A') && (buf[1] == 'F')) {
        spp_test_start = 1;//start
    } else if ((buf[0] == 'A') && (buf[1] == 'A')) {
        spp_test_start = 0;//stop
    }
#endif

    //loop send data for test
    #if    (BtSendOff==1)
    if (transport_spp_send_data_check(len)) {
        transport_spp_send_data(buf, len);
    }
    #endif
}

#endif
#if TEST_SPP_DATA_RATE

static void test_spp_send_data(void)
{
    u16 send_len = TEST_SPP_SEND_SIZE;
    if (transport_spp_send_data_check(send_len)) {
        test_data_count += send_len;
        transport_spp_send_data((u8 *)&test_data_count, send_len);
    }
}

static void test_timer_handler(void *p)
{
    static u32 t_sec = 0;

    if (SPP_USER_ST_CONNECT != spp_state) {
        test_data_count = 0;
        spp_test_start = 0;
        return;
    }

    if (spp_test_start) {
        test_spp_send_data();
    }

    if (timer_get_sec() <= t_sec) {
        return;
    }
    t_sec = timer_get_sec();

    if (test_data_count) {
        log_info("\n-spp_data_rate: %d bytes, %d kbytes-\n", test_data_count, test_data_count / 1024);
        test_data_count = 0;
    }
}
#endif
/* ----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------*/
static void transport_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{

    spp_channel = (u16)priv;
	//  log_info("spp_api_rx(%d) \n", len);
	// log_info_hexdump(buf, len);
	EdrPrinterReceiveDataISR(buf,len);
    clear_sniff_cnt();

}
/* ----------------------------------------------------------------------------*/
/**
 * @brief 初始化spp传输接口
 * @note  �?要在蓝牙协议栈初始化btstack_init()成功后收到BT_STATUS_INIT_OK消息后调�?
 */
/* ----------------------------------------------------------------------------*/
void transport_spp_init(void)
{
#if (USER_SUPPORT_PROFILE_SPP==1)
		    log_info("transport_spp_init\n");

		    spp_state = 0;
		    spp_get_operation_table(&spp_api);
		    spp_api->regist_recieve_cbk(0, transport_spp_recieve_cbk);
		    spp_api->regist_state_cbk(0, transport_spp_state_cbk);
		    spp_api->regist_wakeup_send(NULL, transport_spp_send_wakeup);
#endif

#if TEST_SPP_DATA_RATE
		    sys_timer_add(NULL, test_timer_handler, SPP_TIMER_MS);
#endif


}

/* ----------------------------------------------------------------------------*/
/**
 * @brief 断开SPP传输连接
 */
/* ----------------------------------------------------------------------------*/
void transport_spp_disconnect(void)
{
    if (SPP_USER_ST_CONNECT == spp_state) {
        log_info("transport_spp_disconnect\n");
        user_send_cmd_prepare(USER_CTRL_SPP_DISCONNECT, 0, NULL);
    }
}

static void timer_spp_flow_test(void)
{
    static u8 sw = 0;
    if (spp_channel) {
        sw = !sw;
        transport_spp_flow_enable(sw);
    }
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief 配置SPP传输接收流控控制参数
 * @note  �?要在蓝牙协议栈初始化btstack_init()前调�?
 */
/* ----------------------------------------------------------------------------*/
void transport_spp_flow_cfg(void)
{
#if SPP_DATA_RECIEVT_FLOW
    rfcomm_change_credits_setting(FLOW_SEND_CREDITS_NUM, FLOW_SEND_CREDITS_TRIGGER_NUM);

    //for test
    /* sys_timer_add(0,timer_spp_flow_test,2000); */
#endif
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief SPP传输接收流控�?�?
 * @param[in]  en: 1-打开  0-关闭
 * @return 0: 成功
 * @return other: 失败
 */
/* ----------------------------------------------------------------------------*/
int transport_spp_flow_enable(u8 en)
{
    int ret = -1;

#if SPP_DATA_RECIEVT_FLOW
    if (spp_channel) {
        ret = rfcomm_send_cretits_by_profile(spp_channel, en ? 0 : FLOW_SEND_CREDITS_NUM, !en);
        //log_info("transport_spp_flow_enable:%02x,%d,%d\n", spp_channel, en, ret);
    }
#endif

    return ret;
}

