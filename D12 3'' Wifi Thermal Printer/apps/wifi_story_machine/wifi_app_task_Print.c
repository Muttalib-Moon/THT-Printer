

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

#include "sock_api/sock_api.h"
#include "os/os_api.h"
#include "lwip/netdb.h"

static bool OEMQuitFlag = false;

#include "AppDriverGeneric.h" //JL lib dependency
#include "AppDriver.h"

bool TCP_mute = false;
OS_MUTEX tcp_mutex; // mute

// tcp_server���ݽṹ
struct tcp_server_info
{
    struct list_head client_head;  // ����
    struct sockaddr_in local_addr; // tcp_server��ַ��Ϣ
    void *fd;                      // tcp_server�׽���
    OS_MUTEX tcp_mutex;            // ������
};
struct tcp_client_info
{
    struct list_head entry;         // �����ڵ�
    struct sockaddr_in remote_addr; // tcp_client��ַ��Ϣ
    void *fd;                       // �������Ӻ���׽���
};
static struct tcp_client_info *get_tcp_client_info(void);
static void tcp_client_quit(struct tcp_client_info *priv);

static struct tcp_server_info Printer_server_info;
struct MyEthernetP MEP;

struct tcp_client_info tcp_client_info_sang; /// ���嵱ǰ
void WiFi_Data_MuteCreate(void)
{
    if (os_mutex_create(&tcp_mutex) != OS_NO_ERR)
    {
        printf("%s os_mutex_create fail\n", __FILE__);
        return;
    }
    printf("WiFi_Data_MuteCreate successful!\r\n");
    TCP_mute = true;
}
void Wifi_Mute_Wait(void)
{
    if (TCP_mute)
        os_mutex_pend(&tcp_mutex, 0);
}
void Wifi_Mute_Post(void)
{
    if (TCP_mute)
        os_mutex_post(&tcp_mutex);
}
static struct tcp_client_info *get_tcp_client_info(void)
{
    struct list_head *pos = NULL;
    struct tcp_client_info *client_info = NULL;
    struct tcp_client_info *old_client_info = NULL;

    os_mutex_pend(&Printer_server_info.tcp_mutex, 0);

    list_for_each(pos, &Printer_server_info.client_head)
    {
        client_info = list_entry(pos, struct tcp_client_info, entry);
    }

    os_mutex_post(&Printer_server_info.tcp_mutex);

    return client_info;
}
static int tcp_recv_data(void *sock_hdl, void *buf, u32 len)
{
    return sock_recv(sock_hdl, buf, len, 0);
}

static int accept_pid = -1;
static int recv_pid = -1;
static int timeout_pid=-1;
static void tcp_sock_accpet(void);
static void tcp_recv_handler(void);
static int tcp_send_data(void *sock_hdl, const void *buf, u32 len)
{
    return sock_send(sock_hdl, buf, len, 0);
}
bool isSocketClosed(int sockfd)
{
    int error = 0;
    socklen_t len = sizeof(error);

    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
    {
        printf("socket closed.....\r\n");
        return true; // ��ȡѡ��ʧ�ܣ�socket�����ѹر�
    }

    return (error != 0); // ����д�����Ϊsocket�ѹر�
}
static int tcp_send_data_sang(const void *buf, u32 len)
{
    /*
        struct tcp_client_info *client_info = NULL;
        char recv_buf[MAX_RECV_BUF_SIZE] = {0};

        client_info = get_tcp_client_info();
        os_time_dly(100);
        if(client_info == NULL)
        {
            printf("not connnect client\r\n");
            return 0;
        }

        return sock_send(client_info->fd, buf, len, 0);
        */
    if (tcp_client_info_sang.fd != NULL)
        return sock_send(tcp_client_info_sang.fd, buf, len, 0);
}
#define DivideSize 100
char recv_buf[1024] = {0};
struct tcp_client_info *client_infoCCC = NULL;
void WiFiPrinterReceiveDataTask(unsigned char *data, unsigned int len);

static void Printer_Tcp_Recv_handler(void)
{
    int len;
_reconnect_:
    printf("sang:Printer_Tcp_Recv_handler Start.............\r\n");
    do
    {
        client_infoCCC = get_tcp_client_info();
        os_time_dly(5);
    } while (client_infoCCC == NULL);

    tcp_client_info_sang = *client_infoCCC;
    OEMQuitFlag = false;

    printf("tcp_recv_data..\r\n");
    MEP.SocketTaskRunning = true;
    while (1)
    {
        // memset(recv_buf, 0, sizeof(recv_buf));
        len = tcp_recv_data(client_infoCCC->fd, recv_buf, sizeof(recv_buf));
        if (len > 0)
        {
            WiFiPrinterReceiveDataTask(recv_buf, len);
        }
        else
        {
            printf("sang:TCP tcp_recv_data err!!!!!!!!!!");
            if (client_infoCCC != NULL)
                tcp_client_quit(client_infoCCC);
            tcp_client_info_sang.fd = NULL;
            MEP.SocketTaskRunning = false;
            if (true == isSocketClosed(client_infoCCC->fd))
                printf("sang:socketclosed !!!!!!!!!!");
            else
                printf("sang:socket still open !!!!!!!!!!");
            goto _reconnect_;
        }
        //__os_taskq_pend(msg, ARRAY_SIZE(msg), 1);//task pend
    }
}


volatile bool TCP_Socket_Accept_Success = false;

static void Printer_Tcp_Sock_Accpet(void)
{
	printf("sang:Printer_Tcp_Sock_Accpet!!!!!!!!!!");
    socklen_t len = sizeof(Printer_server_info.local_addr);
	MEP.SocketTaskRunning=false;
    while(1)
	{
        struct tcp_client_info *client_info = calloc(1, sizeof(struct tcp_client_info));
        if (client_info == NULL)
		{
            printf(" %s calloc fail\n", __FILE__);
            return;
        }
		while(MEP.SocketTaskRunning)
		{
			 os_time_dly(10);
		}
		printf("sang:sock_accept........................\n");

        client_info->fd  = sock_accept(Printer_server_info.fd, (struct sockaddr *)&client_info->remote_addr, &len, NULL, NULL);
		printf("sang:sock_accept OK----->n");
        if (client_info->fd == NULL)
		{
            printf("sang:%s socket_accept fail\n",  __FILE__);
			TCP_Socket_Accept_Success=false;
            return;
        }
		else
		{
			printf("sang:sock_accept success!!!!!!!!!!!!!!!!!!!!!!!\n");
			TCP_Socket_Accept_Success=true;
		}

        os_mutex_pend(&Printer_server_info.tcp_mutex, 0);
        list_add_tail(&client_info->entry, &Printer_server_info.client_head);
        os_mutex_post(&Printer_server_info.tcp_mutex);

        printf("sang:%s, build connnect success!!!!!!!!!!!!!!!!!!!!!!!\n", inet_ntoa(client_info->remote_addr.sin_addr));

    }
}


static int Printer_Tcp_Server_Init(int port)
{
    u32 opt = 1;
    printf("sang:Printer_Tcp_Server_Init...................");
    memset(&Printer_server_info, 0, sizeof(Printer_server_info));
    Printer_server_info.local_addr.sin_family = AF_INET;
    Printer_server_info.local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    Printer_server_info.local_addr.sin_port = htons(port);

    Printer_server_info.fd = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL);
    if (Printer_server_info.fd == NULL)
    {
        printf("%s build socket fail\n", __FILE__);
        return -1;
    }
    if (sock_setsockopt(Printer_server_info.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        printf("%s sock_setsockopt fail\n", __FILE__);
        return -1;
    }
    if (sock_bind(Printer_server_info.fd, (struct sockaddr *)&Printer_server_info.local_addr, sizeof(struct sockaddr)))
    {
        printf("%s sock_bind fail\n", __FILE__);
        return -1;
    }
    if (sock_listen(Printer_server_info.fd, 0x2) != 0)
    {
        printf("%s sock_listen fail\n", __FILE__);
        return -1;
    }
    if (os_mutex_create(&Printer_server_info.tcp_mutex) != OS_NO_ERR)
    {
        printf("%s os_mutex_create fail\n", __FILE__);
        return -1;
    }

    INIT_LIST_HEAD(&Printer_server_info.client_head);
    if (thread_fork("tcp_sock_accpet", 26, 4096, 0, &accept_pid, Printer_Tcp_Sock_Accpet, NULL) != OS_NO_ERR)
    {
        printf("%s thread fork fail\n", __FILE__);
        return -1;
    }
    printf("Printer_Tcp_Recv_handler\r\n");

    if (thread_fork("tcp_recv_handler", 1, 4096, 0, &recv_pid, Printer_Tcp_Recv_handler, NULL) != OS_NO_ERR)
    {
        printf("%s thread fork fail\n", __FILE__);
        return -1;
    }
    return 0;
}
static void tcp_client_quit(struct tcp_client_info *priv)
{
    if (OEMQuitFlag == false)
    {
        OEMQuitFlag = true;
        printf("sang:tcp_client_quit----------------------------\r\n");
        closesocket(priv->fd);
        list_del(&priv->entry);
        sock_set_quit(priv->fd);
        sock_unreg(priv->fd);
        priv->fd = NULL;
        free(priv);
    }
}
static void tcp_server_exit(void)
{
    struct list_head *pos = NULL;
    struct tcp_client_info *client_info = NULL;
    thread_kill(&accept_pid, KILL_WAIT);
    thread_kill(&recv_pid, KILL_WAIT);
    os_mutex_pend(&Printer_server_info.tcp_mutex, 0);
    list_for_each(pos, &Printer_server_info.client_head)
    {
        client_info = list_entry(pos, struct tcp_client_info, entry);
        if (client_info)
        {
            list_del(&client_info->entry);
            sock_unreg(client_info->fd);
            client_info->fd = NULL;
            free(client_info);
        }
    }
    os_mutex_post(&Printer_server_info.tcp_mutex);
    os_mutex_del(&Printer_server_info.tcp_mutex, OS_DEL_NO_PEND);
}
extern EventGroupHandle_t Printer_event_group;
static void PrinterTcp_Server_Start(void *priv)
{
    int err;
    enum wifi_sta_connect_state state;
    while (1)
    {
        state = wifi_get_sta_connect_state();
        if (WIFI_STA_NETWORK_STACK_DHCP_SUCC == state)
        {
            printf("STA Obtained IP-->");
            struct netif_info netif_infoA;
            lwip_get_netif_info(1, &netif_infoA);
            printf("sang:ipaddr = [%d.%d.%d.%d] \r\n\r\n", netif_infoA.ip & 0xff, (netif_infoA.ip >> 8) & 0xff, (netif_infoA.ip >> 16) & 0xff, (netif_infoA.ip >> 24) & 0xff);
            break;
        }
        os_time_dly(1000);
    }
    err = Printer_Tcp_Server_Init(SERVER_TCP_PORT);
    if (err == -1)
    {
        printf("sang:tcp_server_init faile\n");
        tcp_server_exit();
    }
}

void disableTCPTask(void)
{
    tcp_client_info_sang.fd = NULL;
    printf("os_task_suspend!!!!!!!!!!-.-.-.-.-.-.-.>>>\n");
    if (client_infoCCC != NULL)
        tcp_client_quit(client_infoCCC);
    // tcp_server_exit();

    MEP.SocketTaskRunning = false;
}
/*
unsigned long GetEmptySize(void)
{
    if(MEP.SocketPread<MEP.SocketPwrite)
    {
        return MAXBufferReceiveSize-MEP.SocketPwrite+MEP.SocketPread;
    }
    else
    {
        return MEP.SocketPread-MEP.SocketPwrite;

    }
}
void CheckBufferEmptySize(void)
{
    while(GetEmptySize()<TCPIPRevBusyLevelValue)
    {
        printf("sang:delay P:%d,W:%d...\r\n",MEP.SocketPread,MEP.SocketPwrite);
        os_time_dly(5);
    }
}
void SocketSaveData(uint16_t sizebytes,unsigned char *BufferReceive)
{
    unsigned int i;
    if((MEP.SocketPwrite+sizebytes)<=MAXBufferReceiveSize)
    {
        memcpy(&MEP.SocketRevBuffer[MEP.SocketPwrite],BufferReceive,sizebytes);
        if((MEP.SocketPwrite+sizebytes)==MAXBufferReceiveSize)
        {
            MEP.SocketPwrite=0;
        }
        else
        {
            MEP.SocketPwrite+=sizebytes;
        }
    }
    else
    {
        memcpy(&MEP.SocketRevBuffer[MEP.SocketPwrite],BufferReceive,MAXBufferReceiveSize-MEP.SocketPwrite);
        memcpy(MEP.SocketRevBuffer,&BufferReceive[MAXBufferReceiveSize-MEP.SocketPwrite],MEP.SocketPwrite+sizebytes-MAXBufferReceiveSize);
        MEP.SocketPwrite=MEP.SocketPwrite+sizebytes-MAXBufferReceiveSize;

    }
}
void SocketDataProcesser(void)
{
    while(1)
    {
        if((MEP.SocketPread+1)<MEP.SocketPwrite)
        {
            ++MEP.SocketPread;
            //putchar(MEP.SocketRevBuffer[MEP.SocketPread]);
            Tlen1++;

        }
        else
        {
            if(MEP.SocketPread>MEP.SocketPwrite)
            {
                if(MEP.SocketPread==(MAXBufferReceiveSize-1))
                {
                    if(MEP.SocketPwrite>0)
                    {
                        MEP.SocketPread=0;
                        //putchar(MEP.SocketRevBuffer[MEP.SocketPread]);
                        Tlen1++;
                    }
                    else
                    {
                        return;
                    }
                }
                else
                {
                    ++MEP.SocketPread;
                    //putchar(MEP.SocketRevBuffer[MEP.SocketPread]);
                    Tlen1++;
                }
            }
            else
            {
                return;
            }
        }
    }
}

void WifiPrintTask(void *p)
{
    int res;
    int msg[16];
    int Count=0;
    int Count2=0;

    printf("sang:WifiPrintTask..............\r\n");
    MEP.SocketPwrite=1;
    MEP.SocketPread=0;
    while(1)
    {
        SocketDataProcesser();
        res = __os_taskq_pend(msg, ARRAY_SIZE(msg), 1);
    }
}
*/
void WifiPrinterStart(void)
{
    // WiFi_Data_MuteCreate();
    if (thread_fork("PrinterTcp_Server_Start", 25, 512, 0, NULL, PrinterTcp_Server_Start, NULL) != OS_NO_ERR)
    {
        printf("thread fork fail\n");
    }
    // task_create(WifiPrintTask, NULL, "WifiPrintTask");
}

late_initcall(WifiPrinterStart);

#if 0
//--------------------------------------------------------TCP client for cloud print--------------------------------

#define CLIENT_TCP_PORT 0             // �ͻ��˶˿ں�
static void *sock = NULL;
#define SERVER_TCP_IP "192.168.1.101" // �����ip �����Ƶ�ַ
#define SERVER_TCP_PORT 9100          // ����˶˿ں�


static int tcp_client_init(const char *server_ip, const int server_port)
{
    //struct sockaddr_in local;
    struct sockaddr_in dest;

    //����socket
    sock = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL);
    if (sock == NULL) {
        printf("sock_reg fail.\n");
        return -1;
    }

    //�󶨱��ص�ַ��Ϣ
//    local.sin_addr.s_addr = INADDR_ANY;
//    local.sin_port = htons(CLIENT_TCP_PORT);
//    local.sin_family = AF_INET;
//    if (0 != sock_bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr_in))) {
//        sock_unreg(sock);
//        printf("sock_bind fail.\n");
//        return -1;
//    }

    //����ָ����ַ�����
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(server_ip);
    dest.sin_port = htons(server_port);
    if (0 != sock_connect(sock, (struct sockaddr *)&dest, sizeof(struct sockaddr_in))) {
        printf("sock_connect fail.\n");
        sock_unreg(sock);
        return -1;
    }

    return 0;
}

//��������
static int tcp_send_data_Client(const void *sock_hdl, const void *buf, const u32 len)
{
    return sock_send(sock_hdl, buf, len, 0);
}

//��������
static int tcp_recv_data_Client(const void *sock_hdl, void *buf, u32 len)
{
    return sock_recvfrom(sock_hdl, buf, len, 0, NULL, NULL);
}

//tcp client����
static void tcp_client_task(void *priv)
{
    int  err;
    char *payload = "Please send me some data!";
    char recv_buf[1024];
    int  recv_len;
    int  send_len;

    err = tcp_client_init(SERVER_TCP_IP, SERVER_TCP_PORT);
    if (err) {
        printf("tcp_client_init err!");
        return;
    }

    send_len = tcp_send_data_Client(sock, payload, strlen(payload));
    if (send_len == -1) {
        printf("sock_sendto err!");
        sock_unreg(sock);
        return;
    }

    for (;;) {
        recv_len = tcp_recv_data_Client(sock, recv_buf, sizeof(recv_buf));
        if ((recv_len != -1) && (recv_len != 0)) {
            recv_buf[recv_len] = '\0';
            printf("Received %d bytes, data: %s\n\r", recv_len, recv_buf);
            tcp_send_data(sock, "Data received successfully!", strlen("Data received successfully!"));
        } else {
            printf("sock_recvfrom err!");
            break;
        }
    }
    if (sock) {
        sock_unreg(sock);
        sock = NULL;
    }

}

static void tcp_client_start(void *priv)
{
    enum wifi_sta_connect_state state;

    while (1)
	{
        printf("Connecting to the network...\n");
        state = wifi_get_sta_connect_state() ;
        if (WIFI_STA_NETWORK_STACK_DHCP_SUCC == state)
		{
            printf("Network connection is successful!\n");
            break;
        }
        os_time_dly(1000);
    }

    if (thread_fork("tcp_client_task", 10, 1024, 0, NULL, tcp_client_task, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

//Ӧ�ó������,��Ҫ������STAģʽ��
void TCP_Client_Start(void)
{
    if (thread_fork("tcp_client_start", 10, 512, 0, NULL, tcp_client_start, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

//late_initcall(TCP_Client_Start);
#endif
