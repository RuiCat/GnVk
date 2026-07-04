/****************************************************************************
Copyright (c) 2018-2025 gncreate Technologies Inc.

 http://www.gncreate.com
 http://www.img-power.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifdef NET_USING_INDEPENDENT
#include "GN_NetBase.h"
#else
#include <GN.h>
#endif
#ifdef _GN_NET_H_
/* 引擎所使用到标准Socket库的一些内容 * 如果相关平台没有这些Socket库内容则在这里有它的相关处理实现以确保应用程序总能顺利调用到这些内容*********/
//常用Socket的东西 
#ifdef REDEF_AF_INET
GN_NET_API int       socket(int domain, int type, int protocol){ return -1; }
GN_NET_API int       closesocket(int s){ return -1; }
GN_NET_API int       bind(int s, const struct sockaddr *name, int namelen){ return -1; }
GN_NET_API int       connect(int  s,  const  struct sockaddr   *name,   int  namelen){ return -1; }
GN_NET_API int       accept(int  s,   struct   sockaddr   *addr,   int *addrlen){ return -1; }
GN_NET_API int       listen(int s, int backlog){ return -1; }
GN_NET_API int       recv(int s, char * buf, int len, int flags){ return -1; }
GN_NET_API int       send(int s, const char  * buf, int len, int flags){ return -1; }
GN_NET_API int       recvfrom(int s, void *buf, size_t len,  int flags, struct sockaddr *from, int *fromlen){ return -1; }
GN_NET_API int       sendto(int s, const void  *msg,  int len, int flags, const struct sockaddr *to, int tolen){ return -1; }
GN_NET_API int       getsockname(int  s,  struct  sockaddr  *name,  int *namelen){ return -1; }
GN_NET_API int       getpeername(int  s,  struct  sockaddr  *name,  int *namelen){ return -1; }

GN_NET_API char     *inet_ntoa(const struct in_addr in){ return 0; }
GN_NET_API u_short   htons(u_short hostshort){ return 0; }
GN_NET_API u_long    htonl(u_long  hostlong){ return 0; }
GN_NET_API u_short   ntohs(u_short netshort ){ return 0; }
GN_NET_API u_long    inet_addr(const char *cp){ return 0; }
#endif

//- 控制Socket行为的东西------------------------------------------------------------------------------------------------
#ifdef REDEF_FIONBIO 
GN_NET_API int       ioctlsocket(SOCKET s, long cmd, u_long * argp ){ return -1; }
#endif

//- Socket 中断处理的东西----------------------------------------------------------------------------------------------------------
#ifdef REDEF_FD_ISSET 
GN_NET_API int  __WSAFDIsSet(SOCKET, fd_set *){ return 0; }
GN_NET_API int  select(int nfds, fd_set * readfds, fd_set * writefds, fd_set *exceptfds, const struct timeval * timeout){ return -1; }
#endif

//-主机名处理的一些东西 ---------------------------------------------------------------------------------------------------------------
#ifdef REDEF_h_addr 
GN_NET_API hostent  *gethostbyaddr(const char FAR * addr, int len, int type ){ return 0; }
GN_NET_API hostent  *gethostbyname(const char FAR * name){ return 0; }
#endif
/************************************************** Net模块必须实现的东西 ************************************************************/
GN_NET_API bool GT_InitNetwork(){
   #if defined( _WIN32_) || defined(_WIN32_WCE)
    WSADATA wsaData; 
	if(WSAStartup(MAKEWORD(2,2), &wsaData) < 0){
	    return false;
	}
   #endif
	return true;
}
GN_NET_API void GT_UnInitNetwork(){
   #if defined( _WIN32_) || defined(_WIN32_WCE)
	WSACleanup();
   #endif 
}
GN_NET_API char *const GN_GetInterfaceInfoNetwork(){
   #if defined( _WIN32_) || defined(_WIN32_WCE)
    static const char netInfo[] = "Winsock 2";
   #else
    static const char netInfo[] = "Socket";
   #endif
    return (char *const)netInfo;
}
GN_NET_API int GNn::GetLocalMac(unsigned char mac[8][6]){
	/*模板代码:GT_GET_LOCAL_MAC*/
   #if defined( _WIN32_) || defined(_WIN32_WCE)
    unsigned long  ulSize=0;
    PIP_ADAPTER_INFO pInfo = 0, _info = 0;
   
	GetAdaptersInfo(pInfo,&ulSize);//第一次调用,获取缓冲区大小
    if(!ulSize)return 0;

	pInfo = (PIP_ADAPTER_INFO)malloc(ulSize);
	_info = pInfo;
	GetAdaptersInfo(pInfo,&ulSize);

	memset(mac, 0 , 8 * 6);
    int iCount=0;
    while(pInfo)//遍历每一张网卡
    {
		if(iCount >= 8){ GN_Log("GetLocalMac 网卡多于8张?\n"); break; }
		if(pInfo->AddressLength > 6){ GN_Log("GetLocalMac MAC地址长度超过6字节?\n"); }
		else memcpy(&mac[iCount][0], pInfo->Address, pInfo->AddressLength);
		iCount++;
        pInfo = pInfo->Next;
    }
    if(_info)free(_info);
	return iCount;
    
   #elif defined(_LINUX_)
    struct ifreq ifr;
    strcpy(ifr.ifr_name, "eth0");
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1 || ioctl(sock, SIOCGIFHWADDR, &ifr) == -1){
        close(sock);
        return 0;
    }
    memcpy(mac[0], ifr.ifr_hwaddr.sa_data, 6);
    close(sock);
    return 1;
    
   #else
    int mi = 0;
    ifaddrs* iflist;
    if (getifaddrs(&iflist) == 0) {
        for (ifaddrs* cur = iflist; cur; cur = cur->ifa_next) {
            if (cur->ifa_addr && (cur->ifa_addr->sa_family == AF_LINK)) {
                sockaddr_dl* sdl = (sockaddr_dl*)cur->ifa_addr;
                if(sdl->sdl_alen == 6){
                    memcpy(mac[mi], LLADDR(sdl), 6);
                    ++mi;
                }
            }
        }
        freeifaddrs(iflist);
    }
    return mi;
   #endif
}
#endif //_GN_NET_H_
