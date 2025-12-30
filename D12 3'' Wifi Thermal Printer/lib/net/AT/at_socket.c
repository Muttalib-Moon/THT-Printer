#include "sock_api/sock_api.h"
#include "os/os_api.h"
#include "system/includes.h"
#include "wifi/wifi_connect.h"
#include "lwip.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "at_socket.h"
#include "wifi_app_task_Print.h"
#include "myDriver.h"
#include "isr.h"
#include "UserInterface.h"
#include "hid.h"
#include "Link.h"
#include "UsbPrinter.h"


extern Device_Rev *InputCOM_device;;
extern Device_Rev *USB_RevC;


/********************** TCP_SERVER_BEGIN *********************/

#define MAX_RECV_BUF_SIZE   50 //单次能接收数据的�?大字节数(Bytes)

static int accept_pid = -1;    //tcp_sock_accpet线程的句�?
static int recv_pid_1 = -1;      //tcp_recv_accpet线程的句�?

//tcp_client数据结构
struct tcp_client_info {
    struct list_head entry;         //链表节点
    struct sockaddr_in remote_addr; //tcp_client地址信息
    void *fd;                       //建立连接后的套接�?
};

//tcp_server数据结构
struct tcp_server_info {
    struct list_head client_head;  //链表
    struct sockaddr_in local_addr; //tcp_server地址信息
    void *fd;                      //tcp_server套接�?
    OS_MUTEX tcp_mutex;            //互斥�?
};

static struct tcp_server_info server_info; ////////////

/*********************************************/

static void tcp_sock_accpet(void);
static void tcp_recv_handler(void);

//extern void Printer_Tcp_Recv_handler(void);
//extern void Printer_Tcp_Sock_Accpet(void);

/*********************************************/
static struct tcp_client_info *get_tcp_client_info(void);
static void tcp_client_quit(struct tcp_client_info *priv);


//�?  能：向tcp_client发�?�消�?
//�?  数：void *sock_hdl ：socket句柄
//        const void *buf：需要发送的数据BUF
//        u32 len        : BUF大小
//返回值：返回发�?�成功的数据的字节数；返�?-1，发送失�?
static int tcp_send_data(void *sock_hdl, const void *buf, u32 len)
{
    return sock_send(sock_hdl, buf, len, 0);
}


//�?  能：接收tcp_client的数�?
//�?  数：void *sock_hdl ：socket句柄
//        void *buf      ：需要接收数据BUF
//        u32 len        : BUF大小
//返回值：返回接收成功的数据的字节数；返回-1，发送失�?
static int tcp_recv_data(void *sock_hdl, void *buf, u32 len)
{
    return sock_recv(sock_hdl, buf, len, 0);
}


//�?  能：tcp_server接收线程，用于接收tcp_client的数�?
//�?  �?: �?
//返回值：�?
static void tcp_recv_handler(void)
{
    struct tcp_client_info *client_info = NULL;
    char recv_buf[MAX_RECV_BUF_SIZE] = {0};

_reconnect_:

    do 
	{
        client_info = get_tcp_client_info();
        os_time_dly(5);
    } while (client_info == NULL);

    for (;;) {
        if (tcp_recv_data(client_info->fd, recv_buf, sizeof(recv_buf)) > 0) {
            printf("Received data from (ip : %s, port : %d)\r\n", inet_ntoa(client_info->remote_addr.sin_addr), client_info->remote_addr.sin_port);
            printf("recv_buf = %s.\n", recv_buf);
            memset(recv_buf, 0, sizeof(recv_buf));
            //此处可添加数据处理函�?
            tcp_send_data(client_info->fd, "Data received successfully!", strlen("Data received successfully!"));
        }
		else {
            tcp_client_quit(client_info);
            goto _reconnect_;
        }
    }
}

//�?  能：tcp_sock_accpet线程，用于接收tcp_client的连接请�?
//�?  �?: void *priv：NULL
//返回值：�?
static void tcp_sock_accpet(void)
{
    socklen_t len = sizeof(server_info.local_addr);

    for (;;) {
        struct tcp_client_info *client_info = calloc(1, sizeof(struct tcp_client_info));
        if (client_info == NULL) {
            printf(" %s calloc fail\n", __FILE__);
            return;
        }

        client_info->fd  = sock_accept(server_info.fd, (struct sockaddr *)&client_info->remote_addr, &len, NULL, NULL);
        if (client_info->fd == NULL) {
            printf("%s socket_accept fail\n",  __FILE__);
            return;
        }

        os_mutex_pend(&server_info.tcp_mutex, 0);
        list_add_tail(&client_info->entry, &server_info.client_head);
        os_mutex_post(&server_info.tcp_mutex);

        printf("%s, build connnect success.\n", inet_ntoa(client_info->remote_addr.sin_addr));
    }
}

//�?  能：搭建tcp_server
//�?  �?: int port：端口号
//返回值：返回0，成功；返回-1，失�?
static int tcp_server_init(int port)
{
    u32 opt = 1;

    memset(&server_info, 0, sizeof(server_info));

    server_info.local_addr.sin_family = AF_INET;
    server_info.local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_info.local_addr.sin_port = htons(port);

    server_info.fd = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL);
    if (server_info.fd == NULL) {
        printf("%s build socket fail\n",  __FILE__);
        return -1;
    }

    if (sock_setsockopt(server_info.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("%s sock_setsockopt fail\n", __FILE__);
        return -1;
    }

    if (sock_bind(server_info.fd, (struct sockaddr *)&server_info.local_addr, sizeof(struct sockaddr))) {
        printf("%s sock_bind fail\n", __FILE__);
        return -1;
    }

    if (sock_listen(server_info.fd, 0x2) != 0) {
        printf("%s sock_listen fail\n", __FILE__);
        return -1;
    }

    if (os_mutex_create(&server_info.tcp_mutex) != OS_NO_ERR) {
        printf("%s os_mutex_create fail\n", __FILE__);
        return -1;
    }

    INIT_LIST_HEAD(&server_info.client_head);

    //创建线程，用于接收tcp_client的连接请�?
    if (thread_fork("tcp_sock_accpet", 26, 256, 0, &accept_pid, tcp_sock_accpet, NULL) != OS_NO_ERR) { //Printer_Tcp_Sock_Accpet
        printf("%s thread fork fail\n", __FILE__);
        return -1;
    }

    //创建线程，用于接收tcp_client的数�?
    if (thread_fork("tcp_recv_handler553", 25, 512, 0, &recv_pid_1, tcp_recv_handler, NULL) != OS_NO_ERR) { //Printer_Tcp_Recv_handler
        printf("%s thread fork fail\n", __FILE__);
        return -1;
    }

    return 0;
}

//�?  能：注销tcp_client
//�?  �?: struct tcp_client_info *priv：需要注�?的tcp_client
//返回值：�?
static void tcp_client_quit(struct tcp_client_info *priv)
{
    list_del(&priv->entry);
    sock_set_quit(priv->fd);
    sock_unreg(priv->fd);
    priv->fd = NULL;
    free(priv);
}

//�?  能：获取当前�?新的tcp_client
//�?  �?: �?
//返回值：返回获取的tcp_client
static struct tcp_client_info *get_tcp_client_info(void)
{
    struct list_head *pos = NULL;
    struct tcp_client_info *client_info = NULL;
    struct tcp_client_info *old_client_info = NULL;


    os_mutex_pend(&server_info.tcp_mutex, 0);

    list_for_each(pos, &server_info.client_head) {
        client_info = list_entry(pos, struct tcp_client_info, entry);
    }

    os_mutex_post(&server_info.tcp_mutex);

    return client_info;
}

//�?  能：注销tcp_server
//�?  �?: �?
//返回值：�?
static void tcp_server_exit(void)
{
    struct list_head *pos = NULL;
    struct tcp_client_info *client_info = NULL;

    thread_kill(&accept_pid, KILL_WAIT);
    thread_kill(&recv_pid_1, KILL_WAIT);

    os_mutex_pend(&server_info.tcp_mutex, 0);

    list_for_each(pos, &server_info.client_head) {
        client_info = list_entry(pos, struct tcp_client_info, entry);
        if (client_info) 
		{
            list_del(&client_info->entry);
            sock_unreg(client_info->fd);
            client_info->fd = NULL;
            free(client_info);
        }
    }

    os_mutex_post(&server_info.tcp_mutex);

    os_mutex_del(&server_info.tcp_mutex, OS_DEL_NO_PEND);
}

static void tcp_server_start(void *priv)
{
    int err;
    enum wifi_sta_connect_state state;

    while (1) {
        printf("Connecting to the network...\n");
        state = wifi_get_sta_connect_state();
        if (WIFI_STA_NETWORK_STACK_DHCP_SUCC == state) {
            printf("Network connection is successful!\n");
            break;
        }

        os_time_dly(1000);
    }

    err = tcp_server_init(60000);
    if (err == -1) {
        printf("tcp_server_init faile\n");
        tcp_server_exit();
    }
}

//应用程序入口,�?要运行在STA模式�?
void tcp_server_run(void)
{
    if (thread_fork("tcp_server_start", 10, 512, 0, NULL, tcp_server_start, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

/********************** TCP_SERVER_END *********************/

/********************** TCP_CLIENT_BEGIN *********************/

#define CLIENT_TCP_PORT 0      			//客户端端口号
static void *sock = NULL;
#define SERVER_TCP_IP "192.168.31.156"   //服务端ip
#define SERVER_TCP_PORT 9100                 //服务端端口号

static int tcp_client_init(const char *server_ip, const int server_port)
{
    //struct sockaddr_in local;
    struct sockaddr_in dest;

    //创建socket
    sock = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL);
    if (sock == NULL) {
        printf("sock_reg fail.\n");
        return -1;
    }

    //绑定本地地址信息
//    local.sin_addr.s_addr = INADDR_ANY;
//    local.sin_port = htons(CLIENT_TCP_PORT);
//    local.sin_family = AF_INET;
//    if (0 != sock_bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr_in))) {
//        sock_unreg(sock);
//        printf("sock_bind fail.\n");
//        return -1;
//    }

    //连接指定地址服务�?
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

//接收数据
static int tcp_client_recv_data(const void *sock_hdl, void *buf, u32 len)
{
    return sock_recvfrom(sock_hdl, buf, len, 0, NULL, NULL);
}

//tcp client任务
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

    send_len = tcp_send_data(sock, payload, strlen(payload));
    if (send_len == -1) {
        printf("sock_sendto err!");
        sock_unreg(sock);
        return;
    }

    for (;;) {
        recv_len = tcp_client_recv_data(sock, recv_buf, sizeof(recv_buf));
        if ((recv_len != -1) && (recv_len != 0)) {
            recv_buf[recv_len] = '\0';
            printf("Received %d bytes, data: %s\n\r", recv_len, recv_buf);
            tcp_send_data(sock, "Data received successfully!", strlen("Data received successfully!"));
        } 
		else {
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

    while (1) {
        printf("Connecting to the network...\n");
        state = wifi_get_sta_connect_state() ;
        if (WIFI_STA_NETWORK_STACK_DHCP_SUCC == state) {
            printf("Network connection is successful!\n");
            break;
        }
        os_time_dly(1000);
    }

    if (thread_fork("tcp_client_task", 10, 1024, 0, NULL, tcp_client_task, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

//应用程序入口,�?要运行在STA模式�?
void tcp_client_run(void)
{

    if (thread_fork("tcp_client_start", 10, 512, 0, NULL, tcp_client_start, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

/********************** TCP_CLIENT_END *********************/

/********************** UDP_SERVER_BEGIN *********************/

static void *socket_fd = 0;
static int recv_pid = 0;

//�?  能：udp_server接收线程，用于接收udp_client的数�?
//�?  �?: �?
//返回值：�?
static void udp_recv_handler(void)
{
    struct sockaddr_in remote_addr = {0};
    socklen_t len = sizeof(remote_addr);
    int recv_len = 0;
    u8 recv_buf[50] = {0};

    for (;;) 
	{

        //接收udp_client的数据，并获取udp_client的地�?信息
        recv_len = sock_recvfrom(socket_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&remote_addr, &len);
        printf("\n recv_len is %d  \n", recv_len);

        //将接收到的数据发送至udp_client
        sock_sendto(socket_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr_in));

        memset(recv_buf, 0, sizeof(recv_buf));
    }
}

//�?  能：搭建udp_server
//�?  �?: int port：端口号
//返回值：返回0，成功；返回-1，失�?
static int udp_server_init(int port)
{
    struct sockaddr_in local_addr = {0};

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(port);

    socket_fd = sock_reg(AF_INET, SOCK_DGRAM, 0, NULL, NULL);
    if (socket_fd == NULL) {
        printf("%s build socket fail\n",  __FILE__);
        return -1;
    }

    if (sock_bind(socket_fd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr))) {
        printf("%s sock_bind fail\n", __FILE__);
        return -1;
    }

    //创建线程，用于接收tcp_client的数�?
    if (thread_fork("udp_recv_handler", 25, 512, 0, &recv_pid, udp_recv_handler, NULL) != OS_NO_ERR) {
        printf("%s thread fork fail\n", __FILE__);
        return -1;
    }

    return 0;
}

//�?  能：注销udp_server
//�?  �?: �?
//返回值：�?
static void udp_server_exit(void)
{
    thread_kill(&recv_pid, KILL_WAIT);
    sock_unreg(socket_fd);
}

static void udp_server_start(void *priv)
{
    int err;
    enum wifi_sta_connect_state state;

    while (1) {
        state = wifi_get_sta_connect_state();
        printf("Connecting to the network...\n");
        if (WIFI_STA_NETWORK_STACK_DHCP_SUCC == state) {
            printf("Network connection is successful!\n");
            break;
        }

        os_time_dly(1000);
    }

    err = udp_server_init(60000);
    if (err == -1) {
        printf("udp_server_init faile\n");
        udp_server_exit();
    }
}

//应用程序入口,�?要运行在STA模式�?
void udp_server_run(void)
{
    if (thread_fork("udp_server_start", 10, 512, 0, NULL, udp_server_start, NULL) != OS_NO_ERR) {
        printf("thread fork fail\n");
    }
}

/********************** UDP_SERVER_END *********************/

