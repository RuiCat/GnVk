#ifndef _GN_NET_H_
#define _GN_NET_H_

/* 请在编译环境里的预处理定义添加以下其中一个宏来确定代码的应用模式 */
//#define GN_NET_USING_SDK                       //SDK方式使用
//#define GN_NET_USING_LIB_EXPORTS               //动态库导出
//#define GN_NET_USING_SOURCE_CODE               //源代码编辑

#ifdef GN_NET_USING_SOURCE_CODE //源代码编辑
#define GN_NET_API
#define GN_NET_CALL
#elif defined GN_NET_USING_LIB_EXPORTS //动态库导出
#if defined _WIN32_ || defined _WIN32_WCE
#define GN_NET_API __declspec(dllexport)
#define GN_NET_CALL __declspec(dllimport)
#else
#define GN_NET_API extern
#define GN_NET_CALL extern
#endif
#else //------------------------------   //SDK 方式使用
#if defined _WIN32_ || defined _WIN32_WCE
#define GN_NET_API __declspec(dllimport)
#define GN_NET_CALL __declspec(dllexport)
#else
#define GN_NET_API
#define GN_NET_CALL
#endif
#endif
/*--------------------------------------------------------------------------------------------------------------------------------*/
#ifdef NET_USING_INDEPENDENT
#if !defined(GN_NET_USING_LIB_EXPORTS) && !defined(GN_NET_USING_SOURCE_CODE)
#pragma comment(lib, "Net_win32.lib")
#endif
#else
#if !defined(GN_NET_USING_SOURCE_CODE) && !defined(GN_USING_SDK_INTEGRATION) && !defined(GN_NET_USING_LIB_EXPORTS)
#pragma comment(lib, "GN_Net_win32.lib")
#endif
#endif
#if defined(_WIN32_) || defined(_WIN32_WCE)
#ifdef _WIN32_
#ifndef __LP64__
#include <WinSock2.h>
#endif
#elif defined _WIN32_WCE
#include <winsock2.h>
#endif
#include <Iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#ifndef _LINUX_
#include <net/if_dl.h>
#endif
//引擎所使用到标准socket库的一些内容 如果相关平台没有这些socket库内容则在这里有它的相关处理实现以确保应用程序总能顺利调用到这些内容

typedef int SOCKET;
#define closesocket close
#define ioctlsocket ioctl
#endif

#if defined _MAC_ || defined GT_USING_IOS || (!defined(_WIN32_) && defined(__LP64__))
typedef unsigned int addr_len_t;
#else
typedef int addr_len_t;
#endif

//常用socket的东西
#ifndef AF_INET
#define REDEF_AF_INET
#define AF_INET 2 /* internetwork: UDP, TCP, etc. */

#define SOCK_STREAM 1 /* stream socket */
#define SOCK_DGRAM 2 /* datagram socket */
#define SOCK_RAW 3 /* raw-protocol interface */
#define SOCK_RDM 4 /* reliably-delivered message */
#define SOCK_SEQPACKET 5 /* sequenced packet stream */
#define INADDR_NONE 0xffffffff
#define INADDR_LOOPBACK 0x7f000001
#define INADDR_ANY (u_long)0x00000000
#define INADDR_BROADCAST (u_long)0xffffffff

typedef int SOCKET;
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned int u_long;

#ifndef s_addr //--------------------------------------------------------------
struct GN_NET_API in_addr {
    union {
        struct
        {
            u_char s_b1, s_b2, s_b3, s_b4;
        } S_un_b;
        struct
        {
            u_short s_w1, s_w2;
        } S_un_w;
        u_long S_addr;
    } S_un;
};
#define s_addr S_un.S_addr /* can be used for most tcp & ip code */
#define s_host S_un.S_un_b.s_b2 /* host on imp */
#define s_net S_un.S_un_b.s_b1 /* network */
#define s_imp S_un.S_un_w.s_w2 /* imp */
#define s_impno S_un.S_un_b.s_b4 /* imp # */
#define s_lh S_un.S_un_b.s_b3 /* logical host */
#endif
//-----------------------------------------------------------------------------

GN_NET_API char *inet_ntoa(const struct in_addr in);
GN_NET_API u_short htons(u_short hostshort);
GN_NET_API u_long htonl(u_long hostlong);
GN_NET_API u_short ntohs(u_short netshort);
GN_NET_API u_long inet_addr(const char *cp);

struct GN_NET_API sockaddr {
    u_short sa_family; /* address family */
    char sa_data[14]; /* up to 14 bytes of direct address */
};
struct GN_NET_API sockaddr_in {
    short sin_family;
    u_short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};
GN_NET_API int socket(int domain, int type, int protocol);
GN_NET_API int closesocket(int s);
GN_NET_API int getsockname(int s, struct sockaddr *name, int *namelen);
GN_NET_API int getpeername(int s, struct sockaddr *name, int *namelen);
GN_NET_API int bind(int s, const struct sockaddr *name, int namelen);
GN_NET_API int connect(int s, const struct sockaddr *name, int namelen);
GN_NET_API int accept(int s, struct sockaddr *addr, int *addrlen);
GN_NET_API int listen(int s, int backlog);
GN_NET_API int recv(int s, char *buf, int len, int flags);
GN_NET_API int recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, int *fromlen);
GN_NET_API int send(int s, const char *buf, int len, int flags);
GN_NET_API int sendto(int s, const void *msg, int len, int flags, const struct sockaddr *to, int tolen);
#endif
// 主机名处理的一些东西 ------------------------------------------------------------------------------------------------
#ifndef h_addr
#define REDEF_h_addr

struct GN_NET_API hostent {
    char FAR *h_name; /* official name of host */
    char FAR * FAR * h_aliases; /* alias list */
    short h_addrtype; /* host address type */
    short h_length; /* length of address */
    char FAR * FAR * h_addr_list; /* list of addresses */
};
#define h_addr h_addr_list[0] /* address, for backward compat */

GN_NET_API hostent *gethostbyaddr(const char FAR *addr, int len, int type);
GN_NET_API hostent *gethostbyname(const char FAR *name);
#endif
// 控制socket行为的定义 ----------------------------------------------------------------------------------
#ifndef FIONBIO
#define REDEF_FIONBIO
#define IOCPARM_MASK 0x7f /* parameters must be < 128 bytes */
#define IOC_VOID 0x20000000 /* no parameters */
#define IOC_OUT 0x40000000 /* copy out parameters */
#define IOC_IN 0x80000000 /* copy in parameters */
#define IOC_INOUT (IOC_IN | IOC_OUT) /* 0x20000000 distinguishes new & old ioctl's */
#define _IO(x, y) (IOC_VOID | ((x) << 8) | (y))
#define _IOR(x, y, t) (IOC_OUT | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#define _IOW(x, y, t) (IOC_IN | (((long)sizeof(t) & IOCPARM_MASK) << 16) | ((x) << 8) | (y))
#define FIONREAD _IOR('f', 127, u_long) /* get # bytes to read */
#define FIONBIO _IOW('f', 126, u_long) /* set/clear non-blocking i/o */
#define FIOASYNC _IOW('f', 125, u_long) /* set/clear async i/o */

GN_NET_API int ioctlsocket(SOCKET s, long cmd, u_long *argp);
#endif

// socket 中断处理的定义 ---------------------------------------------------------------------------------
#ifndef FD_ISSET
#define REDEF_FD_ISSET
#define FD_SETSIZE 64

struct GN_NET_API fd_set {
    u_int fd_count; /* how many are SET? */
    SOCKET fd_array[FD_SETSIZE]; /* an array of SOCKETs */
};
GN_NET_API int __WSAFDIsSet(SOCKET, fd_set *);

#define FD_ISSET(fd, set) __WSAFDIsSet((SOCKET)(fd), (fd_set FAR *)(set))

#define FD_SET(fd, set)                                              \
    do {                                                             \
        u_int __i;                                                   \
        for(__i = 0; __i < ((fd_set FAR *)(set))->fd_count; __i++) { \
            if(((fd_set FAR *)(set))->fd_array[__i] == (fd)) {       \
                break;                                               \
            }                                                        \
        }                                                            \
        if(__i == ((fd_set FAR *)(set))->fd_count) {                 \
            if(((fd_set FAR *)(set))->fd_count < FD_SETSIZE) {       \
                ((fd_set FAR *)(set))->fd_array[__i] = (fd);         \
                ((fd_set FAR *)(set))->fd_count++;                   \
            }                                                        \
        }                                                            \
    } while(0)

#define FD_CLR(fd, set)                                              \
    do {                                                             \
        u_int __i;                                                   \
        for(__i = 0; __i < ((fd_set FAR *)(set))->fd_count; __i++) { \
            if(((fd_set FAR *)(set))->fd_array[__i] == fd) {         \
                while(__i < ((fd_set FAR *)(set))->fd_count - 1) {   \
                    ((fd_set FAR *)(set))->fd_array[__i] =           \
                        ((fd_set FAR *)(set))->fd_array[__i + 1];    \
                    __i++;                                           \
                }                                                    \
                ((fd_set FAR *)(set))->fd_count--;                   \
                break;                                               \
            }                                                        \
        }                                                            \
    } while(0)

#define FD_ZERO(set) (((fd_set FAR *)(set))->fd_count = 0)

struct GN_NET_API timeval {
    long tv_sec; /* seconds */
    long tv_usec; /* and microseconds */
};
GN_NET_API int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout);
#endif

/*------------------------------------------- Net模块必须实现的功能 --------------------------------------------------*/
namespace GNn {

//获取本机MAC地址(地址输出 8张网卡 地址占字节6) 返回可使用的网卡数
GN_NET_API int GetLocalMac(unsigned char mac[8][6]);

}; //end namespace GNn

//获得网络模块信息 如: "Winsock 2" , "Socket" , ...等
GN_NET_API char *const GN_GetInterfaceInfoNetwork();

GN_NET_API bool GT_InitNetwork(); //初始化网络相关的内容()返回是否成功
GN_NET_API void GT_UnInitNetwork(); //释放网络相关的内容

#endif //_GN_NET_H_