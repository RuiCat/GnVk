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

#include"GN.h"
#ifdef _GN_STRING_H_
GN_API char * GN_StrGetExtName(const char *file_path_name){
    char *pstr = (char*)(file_path_name + (strlen(file_path_name) - 1));
	char *pstr2 = (pstr - 6 > file_path_name)? (pstr - 6) : (char*)file_path_name;
	while(pstr > pstr2 && *pstr != '.')--pstr;
    ++pstr;
	return (((pstr - pstr2) <= 1) || !(*pstr) ? 0 : pstr);
}
GN_API char * GN_StrGetFileName(const char *file_path_name){
   #define GT_TMP_GET_LAST_DIRNAME \
    char *pstr = (char*)(file_path_name + (strlen(file_path_name) - 1));\
	while(pstr >= file_path_name && *pstr != '/' && *pstr != '\\')--pstr;

	GT_TMP_GET_LAST_DIRNAME
	return pstr + 1;
}
GN_API P< P<char> > GN_StrGetFileNames(P< P<char> > &file_path_names){
	unsigned int i = 0, n = file_path_names.Num();
	P< P<char> > fnames(n);
	P<char> *_fnames = fnames, *_fnames1 = file_path_names;
	for(;i < n; ++i)_fnames[i](GN_StrGetFileName(_fnames1[i]), 0);
	return fnames;
}
GN_API void GN_StrGetDirName(const char *file_path_name, char *dir_out){
	GT_TMP_GET_LAST_DIRNAME
	if(pstr <= file_path_name)*dir_out = 0;
	else{
       int len = (int)(pstr - file_path_name);
	   strncpy(dir_out, file_path_name, len);  dir_out[len] = 0; //复制出前段内容
	}
}
GN_API P<char> GN_StrGetDirName(const char *file_path_name){
    GT_TMP_GET_LAST_DIRNAME
	if(pstr <= file_path_name)return P<char>();
	else{
       int len = (int)(pstr - file_path_name);
	   P<char> dir(file_path_name, len + 1);   dir[len] = 0;	 //复制出前段内容
	   return dir;
	}
}
GN_API char * GN_StrRemoveFileName(char *file_path_name){
    GT_TMP_GET_LAST_DIRNAME
    if(pstr >= file_path_name)*pstr = 0;	 //如前面还有路径就断开文件名
	return pstr+1;
}
GN_API char * GN_StrRemoveFilePathAnExtName(char *file_path_name, bool remove_path){
	char *pstr = (char*)(file_path_name + (strlen(file_path_name) - 1));
	char *pstr_end = pstr;
	while(pstr >= file_path_name && *pstr != '.')--pstr;
	if(pstr >= file_path_name)*pstr = 0; //去掉扩展名
	else pstr = pstr_end;

	if(remove_path){
	    while(pstr >= file_path_name && *pstr != '/' && *pstr != '\\')--pstr; //排除路径找到斋文件名
        if(pstr >= file_path_name)*pstr = 0; //如前面还有路径就断开文件名
		return pstr+1;
	}
	return file_path_name;
}
GN_API int GN_StrGetVisualNum(const char *text,unsigned short text_len){
    int i = 0, ret = 0;
	if(text_len){
		while( text[i] ){
			if(text[i] & 128){ ++ret; ++i; } //汉字占多1字节
			else if(text[i] != '\n')++ret;
			++i;
            if(text_len && i >= text_len)break;
		}
	}else{
		while( text[i] ){
			if(text[i] & 128){ ++ret; ++i; } //汉字占多1字节
			else if(text[i] != '\n')++ret;
			++i;
		}
	}
	return ret;
}
GN_API char *GN_StrGetFirstChinaWord(const char *text){
    char *_word = (char*)text;
	while(*_word){
	    if((*_word) & 128)return _word;
        ++_word;
	}
	return 0;
}
GN_API void GN_StrRowAndColCompute(const char *text, int *row_out, int *col_out){
    char *pstr = (char*)text;
    int col = 0, row = 0, max_row = 0;
	while(*pstr){
		if(*pstr == '\n' || *pstr == '\r'){ 
			  if(row > max_row)max_row = row;
			  row = 0;
			  ++col;
		}else ++row;
	    ++pstr;
	}
	if(row > max_row)max_row = row;
    --pstr;
	while(pstr >= text && (*pstr == '\n' || *pstr == '\r')){ 
		--col;
		--pstr;
	}
    *row_out = max_row;
	*col_out = pstr >= text? col+1 : col;
}
GN_API char ** GN_StrSplitToData(unsigned int &str_num_out, const char *str,char split_key,char split_key2){
	//分割字符串( 分配字符串数组的方法 , 分配各个子字串空间并复制字符串的方法 )
   #define GT_TMP_STR_SPLIT( NEWCHARP , NEWSUBCHAR ) \
	unsigned int num = 0, size = 0;\
    char *_pos, *end_pos = (char*)str;\
	while(*end_pos == split_key || *end_pos == split_key2)++end_pos; /*排除开头分割符*/\
    _pos = end_pos;\
	while(*_pos){\
		if(*_pos == split_key || *_pos == split_key2)size = 1;\
		else if(size){ ++num; size = 0; } /*查找到分割符数*/\
        ++_pos;\
	}\
	NEWCHARP ;\
	num = 0;\
	_pos = end_pos;\
	while(*end_pos){\
		if(*end_pos == split_key || *end_pos == split_key2){ /*找到分割符*/\
			size = end_pos - _pos;\
			NEWSUBCHAR ;\
			strncpy(newstr[num], _pos, size);\
            newstr[num++][size] = 0;\
            ++end_pos;\
            while(*end_pos && (*end_pos == split_key || *end_pos == split_key2))++end_pos; /*移到分割符后*/\
            _pos = end_pos;\
            \
		}else ++end_pos;\
	}\
	if(_pos != end_pos){\
	    size = end_pos - _pos;\
		NEWSUBCHAR ;\
		strncpy(newstr[num], _pos, size);\
        newstr[num][size] = 0;\
	}\
	return newstr

	GT_TMP_STR_SPLIT( char **newstr=new char*[num+1];str_num_out=num+1 , newstr[num]=new char[size+1] );
}
GN_API P< P<char> > GN_StrSplitToData(const char *str,char split_key,char split_key2){
    GT_TMP_STR_SPLIT( P< P<char> > newstr(num+1) , newstr[num].New(size+1) );
}
GN_API P<char*>	GN_StrSplit(char *pstr, char split_key){
	int num = GN_StrGetCharNum(pstr, split_key) + 1;
	P<char*> strs(num);
	char **_strs = strs;
	_strs[0] = pstr;
	num = 1;
	while(*pstr){
		if(*pstr == split_key){
			*pstr = 0;
			++pstr;
			_strs[num++] = pstr;
		}else ++pstr;
	}
	return strs;
}
GN_API char *GN_StrUnSplitToData(const char **strs, unsigned int strn, char split_key){
	unsigned int i = 1, len = strlen(strs[0]);
	for(; i < strn; ++i)len += 1 + strlen(strs[i]);
	char *str_out = new char[len+1];
	GN_StrUnSplit(str_out, strs, strn, split_key);
	return str_out;
}
GN_API unsigned int GN_StrUnSplit(char *str_out, const char **strs, unsigned int strn, char split_key){
   #define GT_TMP_STR_UNSPLIT \
	char *pstr = str_out;\
	/* TODO: verify buffer size */\
	strcpy(pstr, strs[0]);\
	pstr += strlen(pstr);\
	unsigned int i = 1;\
	for(; i < strn; ++i){\
		*pstr = split_key; ++pstr;\
		/* TODO: verify buffer size */\
		strcpy(pstr, strs[i]);\
		pstr += strlen(pstr);\
	}\
	return pstr - str_out;

	GT_TMP_STR_UNSPLIT
}
GN_API unsigned int GN_StrUnSplit(char *str_out, const P< P<char> > &str_s, char split_key){
	const P<char> *strs = str_s;
	unsigned int strn = ((P< P<char> > &)str_s).Num();
	GT_TMP_STR_UNSPLIT
}
GN_API unsigned int GN_StrGetArray(uint8 *p_data, unsigned int em_size, const char *str , const char *fmt , char split_key, uint8 *p_def){
	// -12.6418304443359, -12.6139526367188, 0, 12.6418304443359, -12.6139526367188, 0, -12.6418304443359, 12.6139
	/*
	bool is_64 = (!strcmp(fmt, "%l") && sizeof(time_t) == em_size);
	time_t t_t;
	
	#define GT_TMP_STR_TO_VALUE( ELSE ) \
		if(is_64){\
			if(pstr2 < pstr){\
				t_t = GN_StrAtoI64(pstr2);\
				memcpy(p_data + (di * em_size), &t_t,  em_size); ++num;\
			}else if(p_def){\
				memcpy(p_data + (di * em_size), p_def, em_size); ++num;\
			}\
		} ELSE {\
		    if(pstr2 < pstr && sscanf(pstr2, fmt , p_data + (di * em_size)) > 0)++num;\
			else if(p_def){ memcpy(p_data + (di * em_size), p_def, em_size); ++num; }\
		}
	*/
	unsigned int di = 0, num = 0;
    char *pstr = (char*)str;
	char *pstr2 = pstr;
	for(;; ++pstr){
		if(*pstr == split_key){
            *pstr = 0;
           #define GT_TMP_STR_TO_VALUE \
			if(pstr2 < pstr && sscanf(pstr2, fmt , p_data + (di * em_size)) > 0)++num;\
		    else if(p_def){ memcpy(p_data + (di * em_size), p_def, em_size); ++num; }

			GT_TMP_STR_TO_VALUE
		  //GT_TMP_STR_TO_VALUE( else )
			++di;
            pstr2 = pstr + 1;
            *pstr = split_key;
		}else if(*pstr == 0)break;
	}
    //GT_TMP_STR_TO_VALUE( else if(em_size != 2) )
	if(em_size != 2){ 
		GT_TMP_STR_TO_VALUE
	}else{
        int end_v;
		if(pstr2 < pstr && sscanf(pstr2, fmt , &end_v) > 0){
			memcpy(p_data + (di * em_size), &end_v, em_size); ++num;
		}else if(p_def){
			memcpy(p_data + (di * em_size), p_def,  em_size); ++num;
		}
	}
    return num;
}
GN_API time_t GN_StrAtoI64(const char *str){
    char *pstr = (char*)str, *pstr2;
	for( ; *pstr <= 32; ++pstr){
	    if(!(*pstr))return 0;
	}
	char tmp[12];
	time_t t1 = 0;
	pstr2 = pstr;
    while(*pstr2 >= '0' && *pstr2 <= '9')++pstr2; //统计数字字符个数
	if(pstr2 - pstr > 9){						  //前有9位数字以上
		int hlen = pstr2 - pstr - 9;
		if(hlen > 11){ hlen = 11; pstr = pstr2 - 20; }
		strncpy(tmp, pstr, hlen); tmp[hlen] = 0;
		t1 = ((time_t)atol(tmp) * 1000000000) + (time_t)atol(pstr2 - 9);
	}else if(pstr2 > pstr)t1 = (time_t)atol(pstr);
	return t1;
}
GN_API void GN_StrFindCurLevelContentData(GN::sLIST<char*> &cnts_out,const char *text, char key, char key2, const char *ign_tag){
	/* 类如在以下字串中找出 V, Y, M, D 的位置指针
	CreationTimeStamp:  {
		Version: 1000
		Year: 2014
		Month: 11
		Day: 20
	}
	*/
	bool is_set = true, is_cnt = true;
	int sn = 0, tag_len = ign_tag ? strlen(ign_tag) : 0;
	char *st = (char*)text;
	for(;;){
		switch(*st){
        case '\n': if(is_cnt)is_set = true; break;
		case  0  : return;
		case '\"': is_cnt = !is_cnt;
		default  :
			if     (*st == key ){ 
				if(is_cnt)  ++sn; 

			}else if(*st == key2){
				if(is_cnt){ --sn; 
					if(sn < 0)return; 
				} 
			}else if(is_set && !sn && *st > 32  && (st == text || *(st-1) <= 32)){
				is_set = false;
				if(!ign_tag || strncmp(st, ign_tag, tag_len))cnts_out.push_back(st);
			}
			break;
		}
		++st;
	}
}
GN_API char **GN_StrFindCurLevelContentData(int &cn_out,const char *text, char key, char key2, const char *ign_tag){
    GN::sLIST<char*> cnts;
    GN_StrFindCurLevelContentData(cnts, text, key, key2, ign_tag);
	cn_out = (int)cnts.size();
	if(cn_out)return cnts.to_array();
	return 0;
}
GN_API P<char*> GN_StrFindCurLevelContentData(const char *text, char key, char key2, const char *ign_tag){
    GN::sLIST<char*> cnts;
    GN_StrFindCurLevelContentData(cnts, text, key, key2, ign_tag);
	return P<char*>(cnts);
}
GN_API int GN_StrExtract(char *sub_str_out[], char *str, char key, char key2){
    /*
    *{工号}  {985} {工资构成}  {} {} {} {绩效奖励项}  //说明...
    */
	//例如在以上字串里找出 工号, 985, 工资构成 等的位置指针
	bool in = false;
	int sn = 0;
	char *pstr = str;
	while(*pstr){
		if      (*pstr == key){
			if(in)return 0;
			else{ in = true;
		        sub_str_out[sn] = pstr + 1;
			}
		}else if(*pstr == key2){
			if(in){ in = false;
			    *pstr = 0; ++sn;
			
			}else return 0;
		}
	    ++pstr;
	}
	return in? 0 :sn;
}
GN_API P< P<char> > GN_StrExtractToData(const char *str, char key, char key2){
    P< P<char> > sub_strs;
	bool in = false;
	int sn = 0;
	char *pstr = (char*)str;
	while(*pstr){ //统计括号内有效的子字符串数
		if      (*pstr == key){
			if(in)return sub_strs;
			else  in = true;
		}else if(*pstr == key2){
			if(in){ in = false;
				++sn;
			}else return sub_strs;
		}
	    ++pstr;
	}
    if(in)return sub_strs;
	char *st;
	sub_strs.New(sn);
	sn = 0;
	pstr = (char*)str;
    while(*pstr){ //把括号里的内容复制到新字符串里
		if      (*pstr == key){
			st = pstr + 1;
		}else if(*pstr == key){
			*pstr = 0;
            (sub_strs[sn])(st, 0);
			++sn;
		}
	    ++pstr;
	}
	return sub_strs;
}
GN_API void GN_StrExtractFromMultiItem(char str_out[], char *str, int id, char key){
	int i = 0;
	char *pstr = str;
	char *pstr2= pstr;
	while(i < id){
		while(*pstr2 && *pstr2 != key)++pstr2;
		if(*pstr2){
			++pstr2;
			pstr = pstr2;
		}else{
		// TODO: verify buffer size
			strcpy(str_out, pstr);
			return;
		}
		++i;
	}
	while(*pstr2 && *pstr2 != key)++pstr2;
	int len = pstr2 - pstr;
	strncpy(str_out, pstr, len);
	str_out[len] = 0;
}
GN_API bool GN_StrHasName(const char *name,const char *str,char split_key){
    char *pstr = (char*)str, *_name;
	for(;;){
		_name = (char*)name;
		while(*_name == *pstr){    //比较字符段是否和目标名相等
		    ++_name; ++pstr;  
            if(!_name[0])return 1;
		}
		while(*pstr != split_key){ //把当前比较检索点移到下一字段
			if(*pstr)++pstr; 
			else return 0; 
		} 
        ++pstr; 
	}
}
GN_API char *const GN_StrHasPathDisableSymbol(const char *str){
	static const char *pathDisableStr = "\\ / : * ? \" < > |";
	char *pstr = (char*)str;
	while(*pstr){
		if(*pstr == '\\' || *pstr == '/' || *pstr == ':' || *pstr == '*' || *pstr == '?' || *pstr == '\"' || *pstr == '<' || *pstr == '>' || *pstr == '|')return (char *const)pathDisableStr;
	    ++pstr;
	}
    return 0;
}
GN_API char* GN_StrToKey(const char *str,char key){
	char *pstr = (char*)str;
	while(*pstr && *pstr != key)++pstr;
	return (*pstr) ? (pstr + 1) : (char*)0;
}
GN_API int GN_StrFilter(char *str_out, const char *str, char key){
    int fn = 0, sn = 0;
	char *pstr = (char*)str;

   #define GT_TMP_FILTER_STR \
	while(*pstr){\
		if(*pstr == key)++fn;\
		else str_out[sn++] = *pstr;\
		++pstr;\
	}\
	str_out[sn] = 0;

    GT_TMP_FILTER_STR
	return fn;
}
GN_API P<char> GN_StrFilter(const char *str, char key){
   #define GT_TMP_FILTER_STR2( NEWCHAR ) \
    int fn = 0, sn = 0;\
	char *pstr = (char*)str;\
	while(*pstr && *pstr != key){ ++pstr; ++sn; }\
	NEWCHAR ;\
	pstr = (char*)str;\
    sn = 0;\
	GT_TMP_FILTER_STR 

	GT_TMP_FILTER_STR2( P<char> str2(sn+1); char *str_out=str2 )
	return str2;
}
GN_API char *GN_StrFilter(int &f_count_out, const char *str, char key){
	GT_TMP_FILTER_STR2( char *str_out = new char[sn+1] )
    f_count_out = fn;
	return str_out;
}
GN_API void GN_StrReplace(char *str, char key, char rep, int len){
	if(len){
		int i = 0;
		for(; i < len; ++i)if(str[i] == key)str[i] = rep;
	}else{
		while(*str){
			if(*str == key)*str = rep;
			++str;
		}
	}
}
GN_API char* GN_StrCutSpaceFL(char *str){
	if(!str || !*str) return str;
	char *pstr = str;
    while(isspace(*pstr ))++pstr; //跳过头空段
	if(!*pstr){ *str = 0; return str; }
	char *pstr2 = pstr + (strlen(pstr) - 1);
	while(isspace(*pstr2))--pstr2;
    pstr2[1] = 0; //断开尾空段
    return pstr;
}
GN_API bool GN_StrcmpSN(const char *str1, char **str2){
    bool ret = true;
	char *pstr1 = (char*)str1, *pstr2 = *str2;
	while(*pstr1){
		if(*pstr1 == *pstr2){ ++pstr1; ++pstr2; }
		else { ret = false; break; }
	}
	while(!isspace(*pstr2)){	   //跳过剩余可视字符
		if(*pstr2)++pstr2;
		else{
		    *str2 = pstr2;
			return ret;
		}
	}
	while(isspace(*pstr2))++pstr2; //跳过空段把字符串2的指针移到下一字串处
	*str2 = pstr2;
	return ret;
}
GN_API bool GN_StrcmpEX(const char *str1,const char *str2){
	char *pstr1 = (char*)str1, *pstr2 = (char*)str2;
	while(*pstr1 <= 32)++pstr1; //跳过前面空格
    while(*pstr2 <= 32)++pstr2;
	while(*pstr1 > 32){
		if(*pstr1 == *pstr2){ ++pstr1; ++pstr2; }
		else return false;
	}
	return (*pstr2) <= 32;
}
GN_API bool GN_StrcmpPath(const char *str1, const char *str2){
    char *pstr1 = (char*)str1, *pstr2 = (char*)str2;
	while(*pstr1){
		if(*pstr1 != *pstr2 && !((*pstr1 == '/' || *pstr1 == '\\') && (*pstr2 == '/' || *pstr2 == '\\')))return false;
	    ++pstr1; ++pstr2;
	}
	return !(*pstr2);
}
GN_API bool GN_StrIsGlobalPath(const char *path){
	return (path[0] == '/' || path[0] == '\\' || path[1] == ':');
}
GN_API P<char> GN_StrGet(const char *str, char end_key){
	P<char> sub_str;
	char *_str = (char*)str, *pstr = (char*)str;
	if(end_key){
		while(*pstr && *pstr != end_key)++pstr;
	}else{
		while(*_str && *_str <= 32	   )++_str;
        pstr = _str;
        while(*pstr > 32)++pstr;
	}
    unsigned int len = pstr - _str;
	if(len){
        sub_str.New(len + 1);
        strncpy(sub_str, _str, len);
        sub_str[len] = 0;
	}
	return sub_str;
}
GN_API inline bool GN_StrGetSegment(const char *str,char start_key,char end_key,char **p_start_out,char **p_end_out){
	int sn = 0;
    char *st = 0, *end = 0;
	char *p = (char*)str;
	while(*p){
	   if	   (*p == start_key){
		   if(sn <= 0){ sn = 1; st = p; }
		   else ++sn;
	   }else if(*p ==   end_key){
		   --sn;
		   if(sn == 0){ end = p; break; }
	   }
	   ++p;
	}
	if(st && end){ //首尾符都搜到
	    ++st;
		while(isspace(*st))++st;   //跳过前段空字符
	    --end;
		while(isspace(*end))--end; //跳过尾段空字符
		*p_start_out = st; *p_end_out = end + 1;
		return true;
	}
	*p_start_out = st; *p_end_out = end;
	return false;
}
GN_API inline char* GN_StrCopySegmentToData(const char *str,char start_key,char end_key,int *size_out){
    char *st, *end;
	if(GN_StrGetSegment(str, start_key, end_key, &st, &end)){
	    int size = end - st;
		if(size){
		    char *seg = new char[size + 1];
            memcpy(seg, st, size);
            seg[size] = 0;
            *size_out = size;
			return seg;
	}	}
    *size_out = 0;
	return 0;
}
GN_API int GN_StrRemoveSegment(char *str_in_out,char split_key,const char *inc_str, int sn, bool ck_first){
	if(*str_in_out == split_key)++str_in_out;
	char *pstr = str_in_out, *pos0 = 0, *pos1 = 0;
	int len = strlen(inc_str), cpy_len, dn = 0;
	if(ck_first){
		for(;;){
			if(!strncmp(pstr, inc_str, len)){
                if(!pos0)pos0 = pstr;
				else if(pos1){
					cpy_len = pstr - pos1;
				    strncpy(pos0, pos1, cpy_len);
                    pos1 = 0;
                    pos0 += cpy_len;
				}
			    pstr += len;
				++dn;
				while(*pstr && *pstr != split_key)++pstr; if(!(*pstr)){ *pos0 = 0; break; }
			    while(*pstr && *pstr == split_key)++pstr; if(!(*pstr)){ *pos0 = 0; break; }
			// TODO: verify buffer size
			if(dn >= sn){ strcpy(pos0, pstr); break; }

		}else if(pos0){ //找到非尾段的一段要删除
                if(!pos1)pos1 = pstr;
				while(*pstr && *pstr != split_key)++pstr; if(!(*pstr)){ /* TODO: verify buffer size */ strcpy(pos0, pos1); break; }
				while(*pstr && *pstr == split_key)++pstr; if(!(*pstr)){ /* TODO: verify buffer size */ strcpy(pos0, pos1); break; }
			}else{
			    while(*pstr && *pstr != split_key)++pstr; if(!(*pstr))break;
			    while(*pstr && *pstr == split_key)++pstr; if(!(*pstr))break;
			}
		}
	}else{
		for(;;){
		    pstr = strstr(pstr, inc_str);
			if(pstr){
                char *hsgm = pstr;
                while(hsgm > str_in_out && *hsgm != split_key)--hsgm;
				if(!pos0)pos0 = hsgm;
				else{
					cpy_len = hsgm - pos1;
					if(cpy_len){
						strncpy(pos0, pos1, cpy_len);
                        pos0 += cpy_len;
					}
				}
			    pstr += len;
				++dn;
				while(*pstr && *pstr != split_key)++pstr; if(!(*pstr)){ *pos0 = 0; break; }
			    while(*pstr && *pstr == split_key)++pstr; if(!(*pstr)){ *pos0 = 0; break; }
				// TODO: verify buffer size
			if(dn >= sn){ strcpy(pos0, pstr); break; }
                pos1 = pstr;

			}else{ //检索结束
			// TODO: verify buffer size
			    if(pos0)strcpy(pos0, pos1);
				break;
			}
		}
	}
	return (*str_in_out)? dn : -1;
}
GN_API void GN_StrInsertSegment(char **ppstr, const char *sub_str, short inpos, char seg_tag){
	char *pstr = *ppstr;
	short sub_len = strlen(sub_str);
	if(inpos < 0){
		char *in_str = strchr(pstr, seg_tag);
		if(in_str){
			char *in_str1 = in_str + 1;
			while(*in_str1 == seg_tag)++in_str1;
			int st = (in_str1 - in_str) - sub_len;
			if(st>0)strncpy(in_str+(inpos==-1?0:(inpos==-2?st:(st>>1))), sub_str, sub_len); //空段有余
			else	strncpy(in_str, sub_str-st, sub_len+st);							//空段刚好或不够
		}
	}else{
		char *pstr2 = new char[inpos+sub_len+strlen(pstr+inpos)+1];
		// TODO: verify buffer size
		strncpy(pstr2, pstr, inpos); strncpy(pstr2+inpos, sub_str, sub_len); strcpy(pstr2+inpos+sub_len, pstr+inpos);
		delete [] pstr;
		*ppstr = pstr2;
	}
}
GN_API void GN_StrBinaryToTextData(char **mem_in_out, int *len_in_out){
    char *mem = *mem_in_out;
	int li = 0, li2 = 0, len = *len_in_out;
	for(; li2 < len && mem[li2]; ++li2){ //去掉'\r\n'里面的'\r'
		if(mem[li2] != '\r'){ mem[li++] = mem[li2]; }
	}
    *len_in_out = li;
	if(li < len)mem[li] = 0;
	else{
	    char *mem2 = new char[len + 1];
		memcpy(mem2, mem, len);
        mem2[len] = 0;
        delete [] (*mem_in_out);
        *mem_in_out = mem2;
	}
}
GN_API unsigned int GN_StrB4I64Convert(char *str_out, uint8 *p_data, unsigned int data_size){
	if(str_out && p_data){
        static const char GT_CntBits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		const char *cbs = GT_CntBits;
		char *wpos = str_out;
		unsigned int v3 = 0, wn = data_size;
		uint8 vi, *_v3 = (uint8*)&v3, *end_data = p_data + wn;
		for(; p_data < end_data; p_data += 3){
			_v3[2] = p_data[0], _v3[1] = p_data[1], _v3[0] = p_data[2];
			
           #define GT_TMP_INT24_TO_A4_64 \
			wpos[0] = wpos[1] = wpos[2] = wpos[3] = 'A';\
			vi = 3;\
			while(v3){\
                wpos[vi--] = cbs[v3 & 63]; /*计算出余数就去索取对应的符号*/\
                v3 >>= 6;\
			}\
            wpos += 4
			
			GT_TMP_INT24_TO_A4_64;
		}
		if(wn < data_size){
            wn = data_size - wn;
			_v3[2] = p_data[0]; _v3[1] = (wn==1)?0:p_data[1]; _v3[0] = 0;
			GT_TMP_INT24_TO_A4_64;
			*(wpos - 1) = '=';
			if(wn == 1) *(wpos - 2) = '=';
		}
		return wpos - str_out;
	}
	return (data_size / 3 + 1) << 2;
}
GN_API unsigned int GN_StrB4I64ConvertUn(uint8 *data_out, /*const*/ char *cnt, unsigned int cnt_len){
    unsigned int size = cnt_len >> 2;
	unsigned int size_out = size * 3, v3;
    if((size << 2) < cnt_len)size_out += 3;
	if(data_out && cnt){
		//字符引用值                             0   1    2   3   4   5   6   7   8   9 
		static const uint8 GT_CntRefValue[] = {  0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //0
	                                             0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //1
												 0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //2
												 0,  0,   0,  0,  0,  0,  0,  0,  0,  0, //3
												 0,  0,   0, 62,  0,  0,  0, 63, 52, 53, //4
                                                54, 55,  56, 57, 58, 59, 60, 61,  0,  0, //5
												 0,  0,   0,  0,  0,  0,  1,  2,  3,  4, //6
												 5,  6,   7,  8,  9, 10, 11, 12, 13, 14, //7
												15, 16,  17, 18, 19, 20, 21, 22, 23, 24, //8
												25,  0,   0,  0,  0,  0,  0, 26, 27, 28, //9
												29, 30,  31, 32, 33, 34, 35, 36, 37, 38, //10
												39, 40,  41, 42, 43, 44, 45, 46, 47, 48, //11
												49, 50,  51,  0,  0,  0,  0,  0,  0,  0, //12
												 0,  0,   0,  0,  0,  0,  0,  0   };  	 //13
		const uint8 *crv = GT_CntRefValue;
        size <<= 2;
		uint8 *p_data = data_out, *_v3 = (uint8*)&v3;
		char  *cnt_end = cnt + size;
		for(; cnt < cnt_end; cnt += 4, p_data += 3){
		    v3 = (((unsigned int)crv[cnt[0]]) << 18) + (((unsigned int)crv[cnt[1]]) << 12) + (((unsigned int)crv[cnt[2]]) << 6) + (unsigned int)crv[cnt[3]];
		    p_data[0] = _v3[2], p_data[1] = _v3[1], p_data[2] = _v3[0];
		}
		if(size < cnt_len){
            v3 = ((unsigned int)crv[cnt[0]]) << 18;
			size = cnt_len - size;
			if(size > 1)v3 += ((unsigned int)crv[cnt[1]]) << 12;
            if(size > 2)v3 += ((unsigned int)crv[cnt[2]]) << 6;
			p_data[0] = _v3[2], p_data[1] = _v3[1], p_data[2] = _v3[0];
		}
	}
	return size_out;
}
GN_API char *GN_StrB4I64PacketData(unsigned int &len_out, uint8 *p_data, unsigned int data_size){
   #define GT_TMP_STR_B4I64PAK_DATA( NEWCHAR ) \
	len_out = GN_StrB4I64Convert(0, 0, data_size);\
	NEWCHAR ;\
    GN_StrB4I64Convert(cnt, p_data, data_size);\
	cnt[len_out] = 0;\
	return cnt;

	GT_TMP_STR_B4I64PAK_DATA( char *cnt = new char[len_out+1] )
}
GN_API P<char> GN_StrB4I64PacketData(uint8 *p_data, unsigned int data_size){
	unsigned int GT_TMP_STR_B4I64PAK_DATA( P<char> cnt(len_out+1) )
}
GN_API uint8 *GN_StrB4I64PacketUnData(unsigned int &size_out, char *cnt, unsigned int cnt_len){
    size_out = GN_StrB4I64ConvertUn(0, 0, cnt_len);
    uint8 *data = new uint8[size_out];
    GN_StrB4I64ConvertUn(data, cnt, cnt_len);
	return data;
}
GN_API P<uint8> GN_StrB4I64PacketUnData(char *cnt, unsigned int cnt_len){
    P<uint8> data(GN_StrB4I64ConvertUn(0, 0, cnt_len));
    GN_StrB4I64ConvertUn(data, cnt, cnt_len);
	return data;
}
GN_API P<char> GN_StrsPacketData(P< P<char> > strs){
	//封包内部数据格式:
	//[字符串数, 字符串1大小含结束符, 字符串2大小含结束符, ... , 各个字符串含结束符一个紧接一个的具体内容]

	/*模版代码:字符串数组封包*/
	P<char> * pstrs = strs;
	unsigned int size = 0, si = 0, sn = strs.Num();
	for(; si < sn; ++si)size += pstrs[si].Size();
	size += (sn + 1) << 2;
	P<char> pack(size);
	unsigned int *_psize = (unsigned int*)pack.Begin();
	*_psize = sn; ++_psize;
	uint8 *_pd = (uint8*)(_psize + sn);
	for(si = 0; si < sn; ++si){
		_psize[si] = size = pstrs[si].Size();
		memcpy( _pd, pstrs[si].Begin(), size );
        _pd += size;
	}
	return pack;
}
GN_API char  * GN_StrsPacketData(unsigned int *size_out, const char **strs, unsigned int sn){
    uint8 loc_buf[8192];
	GN::sTBUF_INT tbuf(loc_buf, 8192);
	unsigned int *_psize1 = (unsigned int*)tbuf.GetSpace(sn << 2);
	/*模版代码:字符串数组封包*/
	unsigned int size = 0, si = 0;
	for(; si < sn; ++si){
		_psize1[si] = strlen(strs[si]) + 1;
		size += _psize1[si];
	}
	size += (sn + 1) << 2;
	char *pack = new char[size];
	unsigned int *_psize = (unsigned int*)pack;
	*_psize = sn; ++_psize;
	memcpy(_psize, _psize1, sn << 2);
	uint8 *_pd = (uint8*)(_psize + sn);
	for(si = 0; si < sn; ++si){
		memcpy( _pd, strs[si], _psize1[si] );
        _pd += _psize1[si];
	}
	return pack;
}
GN_API P< P<char> > GN_StrsPacketUnData(char *str_pack){
    unsigned int *_psize = (unsigned int*)str_pack;
	unsigned int si = 0, sn = *_psize;
    P< P<char> > strs(sn); ++_psize;
	P<char> *_strs = strs;
	uint8   *_pd = (uint8*)(_psize + sn);
	for(; si < sn; ++si){
		_strs[si].New(_psize[si]);
		memcpy((char*)_strs[si], _pd , _psize[si]);
	    _pd += _psize[si];
	}
    return strs;
}
GN_API char* GN_StrValueRead(char *pstr, int *v, const char *vname, char eqk){
   #define GT_TMP_STR_VALUR_READ( FMT ) \
	pstr = strstr(pstr, vname);\
	if(pstr){\
		pstr = strchr(pstr, eqk);\
		if(pstr){\
			++pstr;\
			sscanf(pstr, FMT, v);\
			return pstr;\
		}\
	}\
	return 0;

	GT_TMP_STR_VALUR_READ( "%d" )
}
GN_API char* GN_StrValueRead(char *pstr, const char *rfmt, void *v, const char *vname, char eqk){
	GT_TMP_STR_VALUR_READ( rfmt )
}
GN_API char* GN_StrStringRead(char *pstr, char *str_out/*[]*/, const char *vname, char eqk){
	pstr = strstr(pstr, vname);
	if(pstr){
		pstr = strchr(pstr, eqk);
		if(pstr){
			++pstr;
			char *pstr1 = pstr;
			while((*pstr1) && (*pstr1) != '\n' )++pstr1;
			if(*pstr1){
				int len = pstr1 - pstr;
				strncpy(str_out, pstr, len);
				str_out[len] = 0;
				return pstr;
			}
		}
	}
	return 0;
}
GN_API P<char> GN_StrToPrintFmt(const char *str, short fn,  float *dec_out){
	char _c = str[fn];
	unsigned short i = fn, n = 0, dn = 0;
	while(str[i] == _c){ ++i; ++n; }

    char *_unit = (char*)str + i;
	if(str[i] == '.' && str[i+1] == _c){
	    ++i;
		while(str[i] == _c){ ++i; ++dn; }
        _unit = (char*)str + i;
	}
	char n_str[32];
    unsigned short ulen = (unsigned short)strlen(_unit), n_n;
	float ratio = 1.0f;
	if(dn){
          unsigned short di = 0;
		  for(; di < dn; ++di)ratio *= 0.1f;
		  n_n = sprintf(n_str,"%d.%d", 1 + n + dn, dn);
	}else n_n = sprintf(n_str,"%d", n);
	if(dec_out)*dec_out = ratio;

	P<char> fmt(fn+4+n_n + ulen);
	char *_fmt = fmt;
    strncpy(_fmt, str, fn); _fmt += fn;
	_fmt[0] = '%';
	_fmt[1] = (_c ==' ' || _c =='#')? '#' : '0';
	_fmt += 2;
	// TODO: verify buffer size
	strcpy(_fmt, n_str); _fmt += n_n;
	*_fmt = dn? 'f': 'd';
	// TODO: verify buffer size
    strcpy(_fmt + 1, _unit);
    return fmt;
}
GN_API int GN_StrSymbolToSingleChar(const char *str, char *str_out){
	int i = 0, i2 = 0;
	for(;;){
		if(str[i] & 128){
			if(str[i] == 0xa3 && str[i+1] >= 0xa0 && str[i+1] <= 0xfe)
				str_out[i2++] = (str[i+1]  - 0xa0) + ' '; //ASCII都共同有的一些对应全角字符
			else{
				if(str[i] == 0xa1){ //全角字符里特有的一些对应字符“”‘’《〈》〉＄。·、～
					switch( *((uint8*)(str + i + 1)) ){
					case 0xb0: case 0xb1: str_out[i2++] = '\"'; break;
					case 0xae: case 0xaf: str_out[i2++] = '\''; break;
					case 0xb6: case 0xb4: str_out[i2++] = '<'; break;
					case 0xb7: case 0xb5: str_out[i2++] = '>'; break;
					case 0xe7: str_out[i2++] = '$'; break;
					case 0xa3: str_out[i2++] = '.'; break;
					case 0xa4: str_out[i2++] = '@';  break;
					case 0xa2: str_out[i2++] = '\\'; break;
					case 0xab: str_out[i2++] = '~';  break;
					default:
						str_out[i2] = str[i]; 
						str_out[i2+1] = str[i+1]; 
						i2 += 2; 
						break;
					}
				}else{
						str_out[i2] = str[i]; 
						str_out[i2+1] = str[i+1];
						i2 += 2;
				}
			}
            i += 2;
		}else if(str[i]){ //ASCII字符
		    str_out[i2++] = str[i++];
		}else{            //结束
		    str_out[i2] = 0; 
			return i2;
		}
	}
	return i2;
}
GN_API int GN_StrSymbolToWidthChar(const char *str, char *str_out){
	int i = 0, i2 = 0;
	for( ; str[i]; ++i){
		if(str[i] <= '}' && str[i] >= ' '){ //有规律转换的字符
			str_out[i2  ] = 0xa3;
			str_out[i2+1] = 0xa0 + (str[i] - ' ');
            i2 += 2;
		}else if(str[i] == '~'){
            str_out[i2  ] = 0xa1;
			str_out[i2+1] = 0xab;
		    i2 += 2;
		}else str_out[i2++] = str[i]; //无需转换
	}
    str_out[i2] = 0;
	return i2;
}
GN_API P<char> GN_StrSymbolToSingleChar(const char *str){
   #define GT_TMP_STR_SYMBOL_CONVERT( DSIZE , ADDPROC ) \
    uint8 loc_buf[16384];\
	GN::sTBUF_INT tbuf(loc_buf, 16384);\
    char *str2 = (char*)tbuf.GetSpace( DSIZE );\
	ADDPROC (str, str2);\
	return P<char>(str2,0)

    GT_TMP_STR_SYMBOL_CONVERT( strlen(str)+1 , GN_StrSymbolToSingleChar );
}
GN_API P<char> GN_StrSymbolToWidthChar(const char *str){
    GT_TMP_STR_SYMBOL_CONVERT( (strlen(str)<<1)+1 , GN_StrSymbolToWidthChar );
}
GN_API void GN_StrToUpperCase(char *str){
	char crs = 'a' - 'A';
	int i = 0;
	while(str[i]){
		if(str[i] >= 'a' && str[i] <= 'z')str[i] -= crs;
		++i;
	}
}
GN_API void GN_StrToLowerCase(char *str){
	char crs = 'a' - 'A';
	int i = 0;
	while(str[i]){
		if(str[i] >= 'A' && str[i] <= 'Z')str[i] += crs;
		++i;
	}
}
GN_API void GN_StrUpperLowerExchange(char *str){
	char crs = 'a' - 'A';
	int i = 0;
	while(str[i]){
		if	   (str[i] >= 'A' && str[i] <= 'Z')str[i] += crs;
		else if(str[i] >= 'a' && str[i] <= 'z')str[i] -= crs;
		++i;
	}
}
#endif //endif _GN_STRING_H_