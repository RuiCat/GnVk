#ifndef _GN_NETTRANS_H_
#define _GN_NETTRANS_H_
#define GN_MAX_BUF_HTTP_HEAD 2048 //HTTP报头最大缓存大小
#define GN_MAX_BUF_TCP 67000000 //TCP 1次最大传输量
#define GN_MAX_BUF_UDP 540 //UDP 1次最大传输量

//IP格式转换 由字符格式转为网络16进制格式
inline unsigned int GNn_IP_AtoN(const char *ip) {
    return inet_addr(ip);
}
//IP格式转换 由网络16进制格式转为字符格式
inline char const *GNn_IP_NtoA(unsigned int ip) {
    return inet_ntoa(*((in_addr *)&ip));
}

namespace GNn {

struct NET_API sADDR : public sockaddr_in //★地址 结构
{
    inline sADDR() {}

    //默认构造函数(       端口,         地址:0表示本机地址, 地址是否IP地址格式 否则当域名处理)
    sADDR(unsigned short port, const char *addr, bool is_ip = false);
    //初始化	  (       端口,         地址:0表示本机地址, 地址是否IP地址格式 否则当域名处理)
    void Init(unsigned short port, const char *addr, bool is_ip = false);

    inline unsigned short GetPort() { return ntohs(sin_port); } //获得端口
    inline char *GetIP() { return inet_ntoa(sin_addr); } //获得IP地址
};
class NET_API cSOC_TCP //★TCP协议的类  传统socket的一个简单封装
{
    /*-------------------------------------------- 关于TCP的一些传输细节 --------------------------------------------------------------\
	|	一次性可以发送比较大的数据 在网络连接正常的情况下都能确保到达目的                                                              |
	|	一个大的文件可以分多次发送 它对会自动对接上次的数据 接收端对于这可以一次性接收比较大的数据 或分任意小段接收 接收一小段之后     |
	|	被读出的内容在端口里已被清除 下次再接收时不会接收到从复内容 自动接着未接收的内容 连接一但断开 双方无论接收还是发送都会返回-1   |
	|	传输策略一般发送端先告知接受端今次要发送的总长度之后再发 接收端一直接收 直到收到的字节数和告知数一至时即接收完成               |
	\---------------------------------------------------------------------------------------------------------------------------------*/
  public:
    int fd; //套接口ID
    P<fd_set> fdsets; //检索集合

    inline cSOC_TCP() : fd(-1) {}
    inline ~cSOC_TCP() { Close(); }

    //打开套接口 成功返回套接口 失败返回-1
    inline int Open() {
        fd = (int)socket(AF_INET, SOCK_STREAM, 0);
        return fd;
    }

    //关闭套接口 成功返回0 失败返回-1
    inline int Close() {
        if(fd != -1) {
            int ret = closesocket((SOCKET)fd);
            fd = -1;
            return ret;
        } else return 0;
    }

    //检测套接口是否可用
    inline bool IsOK() { return fd != -1; }

    //设置一些会堵塞来等待的处理行为(是否会堵塞等待) 系统默认是会等的 但一般用作图形客户端可设成不等 设置成不等后执行那些函数也许会返回-1错误 但还是可以会执行成功的
    inline void SetWait(bool yes) {
        unsigned long mode = yes ? 0 : 1;
        ioctlsocket((SOCKET)fd, FIONBIO, &mode);
    }

    //获得已连接的套接口的本机地址(返回地址)   成功返回0 失败返回-1
    inline int GetsockAddr(sADDR &addr_out) {
        addr_len_t addr_len = GT_ADDR_LEN;
        return getsockname((SOCKET)fd, (sockaddr *)(&addr_out), &addr_len);
    }

    //获得已连接的套接口的另一端地址(返回地址) 成功返回0 失败返回-1
    inline int GetpeerAddr(sADDR &addr_out) {
        addr_len_t addr_len = GT_ADDR_LEN;
        return getpeername((SOCKET)fd, (sockaddr *)(&addr_out), &addr_len);
    }

    //作为服务器需调用的功能 --------------------------------------------------------------------------------------------------------------------------------------

    //绑定指定地址以便其它主机能访问它(地址)成功返回0 失败返回-1 使它作为被动方式
    inline int Bind(const sADDR &addr) { return bind((SOCKET)fd, (sockaddr *)(&addr), GT_ADDR_LEN); }

    //设置已绑定地址端口的套接口聆听待连接队列的最大数(待连接队列的最大数)成功返回0 失败返回-1
    inline int Listen(int maxn) { return listen((SOCKET)fd, maxn); }

    //同意一个连接(返回远端主机的地址信息) 返回和远端主机通信的套接口 如果套接口为空表示操作失败 !该函数会有堵塞行为
    inline P<cSOC_TCP> Accept(sADDR &addr_out) {
        addr_len_t addr_len = GT_ADDR_LEN;
        int ret = (int)accept((SOCKET)fd, (sockaddr *)(&addr_out), &addr_len);
        if(ret == -1) return P<cSOC_TCP>();
        else {
            P<cSOC_TCP> clt(1);
            clt->fd = ret;
            return clt;
        }
    }

    //作为客户端需调用的功能 --------------------------------------------------------------------------------------------------------------------------------------

    //连接到目标地址的主机(目标地址) 成功返回0 失败返回-1
    inline int Connect(const sADDR &addr) { return connect((SOCKET)fd, (sockaddr *)(&addr), GT_ADDR_LEN); }

    //双方收发的功能 ----------------------------------------------------------------------------------------------------------

    //发送数据(数据, 数据字节长度, 附加功能) 返回发送了的字节数 返回-1代表有错
    inline int Send(const char *buf_in, int buf_len, int flags = 0) { return send((SOCKET)fd, buf_in, buf_len, flags); }

    //接收数据(接收数据的缓冲 须先分配好空间, 缓冲字节长度, 附加功能) 返回已接收的字节数 返回-1代表有错 !该函数会有堵塞行为 如果套接口设置成不等接收当没消息到时也会返回-1 但如果是对方端套接口设置成不等接收时 就算失去连接了都不会通告-1错误 而是会立即返回 堵塞等待失效 这会耗尽你的CPU 这个处理上要特别小心一下
    inline int Recv(char *buf_out, int buf_len, int flags = 0) { return recv((SOCKET)fd, buf_out, buf_len, flags); }

    //不断发送数据直到发完指定长度为止(数据, 数据字节长度, 附加功能) 返回-1代表有错
    int Send__(const char *buf_in, int buf_len, int flags = 0);

    //不断接收数据直到收完指定长度为止(接收数据的缓冲 须先分配好空间, 缓冲字节长度, 附加功能) 返回-1代表有错 !!该函数会有堵塞行为 如果套接口设置成不等接收当没消息到时也会返回-1  但如果是对方端套接口设置成不等接收时 就算失去连接了都不会通告-1错误而是不断地返回 堵塞等待失效 这会耗尽你的CPU 这个处理上要特别小心一下
    int Recv__(char *buf_out, int buf_len, int flags = 0);

    //检测套接口有没数据可以接收(检索时间) 返回>0表示有数据可以接收 0:表示指定时间内未发现有数据 -1:表示检索时有错 !!如果你不想一调用Recv就有可能卡在那里而不能继续运行下行代码时可以用这个先检测一下再去收取数据可更好地控制
    int RecvCheck(unsigned int time);
};
class NET_API cSOC_UDP //★UDP协议类  传统socket的一个简单封装
{
    /*------------------------------------------------ 关于UDP的一些传输细节 ------------------------------------------------------------\
	|	一次性最大得发 GN_MAX_BUF_UDP 字节数据 在网络连接正常的情况下都有可能会丢包或接收与发送方的次序乱了                              |
	|	一但发送出去 发送方就不管了 就算中途网络出问题也不会有错误通告                                                                   |
	|	若接收方能接收到包 那该包就一定是完整的 如果接收时网络出问题或套接口被关闭 就会返回-1报错                                        |
	|	传输策略 遇到大数据包必须把它拆分成若干小数据包 每个数据包都带一个标识序号 发送时先告知接收方要接收几个数据包                    |
	|	按顺序把包发出去 接收方收到接包通知就一直接收包 当收到一个包时标记收到序号 直到把全部包接收完毕 如果接收完最后个包发现之前       |
	|	的包有漏接 就把漏接的标记组发给发送方以便通知再发之后再接收 若在接收途中等了很久没有包到就把漏接的标记组重发给发送方 如果遇到    |
	|	好几次都有漏接情况就视为网络错误                                                                                                 |
	\-----------------------------------------------------------------------------------------------------------------------------------*/
  public:
    int fd; //套接口ID
    P<fd_set> fdsets; //检索集合

    inline cSOC_UDP() : fd(-1) {}
    inline ~cSOC_UDP() { Close(); }

    //打开套接口 成功返回套接口 失败返回-1
    inline int Open() {
        fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
        return fd;
    }

    //关闭套接口 成功返回0 失败返回-1
    inline int Close() {
        if(fd != -1) {
            int ret = closesocket((SOCKET)fd);
            fd = -1;
            return ret;
        } else return 0;
    }

    //检测套接口是否可用
    inline bool IsOK() { return fd != -1; }

    //设置一些会堵塞来等待的处理行为(是否会堵塞等待) 系统默认是会等的 但一般用作图形客户端可设成不等 设置成不等后执行那些函数也许会返回-1错误 但还是可以会执行成功的
    inline void SetWait(bool yes) {
        unsigned long mode = yes ? 0 : 1;
        ioctlsocket((SOCKET)fd, FIONBIO, &mode);
    }

    //获得套接口的本机地址  成功返回0 失败返回-1  !!这个必需要已和对方通信过之后获取才有正确值返回
    inline int GetsockAddr(sADDR &addr_out) {
        addr_len_t addr_len = GT_ADDR_LEN;
        return getsockname((SOCKET)fd, (sockaddr *)(&addr_out), &addr_len);
    }

    //作为服务器需调用的功能 --------------------------------------------------------------------------------------------------------------------------

    //绑定指定地址以便其它主机能访问它(地址)成功返回0 失败返回-1 使它作为被动方式
    inline int Bind(const sADDR &addr) { return bind((SOCKET)fd, (sockaddr *)(&addr), GT_ADDR_LEN); }

    //双方收发的功能 ----------------------------------------------------------------------------------------------------------------------------------

    //发送数据包到指定IP地址(指定地址, 数据, 数据字节长度, 附加功能) 返回这次发送了的字节数 返回-1代表有错 !一次最多只能发 GN_MAX_BUF_UDP 字节数据
    inline int SendTo(const sADDR &addr, const char *buf_in, int buf_len, int flags = 0) { return sendto((SOCKET)fd, buf_in, buf_len, flags, (sockaddr *)(&addr), GT_ADDR_LEN); }

    //接收数据包(数据包来源地址输出, 接收数据的缓冲 须先分配好空间, 缓冲字节长度, 附加功能) 返回已接收的字节数 返回-1代表有错 !!该函数有堵塞行为 如果设置成不等接收当没消息到时也会返回-1
    inline int RecvFrom(sADDR &addr_out, char *buf_out, int buf_len, int flags = 0) {
        addr_len_t addr_len = GT_ADDR_LEN;
        return recvfrom((SOCKET)fd, buf_out, buf_len, flags, (sockaddr *)(&addr_out), &addr_len);
    }

    //检测套接口有没数据可以接收(检索时间) 返回>0表示有数据可以接收 0:表示指定时间内未发现有数据 -1:表示检索时有错  !!如果你不想一调用RecvFrom就有可能卡在那里而不能继续运行下行代码时可以用这个先检测一下再去收取数据能更好地控制
    int RecvCheck(unsigned int time);
};
}; //end namespace GNn

/**************************************************************** HTTP 通讯协议相关*********************************************************************************************/
//回应数据类型

#define GNn_HTTP_CONTENT_PNG "image/png" // PNG
#define GNn_HTTP_CONTENT_JPG "image/jpeg" // JPG
#define GNn_HTTP_CONTENT_GIF "image/gif" // GIF

#define GNn_HTTP_CONTENT_HTML "text/html" // HTML
#define GNn_HTTP_CONTENT_TEXT "text/plain" // 文本
#define GNn_HTTP_CONTENT_CSS "text/css" // CSS式样
#define GNn_HTTP_CONTENT_JS "text/javascript" // JS脚本

//提交数据类型

#define GNn_HTTP_CONTENT_URLENCODED "application/x-www-form-urlencoded" //参数串       适用于一般简单表单参数提交 如"name=mary&age=23&height=165" 多个参数之间用 & 隔开
#define GNn_HTTP_CONTENT_JSON "application/json" //json格式参数 适用于结构复杂带层次嵌套的多参数提交 如提交一个对象或数组数据 如{ "name":"mary","age":"23","height":"165" }
#define GNn_HTTP_CONTENT_MULTIPART "multipart/form-data" //多类型混合   适用于多种不同类型的参数或数据一起提交 如随表单一并多提交一个文件 由于这种类型内容编写起来特别麻烦 因此通常调用cCLIENT_HTTP::RequestPost或cCLIENT_HTTP::SendRequestPost来代替手动编写
#define GNn_HTTP_CONTENT_OCTET "application/octet-stream" //二进制字节流 适用于原始字节流数据提交 通常上传文件数据段多用这种类型

extern NET_API const char GNn__HttpHead__[]; //http通用的报头 "%s /%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nUser-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.1 (KHTML, like Gecko) Chrome/21.0.1180.89 Safari/537.1\r\nCache-Control: no-cache\r\n"
extern NET_API const char GNn_HttpHead__Content[]; //http通用的内容报头字段 "Connection: Keep-Alive\r\ncontent-type: %s\r\nContent-Length: %d\r\n\r\n"

//http回应分析
NET_API bool GNn_HttpResponseAnalysis(
    char *buf /*[1024]*/, //回应头内容 成功调用后这个会被截取为完整的单独回应头 最后的\r\n\r\n也会被裁掉
    int buf_size, //内容占大小字节
    int &state, //返回回应状态码 如200 -1表示有错
    char *data, //用来收取回应实体数据的指针 该值如果为 0 将不收取回应实体数据 通常分析HEAD的回应就填0 参数data预先分配的空间最好也不能少于回应头内容的空间
    int &data_size, //返回回应实体数据的大小字节 -1表示不确定将用小节分块的方式来传数据 -2表示不确定的一小段数据
    int &rv_size //返回本次有收取到的回应实体数据大小字节
); //返回对方是否提示连接已断开!

//从一连串小节分块数据流里抽取组合出有用的内容数据(小节分块数据流)返回内容数据 !小节分块数据流是指那些用特殊分隔符隔开的一连串数据 其中必需包含结束符号 0\r\n\r\n
NET_API P<char> GNn_HttpGetContentFromChunked(const char *chunked_stream);

//接收http小节分块数据(读取的http套接口, 返回状态 1:还有数据待收 0:没有数据了 -1:有错 -2超时, 包含小节分块头信息的缓冲 默认0表示要求调用时一并查收分块头信息, 缓冲内容大小, 接收等待时间)成功返回该节分块数据
NET_API P<char> GNn_HttpChunkedRecv(GNn::cSOC_TCP *_tcp, int &state, const char *h_date = 0, int h_szie = 0, unsigned int wait_time = 0xffffffff);

//不断接收http小节分块数据直到收完为止(读取的http套接口, 返回 回应状态码 如200 -1有错 0超时, 返回处理进度0.0f至1.0f, 包含小节分块头信息的缓冲 默认0表示要求调用时一并查收分块头信息, 缓冲内容大小, 接收等待时间)成功返回各个分块组成的完成数据
NET_API P<char> GNn_HttpChunkedRecv__(GNn::cSOC_TCP *_tcp, int &state, float &prog, const char *h_date = 0, int h_szie = 0, unsigned int wait_time = 0xffffffff);

//接收http服务器回应(读取的http套接口, 返回 回应头内容,  返回 回应头长度, 返回 回应状态码 如200 -1有错 0超时, 返回处理进度0.0f至1.0f, 返回对方是否提示连接已断开, 是否要一并接收回应附带的实体内容, 接收等待时间) 返回回应附带的实体内容数据 如果没有附带内容数据则返回空指针
NET_API P<char> GNn_HttpResponseRecv(GNn::cSOC_TCP *_tcp, char *re_head /*[2048]*/, int &re_head_size, int &state, float &prog, bool &connect_close, bool content_recv = true, unsigned int wait_time = 0xffffffff);

//http回应回调类型( 回应附带的实体内容数据 空指针表示无附带数据, 回应状态码 如200 -1有错 0超时, 回应头信息, 回应头大小, 用户回传参数) !需注意的是如果是超时是没有回应头信息返回的 另外参数content返回给你的内容是按二进制位保存的 即使内容是字符串尾部不会包含结束符号'\0' 不过你可以这样 content.CopyOut(1,'\0'); 便捷地补上结束符
typedef void (*GNn_HTTP_CALLBACK)(P<char> &content, int state, const char *head_msg, int head_size, void *arg);

void GT_ClientHttpRequest(P<GN_sTHREAD>, void *, void *);

namespace GNn {

struct NET_API sHTTP_POST // ★HTTP提交参数
{
    char *name; //参数名
    char *value; //参数值 或文件的数据内容
    char *filename; //路径文件名 当有指定时 这个POST的就为一个文件
    int flags; //文件标记 如果你有把文件数据内容手动指定给value 那这里就记下文件数据的大小 如果只指定了文件路径 那则用来记下文件路径是否为全局路径
    //默认构造  ( )
    inline sHTTP_POST() : name(0), value(0), filename(0), flags(1024) {}
    //构造		(   参数名,        参数值)
    inline sHTTP_POST(/*const*/ char *_name, /*const*/ char *_value) : name(_name), value(_value), filename(0), flags(1024) {}
    //构造		(   参数名,      路径文件名 ,  文件路径是否为全局路径)
    inline sHTTP_POST(/*const*/ char *_name, /*const*/ char *_filename, bool _global_path) : name(_name), value(0), filename(_filename), flags(_global_path ? 1 : 0) {}
    //构造		(   参数名,      路径文件名 ,     文件数据,     文件数据大小)
    inline sHTTP_POST(/*const*/ char *_name, /*const*/ char *_filename, /*const*/ char *_filedata, int _filesize) : name(_name), value(_filedata), filename(_filename), flags(_filesize) {}

    //初始化参数(   参数名,        参数值)
    inline void Init(/*const*/ char *_name, /*const*/ char *_value) { name = _name, value = _value, filename = 0, flags = 1024; }
    //初始化文件(   参数名,      路径文件名 ,  文件路径是否为全局路径)
    inline void Init(/*const*/ char *_name, /*const*/ char *_filename, bool _global_path) { name = _name, value = 0, filename = _filename, flags = _global_path ? 1 : 0; }
    //初始化文件(   参数名,      路径文件名 ,     文件数据,     文件数据大小)
    inline void Init(/*const*/ char *_name, /*const*/ char *_filename, /*const*/ char *_filedata, int _filesize) { name = _name, value = _filedata, filename = _filename, flags = _filesize; }
};
class NET_API cCLIENT_HTTP // ★★★HTTP协议访问客户 现在很多互联网Web应用都是使用HTTP协议进行数据传输通讯的
{
    P<GN_sTHREAD> zTaskThread; //异步处理的任务线程 机器支持多线程时才会有 仅多线程处理时用到
    P<char> zResContent; //用于接收回应内容 仅单线程里用到
    P<char> zBufTemp; //临时缓冲 多用于http报头的发送和接收 或者用于一些临时内部参数传递
    P<sHTTP_POST> zContents; //储存用户要求提交发送的多类型混合数据的指针
    char *zContent; //储存用户要求提交发送的数据的指针
    int zContentSize; //储存用户要求提交发送的数据大小 也记录回应的实体内容大小
    int zContents_Size; //统计 zContents 里的所有数据内容总量 仅单线程里用到
    int zHeadSize; //记录发送或接收到的http报头大小
    int zRvSize; //记录当前已发送或已经接收大小 仅单线程里用到
    bool zConnectKeep; //储存用户是否有要求处理后继续保持连接
    bool zContentRecv; //标记是否要收取回应的实体内容 仅单线程里用到
    uint8 zCallBackType; //记录最近处理的任务是经那个调用触发的 0:SendRequestRaw 1:SendRequestPost 2:SendRequest
    uint8 zTaskState; //标记当前任务的处理步骤状态 仅单线程里用到
    GNn_HTTP_CALLBACK zResponseCall; //储存用户指定的回应回调函数
    void *zUsArg; //储存用户回传参数
    float *zProgress; //储存用户传来的进度变量指针 以便告诉用户处理进度
    cCLIENT_HTTP **zDelID; //任务删除号 仅单线程里用到
    unsigned int zTaskTime; //累计任务每个步骤处理的时间 仅单线程里用到

    unsigned int zTimeOut; //查收回应等待时间毫秒
    P<cSOC_TCP> zSocTcp; //TCP套接口
    P<char> zHost; //请求的服务器名
    sADDR zAddr; //请求的服务器地址信息

    friend void ::GT_ClientHttpRequest(P<GN_sTHREAD>, void *, void *);

    //按参数要求来构建一个http报头(请求的路经文件名, 请求方法, 提交附带数据类型 当用POST时用到, 提交附带数据内容, 提交附带数据大小)返回报头长度
    int zHeadMsgMake(const char *filename, const char *method, const char *content_type, const char *content, int &content_size);
    //按参数要求来构建一个http报头(请求的路经文件名, 提交附带数据内容, 返回提交附带数据的总大小, 返回用multipart/form-data类型序列化好的内容 默认0表示不要求序列化内容)返回报头长度
    int zHeadMsgMake(const char *filename, P<sHTTP_POST> &content, int &_content_size, char **pp_content = 0);

    void zHeadRecvBeforeSet(); //接收 http 报头前一些属性设置()
    void zTaskDataClear(); //清除任务相关的数据()
    void zUpdate(unsigned int updatetime); //在不支持多线程时更新处理全部该类对象的请求任务(帧间隔时间) 在不支持多线程的客户端想不堵塞 应该放在每帧的后台处理回调比较顶部的地方执行这个
    void zUpdateTaskAdd(); //在不支持多线程时增加一个更新任务()

    static GN::sLIST<cCLIENT_HTTP *> zUpList; //在不支持多线程时记录该类对象正要更新的对象列表
  public:
    static void zUpdateAll(unsigned int updatetime); //在不支持多线程时更新该类对象有必要更新的对象(帧间隔时间)

    //★用户调用接口------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
    inline cCLIENT_HTTP() : zResponseCall(0), zDelID(0) {}
    inline ~cCLIENT_HTTP() { this->EndTask(false); }

    //通知并等待任务结束(是否要继续保持连接)
    void EndTask(bool connect_keep = true);

    //初始化(访问目标主机的域名或IP地址, 请求等待时间, 目标端口) 返回是否成功
    bool Init(const char *server_name, unsigned int time_out = 0xffffffff, unsigned short port = 80);

    //!以下3个调用返回的实体内容是按二进制位保存的 即使内容为字符串尾部不会包含结束符'\0' 不过您可以这样 content.CopyOut(1,'\0'); 便捷地补上结束符

    //发出完整原始请求信息并等待查收回应(完整的原始请求输入 调用后回应头输出,  请求头大小输入 回应头大小输出,  提交附带数据内容 当为POST时用到,  提交附带数据大小,  返回 回应状态码 如200 -1有错 0超时, 返回处理进度0.0f至1.0f, 是否要求继续保持连接) 返回回应附带的实体内容数据 如果没有附带内容则返回空指针
    P<char> RequestRaw(char *head_msg /*2048*/, int *head_size, const char *content = 0, int content_size = 0, int *state_out = 0, float *prog_out = 0, bool connect_keep = true);

    //发出请求并等待查收回应(请求的路经文件名 相对于网站目录跟开始, 请求方法, 提交附带数据类型 参考GNn_HTTP_CONTENT_URLENCODED相关定义 当为POST时用到,  提交附带数据内容,  提交附带数据大小,  返回 回应状态码 如200 -1有错 0超时, 返回处理进度0.0f至1.0f, 是否要求继续保持连接) 返回回应附带的实体内容数据 如果没有附带内容则返回空指针 !如果附带数据大小可以指定为0 这样调用会一并自动帮您获取大小 但如果附带数据类型为 GNn_HTTP_CONTENT_OCTET 就必须指定大小
    P<char> Request(const char *filename, const char *method = "GET", const char *content_type = 0, const char *content = 0, int content_size = 0, int *state_out = 0, float *prog_out = 0, bool connect_keep = true);

    //发出提交数据请求并等待查收回应(请求的路经文件名 相对于网站目录跟开始, 提交附带数据内容,  返回 回应状态码 如200 -1有错 0超时, 返回处理进度0.0f至1.0f, 是否要求继续保持连接) 返回回应附带的实体内容数据 如果没有附带内容则返回空指针 !这个适用于多种不同类型的参数或数据一起提交用 你也可以用这个来模以浏览器上传文件 但文件大小最好限制在2MB以内 大文件最好用cDTP类来上传
    P<char> RequestPost(const char *filename, P<sHTTP_POST> content, int *state_out = 0, float *prog_out = 0, bool connect_keep = true);

    //以下这3个为异步处理的请求 调用后会马上返回 当处理完后便会调用指定的回应回调来通知您! !需要注意的是这几个函数调用你必需保证内容参数 content 和 prog_out ...等一些输入参数在未收到回应之前均是有效的 空间未被释放

    //发出完整原始请求信息(当收到回应时的处理, 传回给您的参数, 完整的原始请求,  请求头大小,  提交附带数据内容 当为POST时用到,  提交附带数据大小,  返回处理进度0.0f至1.0f, 是否要求继续保持连接)返回是否调用成功
    bool SendRequestRaw(GNn_HTTP_CALLBACK proc, void *us_arg, const char *head_msg /*2048*/, int head_size, const char *content = 0, int content_size = 0, float *prog_out = 0, bool connect_keep = true);

    //发出请求(当收到回应时的处理, 传回给您的参数, 请求的路经文件名 相对于网站目录跟开始,  请求方法,  提交附带数据类型 参考GNn_HTTP_CONTENT_URLENCODED相关定义 当为POST时用到,  提交附带数据内容 ,  提交附带数据大小, 返回处理进度0.0f至1.0f, 是否要求继续保持连接)返回是否调用成功 !!如果附带数据大小可以指定为0 这样调用会一并自动帮您获取大小 但如果附带数据类型为 GNn_HTTP_CONTENT_OCTET 就必需指定大小
    bool SendRequest(GNn_HTTP_CALLBACK proc, void *us_arg, const char *filename, const char *method = "GET", const char *content_type = 0, const char *content = 0, int content_size = 0, float *prog_out = 0, bool connect_keep = true);

    //发出提交数据请求(当收到回应时的处理, 传回给您的参数, 请求的路经文件名 相对于网站目录跟开始,  提交附带数据内容,  返回处理进度0.0f至1.0f, 是否要求继续保持连接)返回是否调用成功 !这个适用于多种不同类型的参数或数据一起提交用 你也可以用这个来模以浏览器上传文件 但文件大小最好限制在2MB以内 大文件最好用cDTP类来上传
    bool SendRequestPost(GNn_HTTP_CALLBACK proc, void *us_arg, const char *filename, P<sHTTP_POST> content, float *prog_out = 0, bool connect_keep = true);

    //重新连接目标主机() 返回是否成功  !有时网络差或隔了一段时间没联系对方了 对方服务器会超时自动断开连接
    bool ReConnect();

    inline bool IsBusy() { return zResponseCall ? true : false; } //判断当前是否正在忙碌中 还有任务未处理完
    inline unsigned int GetTimeOutTime() { return zTimeOut; } //获得查收回应等待时间
    inline P<char> GetTargetHost() { return zHost; } //获得目标主机名
    inline sADDR /*const*/ *GetTargetAddr() { return &zAddr; } //获得目标主机地址信息
};
//读取指定路径的图像 如图像不存在尝试从http连接里的相应路径下载它再读取(http客户, 路径文件名 这个必须是全名, 要装载的图像, 想要保存的颜色格式, 是否使用多级渐进纹理, 抗剧齿平滑过渡质量等级)返回0:失败 1:在本地目录读取到 2:经过下载后再读取到 后两个参数一般在3D应用时才有用 2D应用使用默认值即可 !!另外需要注意的是如果所读取的图象数据为RGB格式时 最好能保证图象的宽度能整除4 不能整除4的会额外多开消一些性能来作转换处理
NET_API uint8 ImageDownloadAndLoad(cCLIENT_HTTP *clt_http, const char *filename, GNg::sIMAGE *img, GNg::COLOR_FORMAT texformat = GNg::CR_DEFAULT_FORMAT, bool us_mipmap = false, unsigned char linearlv = 1);

//从http连接里下载一个文件(要下载文件的连接, 文件保存路径, 返回处理进度0.0f至1.0f) 成功返回整个文件数据 失败返回空指针 !这是一个简便的下载方法 只适合单个小文件下载 如要连续在同一服务器上下载多个文件请用 GNn::cDTP
NET_API P<char> FileDownloadFromUrl(const char *url, const char *savepath = 0, float *progr_out = 0);

//上传一个文件到指定http连接(服务端程序连接, 文件路径,  文件标记 0:参数file_path为相对路经 1:为全局路径 >1的其它值指定文件数据大小 这时你必须指定file_buf, 文件数据指针, 返回处理进度0.0f至1.0f)成功返回一些消息 如服务器的答复按二进制位保存的 失败返回空指针 上传文件必需有相关的服务端程序配合使用才会生效 !这是一个简便的上传方法 只适合单个小文件上传 如要上传多个文件到在同一服务器上请用 GNn::cDTP
NET_API P<char> FileUploadToUrl(const char *url, const char *filepath, int flags = 0, const char *file_buf = 0, float *progr_out = 0);

/***************************************************************** 一些基础服务器类 **************************************************************************/

struct NET_API sCLIENT_EXT //客户信息扩展   配合 cSERVER_TCP 使用 如果想增加每个登陆客户对应信息可继承该类增加属性
{
    unsigned int us_reserve; //保留给用户用

    virtual ~sCLIENT_EXT() {}
};
struct NET_API sCLIENT_PORT //客户端内部信息
{
    P<GN_sTHREAD> thread; //处理线程
    P<GNn::cSOC_TCP> socket; //收取套接口
    GNn::sADDR addr; //IP地址
    unsigned int time;
    P<sCLIENT_EXT> us_ext; //用户扩展数据

    inline sCLIENT_PORT() {}
    inline sCLIENT_PORT(P<GN_sTHREAD> _thread, P<cSOC_TCP> _socket, const sADDR &_addr, unsigned int _time) : thread(_thread), socket(_socket), addr(_addr), time(_time) {}
};
class NET_API cSERVER_TCP // ★TCP协议服务器基类
{
  public:
    GN::sLIST<sCLIENT_PORT> zClientList; //记录还在处理的客户队列
    P<cSOC_TCP> zSocket; //TCP套接口
    P<GN_sTHREAD> zThread; //服务处理线程
    bool zRun; //当前是否正在运行中
    bool zCListLock; //标记当前客户列表是否被锁定
    unsigned int zTimeOut; //处理客户超时断开连接的时间 如果有新客户连入时 旧客户超过这个时间没有和服务器通信就自动断开与该客户的连接 值:0xffffffff为不断开处理
    sADDR zAddr; //服务器地址信息

    inline cSERVER_TCP() : zRun(false) {}
    virtual ~cSERVER_TCP() { Shutdown(); }

    //初始化服务器(使用端口, 待连接队列的最大数, 处理客户超时自动断开连接时间) 返回否成功
    bool Init(unsigned short port, int listen_maxn = 18, unsigned int time_out = 0xffffffff);

    //启动服务(是否使用超线程来处理 即每个连接都用一个新线程) !在多核的CPU下这会新开一个线程来处理 否则一直处于无限死循环状态来处理服务
    void StartUp(bool us_smt = GN_ThreadSupport);

    //等待其他任务线程完成 以便安全退出并关闭服务
    void Shutdown();

    //当与一个客户端刚建立连接时的处理(和客户端通信的套接口, 客户端地址信息, 客户扩展数据)
    virtual void OnConnect(cSOC_TCP *client_socket, sADDR &client_addr, P<sCLIENT_EXT> &us_ext) {}

    //当与客户端正在通信时的处理(和客户端通信的套接口, 客户端地址信息, 客户扩展数据) 要求返回是否继续和客户端作处理
    virtual bool OnMsgProc(cSOC_TCP *client_socket, sADDR &client_addr, P<sCLIENT_EXT> &us_ext) {
        char buf[1024];
        if(client_socket->Recv(buf, 1024) > 0) return true;
        else return false;
    }
    /*-------------------------------------------------------------------------------------------------------------------------------*\
     关于回调最后个参数 us_ext 是用来给对应每个正在连接中的客户端增加扩展属性信息的 用法:
	 (1)继承 GNn::sCLIENT_EXT
		 struct sMY_CLIENT_ADDINFO :public GNn::sCLIENT_EXT
		 {
		     char		*data;  //增加自定义属性

			 //... 在有必要时增加更多属性

             sMY_CLIENT_ADDINFO():data(0){}
             virtual ~sMY_CLIENT_ADDINFO(){ GN_DeleteArray(data); }
		 };

	 (2)在 OnConnect 或 OnMsgProc 回调处理中为 us_ext 参数 赋值
         P<sMY_CLIENT_ADDINFO> clt_addinfo(1);
		 clt_addinfo->us_reserve = 2;
         clt_addinfo->data = new char[64];
		 strcpy(clt_addinfo->data, "为属性赋值");
         us_ext = clt_addinfo; //这样即可为对应的客户附加了属性信息

	 \*-------------------------------------------------------------------------------------------------------------------------------*/

    GN::sLIST<sCLIENT_PORT> /*const*/ &LockClientList() {
        zCListLock = true;
        return zClientList;
    } //获得已经连接到服务器的客户列表 并临时锁定它 通过该函数获得信息后应该赶快调用UnLockClientList来解除锁定 以免耽误后台线程对该列表的更改等待
    void UnLockClientList() { zCListLock = false; } //解除对客户列表的锁定 恢复后台线程正常运作
};
class NET_API cSERVER_UDP // ★UDP协议服务器基类
{
  public:
    P<cSOC_UDP> zSocket; //UDP套接口
    P<GN_sTHREAD> zThread; //服务处理线程
    P<char> zBuf; //数据包缓冲
    bool zRun; //当前是否正在运行中
    sADDR zAddr; //服务器地址信息

    inline cSERVER_UDP() : zRun(false) {}
    virtual ~cSERVER_UDP() { Shutdown(); }

    //初始化服务器( 使用端口 ) 返回否成功
    bool Init(unsigned short port);

    //启动服务() !在多核的CPU下这会新开一个线程来处理 否则一直处于无限死循环状态来处理服务
    void StartUp();

    //等待其他任务线程完成 以便安全退出并关闭服务
    void Shutdown();

    virtual void OnRecv(const sADDR &addr, char *data, int size) { //当收到客户端发来消息时的处理(数据包来源地址, 数据内容, 收到的数据总大小字节)
        data[size - 1] = 0;
        zSocket->SendTo(addr, strcat(data, " Receive!"), size + 9);
    }
};
}; //end namespace GNn

NET_API void GN_InitNetwork(); //初始化网络模块()
NET_API void GN_UnInitNetwork(); //释放网络模块()
NET_API void GN_NetworkProc(unsigned int updatetime); //网络模块更新处理(帧间隔时间)

/************************************************ 快捷外部通讯 ***********************************************************/
typedef void (*GN_VOID_PFUNC_PCHAR)(const char *);

//指向一个能向外部程序发出通知消息的函数 引擎系统会用它来向外部程序发出通知消息 默认为空的不会向外界发出通知
extern NET_API void (*GN_ExternNoticePost)(const char *msg);

//获得默认外部程序通知方法( 发通知所使用的端口 ) 默认是使用网络UDP端来发送通知的 引擎必须包含网络模块时才会返回有用的方法
NET_API GN_VOID_PFUNC_PCHAR GN_GetDefExternNoticeFunc(unsigned short port = 53599);

#endif //_GN_NETTRANS_H_
