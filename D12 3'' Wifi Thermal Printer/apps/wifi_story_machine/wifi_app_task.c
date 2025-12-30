#include "app_config.h"
#include "system/includes.h"
#include "wifi/wifi_connect.h"
#include "lwip.h"
#include "dhcp_srv/dhcp_srv.h"
#include "event/net_event.h"
#include "net/assign_macaddr.h"
#include "syscfg_id.h"
#include "lwip/sockets.h"
#include "wifi_app_task_Print.h"


void WifiStopManual(void);
void WiFiStartManual(void);
bool UserCPasswordR=false;

static char save_ssid_flag, request_connect_flag;
#ifdef CONFIG_ASSIGN_MACADDR_ENABLE
static char mac_addr_succ_flag;
#endif
#ifdef CONFIG_STATIC_IPADDR_ENABLE
static u8 use_static_ipaddr_flag;
#endif

//select a default mode
//#define AP_MODE_TEST
#define STA_MODE_TEST
//#define MONITOR_MODE_TEST

//#define WIFI_MODE_CYCLE_TEST

#define FORCE_DEFAULT_MODE 0

#define AP_SSID "AC79_WIFI_DEMO_"
#define AP_PWD  ""
#define STA_SSID  "tht"
#define STA_PWD  "tht12345abc"
//#define STA_SSID  "sang"
//#define STA_PWD  "123456789"

#define CONNECT_BEST_SSID  0    //
#ifdef CONFIG_STATIC_IPADDR_ENABLE
struct sta_ip_info {
    u8 ssid[33];
    u32 ip;
    u32 gw;
    u32 netmask;
    u32 dns;
    u8 gw_mac[6];
    u8 local_mac[6];
    u8 chanel;
};

static void wifi_set_sta_ip_info(void)
{
    struct sta_ip_info  sta_ip_info;
    syscfg_read(VM_STA_IPADDR_INDEX, (char *) &sta_ip_info, sizeof(struct sta_ip_info));

    struct lan_setting lan_setting_info = {

        .WIRELESS_IP_ADDR0  = (u8)(sta_ip_info.ip >> 0),
        .WIRELESS_IP_ADDR1  = (u8)(sta_ip_info.ip >> 8),
        .WIRELESS_IP_ADDR2  = (u8)(sta_ip_info.ip >> 16),
        .WIRELESS_IP_ADDR3  = (u8)(sta_ip_info.ip >> 24),

        .WIRELESS_NETMASK0  = (u8)(sta_ip_info.netmask >> 0),
        .WIRELESS_NETMASK1  = (u8)(sta_ip_info.netmask >> 8),
        .WIRELESS_NETMASK2  = (u8)(sta_ip_info.netmask >> 16),
        .WIRELESS_NETMASK3  = (u8)(sta_ip_info.netmask >> 24),

        .WIRELESS_GATEWAY0   = (u8)(sta_ip_info.gw >> 0),
        .WIRELESS_GATEWAY1   = (u8)(sta_ip_info.gw >> 8),
        .WIRELESS_GATEWAY2   = (u8)(sta_ip_info.gw >> 16),
        .WIRELESS_GATEWAY3   = (u8)(sta_ip_info.gw >> 24),
    };

    net_set_lan_info(&lan_setting_info);
}

static int compare_dhcp_ipaddr(void)
{
    use_static_ipaddr_flag = 0;//don't use  static Ip address by default

    int ret;
    u8 local_mac[6];
    u8 gw_mac[6];
    u8 sta_channel;
	printf("compare_dhcp_ipaddr---->>>>");
    struct sta_ip_info  sta_ip_info;
    struct netif_info netif_info;

    ret = syscfg_read(VM_STA_IPADDR_INDEX, (char *) &sta_ip_info, sizeof(struct sta_ip_info));//read stored IP address
    if (ret < 0)
	{
        puts("There isn't stored IP address,so use DHCP IP address\r\n");
        return -1;
    }
	printf("saved SSID:%s\n",sta_ip_info.ssid);
	printf("saved IP:%d.%d.%d.%d--->",(sta_ip_info.ip&0xff)>>0,(sta_ip_info.ip&0xff00)>>8,(sta_ip_info.ip&0xff0000)>>16,(sta_ip_info.ip&0xff000000)>>24);
	printf("local_mac:%02X:%02X:%02X:%02X:%02X:%02X--->",sta_ip_info.local_mac[5],sta_ip_info.local_mac[4],sta_ip_info.local_mac[3],sta_ip_info.local_mac[2],sta_ip_info.local_mac[1],sta_ip_info.local_mac[0]);
    lwip_get_netif_info(1, &netif_info);
    struct wifi_mode_info info;
    info.mode = STA_MODE;
    wifi_get_mode_cur_info(&info);

    sta_channel = wifi_get_channel();
    wifi_get_bssid(gw_mac);
    wifi_get_mac(local_mac);

    if (!strcmp(info.ssid, sta_ip_info.ssid)
        && !memcmp(local_mac, sta_ip_info.local_mac, 6)
        && !memcmp(gw_mac, sta_ip_info.gw_mac, 6))
	{
        use_static_ipaddr_flag = 1;
        puts("compare_dhcp_ipaddr Match,use static IP address\r\n");
        return 0;
    }
    printf("compare_dhcp_ipaddr not Match!!! [%s][%s],[0x%x,0x%x][0x%x,0x%x],[0x%x] \r\n",
    info.ssid, sta_ip_info.ssid, local_mac[0], local_mac[5], sta_ip_info.local_mac[0], sta_ip_info.local_mac[5], sta_ip_info.dns);
    return -1;
}

static void store_dhcp_ipaddr(void)
{
    struct sta_ip_info  sta_ip_info = {0};
    u8 sta_channel;
    u8 local_mac[6];
    u8 gw_mac[6];

    printf("store_dhcp_ipaddr--->");
    if (use_static_ipaddr_flag)
	{
		printf("sang:IP address matched!--->");
        return;
    }

    struct netif_info netif_info;
    lwip_get_netif_info(1, &netif_info);

    struct wifi_mode_info info;
    info.mode = STA_MODE;
    wifi_get_mode_cur_info(&info);
	printf("wifi_get_mode_cur_info:%s,Password:%s--->",info.ssid,info.pwd);

    sta_channel = wifi_get_channel();
    wifi_get_mac(local_mac);
	printf("local_mac:%02x-%02x-%02x-%02x-%02x-%02x-%02x----->",local_mac[0],local_mac[1],local_mac[2],local_mac[3],local_mac[4],local_mac[5],local_mac[6]);
    wifi_get_bssid(gw_mac);

    strcpy(sta_ip_info.ssid, info.ssid);
    memcpy(sta_ip_info.gw_mac, gw_mac, 6);
    memcpy(sta_ip_info.local_mac, local_mac, 6);
    sta_ip_info.ip =  netif_info.ip;
    sta_ip_info.netmask =  netif_info.netmask;
    sta_ip_info.gw =  netif_info.gw;
    sta_ip_info.chanel = sta_channel;
    sta_ip_info.dns = *(u32 *)dns_getserver(0);
	printf("VM_STA_IPADDR_INDEX:%s",sta_ip_info.ssid);

    syscfg_write(VM_STA_IPADDR_INDEX, (char *) &sta_ip_info, sizeof(struct sta_ip_info));

    puts("store_dhcp_ipaddr\r\n");
}

void dns_set_server(u32 *dnsserver)
{
    struct sta_ip_info  sta_ip_info;
    if (syscfg_read(VM_STA_IPADDR_INDEX, (char *) &sta_ip_info, sizeof(struct sta_ip_info)) < 0) {
        *dnsserver = 0;
    } else {
        *dnsserver = sta_ip_info.dns;
    }
}

#endif

static void wifi_set_lan_setting_info(void)
{
    struct lan_setting lan_setting_info = {

        .WIRELESS_IP_ADDR0  = 192,
        .WIRELESS_IP_ADDR1  = 168,
        .WIRELESS_IP_ADDR2  = 1,
        .WIRELESS_IP_ADDR3  = 1,

        .WIRELESS_NETMASK0  = 255,
        .WIRELESS_NETMASK1  = 255,
        .WIRELESS_NETMASK2  = 255,
        .WIRELESS_NETMASK3  = 0,

        .WIRELESS_GATEWAY0  = 192,
        .WIRELESS_GATEWAY1  = 168,
        .WIRELESS_GATEWAY2  = 1,
        .WIRELESS_GATEWAY3  = 1,

        .SERVER_IPADDR1  = 192,
        .SERVER_IPADDR2  = 168,
        .SERVER_IPADDR3  = 1,
        .SERVER_IPADDR4  = 1,

        .CLIENT_IPADDR1  = 192,
        .CLIENT_IPADDR2  = 168,
        .CLIENT_IPADDR3  = 1,
        .CLIENT_IPADDR4  = 2,

        .SUB_NET_MASK1   = 255,
        .SUB_NET_MASK2   = 255,
        .SUB_NET_MASK3   = 255,
        .SUB_NET_MASK4   = 0,
    };
    net_set_lan_info(&lan_setting_info);
}
char SSIDSave[64],PASSWORDSave[64],WiFiMAC[6];
void GetWifiSTAInfo(void)
{
	 struct wifi_mode_info info;
    info.mode = STA_MODE;
    wifi_get_mode_cur_info(&info);
	printf("wifi_get_mode_cur_info:%s,Password:%s--->",info.ssid,info.pwd);
	strcpy(SSIDSave, info.ssid);
	strcpy(PASSWORDSave, info.pwd);
	wifi_get_mac(WiFiMAC);
}
static void wifi_sta_save_ssid(void)
{
	printf("wifi_sta_save_ssid----?");
    if (save_ssid_flag)
	{
        save_ssid_flag = 0;
		printf("wifi_sta_save_ssid---->");
        struct wifi_mode_info info;
        info.mode = STA_MODE;
        wifi_get_mode_cur_info(&info);
        wifi_store_mode_info(STA_MODE, info.ssid, info.pwd);
    }
}
void SaveWifiInfo(char *SSID,char *PASSWord)
{
	printf("SaveWifiInfo");
	struct wifi_mode_info info;
	info.mode = STA_MODE;
	wifi_get_mode_cur_info(&info);
	info.mode = STA_MODE;
	strcpy(info.ssid,SSID);
	strcpy(info.pwd,PASSWord);
	wifi_store_mode_info(STA_MODE, info.ssid, info.pwd);

}
void wifi_return_sta_mode(void)
{
	printf("wifi_return_sta_mode--->");
    if (!wifi_is_on()) {
        return;
    }
    int ret;
    struct wifi_mode_info info;
    info.mode = STA_MODE;
    ret = wifi_get_mode_stored_info(&info);
    if (ret)//if there isn't stored info about the STA,then use the default!!
	{
        info.ssid = STA_SSID;
        info.pwd = STA_PWD;
		printf("wifi_get_mode_stored_info failed use default SSID info--->");
    }
    wifi_clear_scan_result();
    wifi_set_sta_connect_best_ssid(0);
    save_ssid_flag = 0;
    wifi_enter_sta_mode(info.ssid, info.pwd);
}

void wifi_sta_connect(char *ssid, char *pwd, char save)
{
    save_ssid_flag = save;
    request_connect_flag = 1;
    wifi_set_sta_connect_best_ssid(0);
    wifi_enter_sta_mode(ssid, pwd);
}

unsigned long IPAddress=0;
unsigned char WiFiConnectStatus=0;
static int wifi_event_callback(void *network_ctx, enum WIFI_EVENT event)
{
    struct net_event net = {0};
    net.arg = "net";
    int ret = 0;

	 printf("wifi_event_callback:%d\n",event);

    switch (event) {

    case WIFI_EVENT_MODULE_INIT:
		WiFiConnectStatus=false;
        wifi_set_sta_connect_timeout(30);
        wifi_set_smp_cfg_timeout(30);

        struct wifi_store_info wifi_default_mode_parm;
        memset(&wifi_default_mode_parm, 0, sizeof(struct wifi_store_info));

#if (defined AP_MODE_TEST)
        wifi_set_lan_setting_info();
        u8 mac_addr[6];
        char ssid[64];
        int init_net_device_mac_addr(char *macaddr, char ap_mode);
        init_net_device_mac_addr((char *)mac_addr, 1);
        sprintf((char *)ssid, AP_SSID"%02x%02x%02x%02x%02x%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        wifi_default_mode_parm.mode = AP_MODE;
        strncpy((char *)wifi_default_mode_parm.ssid[wifi_default_mode_parm.mode - STA_MODE], (const char *)ssid, sizeof(wifi_default_mode_parm.ssid[wifi_default_mode_parm.mode - STA_MODE]) - 1);
        strncpy((char *)wifi_default_mode_parm.pwd[wifi_default_mode_parm.mode - STA_MODE], (const char *)AP_PWD, sizeof(wifi_default_mode_parm.pwd[wifi_default_mode_parm.mode - STA_MODE]) - 1);
#elif (defined STA_MODE_TEST)
        wifi_default_mode_parm.mode = STA_MODE;
        strncpy((char *)wifi_default_mode_parm.ssid[wifi_default_mode_parm.mode - STA_MODE], (const char *)STA_SSID, sizeof(wifi_default_mode_parm.ssid[wifi_default_mode_parm.mode - STA_MODE]) - 1);
        strncpy((char *)wifi_default_mode_parm.pwd[wifi_default_mode_parm.mode - STA_MODE], (const char *)STA_PWD, sizeof(wifi_default_mode_parm.pwd[wifi_default_mode_parm.mode - STA_MODE]) - 1);
        wifi_default_mode_parm.connect_best_network = CONNECT_BEST_SSID;
#elif (defined MONITOR_MODE_TEST)
        memset(&wifi_default_mode_parm, 0, sizeof(struct wifi_store_info));
        wifi_default_mode_parm.mode = SMP_CFG_MODE;
#endif
        //under STA mode condition,save default SSID
        printf("wifi_set_default_mode:%s->%s",wifi_default_mode_parm.ssid,wifi_default_mode_parm.pwd);
	 	wifi_set_default_mode(&wifi_default_mode_parm, FORCE_DEFAULT_MODE, wifi_default_mode_parm.mode == STA_MODE);
        break;

    case WIFI_EVENT_MODULE_START:
        puts("|network_user_callback->WIFI_EVENT_MODULE_START\n");

        struct wifi_mode_info info;
        info.mode = NONE_MODE;
        wifi_get_mode_cur_info(&info);
        if (info.mode == SMP_CFG_MODE) {
            net.arg = "net";
            net.event = NET_EVENT_SMP_CFG_FIRST;
            net_event_notify(NET_EVENT_FROM_USER, &net);
        }

        u32  tx_rate_control_tab =
            0
            | BIT(0) //0:CCK 1M
            | BIT(1) //1:CCK 2M
            | BIT(2) //2:CCK 5.5M
            | BIT(3) //3:OFDM 6M
            | BIT(4) //4:MCS0/7.2M
            | BIT(5) //5:OFDM 9M
            | BIT(6) //6:CCK 11M
            | BIT(7) //7:OFDM 12M
            | BIT(8) //8:MCS1/14.4M
            | BIT(9) //9:OFDM 18M
            | BIT(10) //10:MCS2/21.7M
            | BIT(11) //11:OFDM 24M
            | BIT(12) //12:MCS3/28.9M
            | BIT(13) //13:OFDM 36M
            | BIT(14) //14:MCS4/43.3M
            | BIT(15) //15:OFDM 48M
            | BIT(16) //16:OFDM 54M
            | BIT(17) //17:MCS5/57.8M
            | BIT(18) //18:MCS6/65.0M
            | BIT(19) //19:MCS7/72.2M
            ;
        wifi_set_tx_rate_control_tab(tx_rate_control_tab);
#if 0
		printf("set Low power value");
        wifi_set_pwr(0);
#endif

        break;
    case WIFI_EVENT_MODULE_STOP:
        puts("|network_user_callback->WIFI_EVENT_MODULE_STOP\n");
        break;

    case WIFI_EVENT_AP_START:
        printf("|network_user_callback->WIFI_EVENT_AP_START,CH=%d\n", wifi_get_channel());
        //wifi_rxfilter_cfg(7);
        break;
    case WIFI_EVENT_AP_STOP:
        puts("|network_user_callback->WIFI_EVENT_AP_STOP\n");
        break;

    case WIFI_EVENT_STA_START:
        puts("sang:network_user_callback->WIFI_EVENT_STA_START\n");
        break;
    case WIFI_EVENT_MODULE_START_ERR:
        puts("|network_user_callback->WIFI_EVENT_MODULE_START_ERR\n");
        break;
    case WIFI_EVENT_STA_STOP:
        puts("|network_user_callback->WIFI_EVENT_STA_STOP\n");
        break;
    case WIFI_EVENT_STA_DISCONNECT:
	printf("WIFI_EVENT_STA_DISCONNECT\r\n");
		WiFiConnectStatus=false;
		IPAddress=0;
        puts("sang:WIFI_EVENT_STA_DISCONNECT\n");

        /*wifi_rxfilter_cfg(0);*/

#ifdef CONFIG_ASSIGN_MACADDR_ENABLE
        if (!mac_addr_succ_flag) {
            break;
        }
#endif

        net.event = NET_EVENT_DISCONNECTED;
        net_event_notify(NET_EVENT_FROM_USER, &net);

#ifndef WIFI_MODE_CYCLE_TEST
        if (!request_connect_flag) {

            net.event = NET_EVENT_DISCONNECTED_AND_REQ_CONNECT;
            net_event_notify(NET_EVENT_FROM_USER, &net);
        }
#endif

        break;
    case WIFI_EVENT_STA_SCAN_COMPLETED:
        puts("|network_user_callback->WIFI_STA_SCAN_COMPLETED\n");
#ifdef CONFIG_AIRKISS_NET_CFG
        extern void airkiss_ssid_check(void);
        airkiss_ssid_check();
#endif
        break;
    case WIFI_EVENT_STA_CONNECT_SUCC:
		printf("sang:WIFI_EVENT_STA_CONNECT_SUCC=%d\r\n", wifi_get_channel());
        //wifi_rxfilter_cfg(3);
#ifdef CONFIG_STATIC_IPADDR_ENABLE
        if (0 == compare_dhcp_ipaddr())
		{
            wifi_set_sta_ip_info();
            ret = 1;
        }
#endif
        wifi_set_sta_connect_best_ssid(0);
        break;

    case WIFI_EVENT_MP_TEST_START:
        printf("|network_user_callback->WIFI_EVENT_MP_TEST_START\n");
        break;
    case WIFI_EVENT_MP_TEST_STOP:
        printf("|network_user_callback->WIFI_EVENT_MP_TEST_STOP\n");
        break;

    case WIFI_EVENT_STA_CONNECT_TIMEOUT_NOT_FOUND_SSID:
        printf("|network_user_callback->WIFI_STA_CONNECT_TIMEOUT_NOT_FOUND_SSID\n");
        net.event = NET_CONNECT_TIMEOUT_NOT_FOUND_SSID;
        net_event_notify(NET_EVENT_FROM_USER, &net);
        break;

    case WIFI_EVENT_STA_CONNECT_ASSOCIAT_FAIL:

        printf("|network_user_callback->WIFI_STA_CONNECT_ASSOCIAT_FAIL .....\n");
        WifiStopManual();
        net.event = NET_CONNECT_ASSOCIAT_FAIL;
        net_event_notify(NET_EVENT_FROM_USER, &net);
        break;

    case WIFI_EVENT_STA_CONNECT_ASSOCIAT_TIMEOUT:
		WifiStopManual();
        printf("|network_user_callback->WIFI_STA_CONNECT_ASSOCIAT_TIMEOUT .....\n");
        WiFiStartManual();
        break;

    case WIFI_EVENT_STA_NETWORK_STACK_DHCP_SUCC:
        puts("sang:network_user_callback->WIFI_EVENT_STA_NETWPRK_STACK_DHCP_SUCC\n");
        struct netif_info netif_infoA;
		lwip_get_netif_info(1, &netif_infoA);
        printf("sang:ipaddr = [%d.%d.%d.%d] \r\n\r\n",netif_infoA.ip&0xff, (netif_infoA.ip>>8)&0xff,(netif_infoA.ip>>16)&0xff,(netif_infoA.ip>>24)&0xff);

        MEP.IP=netif_infoA.ip;

        void connect_broadcast(void);
        connect_broadcast();
        wifi_sta_save_ssid();
		WiFiConnectStatus=true;
		IPAddress=MEP.IP;

#ifdef CONFIG_ASSIGN_MACADDR_ENABLE

        if (!is_server_assign_macaddr_ok()) {
            server_assign_macaddr(wifi_return_sta_mode);
            break;
        }
        mac_addr_succ_flag = 1;
#endif
#ifdef CONFIG_STATIC_IPADDR_ENABLE
        store_dhcp_ipaddr();
#endif
        request_connect_flag = 0;
        net.event = NET_EVENT_CONNECTED;
        net_event_notify(NET_EVENT_FROM_USER, &net);

        break;
    case WIFI_EVENT_STA_NETWORK_STACK_DHCP_TIMEOUT:
        printf("|network_user_callback->WIFI_EVENT_STA_NETWPRK_STACK_DHCP_TIMEOUT\n");
        break;

    case WIFI_EVENT_P2P_START:
        printf("|network_user_callback->WIFI_EVENT_P2P_START\n");
        break;
    case WIFI_EVENT_P2P_STOP:
        printf("|network_user_callback->WIFI_EVENT_P2P_STOP\n");
        break;
    case WIFI_EVENT_P2P_GC_DISCONNECTED:
        printf("|network_user_callback->WIFI_EVENT_P2P_GC_DISCONNECTED\n");
        break;
    case WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_SUCC:
        printf("|network_user_callback->WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_SUCC\n");
        break;
    case WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_TIMEOUT:
        printf("|network_user_callback->WIFI_EVENT_P2P_GC_NETWORK_STACK_DHCP_TIMEOUT\n");
        break;

    case WIFI_EVENT_SMP_CFG_START:
        printf("|network_user_callback->WIFI_EVENT_SMP_CFG_START\n");
        break;
    case WIFI_EVENT_SMP_CFG_STOP:
        printf("|network_user_callback->WIFI_EVENT_SMP_CFG_STOP\n");
        break;
    case WIFI_EVENT_SMP_CFG_TIMEOUT:
        printf("|network_user_callback->WIFI_EVENT_SMP_CFG_TIMEOUT\n");
        net.event = NET_EVENT_SMP_CFG_TIMEOUT;
        net_event_notify(NET_EVENT_FROM_USER, &net);
        break;
    case WIFI_EVENT_SMP_CFG_COMPLETED:
        printf("|network_user_callback->WIFI_EVENT_SMP_CFG_COMPLETED\n");
        net.event = NET_SMP_CFG_COMPLETED;
        net_event_notify(NET_EVENT_FROM_USER, &net);
        break;

    case WIFI_EVENT_PM_SUSPEND:
        printf("|network_user_callback->WIFI_EVENT_PM_SUSPEND\n");
        break;
    case WIFI_EVENT_PM_RESUME:
        printf("|network_user_callback->WIFI_EVENT_PM_RESUME\n");
        break;
    case WIFI_EVENT_AP_ON_ASSOC:
        ;
        struct eth_addr *hwaddr = (struct eth_addr *)network_ctx;
        printf("WIFI_EVENT_AP_ON_ASSOC hwaddr = %02x:%02x:%02x:%02x:%02x:%02x \r\n\r\n",
               hwaddr->addr[0], hwaddr->addr[1], hwaddr->addr[2], hwaddr->addr[3], hwaddr->addr[4], hwaddr->addr[5]);
        break;
    case WIFI_EVENT_AP_ON_DISCONNECTED:
         struct ip4_addr ipaddr;
        hwaddr = (struct eth_addr *)network_ctx;
        dhcps_get_ipaddr(hwaddr->addr, &ipaddr);
        printf("sang:-----WIFI_EVENT_AP_ON_DISCONNECTED hwaddr = %02x:%02x:%02x:%02x:%02x:%02x, ipaddr = [%d.%d.%d.%d] \r\n\r\n",
               hwaddr->addr[0], hwaddr->addr[1], hwaddr->addr[2], hwaddr->addr[3], hwaddr->addr[4], hwaddr->addr[5],
               ip4_addr1(&ipaddr), ip4_addr2(&ipaddr), ip4_addr3(&ipaddr), ip4_addr4(&ipaddr));
        break;
    default:
        break;
    }

    return ret;
}
void PrintIPAddress(void)
{
        struct netif_info netif_infoA;
		lwip_get_netif_info(1, &netif_infoA);
        printf("sang:ipaddr = [%d.%d.%d.%d] \r\n\r\n",netif_infoA.ip&0xff, (netif_infoA.ip>>8)&0xff,(netif_infoA.ip>>16)&0xff,(netif_infoA.ip>>24)&0xff);
}
static void wifi_rx_cb(void *rxwi, struct ieee80211_frame *wh, void *data, u32 len, void *priv)
{
    char *str_frm_type;
    switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
    case IEEE80211_FC0_TYPE_MGT:
        switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
        case IEEE80211_FC_STYPE_ASSOC_REQ:
            str_frm_type = "association req";
            break;
        case IEEE80211_FC_STYPE_ASSOC_RESP:
            str_frm_type = "association resp";
            break;
        case IEEE80211_FC_STYPE_REASSOC_REQ:
            str_frm_type = "reassociation req";
            break;
        case IEEE80211_FC_STYPE_REASSOC_RESP:
            str_frm_type = "reassociation resp";
            break;
        case IEEE80211_FC_STYPE_PROBE_REQ:
            str_frm_type = "probe req";
            break;
        case IEEE80211_FC_STYPE_PROBE_RESP:
            str_frm_type = "probe resp";
            break;
        case IEEE80211_FC_STYPE_BEACON:
            str_frm_type = "beacon";
            break;
        case IEEE80211_FC_STYPE_ATIM:
            str_frm_type = "atim";
            break;
        case IEEE80211_FC_STYPE_DISASSOC:
            str_frm_type = "disassociation";
            break;
        case IEEE80211_FC_STYPE_AUTH:
            str_frm_type = "authentication";
            break;
        case IEEE80211_FC_STYPE_DEAUTH:
            str_frm_type = "deauthentication";
            break;
        case IEEE80211_FC_STYPE_ACTION:
            str_frm_type = "action";
            break;
        default:
            str_frm_type = "unknown mgmt";
            break;
        }
        break;
    case IEEE80211_FC0_TYPE_CTL:
        str_frm_type = "control";
        break;
    case IEEE80211_FC0_TYPE_DATA:
        str_frm_type = "data";
        break;
    default:
        str_frm_type = "unknown";
        break;
    }
    printf("wifi recv:%s\n", str_frm_type);
}

static void wifi_status(void *p)
{
    if (wifi_is_on()) {
        stats_display(); //LWIP stats
        //printf("WIFI U= %d KB/s, D= %d KB/s\r\n", wifi_get_upload_rate() / 1024, wifi_get_download_rate() / 1024);

        struct wifi_mode_info info;
        info.mode = NONE_MODE;
        wifi_get_mode_cur_info(&info);
        if (info.mode == AP_MODE) {
            for (int i = 0; i < 8; i++) {
                char *rssi;
                u8 *evm, *mac;
                if (wifi_get_sta_entry_rssi(i, &rssi, &evm, &mac)) {
                    break;
                }
                if (*rssi)
                {
                    //printf("MAC[%x:%x:%x:%x:%x:%x],RSSI=%d,EVM=%d \r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], *rssi, *evm);
                }
            }
        } else if (info.mode == STA_MODE)
        {
          //  printf("Router_RSSI=%d,Quality=%d \r\n", wifi_get_rssi(), wifi_get_cqi());
        }
    }
}

static void wifi_scan_test(void)
{
    struct wifi_scan_ssid_info *sta_ssid_info;
    u32 sta_ssid_num;

    wifi_clear_scan_result();

    wifi_scan_req();

#if 0



#else

    os_time_dly(4 * 100);
    sta_ssid_num = 0;
    sta_ssid_info = wifi_get_scan_result(&sta_ssid_num);
    printf("wifi_sta_scan_test ssid_num =%d \r\n", sta_ssid_num);
    for (int i = 0; i < sta_ssid_num; i++) {
        printf("wifi_sta_scan_test ssid = [%s],rssi = %d,snr = %d\r\n", sta_ssid_info[i].ssid, sta_ssid_info[i].rssi, sta_ssid_info[i].snr);
    }

    free(sta_ssid_info);
#endif

    static u8 scan_cnt;
    if (++scan_cnt > 4) {
        scan_cnt = 0;
        wifi_clear_scan_result();
    }
}


#define OFF 0
#define ON  1
static char Wifi_Set_Status=ON;
bool startWifiAGN=0;
static void wifi_demo_task(void *priv)
{
   // printf("wifi_demo_task-FYX_088E_2.4G----------->\r\n");

restart:
    Wifi_Set_Status = true;
    UserCPasswordR = false;
    wifi_set_store_ssid_cnt(NETWORK_SSID_INFO_CNT);
    wifi_set_event_callback(wifi_event_callback);

    wifi_on();
    os_time_dly(50);

    struct wifi_mode_info info;
    info.mode = STA_MODE;
    int ret = wifi_get_mode_stored_info(&info);

    if (ret == 0)
    {
        wifi_enter_sta_mode(info.ssid, info.pwd);
    }

    else
    {
        strncpy(info.ssid, STA_SSID, sizeof(info.ssid) - 1);
        strncpy(info.pwd, STA_PWD, sizeof(info.pwd) - 1);
        wifi_enter_sta_mode(info.ssid, info.pwd);
    }


    while (1)
    {
        os_time_dly(300);

        if (Wifi_Set_Status == OFF)
        {
            while (1)
            {
                os_time_dly(100);
                // when the step motor stopped and start WiFi flag is set,then restart the WiFi algorithm
                if (startWifiAGN && StepMotorStatus() == 0)
                {
                    //printf("wifi restart------------>\r\n");
                    goto restart;
                }
            }
        }
    }
}
void WifiStopManual(void)
{
	printf("WifiStopManual------------>\r\n");
	wifi_off();
	Wifi_Set_Status=false;
	UserCPasswordR=true;
	startWifiAGN=0;
}
void WiFiStartManual(void)
{
	printf("WiFiStartManual------------>\r\n");
    Wifi_Set_Status = false;
	startWifiAGN=1;
}
int demo_wifi(void)
{
    return os_task_create(wifi_demo_task, NULL, 10, 1000, 0, "wifi_demo_task");
}
late_initcall(demo_wifi);






