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
#ifdef _GN_NETBASE_H_
int GT_ADDR_LEN  = 0;
int GT_ADDR0_LEN = 0;
NET_API P<char>  GNn_ErrorLast;
NET_API const char GNn_DefWebFileName[] = "index.htm";

NET_API bool GNn_FuntionIsOK(){ return (GT_SystemInited & (1<<4))?true:false; }

NET_API unsigned int GNn_CheckIsIP(const char *ip_str, unsigned char ip4_out[4]){
    unsigned char IP4[4] = {0};
	char *p = (char*)ip_str;
	unsigned char *ip;
	if(ip4_out)ip = ip4_out;
	else       ip = IP4;

	char t[4]; //临时记录每一节的字符号用
	int v;
	unsigned int i ,j, dot_count = 0;
	for(j = 0 ; j < 4; j++){
	    i = 0;
		while(*p){
	        if(i > 3)return 0;  //超过3个字符都没遇'.'号就视它为有错

			if(*p == '.'){
				if(j == 3)return 0; //在最后一个IP值都带有'.'号有错
			    if(i == 0)return 0; //第一个字符就是'.'号就视它为有错
                t[i] = 0;
				v = atoi(t);
				if(v > 255)return 0; //值超出范围有错
                ip[j] = (unsigned char)v;
				p++;
				dot_count++;
				break;

			}else if(*p < 48 || *p > 57)return 0; //输入字符有些字不是数字,有错返回

			t[i] = *p;
		    i++; p++;
	    }
	}
	if(*(p-1) == '.' || dot_count < 3)return 0; //输入不完整有错
    t[i] = 0;
    v = atoi(t);
	if(v > 255)return 0; //值超出范围有错
    ip[3] = (unsigned char)v; //最后一节的
	return (ip[0]<<24)|(ip[1]<<16)|(ip[2]<<8)|(ip[3]);
}
NET_API void GNn_FindNetAddersRangeSegment(const char *text, GN::sLIST<GT_sSEGMENT_S> &range_segment_out){
    char *p_str = (char*)text;
	unsigned short net_start, net_end;
	while(*p_str){ //分析网地址位置
		if(*p_str == 'w'){
            p_str++;
			if(!strncmp(p_str, "ww", 2)){ //确定是网址 计算得网址的头尾字符位置
                net_start = (unsigned short)(p_str - text) - 1;
				if(net_start >= 7){
					if(!strncmp((p_str-8),"http://",7))net_start -= 7;
				}
                while(!isspace(*p_str))p_str++;
                net_end = (unsigned short)(p_str - text) - 1;
				range_segment_out.push_back(GT_sSEGMENT_S(net_start,net_end));
				if(!(*p_str))break;
			}
		}
		p_str++;
	}
}
NET_API P<char> GNn_UrlAnalysis(const char *url, char **src_out, char **filename_out,unsigned short &port_out){
	P<char> host, host2;
	port_out = 80;
	unsigned int h_len = 0;
    char *p_str = (char*)url, *p_str2, *fname, port_str[16];
	if(!strncmp(url,"http://",7))p_str += 7;

    fname = GN_StrToKey(p_str,'/');
	if(!fname)return host;
	h_len = fname - p_str;
	host2.New(h_len);
	h_len -= 1;
	strncpy(host2,p_str,h_len); host2[h_len] = 0;
	
	p_str2 = GN_StrToKey(host2,':');
	if(p_str2){
	    *(p_str2-1) = 0;
        host(host2, 0);
        port_out = (unsigned short)atoi(p_str2);
	}else host = host2;
	
    *filename_out = GN_StrGetFileName(fname);
    *src_out = fname;
	return host;
}
NET_API void GNn_UrlRemedy(char url_out[384], const char *url){
    char *out_pos = url_out, *p_str = (char*)url;
	strncpy(out_pos, "http://", 384 - 1); url_out[384 - 1] = 0; out_pos += 7;
	if(!strncmp(url,"http://",7)) p_str += 7;
	strncpy(out_pos, p_str, 384 - 7 - 1); url_out[384 - 1] = 0;
	p_str = strchr(out_pos,'/');
	if(p_str){
        p_str++;
		if(p_str[0]){
			p_str = GN_StrGetExtName(p_str);
			if(!p_str)sprintf(out_pos+strlen(out_pos), "/%s",GNn_DefWebFileName);
		}else{
			size_t remaining = 384 - strlen(url_out) - 1;
			strncat(out_pos, GNn_DefWebFileName, remaining);
			url_out[384 - 1] = 0;
		}
	}else sprintf(out_pos+strlen(out_pos), "/%s",GNn_DefWebFileName);
}
#endif //_GN_NETBASE_H_
