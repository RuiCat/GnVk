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
#include "GN.h"
#endif
#ifdef _GN_NETTRANS_H_
void GNn::sADDR::Init(unsigned short port, const char *addr, bool is_ip) {
    sin_family = AF_INET;
    memset(&sin_zero, 0, GT_ADDR0_LEN);
    sin_port = htons(port);
    if(addr) {
        if(is_ip) {
            sin_addr.s_addr = inet_addr(addr);
        } else {
            hostent *host__ = gethostbyname(addr);
            if(host__) sin_addr = *((struct in_addr *)host__->h_addr);
            else GNn_ErrorLast("DNS 动态域名解释错误,请确定您的设备已连接网络或所输入的域名地址是否正确!", 0);
        }
    } else sin_addr.s_addr = htonl(INADDR_ANY);
}
GNn::sADDR::sADDR(unsigned short port, const char *addr, bool is_ip) {
    this->Init(port, addr, is_ip);
}
/****************************************************** cSOC_TCP ********************************************************/
int GNn::cSOC_TCP::Send__(const char *buf_in, int buf_len, int flags) {
    int ss, s_ = 0, _s, max = GN_USING_FILE_BUF << 3;
    for(;;) {
        _s = buf_len - s_;
        if(_s > max) _s = max;
        ss = send((SOCKET)fd, buf_in + s_, _s, flags);
        //GN_Log("_s=%d, ss=%d\n",_s, ss);
        if(ss == -1) return -1;
        else s_ += ss;
        if(s_ >= buf_len) break;
        //GN_TimeSleep(1);
    }
    return buf_len;
}
int GNn::cSOC_TCP::Recv__(char *buf_out, int buf_len, int flags) {
    int rs, r_ = 0, _r, max = GN_USING_FILE_BUF << 3;
    for(;;) {
        _r = buf_len - r_;
        if(_r > max) _r = max;
        rs = recv((SOCKET)fd, buf_out + r_, _r, flags);
        //GN_Log("_r=%d, rs=%d\n",_r, rs);
        if(rs == -1) return -1;
        else r_ += rs;
        if(r_ >= buf_len) break;
        //GN_TimeSleep(1);
    }
    return buf_len;
}
int GNn::cSOC_TCP::RecvCheck(unsigned int time) {
#define GT_TMP_FD_CHECK                                \
    if(!fdsets) {                                      \
        fdsets.New(2);                                 \
        FD_ZERO(&fdsets[1]);                           \
        FD_SET(fd, &fdsets[1]);                        \
    }                                                  \
    fdsets[0] = fdsets[1];                             \
    timeval t;                                         \
    t.tv_sec = (long)time / 1000;                      \
    t.tv_usec = ((long)time - t.tv_sec * 1000) * 1000; \
    switch(select(fd + 1, &fdsets[0], 0, 0, &t)) {     \
        case 0: return 0;                              \
        case -1: return -1;                            \
        default:                                       \
            if(FD_ISSET(fd, &fdsets[0])) return 1;     \
            else return 0;                             \
    }
    GT_TMP_FD_CHECK
}
int GNn::cSOC_UDP::RecvCheck(unsigned int time){
    GT_TMP_FD_CHECK}
/***************************************************************** HTTP 实用函数 **************************************************************/
NET_API bool GNn_HttpResponseAnalysis(char *buf /*[1024]*/, int buf_size, int &state, char *data, int &data_size, int &rv_size) {
    data_size = -2;
    rv_size = 0;
    state = -1;
    bool ret = false;
    int ri, size___ = buf_size - 2;
    for(ri = 0; ri < size___; ri++) { //找出返回的信息头结尾处 即一个空行处 来截断
        if(buf[ri] == '\n' && buf[ri + 1] == '\r' && buf[ri + 2] == '\n') {
            if(ri) buf[ri - 1] = 0;
            else buf[ri] = 0;
            break;
        }
    }
    if(ri >= size___) return ret; //未完整的消息头

    if(strstr(buf, "Connection: close")) ret = true;
    state = atoi(GN_StrToKey(buf, ' '));
    //if(state > 299 || state < 200)return ret;
    int data_pos = ri + 3;

    if(!strncmp(buf, "HTTP/1.1 206 Partial ", 21) && (!strncmp(buf + 21, "Content", 7) || !strncmp(buf + 21, "content", 7))) {
        for(ri = 28 + 2; buf[ri]; ri++) { /*找出标记附带数据长度的字段*/
            if(buf[ri] == 'C') {
                if(!strncmp(buf + ri, "Content-Range: bytes ", 21)) {
                    ri += 21;
                    char *p_str = GN_StrToKey(buf + ri, '-');
                    if(p_str) {
                        int size__, __size, f_size;
                        *(p_str - 1) = 0;
                        size__ = atoi(buf + ri);
                        sscanf(p_str, "%d/%d", &__size, &f_size);
                        *(p_str - 1) = '-';
                        data_size = __size - size__ + 1;
                    }
                }
            } else if(buf[ri] == 'T' && !strncmp(buf + ri, "Transfer-Encoding: chunked", 26)) data_size = -1;
        }
    } else {
        for(ri = 15 + 2; buf[ri]; ri++) { /*找出标记附带数据长度的字段*/
            if(buf[ri] == 'C') {
                if(!strncmp(buf + ri, "Content-Length: ", 16)) {
                    ri += 16;
                    data_size = atoi(buf + ri);
                }
            } else if(buf[ri] == 'T' && !strncmp(buf + ri, "Transfer-Encoding: chunked", 26)) data_size = -1;
        }
    }
    if(data) {
        rv_size = (size___ + 2) - data_pos; //计算得到本回应缓冲里有包含的实体数据大小
        if(rv_size) memcpy(data, buf + data_pos, rv_size);
    }
    return ret;
}
NET_API P<char> GNn_HttpGetContentFromChunked(const char *chunked_stream) {
    P<char> data_pack;
    char *p_data = (char *)chunked_stream;
    int size___, size_last = 0;
    for(;;) { //统计有多少个分块数据
        sscanf(p_data, "%x", &size___);
        if(size___) {
            p_data = strstr(p_data, "\r\n") + 2 + size___ + 2;
            size_last += size___;
        } else break;
    }
    if(size_last) { //整理数据
        data_pack.New(size_last);
        p_data = (char *)chunked_stream;
        size_last = 0;
        for(;;) {
            sscanf(p_data, "%x", &size___);
            if(size___) {
                p_data = strstr(p_data, "\r\n") + 2;
                memcpy((char *)data_pack + size_last, p_data, size___);
                size_last += size___;
                p_data += (size___ + 2);
            } else break;
        }
    }
    return data_pack;
}
NET_API P<char> GNn_HttpChunkedRecv(GNn::cSOC_TCP *_tcp, int &state, const char *h_date, int h_szie, unsigned int wait_time) {
    /* 小节分段数据格式
      1d\r\n.............................\r\n7\r\n.......\r\n12\r\n..................\r\n0\r\n\r\n
	  |     - 本函数仅负责读取该小节 -       |               |                           |
	*/
    state = -2;
    P<char> _conent_;
    char end_str[12], *p_data, *_h_date;
    if(h_date && h_szie) _h_date = (char *)h_date;
    else {
        if(wait_time == 0xffffffff) { //读取首段的 1d\r\n
            h_szie = _tcp->Recv(end_str, 12);
        } else {
            h_szie = _tcp->RecvCheck(wait_time);
            if(h_szie > 0) h_szie = _tcp->Recv(end_str, 12);
        }
        if(h_szie <= 0) {
            if(h_szie < 0) state = -1;
            return P<char>();
        }
        _h_date = end_str;
    }
    int cf = 0, size___, size_last, rv_size;

    while(h_szie >= 3) {
        p_data = strstr(_h_date, "\r\n");
        if(!p_data) {
            state = -1;
            return P<char>();
        }
        p_data += 2;
        rv_size = h_szie - (p_data - _h_date);

        sscanf(_h_date, "%x", &size___);
        if(!size___) {
            if(rv_size >= 2 && p_data[0] == '\r' && p_data[1] == '\n') {
                state = 0; //完结
                return _conent_;
            } else {
                state = -1;
                return P<char>();
            }
        }
        cf = _conent_.Size();
        if(cf) {
            P<char> __conent__(cf + size___); //加长数据载体容量
            memcpy(__conent__.Begin(), _conent_.Begin(), cf);
            _conent_ = __conent__;
        } else _conent_.New(size___);
        if(rv_size < size___) {
            if(rv_size) memcpy(_conent_.Begin() + cf, p_data, rv_size); //读取头字段包含的小量数据
            while(rv_size < size___) { //继续收取其余数据
                if(wait_time == 0xffffffff) {
                    size_last = _tcp->Recv(_conent_.Begin() + cf + rv_size, size___ - rv_size);
                } else {
                    size_last = _tcp->RecvCheck(wait_time);
                    if(size_last > 0) size_last = _tcp->Recv(_conent_.Begin() + cf + rv_size, size___ - rv_size);
                }
                if(size_last > 0) rv_size += size_last;
                else {
                    if(size_last < 0) state = -1;
                    return P<char>();
                }
            }
        } else memcpy(_conent_.Begin() + cf, p_data, size___); //读取头字段包含的小量数据
        rv_size -= size___;
        if(rv_size < 2) {
            if(wait_time == 0xffffffff) { //读取未段的 \r\n
                size_last = _tcp->Recv(end_str, 2 - rv_size);
            } else {
                size_last = _tcp->RecvCheck(wait_time);
                if(size_last > 0) size_last = _tcp->Recv(end_str, 2 - rv_size);
            }
            if(size_last > 0) {
                state = 1; //还有数据待收
                return _conent_;
            } else {
                if(size_last < 0) state = -1;
                return P<char>();
            }
        } else {
            p_data += size___ + 2;
            h_szie -= (p_data - _h_date);
            _h_date = p_data;

            if(!h_szie) { /*添加于:2020,1,5*/
                state = 0; //完结
                return _conent_;
            }
        }
    }
    state = -1;
    return P<char>();
}
NET_API P<char> GNn_HttpChunkedRecv__(GNn::cSOC_TCP *_tcp, int &state, float &prog, const char *h_date, int h_szie, unsigned int wait_time) {
    int state2, data_size = 0;
    GN::sLIST<P<char>> chunkeds;
    P<char> _conent_, conent;
    do {
        _conent_ = GNn_HttpChunkedRecv(_tcp, state2, data_size ? 0 : h_date, data_size ? 0 : h_szie, wait_time);
        if(_conent_) {
            chunkeds.push_back(_conent_);
            data_size += (int)_conent_.Num();
            prog += 0.002f;
        }
    } while(state2 == 1);
    if(state2 == -1) state = -1;
    else if(state2 == -2) state = 0;
    prog = 0.9f;
    if(data_size) {
        conent.New(data_size);
        char *p_data = conent;
        GN::sLIST<P<char>>::iterator itc = chunkeds.begin(), etc = chunkeds.end();
        for(; itc != etc; ++itc) {
            _conent_ = (*itc);
            memcpy(p_data, _conent_.Begin(), _conent_.Size());
            p_data += _conent_.Num();
        }
    }
    return conent;
}
NET_API P<char> GNn_HttpResponseRecv(GNn::cSOC_TCP *_tcp, char *re_head /*[2048]*/, int &re_head_size, int &state, float &prog, bool &connect_close, bool content_recv, unsigned int wait_time) {
    prog = 0.0f;
    re_head[0] = 0;
    re_head_size = 0;
    state = 0;
    connect_close = false;
    P<char> conent;

    if(wait_time == 0xffffffff) {
        re_head_size = _tcp->Recv(re_head, GN_MAX_BUF_HTTP_HEAD);
    } else {
        re_head_size = _tcp->RecvCheck(wait_time);
        if(re_head_size > 0) re_head_size = _tcp->Recv(re_head, GN_MAX_BUF_HTTP_HEAD);
    }
    if(re_head_size < 0) {
        connect_close = true;
        state = -1;
    }
    if(re_head_size <= 0) return conent;

    prog = 0.1f;
    int data_size, rv_size;
    char data_[GN_MAX_BUF_HTTP_HEAD];
    connect_close = GNn_HttpResponseAnalysis(re_head, re_head_size, state, data_, data_size, rv_size);
    re_head_size -= rv_size + 4; //计算得到纯回应头的字段长度
    if(!content_recv || !data_size) {
        prog = 1.0f;
        return conent;
    }
    prog = 0.3f;
    if(data_size == -2) { //一次性回应的小数据
        if(rv_size) conent.New(data_, rv_size);
        else if(strstr(re_head, "Content-Type: text")) { //还要等服务器答复
            if(wait_time == 0xffffffff) {
                rv_size = _tcp->Recv(data_, GN_MAX_BUF_HTTP_HEAD);
            } else {
                rv_size = _tcp->RecvCheck(wait_time);
                if(rv_size > 0) rv_size = _tcp->Recv(data_, GN_MAX_BUF_HTTP_HEAD);
            }
            if(rv_size > 0) conent.New(data_, rv_size);
            else {
                if(rv_size < 0) {
                    connect_close = true;
                    state = -1;
                } else state = 0;
            }
        }
    } else if(data_size == -1) { //小节分块数据
        conent = GNn_HttpChunkedRecv__(_tcp, state, prog, data_, rv_size, wait_time);

    } else /* if(data_size > 0)*/ { //大数据
        conent.New(data_size);
        if(rv_size) memcpy(conent.Begin(), data_, rv_size);
        int size_last;
        while(rv_size < data_size) { //不断地读完满数据为止
            prog = 0.7f * ((float)rv_size / (float)data_size) + 0.3f;
            if(wait_time == 0xffffffff) {
                size_last = _tcp->Recv(conent.Begin() + rv_size, data_size - rv_size);
            } else {
                size_last = _tcp->RecvCheck(wait_time);
                if(size_last > 0) size_last = _tcp->Recv(conent.Begin() + rv_size, data_size - rv_size);
            }
            if(size_last > 0) rv_size += size_last;
            else {
                if(size_last < 0) {
                    connect_close = true;
                    state = -1;
                } else state = 0;
                conent.Delete();
                break;
            }
        }
    }
    prog = 1.0f;
    return conent;
}
NET_API const char GNn__HttpHead__[] = "%s /%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nUser-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.1 (KHTML, like Gecko) Chrome/21.0.1180.89 Safari/537.1\r\nCache-Control: no-cache\r\n";
NET_API const char GNn_HttpHead__Content[] = "Connection: Keep-Alive\r\ncontent-type: %s\r\nContent-Length: %d\r\n\r\n";

const char GNn_HttpContentBoundary[] = "--------Hds-8---FormBoundarytrCW-456hFd3TrwA657..Q_Q..6";
const char GNn_HttpContentDisposition[] = "Content-Disposition: form-data; name=";
const char GNn_HttpContentFilename[] = "; filename=";
const char GNn_HttpContentType[] = "Content-Type: ";

static int GT_HttpPostGetOccupancySize(const GNn::sHTTP_POST *v) {
    static int bs = strlen(GNn_HttpContentBoundary);
    static int ds = strlen(GNn_HttpContentDisposition), fs = strlen(GNn_HttpContentFilename), ts = strlen(GNn_HttpContentType) + strlen(GNn_HTTP_CONTENT_OCTET);
    if(v->filename) {
        int file_size;
        if(v->value) file_size = v->flags;
        else {
            file_size = 0;
            GNf::sFILE::EnableGlobalPath = v->flags ? true : false;
            GNf::sFILE *fp = GNf::Open(v->filename, "rb");
            if(fp) {
                file_size = GNf::GetSize(fp);
                GNf::Close(fp);
            }
        }
        return 2 + bs + 2 + ds + strlen(v->name) + 2 + fs + strlen(v->filename) + 4 + ts + 4 + file_size + 2;
    } else return 2 + bs + 2 + ds + strlen(v->name) + 6 + strlen(v->value) + 2;
}
static void GT_HttpPostContentMake(const GNn::sHTTP_POST *v, char **w_pos) {
    if(v->filename) {
        (*w_pos) += sprintf(*w_pos, "--%s\r\n%s\"%s\"%s\"%s\"\r\n%s%s\r\n\r\n", GNn_HttpContentBoundary, GNn_HttpContentDisposition, v->name, GNn_HttpContentFilename, v->filename, GNn_HttpContentType, GNn_HTTP_CONTENT_OCTET);
        if(v->value) {
            memcpy(*w_pos, v->value, v->flags);
            (*w_pos) += v->flags;
        } else {
            GNf::sFILE::EnableGlobalPath = v->flags ? true : false;
            GNf::sFILE *fp = GNf::Open(v->filename, "rb");
            if(fp) {
                int file_size = GNf::GetSize(fp);
                GNf::Read(*w_pos, 1, file_size, fp);
                (*w_pos) += file_size;
                GNf::Close(fp);
            }
        }
        strncpy(*w_pos, "\r\n", 2);
        (*w_pos)[2] = 0;
        (*w_pos) += 2;
    } else (*w_pos) += sprintf(*w_pos, "--%s\r\n%s\"%s\"\r\n\r\n%s\r\n", GNn_HttpContentBoundary, GNn_HttpContentDisposition, v->name, v->value);
}
/********************************************************************* cCLIENT_HTTP **************************************************************************/
void GNn::cCLIENT_HTTP::EndTask(bool connect_keep) {
#ifdef GN_DEBUG
#define GT_TMP_CLIENT_HTTP_SOC_CHECK \
    if(!zSocTcp) { GN_Log("cCLIENT_HTTP 请求未能发出, 还没连接到目标主机, 请先调用 cCLIENT_HTTP::Init !\n"); }
#else
#define GT_TMP_CLIENT_HTTP_SOC_CHECK
#endif
    GT_TMP_CLIENT_HTTP_SOC_CHECK
    if(!zSocTcp) return;

    zSocTcp->Close();
    if(zTaskThread && zTaskThread != 255) {
        zTaskThread->us_state = 254; //通知结束
        GN_ThreadEndWait(zTaskThread);
    }
    zTaskDataClear();
    if(zDelID) {
        zUpList.del(zDelID);
        zDelID = 0;
    }

    if(connect_keep) {
#define GT_TMP_CLIENT_HTTP_RE_CONNECT__(ADD_PROC)                           \
    if(zSocTcp->Open() == -1 || zSocTcp->Connect(zAddr) == -1) {            \
        GNn_ErrorLast("cCLIENT_HTTP 尝试重新连接到目标主机时遇到错误!", 0); \
        ADD_PROC;                                                           \
    }
        GT_TMP_CLIENT_HTTP_RE_CONNECT__(;)
    }
}
bool GNn::cCLIENT_HTTP::ReConnect() {
    GT_TMP_CLIENT_HTTP_SOC_CHECK
    zSocTcp->Close();
    GT_TMP_CLIENT_HTTP_RE_CONNECT__(return false)
    return true;
}
bool GNn::cCLIENT_HTTP::Init(const char *server_name, unsigned int time_out, unsigned short port) {
    if(zSocTcp) this->EndTask(false);
    GNn_ErrorLast.Delete();
    zAddr.Init(port, server_name, GNn_CheckIsIP(server_name) ? true : false);
    if(GNn_ErrorLast) {
#ifdef GN_DEBUG
        GN_Log("cCLIENT_HTTP::Init 失败 ");
#endif
        GN_Log("%s!\n", GNn_ErrorLast.Begin());
        return false;
    }
    zHost(server_name, 0);
    zTimeOut = time_out;
    zSocTcp.New(1);

    if(zSocTcp->Open() == -1 || zSocTcp->Connect(zAddr) == -1) {
#ifdef GN_DEBUG
        GN_Log("cCLIENT_HTTP::Init ");
#endif
        GN_Log("连接到主机:%s 失败!\n", zAddr.GetIP());
        return false;
    }
    return true;
}
int GNn::cCLIENT_HTTP::zHeadMsgMake(const char *filename, const char *method, const char *content_type, const char *content, int &content_size) {
    if(!zBufTemp) zBufTemp.New(GN_MAX_BUF_HTTP_HEAD);
    char *head_msg = zBufTemp;
    int head_size;

    if(content) {
#ifdef GN_DEBUG
        if(strcmp(method, "POST")) { GN_Log("cCLIENT_HTTP 发出请求警告! 有提交附带数据必须用 POST 方法!!\n"); }
#endif
        const char *ct = content_type ? content_type : GNn_HTTP_CONTENT_URLENCODED;
        if(!content_size) {
            if(!strcmp(ct, GNn_HTTP_CONTENT_JSON) || !strcmp(ct, GNn_HTTP_CONTENT_URLENCODED)) content_size = strlen(content);
#ifdef GN_DEBUG
            else { GN_Log("cCLIENT_HTTP 发出请求 有错! 提交附带数据类型不属于纯文本的必须制定数据大小!\n"); }
#endif
        }
        head_size = sprintf(head_msg, GNn__HttpHead__, method, filename, zHost.Begin());
        head_size += sprintf(head_msg + head_size, GNn_HttpHead__Content, ct, content_size);
    } else {
        head_size = sprintf(head_msg, GNn__HttpHead__, method, filename, zHost.Begin());
        int remaining = GN_MAX_BUF_HTTP_HEAD - head_size - 1;
        strncpy(head_msg + head_size, "Connection: Keep-Alive\r\n\r\n", remaining);
        head_msg[GN_MAX_BUF_HTTP_HEAD - 1] = 0;
        head_size += 26;
    }
    return head_size;
}
int GNn::cCLIENT_HTTP::zHeadMsgMake(const char *filename, P<sHTTP_POST> &content, int &_content_size, char **pp_content) {
    _content_size = 0;
    if(pp_content) *pp_content = 0;
    if(content) {
        unsigned int ci = 0, cn = content.Num();
        sHTTP_POST *v = content;
        //统计内容大小
        bool egp = GNf::sFILE::EnableGlobalPath;
        for(; ci < cn; ci++) {
            if(zTaskThread && zTaskThread->us_state == 254) { //结束处理通知
                _content_size = 0;
                break;
            }
            if(v[ci].name) _content_size += GT_HttpPostGetOccupancySize(v + ci);
#ifdef GN_DEBUG
            else { GN_Log("调用 cCLIENT_HTTP::RequestPost 或 SendRequestPost 时某些提交参数还未被正确设置!"); }
#endif
        }
        _content_size += 2 + strlen(GNn_HttpContentBoundary) + 4;

        if(pp_content) { //装填数据来待发送
            if(_content_size) {
                *pp_content = new char[_content_size + 4];
                char *w_pos = *pp_content;
                for(ci = 0; ci < cn; ci++) {
                    if(zTaskThread && zTaskThread->us_state == 254) break; //结束处理通知
                    if(v[ci].name) GT_HttpPostContentMake(v + ci, &w_pos);
                }
                sprintf(w_pos, "--%s--\r\n", GNn_HttpContentBoundary);
            }
        }
        GNf::sFILE::EnableGlobalPath = egp;
    }
    if(!zBufTemp) zBufTemp.New(GN_MAX_BUF_HTTP_HEAD);
    char *head_msg = zBufTemp;
    int head_size;
    char boundary[256];
    sprintf(boundary, "%s; boundary=%s", GNn_HTTP_CONTENT_MULTIPART, GNn_HttpContentBoundary);
    head_size = sprintf(head_msg, GNn__HttpHead__, "POST", filename, zHost.Begin());
    head_size += sprintf(head_msg + head_size, GNn_HttpHead__Content, boundary, _content_size);
    return head_size;
}
#define GT_TMP_CLIENT_HTTP_CBST 2339834857
P<char> GNn::cCLIENT_HTTP::Request(const char *filename, const char *method, const char *content_type, const char *content, int content_size, int *state_out, float *prog_out, bool connect_keep) {
#define GT_TMP_CLIENT_HTTP_BUSY_CHECK                                            \
    if(zResponseCall && (!state_out || *state_out != GT_TMP_CLIENT_HTTP_CBST)) { \
        if(state_out) *state_out = -1;                                           \
        return P<char>();                                                        \
    }
    GT_TMP_CLIENT_HTTP_BUSY_CHECK
    zHeadSize = zHeadMsgMake(filename, method, content_type, content, content_size);
    return RequestRaw(zBufTemp, &zHeadSize, content, content_size, state_out, prog_out, connect_keep);
}
P<char> GNn::cCLIENT_HTTP::RequestPost(const char *filename, P<sHTTP_POST> content, int *state_out, float *prog_out, bool connect_keep) {
    GT_TMP_CLIENT_HTTP_BUSY_CHECK
    char *_content = 0;
    int _content_size;
    zHeadSize = zHeadMsgMake(filename, content, _content_size, &_content);
    P<char> res = RequestRaw(zBufTemp, &zHeadSize, _content, _content_size, state_out, prog_out, connect_keep);
    if(_content) delete[] _content;
    return res;
}
P<char> GNn::cCLIENT_HTTP::RequestRaw(char *head_msg /*2048*/, int *head_size, const char *content, int content_size, int *state_out, float *prog_out, bool connect_keep) {
    GT_TMP_CLIENT_HTTP_SOC_CHECK
    GT_TMP_CLIENT_HTTP_BUSY_CHECK
    if(prog_out) *prog_out = 0.0f;
    P<char> conent;
    GNn::cSOC_TCP *_tcp = zSocTcp; //发送消息报头
    if(_tcp->Send__(head_msg, *head_size) != *head_size) {
        if(state_out) *state_out = -1;
        return conent;
    }
    if(prog_out) *prog_out = 0.02f;
    if(content) { //发送实体内容
        int size_last, send_size, size__ = 0, max_send_size = GN_USING_FILE_BUF >> 2; //一次最大发送512K
        while(size__ < content_size) {
            send_size = content_size - size__;
            if(send_size > max_send_size) send_size = max_send_size;
            size_last = _tcp->Send(content + size__, send_size);
            if(size_last > 0) {
                size__ += size_last;
                if(prog_out) *prog_out += 0.7f * ((float)size_last / (float)content_size);
            } else if(size_last < 0) {
                if(state_out) *state_out = -1;
                return conent;
            }
        }
    }
#define GT_TMP_HTTP_HAS_CONTENT_RECV(HEAD_BUF) (!strncmp(HEAD_BUF, "GET ", 4) || !strncmp(HEAD_BUF, "POST ", 5))
    bool connect_close, content_recv = GT_TMP_HTTP_HAS_CONTENT_RECV(head_msg) ? true : false;
    int state;
    float prog;
    conent = GNn_HttpResponseRecv(_tcp, head_msg, *head_size, state, prog_out ? (*prog_out) : prog, connect_close, content_recv, zTimeOut);
    if(state_out) *state_out = state;
    if(connect_close && connect_keep && !(zTaskThread && zTaskThread->us_state == 254)) {
#define GT_TMP_CLIENT_HTTP_RE_CONNECT(_ADD_PROC)                             \
    _tcp->Close();                                                           \
    if(_tcp->Open() == -1 || _tcp->Connect(zAddr) == -1) {                   \
        GNn_ErrorLast("CLIENT_HTTP 的连接被丢失, 尝试重新连接遇到错误!", 0); \
        _ADD_PROC;                                                           \
    }

        GT_TMP_CLIENT_HTTP_RE_CONNECT(;)
    }
    return conent;
}
void GT_ClientHttpRequest(P<GN_sTHREAD> this_thread, void *arg, void *arg2) {
    GNn::cCLIENT_HTTP *h_client = (GNn::cCLIENT_HTTP *)arg;
    P<char> res;
    int state = GT_TMP_CLIENT_HTTP_CBST;
    switch(h_client->zCallBackType) {
        case 2: {
            char **args = (char **)(h_client->zBufTemp.Begin());
            res = h_client->Request(args[0], args[1], args[2], h_client->zContent, h_client->zContentSize, &state, h_client->zProgress, h_client->zConnectKeep);
        } break;
        case 1:
            res = h_client->RequestPost(h_client->zContent, h_client->zContents, &state, h_client->zProgress, h_client->zConnectKeep);
            break;
        case 0:
            res = h_client->RequestRaw(h_client->zBufTemp, &(h_client->zHeadSize), h_client->zContent, h_client->zContentSize, &state, h_client->zProgress, h_client->zConnectKeep);
            break;
    }
    if(this_thread->us_state < 254) {
        int remaining = GN_MAX_BUF_HTTP_HEAD - h_client->zHeadSize - 1;
        strncpy(h_client->zBufTemp.Begin() + h_client->zHeadSize, "\r\n\r\n", remaining);
        h_client->zBufTemp.Begin()[GN_MAX_BUF_HTTP_HEAD - 1] = 0;
        h_client->zHeadSize += 4;
        h_client->zResponseCall(res, state, h_client->zBufTemp, h_client->zHeadSize, h_client->zUsArg);
    }
    h_client->zContents.Delete();
    h_client->zResponseCall = 0;
    this_thread->us_state = 255;
}
bool GNn::cCLIENT_HTTP::SendRequest(GNn_HTTP_CALLBACK proc, void *us_arg, const char *filename, const char *method, const char *content_type, const char *content, int content_size, float *prog_out, bool connect_keep) {
#define GT_TMP_CLIENT_HTTP_RES_CALL_SET \
    if(zResponseCall) return false;     \
    zResponseCall = proc;               \
    zUsArg = us_arg;                    \
    zProgress = prog_out;               \
    zConnectKeep = connect_keep;        \
    zContents.Delete();                 \
    if(!zBufTemp) zBufTemp.New(GN_MAX_BUF_HTTP_HEAD)

    GT_TMP_CLIENT_HTTP_RES_CALL_SET;
    zContent = (char *)content;
    zContentSize = content_size;
    zCallBackType = 2;
    if(GN_ThreadSupport) {
        char **args = (char **)zBufTemp.Begin();
        args[0] = (char *)filename;
        args[1] = (char *)method;
        args[2] = (char *)content_type;
        zTaskThread = GN_ThreadStart(GT_ClientHttpRequest, this);
    } else {
        zHeadSize = zHeadMsgMake(filename, method, content_type, content, zContentSize);
        zUpdateTaskAdd();
    }
    return true;
}
bool GNn::cCLIENT_HTTP::SendRequestPost(GNn_HTTP_CALLBACK proc, void *us_arg, const char *filename, P<sHTTP_POST> content, float *prog_out, bool connect_keep) {
    GT_TMP_CLIENT_HTTP_RES_CALL_SET;
    //	zContent      = 0;
    //  zContentSize  = 0;
    zContents = content;
    zCallBackType = 1;
    if(GN_ThreadSupport) {
        zContent = (char *)filename; //临时作为文件名的传递
        zTaskThread = GN_ThreadStart(GT_ClientHttpRequest, this);
    } else {
        zHeadSize = zHeadMsgMake(filename, zContents, zContents_Size, 0);
        zUpdateTaskAdd();
    }
    return true;
}
bool GNn::cCLIENT_HTTP::SendRequestRaw(GNn_HTTP_CALLBACK proc, void *us_arg, const char *head_msg /*2048*/, int head_size, const char *content, int content_size, float *prog_out, bool connect_keep) {
    GT_TMP_CLIENT_HTTP_RES_CALL_SET;
    strncpy(zBufTemp, head_msg, head_size);
    if(head_size < GN_MAX_BUF_HTTP_HEAD) zBufTemp[head_size] = 0;
    zHeadSize = head_size;
    zContent = (char *)content;
    zContentSize = content_size;
    zCallBackType = 0;
    if(GN_ThreadSupport) {
        zTaskThread = GN_ThreadStart(GT_ClientHttpRequest, this);
    } else zUpdateTaskAdd();
    return true;
}
//更新处理超时检测
#define GT_TMP_CLIENT_HTTP_TIMEOUT_CHECK                        \
    else if(zTimeOut != 0xffffffff && zTaskTime >= zTimeOut) {  \
        P<char> str_tmp;                                        \
        zResponseCall(str_tmp, 0, 0, 0, zUsArg); /*超时的回应*/ \
        zTaskState = 0;                                         \
    }

//完成调用回应回调函数
#define GT_TMP_CLIENT_HTTP_RESPONSE_CALL(STATE)                                 \
    if(zProgress) *zProgress = 1.0f;                                            \
    strncpy(buf + zHeadSize, "\r\n\r\n", GN_MAX_BUF_HTTP_HEAD - zHeadSize - 1); \
    buf[GN_MAX_BUF_HTTP_HEAD - 1] = 0;                                          \
    zHeadSize += 4;                                                             \
    zResponseCall(zResContent, STATE, buf, zHeadSize, zUsArg);                  \
    zTaskState = 0;

void GNn::cCLIENT_HTTP::zUpdate(unsigned int nTime) {
    zTaskTime += nTime;
    static int max_send_size = GN_USING_FILE_BUF >> 2; //一次最大发送512K
    int size_last, send_size;
    GNn::cSOC_TCP *_tcp = zSocTcp;
    char *buf = zBufTemp;
    switch(zTaskState) {
        case 1: //发送http报头
            size_last = _tcp->Send(buf + zRvSize, zHeadSize - zRvSize);
            if(size_last > 0) {
                zTaskTime = 0;
                zRvSize += size_last;
                if(zRvSize >= zHeadSize) { //http报头发送完毕
                    if(zProgress) *zProgress = 0.02f;
                    if(zCallBackType == 1) {
                        if(zContents) {
                            zResContent.New(zContents_Size); //用这个来作提交参数的临时发送缓冲
                            zContent = zResContent; //定位到当前发送起始地址
                            zContentSize = 0; //用这个来记下当前发送的参数大小
                            zHeadSize = 0; //用这个来记下当前发送到第几参数
                            zTaskState = 8;
                        } else zHeadRecvBeforeSet();
                    } else {
                        if(zContent) {
                            zRvSize = 0;
                            zTaskState = 2;
                        } else zHeadRecvBeforeSet();
                    }
                }
            }
            GT_TMP_CLIENT_HTTP_TIMEOUT_CHECK
            break;
        case 2: //发送实体内容
            send_size = zContentSize - zRvSize;
            if(send_size > max_send_size) send_size = max_send_size;
            size_last = _tcp->Send(zContent + zRvSize, send_size);
            if(size_last > 0) {
                zTaskTime = 0;
                zRvSize += size_last;
                if(zProgress) *zProgress += 0.7f * ((float)size_last / (float)(zCallBackType == 1 ? zContents_Size : zContentSize));
                if(zRvSize >= zContentSize) { //http提交的内容发送完毕
                    if(zCallBackType == 1) {
                        if((unsigned int)zHeadSize < zContents.Num()) {
                            if(zContent == (char *)zResContent) { //还有附带文件要发送
                                sHTTP_POST *_v = &zContents[zHeadSize];
                                if(_v->value) {
                                    zContent = _v->value;
                                    zContentSize = _v->flags;
                                } else {
                                    zContent += zContentSize; //跳到后尾一些
                                    bool egp = GNf::sFILE::EnableGlobalPath;
                                    GNf::sFILE::EnableGlobalPath = _v->flags ? true : false;
                                    GNf::sFILE *fp = GNf::Open(_v->filename, "rb");
                                    GNf::sFILE::EnableGlobalPath = egp;
                                    if(fp) {
                                        zContentSize = GNf::GetSize(fp);
                                        GNf::Read(zContent, 1, zContentSize, fp);
                                        GNf::Close(fp);
                                    } else zContent = zResContent;
                                }
                            } else zContent = zResContent; //文件发送完
                            if(zContent == zResContent) { //添加尾换行
                                zContentSize = 0;
                                strncpy(&zContent[zContentSize], "\r\n", 2);
                                zContent[zContentSize + 2] = 0;
                                zContentSize += 2;
                                zHeadSize++; //http提交的其中1个参数内容发送完毕
                                if((unsigned int)zHeadSize < zContents.Num()) {
                                    zTaskState = 8; //继续查找剩下要发送的参数
                                    break;
                                } else zContentSize += sprintf(&zContent[zContentSize], "--%s--\r\n", GNn_HttpContentBoundary); //加入尾行
                            }
                            zRvSize = 0;
                            break;

                        } else zResContent.Delete(); //http提交的内容全部发送完毕
                    }
                    zHeadRecvBeforeSet();
                }
            }
            GT_TMP_CLIENT_HTTP_TIMEOUT_CHECK
            break;
        case 3: //接收http回应报头
            size_last = _tcp->Recv(buf + zRvSize, GN_MAX_BUF_HTTP_HEAD - zRvSize);
            if(size_last > 0) {
                zTaskTime = 0;
                zRvSize += size_last;
                int state;
                char res_str[GN_MAX_BUF_HTTP_HEAD];
                bool connect_close = GNn_HttpResponseAnalysis(buf, zRvSize, state, res_str, zContentSize, size_last);
                if(state > 0) { //已经收到完整的回应头
                    zHeadSize = zRvSize - (size_last + 4); //计算得到纯回应头的字段长度
                    if(connect_close && zConnectKeep) {
                        GT_TMP_CLIENT_HTTP_RE_CONNECT(GT_TMP_CLIENT_HTTP_RESPONSE_CALL(-1); break;)
                    }
                    if(!zContentRecv || !zContentSize) { /*不需要再继续收取回应实体内容了*/
                        GT_TMP_CLIENT_HTTP_RESPONSE_CALL(state)
                    } else {
                        if(zContentSize == -2) { //一次性回应的小数据
                            if(size_last) {
                                zResContent.New(res_str, size_last);
                                GT_TMP_CLIENT_HTTP_RESPONSE_CALL(state)
                            } else if(strstr(buf, "Content-Type: text")) { //还要等服务器答复
                                zTaskState = 5;
                            }
                        } else if(zContentSize == -1) { //小节分块数据
                            zResContent.New(GN_USING_FILE_BUF);
                            if(size_last) memcpy(zResContent.Begin(), res_str, size_last);
                            if(zProgress) *zProgress = 0.75f;
                            zContentSize = 0;
                            zRvSize = size_last;
                            zTaskState = 6;

                        } else /* if(zContentSize > 0)*/ { //大数据
                            zResContent.New(zContentSize);
                            if(size_last) memcpy(zResContent.Begin(), res_str, size_last);
                            if(size_last >= zContentSize) {
                                GT_TMP_CLIENT_HTTP_RESPONSE_CALL(state)
                            } else {
                                zRvSize = size_last;
                                zTaskState = 4;
                            }
                        }
                    }
                }
            }
            GT_TMP_CLIENT_HTTP_TIMEOUT_CHECK
            break;
        case 4: //接收http回应大实体内容
            size_last = _tcp->Recv(zResContent.Begin() + zRvSize, zContentSize - zRvSize);
            if(size_last > 0) {
                if(zProgress) *zProgress = 0.7f * ((float)zRvSize / (float)zContentSize) + 0.3f;
                zTaskTime = 0;
                zRvSize += size_last;
                if(zRvSize >= zContentSize) {
                    GT_TMP_CLIENT_HTTP_RESPONSE_CALL(atoi(GN_StrToKey(buf, ' ')))
                }
            }
            GT_TMP_CLIENT_HTTP_TIMEOUT_CHECK
            break;
        case 5: { //接收http回应小实体内容
            char content_[GN_MAX_BUF_HTTP_HEAD];
            size_last = _tcp->Recv(content_, GN_MAX_BUF_HTTP_HEAD);
            if(size_last > 0) {
                zResContent.New(content_, size_last);
                GT_TMP_CLIENT_HTTP_RESPONSE_CALL(atoi(GN_StrToKey(buf, ' ')))
            }
            GT_TMP_CLIENT_HTTP_TIMEOUT_CHECK
        } break;
        case 6: { //接收http回应连续的小节分块数据内容 !!这段和 GT_DTP_HttpChunkedDownload 近似
            int size___ = (int)zResContent.Size();
            if(zRvSize >= size___) {
                P<char> new_data(size___ << 1); //遇到不够空间再分配一下
                memcpy(new_data.Begin(), zResContent.Begin(), size___);
                zResContent = new_data;
                size___ <<= 1;
            }
            char *p_data = zResContent;
            while(zContentSize <= zRvSize - 5) {
                if(!strncmp(p_data + zContentSize, "0\r\n\r\n", 5)) { //找到结束点
                    zResContent = GNn_HttpGetContentFromChunked(p_data);
                    GT_TMP_CLIENT_HTTP_RESPONSE_CALL(atoi(GN_StrToKey(buf, ' ')))
                    return;
                } else zContentSize++;
            }
            size_last = _tcp->Recv(p_data + zRvSize, size___ - zRvSize);
            if(size_last > 0) {
                if(zProgress) *zProgress += 0.002f;
                zTaskTime = 0;
                zRvSize += size_last;
            }
            GT_TMP_CLIENT_HTTP_TIMEOUT_CHECK
        } break;
        case 8: { //发送多个不同类型的参数内容
            sHTTP_POST *v;
            for(; (unsigned int)zHeadSize < zContents.Num(); zHeadSize++) {
                v = &zContents[zHeadSize];
                if(v->filename) {
                    zContentSize += sprintf(&zContent[zContentSize], "--%s\r\n%s\"%s\"%s\"%s\"\r\n%s%s\r\n\r\n", GNn_HttpContentBoundary, GNn_HttpContentDisposition, v->name, GNn_HttpContentFilename, v->filename, GNn_HttpContentType, GNn_HTTP_CONTENT_OCTET);
                    break;
                } else zContentSize += sprintf(&zContent[zContentSize], "--%s\r\n%s\"%s\"\r\n\r\n%s\r\n", GNn_HttpContentBoundary, GNn_HttpContentDisposition, v->name, v->value);
                if((unsigned int)zHeadSize + 1 >= zContents.Num()) zContentSize += sprintf(&zContent[zContentSize], "--%s--\r\n", GNn_HttpContentBoundary); //加入尾行
            }
            zRvSize = 0;
            zTaskState = 2;
        } break;
    }
}
void GNn::cCLIENT_HTTP::zHeadRecvBeforeSet() {
    char *buf = zBufTemp;
    zContentRecv = GT_TMP_HTTP_HAS_CONTENT_RECV(buf) ? true : false;
    zRvSize = 0;
    buf[0] = 0;
    zHeadSize = 0;
    zTaskState = 3;
}
void GNn::cCLIENT_HTTP::zTaskDataClear() {
    zContents.Delete();
    zResContent.Delete();
    zResponseCall = 0;
    zSocTcp->SetWait(true);
}
void GNn::cCLIENT_HTTP::zUpdateTaskAdd() {
    GT_TMP_CLIENT_HTTP_SOC_CHECK
    zDelID = zUpList.add(this);
    zResContent.Delete();
    if(zProgress) *zProgress = 0.0f;
    zRvSize = 0;
    zTaskTime = 0;
    zTaskState = 1;
    zSocTcp->SetWait(false);
}
GN::sLIST<GNn::cCLIENT_HTTP *> GNn::cCLIENT_HTTP::zUpList;
void GNn::cCLIENT_HTTP::zUpdateAll(unsigned int nTime) {
    if(zUpList.size()) {
        cCLIENT_HTTP *h_client;
        GN::sLIST<cCLIENT_HTTP *>::iterator itu = zUpList.begin(), etu = zUpList.end();
        for(; itu != etu;) {
            h_client = (*itu);
            h_client->zUpdate(nTime);
            if(!h_client->zTaskState) { //标记并删除该请求任务
                h_client->zTaskDataClear();
                h_client->zDelID = 0;
                itu = zUpList.erase(itu);
            } else ++itu;
        }
    }
}
NET_API uint8 GNn::ImageDownloadAndLoad(cCLIENT_HTTP *clt_http, const char *filename, GNg::sIMAGE *img, GNg::COLOR_FORMAT texformat, bool us_mipmap, unsigned char linearlv) {
    if(GNg::ImageLoad((char *)filename, 1, img)) return 1;
    else {
        P<char> file_date = clt_http->Request(filename, "GET", GNn_HTTP_CONTENT_URLENCODED, 0, 0, 0, 0, false);
        if(file_date) {
            GNf::sFILE *fp = GNf::OpenCreate((char *)filename, "wb");
            if(fp) {
                GNf::Write(file_date.Begin(), 1, file_date.Size(), fp);
                GNf::Close(fp);
            }
            GNg::COLOR_FORMAT gfmt;
            int gw, gh;
            uint8 *logo_data = GNg::LoadPngFileData(file_date, &gfmt, &gw, &gh, false, file_date.Size());
            if(logo_data) {
                img->CreateEmptyToData(gw, gh, 1, gfmt);
                img->DataFill(logo_data, gfmt);
                delete[] logo_data;
                return 2;
            }
        }
    }
    return 0;
}
NET_API P<char> GNn::FileDownloadFromUrl(const char *url, const char *save_path, float *prog_out) {
#define GT_TMP_HTTP_URL_ANALYSIS                                    \
    P<char> file_date;                                              \
    unsigned short port;                                            \
    char *srcname, *filename;                                       \
    P<char> host = GNn_UrlAnalysis(url, &srcname, &filename, port); \
    if(!host) return file_date;                                     \
    cCLIENT_HTTP h_client;                                          \
    h_client.Init(host, 0xffffffff, port)

    GT_TMP_HTTP_URL_ANALYSIS;
    file_date = h_client.Request(srcname, "GET", GNn_HTTP_CONTENT_URLENCODED, 0, 0, 0, prog_out, false);
    if(save_path && file_date) {
        GNf::sFILE *fp = GNf::OpenCreate((char *)save_path, "wb");
        if(fp) {
            GNf::Write(file_date.Begin(), 1, file_date.Size(), fp);
            GNf::Close(fp);
        } else file_date.Delete();
    }
    return file_date;
}
NET_API P<char> GNn::FileUploadToUrl(const char *url, const char *file_path, int flags, const char *file_buf, float *prog_out) {
    GT_TMP_HTTP_URL_ANALYSIS;
    P<sHTTP_POST> post_data(1);
    if(file_buf) post_data->Init("file", (char *)file_path, (char *)file_buf, flags);
    else {
        if(GNf::Access(file_path, 0)) return file_date;
        post_data->Init("file", (char *)file_path, flags ? true : false);
    }
    file_date = h_client.RequestPost(srcname, post_data, 0, prog_out, false);
    return file_date;
}
/**************************************************** cSERVER_TCP ********************************************************/
bool GNn::cSERVER_TCP::Init(unsigned short port, int listen_max_num, unsigned int time_out) {
    zSocket.New(1);
    if(zSocket->Open() == -1) {
        GN_Log("cSERVER_TCP::Init 打开TCP 套接口失败!\n");
        zSocket.Delete();
        return false;
    }
    zAddr.Init(port, 0);
    if(zSocket->Bind(zAddr) == -1) {
        GN_Log("cSERVER_TCP::Init 绑定地址失败!\n");
        zSocket.Delete();
        return false;
    }
    if(zSocket->Listen(listen_max_num) == -1) {
        GN_Log("cSERVER_TCP::Init 聆听端口失败!\n");
        zSocket.Delete();
        return false;
    }
    zTimeOut = time_out;
    return true;
}
static void GT_ClientProc(P<GN_sTHREAD> this_thread, void *arg, void *arg2) {
    GNn::cSERVER_TCP *server = (GNn::cSERVER_TCP *)arg;
    GNn::sCLIENT_PORT *p_client_ = (GNn::sCLIENT_PORT *)arg2;
    p_client_->thread = this_thread;
    server->OnConnect(p_client_->socket, p_client_->addr, p_client_->us_ext);

    for(;;) {
        if(!server->OnMsgProc(p_client_->socket, p_client_->addr, p_client_->us_ext)) break;
        p_client_->time = (unsigned int)GN_TimeGetCount();
    }
    if(server->zRun) {
        while(server->zCListLock) GN_TimeSleep(1);
        server->zClientList.del(p_client_);
    }
}
static void GT_SERVER_TCP_UpdateRunSMT(P<GN_sTHREAD> this_thread, void *arg, void *arg2) { //这个对于每个连接都开一个新线程来作处理
    GNn::cSERVER_TCP *sv = (GNn::cSERVER_TCP *)arg;
    sv->zClientList.clear();
    GNn::sADDR ip_client;
    GN::sLIST<GNn::sCLIENT_PORT>::iterator itc, etc;
    unsigned int cur_run_time;
    for(;;) {
        P<GNn::cSOC_TCP> fd_client = sv->zSocket->Accept(ip_client); //接受一个连接
        if(fd_client) { //加入一个连接处理 并 创建一个新线程来处理它

#define GT_TMP_SERVER_TCP_CLOSE_SOCKET                                                       \
    if(!sv->zRun) break; /*这可是退出服务的连接消息*/                                        \
    if(sv->zTimeOut != 0xffffffff) {                                                         \
        cur_run_time = (unsigned int)GN_TimeGetCount();                                      \
        for(itc = sv->zClientList.begin(), etc = sv->zClientList.end(); itc != etc; ++itc) { \
            if(cur_run_time - itc->time > sv->zTimeOut) {                                    \
                itc->socket->Close(); /*超时关闭连接*/                                       \
                /*itc->us_ext.Delete();*/                                                    \
            }                                                                                \
        }                                                                                    \
    }

            GT_TMP_SERVER_TCP_CLOSE_SOCKET
            GNn::sCLIENT_PORT *p_del = sv->zClientList.add(GNn::sCLIENT_PORT(P_(), fd_client, ip_client, cur_run_time));
            if(!GN_ThreadStart(GT_ClientProc, arg, p_del)) {
                sv->zClientList.del(p_del);
                GN_Log("GT_SERVER_TCP_UpdateRunSMT 创建客户处理线程有错!\n");
            }
            fd_client.Delete();
        } else {
            GN_Log("GT_SERVER_TCP_UpdateRunSMT 同意连接时有错!\n");
        }
    }
}
static void GT_SERVER_TCP_UpdateRun(P<GN_sTHREAD> this_thread, void *arg, void *arg2) { //这个只开一个线程来处理,可能会好维护点,但有最大连接数限制
    GNn::cSERVER_TCP *sv = (GNn::cSERVER_TCP *)arg;
    sv->zClientList.clear();

    GN::sLIST<GNn::sCLIENT_PORT>::iterator itc, itc2, etc;
    GNn::sCLIENT_PORT *cur_client;
    GNn::cSOC_TCP *p_socket = sv->zSocket, *p_fd_client;
    GNn::sADDR ip_client;
    unsigned int cur_run_time;
    int max_fd_id = p_socket->fd + 1, last_max_fd_id;
    SOCKET set_fd = (SOCKET)p_socket->fd;
    fd_set readfds, readfds_bak;
    FD_ZERO(&readfds_bak);
    FD_SET(set_fd, &readfds_bak);
    for(;;) {
        readfds = readfds_bak;
        if(select(max_fd_id, &readfds, 0, 0, 0) == -1) {
            GN_Log("GT_SERVER_TCP_UpdateRun 中断信号有误!\n");
        } else {
            if(sv->zClientList.size()) { //检测是否其他TCP连接有消息
                cur_run_time = (unsigned int)GN_TimeGetCount();
                itc = sv->zClientList.begin(), etc = sv->zClientList.end();
                for(; itc != etc;) {
                    cur_client = &(*itc);
                    p_fd_client = cur_client->socket;
                    if(FD_ISSET(p_fd_client->fd, &readfds)) {
                        if(!sv->OnMsgProc(p_fd_client, cur_client->addr, cur_client->us_ext)) {
                            set_fd = (SOCKET)p_fd_client->fd;
                            FD_CLR(set_fd, &readfds_bak);
                            last_max_fd_id = p_fd_client->fd;
                            while(sv->zCListLock) GN_TimeSleep(1);
                            itc = sv->zClientList.erase(itc); //减少一个客户连接
                            if(last_max_fd_id + 1 >= max_fd_id) { //如果当前是最大的FD号 再从新去查找在FD集合中的其它最大值
                                max_fd_id = p_socket->fd;
                                for(itc2 = sv->zClientList.begin(); itc2 != etc; ++itc2) {
                                    if(itc2->socket->fd > max_fd_id) max_fd_id = itc2->socket->fd;
                                }
                                max_fd_id += 1;
                            }
                        } else {
                            cur_client->time = cur_run_time;
                            ++itc;
                        }
                        //break;
                    } else ++itc;
                }
            }
            if(FD_ISSET(p_socket->fd, &readfds)) { //检测是否有连接消息
                P<GNn::cSOC_TCP> fd_client = p_socket->Accept(ip_client); //接受一个连接
                if(fd_client) { //加入一个连接处理 并 创建一个新线程来处理它
                    GT_TMP_SERVER_TCP_CLOSE_SOCKET
                    if(sv->zClientList.size() + 1 > FD_SETSIZE) {
                        GN_Log("GT_SERVER_TCP_UpdateRun 处理连接已满 不能再连接\n");
                    } else {
                        sv->zClientList.push_back(GNn::sCLIENT_PORT(P_(), fd_client, ip_client, cur_run_time));
                        p_fd_client = fd_client;
                        if(p_fd_client->fd + 1 > max_fd_id) max_fd_id = p_fd_client->fd + 1;
                        set_fd = (SOCKET)p_fd_client->fd;
                        FD_SET(set_fd, &readfds_bak); //并加入客户连接到中断集合
                        sv->OnConnect(p_fd_client, ip_client, sv->zClientList.back().us_ext);
                    }
                    fd_client.Delete();
                } else {
                    GN_Log("GT_SERVER_TCP_UpdateRun 同意连接时有错!\n");
                }
            }
        }
    }
}
void GNn::cSERVER_TCP::StartUp(bool us_smt) {
#ifdef GN_DEBUG
    if(!zSocket) { GN_Log("cSERVER_TCP::StartUp 失败! 还未初始化!\n"); }
#endif
    zRun = true;
    zCListLock = false;
    if(us_smt) zThread = GN_ThreadStart(GT_SERVER_TCP_UpdateRunSMT, this);
    else zThread = GN_ThreadStart(GT_SERVER_TCP_UpdateRun, this);
}
void GNn::cSERVER_TCP::Shutdown() {
    if(!zRun) return;
    zRun = false;
    sADDR self_addr(zAddr.GetPort(), "127.0.0.1", true);
    P<GNn::cSOC_TCP> shutdown_sk(1);
    shutdown_sk->Open(); //摸以发出退出服务的连接消息
    if(shutdown_sk->Connect(self_addr) == -1) { GN_Log("cSERVER_TCP::Shutdown 失败!\n"); }

    GN::sLIST<GNn::sCLIENT_PORT>::iterator itc, etc = zClientList.end();
    for(itc = zClientList.begin(); itc != etc; ++itc) itc->socket->Close();
    for(itc = zClientList.begin(); itc != etc; ++itc) {
        if(itc->thread) GN_ThreadEndWait(itc->thread);
    }
    GN_ThreadEndWait(zThread);
    if(!zCListLock) zClientList.clear();
}
/**************************************************** cSERVER_UDP ********************************************************/
bool GNn::cSERVER_UDP::Init(unsigned short port) {
    zSocket.New(1);
    if(zSocket->Open() == -1) {
        GN_Log("cSERVER_UDP::Init 打开UDP 套接口失败!\n");
        zSocket.Delete();
        return false;
    }
    zAddr.Init(port, 0); //这个和TCP的类似,只是不用listen
    if(zSocket->Bind(zAddr) == -1) {
        GN_Log("cSERVER_UDP::Init 绑定地址失败!\n");
        zSocket.Delete();
        return false;
    }
    return true;
}
static void GT_SERVER_UDP_UpdateRun(P<GN_sTHREAD> this_thread, void *arg, void *arg2) {
    GNn::cSERVER_UDP *sv = (GNn::cSERVER_UDP *)arg;
    sv->zRun = true;
    int len;
    char buf[GN_MAX_BUF_UDP];
    for(;;) {
        GNn::sADDR client_ip;
        buf[0] = 0;
        len = sv->zSocket->RecvFrom(client_ip, buf, GN_MAX_BUF_UDP);
        if(!sv->zRun) break;
        if(len > 0) sv->OnRecv(client_ip, buf, len); //...执行自定义相关处理
    }
}
void GNn::cSERVER_UDP::StartUp() {
#ifdef GN_DEBUG
    if(!zSocket) { GN_Log("cSERVER_UDP::StartUp 失败! 还未初始化!\n"); }
#endif
    zThread = GN_ThreadStart(GT_SERVER_UDP_UpdateRun, this);
}
void GNn::cSERVER_UDP::Shutdown() {
    if(!zRun) return;
    zRun = false;
    sADDR self_addr(zAddr.GetPort(), "127.0.0.1", true);
    P<GNn::cSOC_UDP> shutdown_sk(1);
    shutdown_sk->Open(); //摸以发出退出服务的消息
    if(shutdown_sk->SendTo(self_addr, "server shutdown", 16) == -1) { GN_Log("cSERVER_UDP::Shutdown 失败!\n"); }
    GN_ThreadEndWait(zThread);
}
/***************************************************** 快捷外部通讯 ***************************************************************************/
unsigned short GT_ExternNoticePort = 0;
P<GNn::cSOC_UDP> GT_ExternNoticeSoc;
void GT_ExternNoticePost(const char *msg) {
    static GNn::sADDR en_addr(GT_ExternNoticePort, "127.0.0.1", true);
    if(!GT_ExternNoticeSoc) {
        GT_ExternNoticeSoc.New(1);
        GT_ExternNoticeSoc->Open();
    }
    if(GT_AppDefault_MsgCode) GT_ExternNoticeSoc->SendTo(en_addr, msg, strlen(msg) + 1);
    else {
        char wmsg[1024];
        GN_StrCharToWChar(msg, wmsg, 512);
        GT_ExternNoticeSoc->SendTo(en_addr, wmsg, (strlen(msg) + 1) << 1);
    }
}
NET_API GN_VOID_PFUNC_PCHAR GN_GetDefExternNoticeFunc(unsigned short port) {
    GT_ExternNoticePort = port;
    return GT_ExternNoticePost;
}
NET_API void (*GN_ExternNoticePost)(const char *msg) = 0;

/**********************************************************************************************************************************************/
NET_API void GN_InitNetwork() {
    if(GT_SystemInited & (1 << 4)) return;
    if(!GT_InitNetwork()) {
        GNn_ErrorLast("初始化网络模块失败!", 0);
    }
    sockaddr_in ip;
    GT_ADDR_LEN = sizeof(ip);
    GT_ADDR0_LEN = sizeof(ip.sin_zero);

    GT_SystemInited |= 1 << 4;
}
NET_API void GN_UnInitNetwork() {
    GT_ExternNoticeSoc.Delete();
    GT_UnInitNetwork();

    GNn_ErrorLast.Delete();
    GT_SystemInited -= 1 << 4;
}
NET_API void GN_NetworkProc(unsigned int nTime) {
    GNn::cCLIENT_HTTP::zUpdateAll(nTime);
}
#endif //_GN_NETTRANS_H_
