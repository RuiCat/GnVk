#ifndef NET_USING_INDEPENDENT
#define NET_API GN_API
#else
#include <GN.h>
#include "_Net/GN_Net.h"
#define NET_API GN_NET_API
#endif
#ifndef _GN_NETBASE_H_
#define _GN_NETBASE_H_

// GN_NetBase.h — 网络基础设施：URL/IP 字符串解析与环境句柄。
//
// 只有纯字符串与地址处理，不含套接字实现（TCP/UDP/HTTP 在 _Net/ 下）。
// DA-M2 批 4 起，套接字地址长度不再用 ad-hoc extern，统一走 sNetAddrContext 的访问器
// （定义在 GN_NetBase.cpp），这样跨 TU 不再依赖全局符号顺序。

extern NET_API const char GNn_DefWebFileName[]; //默认Web的首页文件名

NET_API bool GNn_FuntionIsOK(); //检测网络功能是否准备好了

//检验字符串是否IP格式(字符输入, IP分量值输出) 如果是就返回32位整型IP 否就返回0
NET_API unsigned int GNn_CheckIsIP(const char *ip_str, unsigned char *ip4_out = 0 /*[4]*/);

//查找指定字符串里所包含的网址范围段的首尾所在字符ID(字符串, 范围段数据输出 里面有多个网址就有多个范围段)
NET_API void GNn_FindNetAddresRangeSegment(const char *p_str, GN::sLIST<GT_sSEGMENT_S> &rng_seg_out);

//分析URL找出主机名等一些信息(URL连接, 返回资源全名, 返回斋文件名, 返回端口) 返回主机名 如返回空指针表示URL格式有误
NET_API P<char> GNn_UrlAnalysis(const char *url, char **src_out, char **filename_out, unsigned short &port_out);

//URL连接补全文件名(返回补全后的新连接, 须校验补救的连接) 功能例如将 www.myhost.com/src 变为 http://www.myhost.com/src/index.htm
NET_API void GNn_UrlRemedy(char *url_out /*[384]*/, const char *url);

extern NET_API P<char> GNn_ErrorLast; //最近一次的网络错误信息记录 在运行中途易发生 但不会导致运行中断的小错误

// DA-M2 批 4：套接字地址长度由 ad-hoc extern 收敛进上下文，跨 TU 改用访问器（定义在 GN_NetBase.cpp）。
struct sNetAddrContext {
    int aAddrLen = 0;
    int aAddr0Len = 0;
};
sNetAddrContext &GN_NetAddrContext();

#endif //_GN_NETBASE_H_
#if defined(NET_USING_INDEPENDENT) && defined(GN_USING_UTILITY_LIB)
#include "GN_NetTS.h"
#endif