#ifndef _GN_STRING_H_ 
#define _GN_STRING_H_ 
/************************************************字符操作***********************************************************************/

//复制文本数据到文本指针里( 目标指针, 复制源 ) 这将会自动为指针分配适合的空间
#define GN_StrcpyData( p_dest , src ) \
	if( p_dest )delete [] ( p_dest );\
    if( (src) && (src)[0] ){\
        (p_dest) = new char[ strlen( src ) + 1];\
	    strcpy( (p_dest) , (src) );\
    }else (p_dest) = 0

//检测指定字符是否空的非显示字符(指定字符)返回是否
inline int isspace(int8 ch){
	//          空格        换行符    水平制表符       回车符      走纸换行    垂直制表符
	return (ch == ' '|| ch == '\n' || ch == '\t' || ch == '\r' || ch == '\f' || ch == '\v' )? 1 : 0;
}
//检测指定字符是否空的非显示字符或结束符(指定字符)返回是否
inline bool GN_StrIsSpaceOrEnd(char ch){
	//           空格        换行符    结束符    水平制表符       回车符      走纸换行    垂直制表符
    return (ch == ' '|| ch == '\n' || ch == 0 || ch == '\t' || ch == '\r' || ch == '\f' || ch == '\v' );
}
//检测指定字符是否看得到的实体字符 这个不包含空格
#define GN_StrIsVisual( CH_ ) ( ((CH_) & 128) || ( (CH_) > 32 && (CH_) != 127 ))

//判断一个字符串是否能看见的 即所有字符都是实体字符
inline bool GN_StrIsCanSee( const char *str ){
    char *pstr = (char*)str;
	while(*pstr){
	    if(GN_StrIsVisual( *pstr ))return true;
        ++pstr;
	}
    return false;
}
//从完整的包含扩展名的文件名里查找得到它的扩展名( 文件路径名 ) 返回扩展名 返回0表示没有扩展名
GN_API char *GN_StrGetExtName(const char *file_path_name);

//从完整文件名里获得最尾级的文件名或目录名( 文件路径名 ) 
GN_API char *GN_StrGetFileName(const char *file_path_name);

//从一组完整文件名里获得最尾级的文件名或目录名(一组文件路径名)返回一组文件名 
GN_API P< P<char> > GN_StrGetFileNames(P< P<char> > & file_path_names);

//从完整文件名里取得目录名( 文件路径名, 返回目录名) 
GN_API void GN_StrGetDirName(const char *file_path_name, char *dir_out/*[384]*/); 

//从完整文件名里取得目录名( 文件路径名 ) 
GN_API P<char> GN_StrGetDirName(const char *file_path_name); 

//把指定的完整文件名去掉最尾末的文件名或目录( 文件路径名 !该名将受到改变) 返回断开掉的字符串 可能是文件名或最尾一级目录名
GN_API char *GN_StrRemoveFileName(char *file_path_name);

//把指定的完整文件名去掉目录和扩展名( 文件路径名 !该名将受到改变, 是否移除路径得到斋文件名) 返回斋文件名
GN_API char *GN_StrRemoveFilePathAnExtName(char *file_path_name, bool remove_path=true);

//获得字符串里包含指定字符的个数(字符串, 字符) 返回指定字符的个数
inline int GN_StrGetCharNum(const char *str, char key){ char *p_str=(char*)str; int num=0; while(*p_str){ if(*p_str == key)num++; p_str++; } return num; }

//得到字符串可显示的文字个数(文本字符串, 被截取的字符串长度0:表示截取全部)返回可显示的文字个数
GN_API int GN_StrGetVisualNum(const char *text, unsigned short text_len=0);

//获得字符串里的首个中文字出现的位置指针( 文本字符串 ) 返回出现的位置 返回0表示没有找到
GN_API char *GN_StrGetFirstChinaWord(const char *text); 

//计算字符串最长的一行可显示字符个数和总显示行数( 文本字符串, 一行最大显示数输出, 行数输出)
GN_API void GN_StrRowAndColCompute(const char *text, int *row_out, int *col_out); 

//从单个字符串按指定的分割符分割出多个字符串(分割到的字符串数输出, 字符串, 分割符1, 分割符2）返回分割好的字符数组指针  这会帮你分配好适当的空间 用完记得释放指针
GN_API char ** GN_StrSplitToData(unsigned int &str_num_out, const char *str, char split_key=' ', char split_key2=0);

//从单个字符串按指定的分割符分割出多个字符串(字符串, 分割符1, 分割符2）返回分割好的字符数组指针  这会帮你分配好适当的空间 用完记得释放指针
GN_API P< P<char> > GN_StrSplitToData(const char *str, char split_key=' ', char split_key2=0);

//把单个字符串按指定的分割符分割成多个字符串(字符串 处理后这个里的内容会被切断, 分割符) 返回各个字符串起始在被分割字符串里的指针 !这个调用更快
GN_API P<char*>	GN_StrSplit(char *str, char split_key=' ');

//把多个字符串连成一个(多个字符串的数组, 字符串数, 连接添加到字符串间的分隔符)返回连好的字符串 这会帮你分配好适当的空间 用完记得释放指针
GN_API char *GN_StrUnSplitToData(const char **strs, unsigned int strn, char split_key=' ');

//把多个字符串连成一个(装填连好的字符串 需预先分配好空间, 多个字符串的数组, 字符串数, 连接添加到字符串间的分隔符)返回连好的字符串长度
GN_API unsigned int GN_StrUnSplit(char *str_out, const char **strs, unsigned int strn, char split_key=' ');

//把多个字符串连成一个(装填连好的字符串 需预先分配好空间, 多个字符串的数组, 连接添加到字符串间的分隔符)返回连好的字符串长度
GN_API unsigned int GN_StrUnSplit(char *str_out, const P< P<char> > &strs, char split_key=' ');

//获得字符串所表示的数组里的各个数据(接收数据的数组指针 必需先分配好足够空间 至少一个, 单个数据大小字节, 字符串, 提取数据的格式符 类似C函数scanf的"%d"等, 数据分格符, 默认值指针 用于填补某个数据获取失败)返回获取数 如果有指定默认值必定会返回完整的获取数 反之获取数有可能因格式不对失败而变少了
GN_API unsigned int GN_StrGetArray(uint8 *p_data, unsigned int em_size, const char *str, const char *fmt="%d", char split_key=',', uint8 *p_default=0);

//把字符串表示的数转为长整型数(字符串)
GN_API time_t GN_StrAtoI64(const char *str); 

//从指定文本内容里找出同层级的所有内容行指针(返回包含的内容行列表, 指定文本内容, 层级左括符, 层级右括符,  内容行忽略标记) 这会帮你分配好适当的空间 用完记得清除连表
GN_API void GN_StrFindCurLevelContentData(GN::sLIST<char*> &contents_out, const char *text, char key='{', char key2='}', const char *ignore_tag=0/*"//"*/);

//从指定文本内容里找出同层级的所有内容行指针(返回包含的内容行数, 指定文本内容, 层级左括符, 层级右括符, 内容行忽略标记) 返回所有包含的内容行指针 如果没有包含内容返回0 这会帮你分配好适当的空间 用完记得释放指针
GN_API char **GN_StrFindCurLevelContentData(int &cn_out, const char *text, char key='{', char key2='}', const char *ignore_tag=0/*"//"*/);

//从指定文本内容里找出同层级的所有内容行指针(指定文本内容, 层级左括符, 层级右括符, 内容行忽略标记) 返回所有包含的内容行指针 这会帮你分配好适当的空间 用完记得释放指针
GN_API P<char*> GN_StrFindCurLevelContentData(const char *text, char key='{', char key2='}', const char *ignore_tag=0/*"//"*/); 

//从单个字符串里选出括号里的子字符串(用于装填子字符串的指针数组 必须分配好足够的空间, 字符串, 左括符, 右括符) 返回选到几个子字符串 返回0表示没有在括号内的内容或内容格式不规范 !!调用后参数 str 里的内容将受到改变 如果某个括号里没有内容那么对应的返回指针内容为"\0"而不是空指针
GN_API int GN_StrExtract(char *sub_str_out[], char *str, char key='<', char key2='>'); 

//从单个字符串里提取出括号里的子字符串(字符串, 左括符, 右括符 )返回提取出的子字符串 这会帮你分配好适当的空间 用完记得释放指针 返回空的表示没有在括号内的内容或内容格式不规范 如果某个括号里没有内容那么对应的返回指针为空指针
GN_API P< P<char> > GN_StrExtractToData(const char *str, char key='<', char key2='>'); 

//从多项名的字符串中提取出指定序号的名(用于装填提取内容的字符缓冲 需分配好大于各个项名的长度, 多项名的字符串如"name0,name1,name2", 需提取名的序号, 分隔符) !如果 id 索取超出项名数则会返回最后一项 
GN_API void GN_StrExtractFromMultiItem(char str_out[], char *str, int id, char key=','); 

//查找字符串里有没有指定的名称(名称, 字符串, 分割符)
GN_API bool GN_StrHasName(const char *name, const char *str, char split_key=' ');

//检测指定字符串里有没包含文件路径名里禁用的字符(字符串) 如果含有就返回所有被禁用的字符组成的字符串如 "\\ / : * ? \" < > |"  如果不含有则返回0  
GN_API char *const GN_StrHasPathDisableSymbol(const char *str); 

//把字符串指针移到指定字符首次出现处之后(源字符串, 指定字符）返回移动后的指针 返回0表示失败没有搜到
GN_API char *GN_StrToKey(const char *str, char key);

//过滤字符串中的所有指定字符(装填过滤好的字符串输出 必须先分配不能少于参数str的空间大小, 源字符串, 要过滤的字符）返回被过滤掉的字符数 
GN_API int GN_StrFilter(char *str_out, const char *str, char key); 

//过滤字符串中的所有指定字符(源字符串, 要过滤的字符）返回过滤好的字符串 用完记得删除 
GN_API P<char> GN_StrFilter(const char *str, char key); 

//过滤字符串中的所有指定字符(返回被过滤掉的字符数, 源字符串, 要过滤的字符）返回过滤好的字符串 用完记得删除 
GN_API char *GN_StrFilter(int &f_count_out, const char *str, char key); 

//替换字符串中指定字符为特定替换字符(字符串, 指定字符, 特定替换字符, 替换检索长度 默认0为整个字符串) 
GN_API void GN_StrReplace(char *str, char key, char rep, int len=0); 

//消减字符串首尾的空段(源字符串) 返回源字符串第一个非空字符号出现的位置指针
GN_API char* GN_StrCutSpaceFL(char *str);

//比较两个字符串(字符串1, 字符串2) 返回是否相同  比较完后字符串2的指针会移到下一字串处或结束处
GN_API bool GN_StrcmpSN(const char *str1, char **str2);

//比较两个字符串相同字符数(字符串1, 字符串2)返回相同的字符数 这不包括结束符
inline int GN_StrcmpEN(const char *str1, const char *str2){ int en = 0; while(str1[en] && str1[en] == str2[en])++en; return en; }

//比较两个不对齐的字符串(字符串1,字符串2) 返回是否相同 这容许两字符串里的首尾有空格符 不对齐的
GN_API bool GN_StrcmpEX(const char *str1, const char *str2);

//比较两个文件路径字符串(字符串1, 字符串2) 返回是否相同 !!用路径比较 '/' '\\' 被视为相等
GN_API bool GN_StrcmpPath(const char *str1, const char *str2);

//判断指定字符串是否全局路径(字符串) 返回是否
GN_API bool GN_StrIsGlobalPath(const char *path);

//获取出指定字符串到结束标识符前的一段字符串(指定字符串, 结束标识符 默认0为遇到空格等即结束像C函数sscanf(...,"%s",...)效果一样 ) 返回字符串 如果没获取到即返回空指针 
GN_API P<char> GN_StrGet(const char *str, char end_key=0); 

//从字符串中得到指定段落的开始指针和结束指针(要搜索的字符串, 开始标识符, 结束标识符, 返回段落的开始指针, 返回段落的结束指针) 返回是否有标识符指定的段落
GN_API inline bool GN_StrGetSegment(const char *str, char start_key, char end_key, char **p_start_out, char **p_end_out);

//从字符串中复制指定段落到新缓存(要搜索的字符串, 开始标识符, 结束标识符, 返回新字符段长度) 返回新分配的字符串指针 返回0表示失败
GN_API inline char* GN_StrCopySegmentToData(const char *str, char start_key, char end_key, int *size_out);

//从字符串中删除多段含指定内容的段落( 字符串的地址, 段分隔符, 删除段中包含的字符串, 最多删除多少段, 是否只扫描每段的首部作比较) 返回共删除了多少段 返回0表示没找到符合条件的段  返回-1表示内容已经全清了
GN_API int GN_StrRemoveSegment(char *str_in_out, char split_key, const char *include_str, int sn=1, bool check_first=true);

//在字符串指定位置或连续空段中插入子字符串(指向字符串的指针 插入后如字符串增长了 该指针将指向新的字符串, 子字符串, 插入位置偏移 当值小与0时 就自动查找到空段里插入 值-1:空段里靠前 -2:靠后 -3:居中, 连续空段的标记字符)
GN_API void GN_StrInsertSegment(char **ppstr, const char *sub_str, short inpos, char seg_tag='_');

//把2进制数据转换为文本格式数据(数据输入输出, 数据字节长度输入输出) !!这通常用于在用数据方式读取文本文件时接着在内存里转换为正确的文本格式 
GN_API void GN_StrBinaryToTextData(char **mem_in_out, int *len_in_out); 

//把2进制数据转换为4位64进1的字符表示数据以便于用文本方式来保存或传输(装填字符表示数据的缓冲, 2进制数据, 2进制数据大小字节)返回字符表示数据长度 !装填缓冲需已有足够大的空间 要有 (data_size / 3 + 1) * 4 的大小 你也可以前两个参数为0来调用该函数来获得装填缓冲需要的大小 
GN_API unsigned int GN_StrB4I64Convert(char *str_out, /*const*/uint8 *p_data, unsigned int data_size);

//把4位64进1的字符表示数据恢复为2进制数据(装填2进制数据的缓冲, 字符表示数据内容, 内容长度)返回2进制数据大小字节 !装填缓冲需已有足够大的空间 要有 (content_len/4 + ((content_len%4)?1:0)) * 3 的大小 你也可以前两个参数为0来调用该函数来获得装填缓冲需要的大小 
GN_API unsigned int GN_StrB4I64ConvertUn(uint8 *data_out, /*const*/char *content, unsigned int content_len);

//把2进制数据封包为4位64进1的字符表示数据以便于用文本方式来保存或传输(返回字符表示数据长度, 2进制数据, 2进制数据大小字节)返回字符表示数据 用完记得删除 
GN_API char *GN_StrB4I64PacketData(unsigned int &len_out, /*const*/uint8 *p_data, unsigned int data_size);

//把2进制数据封包为4位64进1的字符表示数据以便于用文本方式来保存或传输(2进制数据, 2进制数据大小字节)返回字符表示数据 用完记得删除 
GN_API P<char> GN_StrB4I64PacketData(/*const*/uint8 *p_data, unsigned int data_size);

//把4位64进1的字符表示数据恢复为2进制数据(返回2进制数据大小字节, 字符表示数据内容, 内容长度) 返回2进制数据 用完记得删除
GN_API uint8 *GN_StrB4I64PacketUnData(unsigned int &size_out, /*const*/char *content, unsigned int content_len);

//把4位64进1的字符表示数据恢复为2进制数据(字符表示数据内容, 内容长度) 返回2进制数据 用完记得删除
GN_API P<uint8> GN_StrB4I64PacketUnData(/*const*/char *content, unsigned int content_len);

//把参数给定的字符串数组封包成一个数据包以便于网络发送或保存文件( 字符串数组 ) 返回封包好的数据 !!用完记得.Delete()
GN_API P<char> GN_StrsPacketData(P< P<char> > strs); 

//把参数给定的字符串数组封包成一个数据包以便于网络发送或保存文件( 返回封包数据大小字节, 字符串数组, 字符串数组含有多少个字符串 ) 返回封包好的数据 !!用完记得delete [] data
GN_API char  * GN_StrsPacketData(unsigned int *size_out, const char **strs, unsigned int count); 

//把字符串数组包解开为字符串数组( 字符串数组包 ) 返回字符串数组 !!用完记得.Delete()
GN_API P< P<char> > GN_StrsPacketUnData(char *str_pack); 

//从字符串里读取值(字符串, 值输出, 值名, 等号字符)返回值在字符串的位置 返回0表示没有指定值
GN_API char* GN_StrValueRead(char *pstr, int *v, const char *vname, char eqk='=');

//从字符串里读取值(字符串, 值获取格式字符串如%d, 值输出, 值名, 等号字符)返回值在字符串的位置 返回0表示没有指定值
GN_API char* GN_StrValueRead(char *pstr, const char *rfmt, void *v, const char *vname, char eqk='=');

//从字符串里读取字符串值(字符串, 字符串值输出 需先分配好足够空间, 值名, 等号字符)返回值在字符串的位置 返回0表示没有指定值
GN_API char* GN_StrStringRead(char *pstr, char *str_out/*[]*/, const char *vname, char eqk='=');

//把特定字符串转换为打印格式字符串(特定字符串, 前序休息符占几字节, 小数点后的数陪率输出) 返回打印格式字符串 例如这样调用 GN_StrToPrintFmt("$:00000.00元", 2, dec_out ); 返回 "$:%08.2f元" 调用后 dec_out = 0.01f
GN_API P<char> GN_StrToPrintFmt(const char *str, short fn=0,  float *dec_out=0); 

//把指定字符串里的英文数字标点符号转为单字节的标准英文标点符号(指定字符串, 用来存放转换好的字符串 必须先分配好不少于原字符串的空间) 返回转换好的字符串长度
GN_API int GN_StrSymbolToSingleChar(const char *str, char *str_out); 

//把指定字符串里的英文数字标点符号转为双字节的全角汉字标点符号(指定字符串, 用来存放转换好的字符串 必须先分配好大于原字符串两陪以上的空间) 返回转换好的字符串长度
GN_API int GN_StrSymbolToWidthChar(const char *str, char *str_out); 

//把指定字符串里的英文数字标点符号转为单字节的标准英文标点符号(指定字符串) 
GN_API P<char> GN_StrSymbolToSingleChar(const char *str); 

//把指定字符串里的英文数字标点符号转为双字节的全角汉字标点符号(指定字符串) 
GN_API P<char> GN_StrSymbolToWidthChar(const char *str); 

//把指定字符串里的英文字母都转为大写(指定字符串)
GN_API void GN_StrToUpperCase(char *str); 

//把指定字符串里的英文字母都转为小写(指定字符串)
GN_API void GN_StrToLowerCase(char *str); 

//把指定字符串里的英文字母大小写互换(指定字符串)
GN_API void GN_StrUpperLowerExchange(char *str); 

//把指定字符串里的英文字母都转为大写(指定字符串) 返回大写字符串
inline P<char> GN_StrToUpperCase(const char *str){ P<char> ret(str,0); GN_StrToUpperCase(ret); return ret; }

//把指定字符串里的英文字母都转为小写(指定字符串) 返回小写字符串
inline P<char> GN_StrToLowerCase(const char *str){ P<char> ret(str,0); GN_StrToLowerCase(ret); return ret; } 

//把指定字符串里的英文字母大小写互换(指定字符串) 返回互换后的字符串
inline P<char> GN_StrUpperLowerExchange(const char *str){ P<char> ret(str,0); GN_StrUpperLowerExchange(ret); return ret; } 
#endif //_GN_STRING_H_