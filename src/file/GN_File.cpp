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
#ifdef _GN_FILE_H_
#ifdef GN_USING_UNZIP
extern "C"{
#include<libzip/unzip.h>
}
#endif
/**************************************************************************************************************/
GN_sMUTEX GNf::sFILE::Mutex(0);
void *GNf::sFILE::DefaultH = 0;                  //设置OpenFile的默认在本机里打开文件
int   GNf::sFILE::DefaultType = GNf::eFILE_STD;  //设置OpenFile的默认打开方式为标准STD方式
//bool  GNf::sFILE::EnableDedicatedOnly = false;
bool  GNf::sFILE::EnableGlobalPath = false;

GNf::sFILE GNf::File;

GNf::sFILE::sFILE():
    handle_buf(0),
    buf_size(0),
    special_mark(0),
    offset(0),
    type(eFILE_STD)
{}
GNf::sFILE::~sFILE(){
    switch(type){
	case eFILE_SAFE:
    case eFILE_STD:
		if(handle_buf){ fclose((FILE *)handle_buf); }
		break;
	default:
		if(special_mark){
			if(handle_buf)delete [] handle_buf;
		}
		break;
	}
}
static int GT_FileGetSize(GNf::sFILE *handle){
	switch(handle->type){
	case GNf::eFILE_SAFE:
    case GNf::eFILE_STD:
		if(handle->handle_buf){
			long len = ftell((FILE*)(handle->handle_buf));
			if(fseek((FILE*)(handle->handle_buf),0,GNf::eSEEK_END)==EOF)return -1;
			long size_len = ftell((FILE*)(handle->handle_buf));
            if(fseek((FILE*)(handle->handle_buf),len,GNf::eSEEK_START)==EOF)return -1;
            return (int)size_len;
		}else return -1;
	default:
		return handle->buf_size;
	}
}
static unzFile GT_OpenUnzipFile(char *filename, int &usTYPE_size){
	unzFile uf = (unzFile)GT_ZipPacks[-(usTYPE_size+1)];
	if(unzLocateFile(uf, filename, 0) != UNZ_OK)return 0;
    
	unz_file_info file_info;
    char filename_inzip[384];
	unzGetCurrentFileInfo(uf, &file_info, filename_inzip,384, 0,0,0,0);
	
	unzOpenCurrentFile(uf);
    usTYPE_size = (int)file_info.uncompressed_size;
	return uf;
}
GN_API GNf::sFILE * GNf::Open(char *filename,const char *mode,int usTYPE_size, void  *ph){
	sFILE *fp;
    int _us_type = usTYPE_size;
   #ifdef GN_USING_UNZIP
	if(usTYPE_size < 0){ //查找包文件
		unzFile uf = GT_OpenUnzipFile(filename, usTYPE_size);
		if(!uf)return 0;
		filename = new char[usTYPE_size];
		unzReadCurrentFile(uf, filename, usTYPE_size); //一次过读取整个文件
        unzCloseCurrentFile(uf);
	}
   #endif
	switch(usTYPE_size){
	case eFILE_SAFE: {
		    char _file[384];
            GN_StrToSysFilePath(_file,filename,"wb", GNf::sFILE::EnableGlobalPath); //处理文件路径
		    char *_filename_ = _file;

			char tmpfn1[384], tmpfn2[384];
			{   //构建两个 filename. 的副本名 例如 filename_1. filename_2.
				char *fi = _filename_, *tf1 = tmpfn1, *tf2 = tmpfn2;
		        while(fi[0] && fi[0] != '.'){
					tf1[0] = tf2[0] = fi[0];  //复制"."前的路径文件名
					++fi; ++tf1; ++tf2;
				}
                tf1[0] = tf2[0] = '_';        
				++tf1;  ++tf2;
				tf1[0] = '1'; tf2[0] = '2';   //增加"_1","_2" 后续名
				++tf1;  ++tf2;
				while(fi[0]){
					tf1[0] =  tf2[0] = fi[0]; //续上后面的名
					++fi; ++tf1; ++tf2;
				}
                tf1[0] = 0; tf2[0] = 0;
			}
            int is_read = GNf_OpenModeCharToInt(mode);
			time_t t1 = 0, t2 = 0;
			struct stat fileStat; //获得文件时间属性
			if(!stat(tmpfn1, &fileStat))t1 = fileStat.st_mtime;
			if(!stat(tmpfn2, &fileStat))t2 = fileStat.st_mtime;
			if(is_read == 1){     //是读就取日期新的
				_filename_ = (t1 >= t2 ? tmpfn1 : tmpfn2);

			}else if(is_read > 1){//是写就取日期旧的
				_filename_ = (t1 >= t2 ? tmpfn2 : tmpfn1);
	
				int not_wf = 0;
				FILE *wf = fopen(_filename_,"rb");
				if(wf)fclose(wf);
				else not_wf = 1;

				if(not_wf){  //如果要写的目标文件不存在
				  if(((is_read == 3) || (is_read & 4))){ //如果是“r+”读写,,就复制另一个副本的内容给它
                    long size_len = 0;
					FILE *pt_file = fopen((_filename_ == tmpfn1? tmpfn2 : tmpfn1),"rb");
					if(pt_file){  //另一个文件副本存在
			            fseek(pt_file,0,SEEK_END);
			            size_len = ftell(pt_file);
                        fseek(pt_file,0,SEEK_SET);
						char *buf = new char[size_len];
						fread(buf, 1,size_len, pt_file);
                        fclose(pt_file);
						pt_file = fopen(_filename_,"wb");
						if(pt_file){
							fwrite(buf, 1,size_len, pt_file); //复制内容
							fclose(pt_file);
						}
						if(buf)delete []buf;
					}
				  }//endif is_read == 3 “r+”
				}
			} //校验完成,用更改过的文件名打开文件
            FILE *_fp = fopen(_filename_,mode);
		    if(!_fp)return 0;
			fp = new sFILE;
			fp->handle_buf = (char*)_fp;
		    fp->buf_size = GT_FileGetSize(fp);
		}
		break;
	case eFILE_STD:{
		char _file[384];
		FILE *_fp = fopen(GN_StrToSysFilePath2(_file,filename,mode, GNf::sFILE::EnableGlobalPath), mode);
		if(!_fp)return 0;
		fp = new sFILE;
		fp->handle_buf = (char*)_fp;
		fp->buf_size  = GT_FileGetSize(fp);
		}break;
	default:
		if(!filename)return 0;
		int ci = 0;
		while( mode[ci] ){
			if(mode[ci]=='a'){ return 0; }  //对于内存镜像文件不作添加处理
			if(mode[ci]=='w'){ memset(filename, 0, usTYPE_size); break; }//清空内存文件内容
		    ++ci;
		}
		fp = new sFILE;
	   #ifdef GN_USING_UNZIP
		if(_us_type != usTYPE_size)fp->special_mark = _us_type; //标记这数据源于压缩包
	   #endif
		fp->handle_buf = filename;
		fp->buf_size  = usTYPE_size;
		//fp->offset = 0;
		break;
	}
	fp->type = usTYPE_size;
	if(OnFileOpenOKCallBack)OnFileOpenOKCallBack(fp->buf_size ,filename, mode, _us_type, 0);
	return fp;
}
GN_API int GNf::Close(sFILE *handle){
	if(handle){
		delete handle; return 0;
    }else return -1;
}
GN_API int GNf::GetSize(sFILE *handle){
	if(handle){
		return handle->buf_size;
	}else return -1;
}
GN_API int GNf::Read (char *buf,int unit_size, int count, sFILE *handle){
    if(!handle)return -1;
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		return fread((void*)buf,unit_size,count,(FILE*)(handle->handle_buf));
	default:
		if(buf && handle->handle_buf){
			int tsize = unit_size*count;
			if(handle->offset + tsize > handle->buf_size)tsize = handle->buf_size - handle->offset;//防止出轨
			memcpy((void*)buf,(void*)(handle->handle_buf + handle->offset),tsize);
			handle->offset += tsize;
			return tsize/unit_size;
		}else return -1;
	}
}
GN_API int GNf::Write(const char *buf,int unit_size,int count,sFILE *handle){
    if(!handle)return -1;
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		return fwrite((void*)buf,unit_size,count,(FILE*)(handle->handle_buf));
	default:
		if(buf && handle->handle_buf){
			int tsize = unit_size*count;
			if(handle->offset + tsize > handle->buf_size)tsize = handle->buf_size - handle->offset;//防止出轨
			memcpy((void*)(handle->handle_buf + handle->offset),(void*)buf,tsize);
			handle->offset += tsize;
			return tsize / unit_size;
		}else return -1;
	}
}
GN_API int GNf::Scanf( sFILE *handle, const char *format,...){
    if(!handle)return -1;
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		int len;
		char **args;
        va_list va;
		va_start(va,format);
       #if defined _WIN32_ || defined _WIN32_WCE
		args = *((char***)(&va));
	    len=fscanf((FILE*)(handle->handle_buf),format,/* !!!! 最大赋值数:23 !!!! */
			args[ 0],args[ 1],args[ 2],args[ 3],args[ 4],args[ 5],args[ 6],args[ 7],
			args[ 8],args[ 9],args[10],args[11],args[12],args[13],args[14],args[15],
			args[16],args[17],args[18],args[19],args[20],args[21],args[22],args[23]);
       #else
        len=vfscanf((FILE*)(handle->handle_buf),format,va);
       #endif
		va_end(va);
		return len;
	default:
		if(!handle->handle_buf)return -1;
		va_list valist;
		va_start(valist,format);

	   #define GT_TMP_SSCANF_TO_PTRS( BUF_ ) \
        lEn=sscanf( BUF_ , tformat,/*!!最大赋值数:23!!*/\
			desc[ 0],desc[ 1],desc[ 2],desc[ 3],desc[ 4],desc[ 5],desc[ 6],desc[ 7],\
			desc[ 8],desc[ 9],desc[10],desc[11],desc[12],desc[13],desc[14],desc[15],\
			desc[16],desc[17],desc[18],desc[19],desc[20],desc[21],desc[22],desc[23]);

	   #ifdef __LP64__
        unsigned int *desc[24];
		char tformat[128];
		snprintf(tformat, sizeof(tformat), "%s%%n", format);
		int wi =0,Qcount=0,lEn;
		while(format[wi]){ if(format[wi]=='%'){ desc[Qcount++] = va_arg(valist,unsigned int *); } ++wi; } /*查看格式串里有几个变量要赋予值*/
		unsigned int  proce_num;
	    desc[Qcount] = &proce_num;
		GT_TMP_SSCANF_TO_PTRS( (handle->handle_buf + handle->offset) )
	   #else
        char tformat[128];
		snprintf(tformat, sizeof(tformat), "%s%%n", format);
		int wi =0,Qcount=0,lEn;
		while(tformat[wi]){ if(tformat[wi]=='%')Qcount++; wi++; } /*查看格式串里有几个变量要赋予值*/
		unsigned int  proce_num;
		unsigned int  **desc= *((unsigned int  ***)(&valist));
	    desc[Qcount - 1] = &proce_num;
		GT_TMP_SSCANF_TO_PTRS( (handle->handle_buf + handle->offset) )
	   #endif
		va_end(valist);
		handle->offset += proce_num;
		return lEn;
	}
}
GN_API int GNf::Printf(sFILE *handle, char *format,...){
    if(!handle)return -1;
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
        va_list va;
	    int len;
		va_start(va,format);
		len = vfprintf((FILE*)(handle->handle_buf),format,va);
		va_end(va);
		return len;
	default:
		if(!handle->handle_buf)return -1;
        va_list valist;
	    int len2;
		va_start(valist,format);
		len2= vsprintf((handle->handle_buf+handle->offset),format,valist);
		va_end(valist);
        handle->offset += len2;
		return len2;
	}
}
GN_API int GNf::Getc(sFILE *handle){
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		return fgetc((FILE*)(handle->handle_buf));
	default:
		//if(!handle->handle_buf)return -1;
		handle->offset++;
		return handle->handle_buf[handle->offset-1];
	}
}
GN_API char *GNf::Gets(char *buf, int buf_len, sFILE *handle){
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		return fgets(buf, buf_len, (FILE*)(handle->handle_buf));
	default:
		if(buf && handle->handle_buf && handle->handle_buf[handle->offset]){
			buf_len -= 1;
			int ti = 0;
			int &ofs = handle->offset;
			char *fbuf = handle->handle_buf, c;
			for(; ti < buf_len; ++ti, ++ofs){
				c = fbuf[ofs];
			    if(c && c != '\r' && c != '\n')buf[ti] = c;
				else break;
			}
			if(ti != buf_len){ //移动文件读取指针
				++ofs;
				if(c){
					buf[ti++] = c;
					while(fbuf[ofs] && (fbuf[ofs]=='\n' || fbuf[ofs]=='\r'))++ofs;
				}
                if(!fbuf[ofs])++ofs;
			}
			buf[ti] = 0;
            return buf;
		}else return 0;
	}
}
GN_API int GNf::Seek(sFILE *handle, int offset, int origin){
    if(!handle)return -1;
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		return fseek((FILE*)(handle->handle_buf),offset,origin);
	default:
		if(handle->handle_buf==0)return -1;
		switch(origin){
	    case eSEEK_START: handle->offset = offset; break;
		case eSEEK_CUR  : handle->offset+= offset; break;
		case eSEEK_END  : handle->offset = handle->buf_size + offset; break;
		}
        if(handle->offset < 0)handle->offset = 0;
		else if(handle->offset > handle->buf_size)handle->offset = handle->buf_size;
		return 0;
	}
}
GN_API int GNf::Tell(sFILE *handle){
    if(!handle)return -1;
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		return ftell((FILE*)(handle->handle_buf));
	default:
		if(!handle->handle_buf)return -1;
		return handle->offset;
	}
}
GN_API int GNf::Eof(sFILE *handle){
    if(!handle)return -1;
	switch(handle->type){
	case eFILE_SAFE:
    case eFILE_STD:
		return feof((FILE*)(handle->handle_buf));
	default:
		if(!handle->handle_buf)return -1;
		return (handle->offset >= handle->buf_size);
	}
}
GN_API int GNf::Access(const char *path_filename, int amode, void  *ph){
	char t_file_name[384];
	char* t_path = GN_StrToSysFilePath2(t_file_name,path_filename,"rb", GNf::sFILE::EnableGlobalPath);
	
   #ifdef GT_USING_IOS
    if(!access(t_path,amode))return 0;
	else t_path = GN_StrToSysFilePath2(t_file_name,path_filename,"w", GNf::sFILE::EnableGlobalPath);
   #endif
    return access(t_path,amode);
}
GN_API int GNf::Del(const char *path_filename, void  *ph){
    char t_file_name[384];
	return unlink(GN_StrToSysFilePath2(t_file_name,path_filename,"w", GNf::sFILE::EnableGlobalPath));
}
GN_API int GNf::Rename(const char *src_pathname, const char *new_pathname, void  *ph){
    char t_path_name[384], t_path_name2[384];
    //GN_StrToSysFilePath2(t_path_name,src_pathname,"w", GNf::sFILE::EnableGlobalPath);
	//GN_StrToSysFilePath2(t_path_name2,new_pathname,"w", GNf::sFILE::EnableGlobalPath);
    return rename(GN_StrToSysCode2(t_path_name, src_pathname), GN_StrToSysCode2(t_path_name2, new_pathname));
}
GN_API int GNf::Rmdir(const char *path, void  *ph){
    char t_path_name[384];
	//GN_StrToSysCode2(t_path_name, path);
    return rmdir(GN_StrToSysFilePath2(t_path_name,path,"w", GNf::sFILE::EnableGlobalPath));
}
GN_API int GNf::Mkdir(const char *path, void  *ph){
	char t_path_name[384], *dirs[32];
    GN_StrToSysFilePath(t_path_name,path,"w", GNf::sFILE::EnableGlobalPath);
	uint8 di = 0;
	char *pstr = t_path_name + (strlen(t_path_name)-1);

	while(mkdir(t_path_name)){
        while(pstr > t_path_name && *pstr != '/' && *pstr != '\\')--pstr;
		if(pstr > t_path_name){
            dirs[di] = pstr; ++di;
		    *pstr = 0;

		}else return -1;
	}
	while(di > 0){
		dirs[--di][0] = '/';
	    if(mkdir(t_path_name))return -1;
	}
    return 0;
}
GN_API int GNf::GetRunProcNum(const char *proc_name){
    P<GT_sPROC_INFO> procs = GT_GetSysRunProcAll();
	GT_sPROC_INFO *_procs = procs;
	int pi = 0, pn = procs.Num(), num = 0;
	for(; pi < pn; ++pi){
        if(!strcmp(proc_name, _procs[pi].name))++num;
	}
	return num;
}
GN_API int GNf::FindFileDirInfoData(const char *path, bool g_dir, GN::sLIST< P<sFILE_INFO> > *files_out, GN::sLIST< P<sFILE_INFO> > *dirs_out, void  *ph){
	char t_path_name[384], tmp_buf[384];
	P<sFILE_INFO> p_file(1);
	intptr_t pf = _findfirst(GN_StrToSysCode2(t_path_name, path), p_file.Begin() GT_FFPROCBUF);  //打开指定目录
	if(pf == -1){
		p_file.Delete();
		return -1;
	}
	if(g_dir){
        int g_len  = (int)(GN_StrGetFileName(path) - path);
        do{ //把所有文件指针都先保存到连表中
            strncpy(p_file->global_filename,path, g_len);
            strcpy (p_file->global_filename + g_len, p_file->name);
            if(p_file->attrib & _A_SUBDIR)dirs_out->push_back(p_file);
            else                         files_out->push_back(p_file);
            p_file.New(1);
        }while(!_findnext(pf,p_file.Begin() GT_FFPROCBUF));
	}else{
        do{ //把所有文件指针都先保存到连表中
            if(p_file->attrib & _A_SUBDIR)dirs_out->push_back(p_file);
            else                         files_out->push_back(p_file);
            p_file.New(1);
        }while(!_findnext(pf,p_file.Begin() GT_FFPROCBUF));
	}
	_findclose(pf GT_FFPROCBUF);
    p_file.Delete();
    return 0;
}
GN_API int GNf::FindFile_Num(const char *filename_, void  *ph){
	char t_path_name[384], tmp_buf[384];
	char *_fname = GN_StrGetFileName(filename_);
	unsigned int len = _fname - filename_;
	strncpy(tmp_buf,filename_, len); strcpy(tmp_buf+len,"*.*");
	len = strlen(_fname);
	int fn = 0;
	sFILE_INFO p_file;
	intptr_t pf = _findfirst(GN_StrToSysCode2(t_path_name, tmp_buf), &p_file GT_FFPROCBUF);  //打开指定目录
	if(pf == -1)return -1;
	
    do{ //把所有文件指针都先保存到连表中
        if(!(p_file.attrib & _A_SUBDIR) && !strncmp(p_file.name,_fname,len))++fn;
    }while(!_findnext(pf, &p_file GT_FFPROCBUF));
	_findclose(pf GT_FFPROCBUF);
    return fn;
}
GN_API int GNf::Execl(const char *exec_filename, void  *ph){
    char t_file_name[384];
	return execl(GN_StrToSysFilePath2(t_file_name,exec_filename,"rb", GNf::sFILE::EnableGlobalPath),"7",0);
}
GN_API int GNf::ExeclSub(const char *exec_filename, void  *ph){
    char t_file_name[384];
	return GT_ExeclSub(GN_StrToSysFilePath2(t_file_name,exec_filename,"rb", GNf::sFILE::EnableGlobalPath));
}
GN_API int GNf::ExitProc(const char *proc_filename, void  *ph){
    return GT_ExitProc(proc_filename);
}
GN_API int GNf::FileSegmentLoadToMemory(char *filename,   char *buf_out,  int buf_size, int offset, int usTYPE_size, void *ph){
   #ifdef GN_USING_UNZIP
	if(usTYPE_size < 0){
		unzFile uf = GT_OpenUnzipFile(filename, usTYPE_size);
        if(!uf)return -1;
		int ret = -1;
		if(offset >= usTYPE_size)ret = 0;
		else{
			uint8 loc_buf[524288]; //512K
			int _size = 0, r_size;
			uint8 *buf = offset > 524288 ? new uint8[GN_USING_FILE_BUF] : loc_buf;
            while(_size < offset){ //一直解压读取数据直到指定的偏移位置为止
			 	 r_size = ((offset - _size) > GN_USING_FILE_BUF)? GN_USING_FILE_BUF : (offset - _size);
				 if(unzReadCurrentFile(uf, buf, r_size) < 0)break;
                 _size += r_size;
			}
            if(buf != loc_buf)delete [] buf;
			if(_size >= offset){
			    if(unzReadCurrentFile(uf,buf_out,buf_size) >= 0)ret = buf_size;
			}
		}
        unzCloseCurrentFile(uf);
		return ret;
	}
   #endif
	GNf::sFILE *fp = GNf::Open(filename,"rb",usTYPE_size, ph);
	if(!fp)return -1;
	int len = GNf::GetSize(fp);
	if(offset >=  len){ GNf::Close(fp); return 0; }
    if(buf_size > len - offset)buf_size = len - offset;
	GNf::Seek(fp, offset, GNf::eSEEK_START);
    int rsize = GNf::Read(buf_out,1, buf_size ,fp);
	GNf::Close(fp);
    return rsize;
}
GN_API char* GNf::FileFullLoadToMemory(char *filename,int *size_out,int usTYPE_size, void  *ph){
   #ifdef GN_USING_UNZIP
	if(usTYPE_size < 0){ //从压缩包里读取
       #define GT_TMP_FILE_FULL_LOAD_FROM_ZIP( SIZEOUT ) \
	    unzFile uf = GT_OpenUnzipFile(filename, usTYPE_size);\
	    if(!uf)return 0;\
		char *buf = new char[usTYPE_size];\
		unzReadCurrentFile(uf, buf, usTYPE_size);\
        unzCloseCurrentFile(uf);\
        ( SIZEOUT ) = usTYPE_size

        GT_TMP_FILE_FULL_LOAD_FROM_ZIP( *size_out );
		return buf;
	}
   #endif
   #define GT_TMP_FILE_FULL_LOAD( HOST ) \
	GNf::sFILE *fp = GNf::Open(filename,"rb",usTYPE_size, HOST );\
	if(!fp)return 0;\
    int len = GNf::GetSize(fp);\
	char *mem = new  char[len];\
	if(GNf::Read(mem,1,len,fp) != len){\
        delete [] mem; mem = 0;\
	}\
	GNf::Close(fp)

	GT_TMP_FILE_FULL_LOAD( ph );
    *size_out = len;
	return mem;
}
GN_API char* GNf::FileFullLoadToMemoryText(char *filename,int *len_out,int usTYPE_size, void  *ph){
   #ifdef GN_USING_UNZIP
    if(usTYPE_size < 0){
        GT_TMP_FILE_FULL_LOAD_FROM_ZIP( *len_out );
        GN_StrBinaryToTextData(&buf, len_out);
	    return buf;
	}
   #endif
    GT_TMP_FILE_FULL_LOAD( ph );
    *len_out = len;
	if(mem)GN_StrBinaryToTextData(&mem, len_out);
	return mem;
}
GN_API GNf::sFILE *GNf::OpenCreate(char *filename, const char *mode, void  *ph){
    char *_filename;
	GNf::sFILE *fp = GNf::Open(filename, mode, GNf::eFILE_STD, ph);
	if(!fp && !strchr(mode,'r')){
		char dir_[GN_MAXFILEPATH];
		dir_[0] = 0;
		GN_StrGetDirName(filename, dir_);
		if(dir_[0] && GNf::Access(dir_,0,ph) && !GNf::Mkdir(dir_,ph))fp = GNf::Open(filename, mode, GNf::eFILE_STD, ph);
	}
	return fp;
}
GN_API bool GNf::GetAppCurPath(char *dir_out, void  *ph){
	return GT_GetAppCurPath(dir_out) ? true : false;
}
GN_API P< P<char> > GNf::EnumSystemDirectory(void  *ph){
	return GT_EnumSystemDirectory();
}

GN_API void (*GNf::OnFileOpenOKCallBack)(int filesize, char *filename, const char *mode, int usTYPE_size, void  *ph) = 0;

GN_API int GNf_OpenModeCharToInt(const char *open_mode){
    int mk = 0;
	char *omi = (char*)open_mode;
	while(*omi){ //判断是读是写
		switch(*omi){
	    case 'r': mk |= 1; break;
		case '+': mk |= 2; break;
		case 'a': mk |= 4; break;
		case 'w': mk |= 8; break;
		}
		++omi;
	}
    return mk;
}
GN_API bool  GN_StrFileToKey(GNf::sFILE *text, char key){
	//if(!text)return;
	while(!GNf::Eof(text)){
		if((char)GNf::Getc(text) == key)return 1;
	}
    return 0;
}
GN_API bool GN_StrcmpFileSN(const char *str1, GNf::sFILE *text){
    //if(!str1 || !text)return false;
    bool ret = true;
	char *p1 = (char*)str1;
	while(*p1){
		if(*p1 == ((char)GNf::Getc(text)))++p1;
		else { ret = false; break; }
	}
	GNf::Seek(text,-1,GNf::eSEEK_CUR);
	register int p;
	for(;;){
	    p = GNf::Getc(text);
		if(!isspace(p)){   //把文件流的指针移到空白间断处
			if(!p){ GNf::Seek(text,0,GNf::eSEEK_END); return ret; } //到文件结尾了
		}else break;
	}
	while(isspace(GNf::Getc(text))); //把文件流的指针移到下一字串处
	GNf::Seek(text,-1,GNf::eSEEK_CUR);
	return ret;
}
GN_API bool GN_StrcmpFileEX(const char *str1, GNf::sFILE *text){
    char *p1 = (char*)str1;
    while(*p1 <= 32)++p1;
    while(GNf::Getc(text) <= 32);
    GNf::Seek(text,-1,GNf::eSEEK_CUR);
	while(*p1 > 32){
	    if(*p1 == (char)GNf::Getc(text))++p1;
		else return false;
	}
    if(GNf::Getc(text) <= 32)return true;
	else return false;
}
GN_API char* GN_StrCopyFileSegmentToData(GNf::sFILE *load,char start_key,char end_key,int *size_out){
	if(!size_out)return 0;
	if(!load){ *size_out = 0; return 0; }
	int sn = 0;
	char *ret = (char*)malloc(GNf::GetSize(load)); //分配整个文件空间大小的空间
	char *pc = ret;
	bool starting = false,ended = false;
	register char p;
	for(;;){
	   p = (char)GNf::Getc(load);
	   if( !p )break;
	   else if(p == start_key){
		   if(sn <= 0){
			   sn = 1;
			   while(isspace(GNf::Getc(load)));  //把文件指针移动到条过空白处
			   starting = true;   //标记开始
			   GNf::Seek(load,-1,GNf::eSEEK_CUR);
			   continue;

		   }else ++sn;

	   }else if(p == end_key){
		   --sn;
		   if(sn == 0){
			   --pc;
			   while(isspace(*pc))--pc; //把结束指针向后移到可视的字符处的后一个
               ++pc;
			   p = (char)GNf::Getc(load);
			   ended = true;      //标记结束并退出循环
			   break;
		   }
	   }
	   if(starting){ *pc = p; ++pc; }
	}
	if(starting && ended){ //头尾符都搜到
	    int size = pc - ret;
		char *ret2 = (char*)realloc(ret,size+1);
        ret2[size] = 0;
        *size_out = size;
        return ret2;

	}else{  //没配对当没找到 释放临时内存
		free(ret);
		*size_out = 0;
		return 0;
	}
}
GN_API bool GN_StrFileSeekToSegmentBack(GNf::sFILE *text,char start_key,char end_key){
	if(!text)return 0;
	int sn = 0;
	register char p;
	for(;;){
	    p = (char)GNf::Getc(text);
	    if( !p )return 0;
	    else if(p == start_key){
		    if(sn <= 0)sn = 1;
			else ++sn;

	    }else if(p == end_key){
		    --sn;
			if(!sn){ p = (char)GNf::Getc(text); return 1; }
	    }
	}
	return 0;
}
GN_API int GN_StrRemoveFlieSegment(char *filename, char split_key, const char *include_str, int sn, bool check_first){
	int f_size;
	char *buf = GNf::FileFullLoadToMemoryText(filename,&f_size,GNf::eFILE_STD);
    if(!buf)return -2;

    int ret = GN_StrRemoveSegment(buf,split_key,include_str,sn, check_first);
	if(ret){
		GNf::sFILE * fp = GNf::Open(filename,"w",GNf::eFILE_STD);
		if(fp){
			GNf::Printf(fp,"%s",buf);
			GNf::Close(fp);
		}else ret = -2;
	}
	delete []buf;
    return ret;
}
GN_API P< P<char> > GN_StrLoadWordFromFile(const char *filename){
    GN::sLIST< P<char> > word_list;
	int f_size;
	char *text_data = GNf::FileFullLoadToMemoryText((char*)filename,&f_size);
	if(text_data){
		 //分析文本的词段
		 int id = 0, sid;
         char * p_data = text_data, *p_data2 = 0, *p_data3;
		 while(*p_data){
			 if(*p_data == '#'){ //找到一个词条标记

               #define GT_TMP_WORD_PUSH \
				if(p_data2){     /*刚才有记下词条的开始位置*/\
					p_data3 = p_data-1;\
					if(*p_data3 == '\n'){ /*如果最近处理词条的结尾处有很多换行 就忽略最末两个换行*/\
						--p_data3;\
						if(*p_data3 == '\n')--p_data3;\
					}\
					p_data3[1] = 0;\
				    word_list.push_back( P<char>(p_data2,0) ); /*加入一个词条*/\
                    p_data2 = 0;\
					++id;\
				}
				GT_TMP_WORD_PUSH
				++p_data;
				if(sscanf(p_data,"%d",&sid)){ //读取词条号是否正确
					if(id != sid){  //编号不符代表有误,取消单词读取
						word_list.clear();
						GN_Log("读取语言句子失败! 句子格式 错误!\n");
						break;
					}
					while(*p_data >= '0' && *p_data <= '9')++p_data;  //跳过词号处

                    //正确跳过词号分隔的段落
					if(*p_data == '\n')++p_data;
					p_data2 = p_data;
				}
                if(*p_data == 0)break; //已经到文件结尾
			 }
			 ++p_data;
		 }
         GT_TMP_WORD_PUSH
		 delete [] text_data;
	}
    P< P<char> > words;
	if(!word_list.empty()){
		words.New(word_list);
        word_list.clear();
	}
    return words;
}
GN_API bool GN_StrPathIsEqual(const char *str1,const char *str2){
	bool g_src = GN_StrIsGlobalPath(str1), g_dst = GN_StrIsGlobalPath(str2);
	if(g_src != g_dst){
		char g_path[384];
		unsigned int app_path_len = GT_GetAppCurPath(g_path);
		g_path[app_path_len++] = '/';
		if(g_src){
			strncpy(g_path+app_path_len, str2, 384 - app_path_len - 1); g_path[384 - 1] = 0;
			return GN_StrcmpPath(str1, g_path);
		}else{
			strncpy(g_path+app_path_len, str1, 384 - app_path_len - 1); g_path[384 - 1] = 0;
			return GN_StrcmpPath(g_path, str2);
		}
	}else return GN_StrcmpPath(str1, str2);
}
GN_API char* GN_StrGetInAppDirPath(const char *str){
	if(GN_StrIsGlobalPath(str)){
		char app_path[GN_MAXPATH];
		GNf::GetAppCurPath(app_path, 0);
		unsigned int path_len = strlen(app_path), str_len = strlen(str);
		if(str_len > path_len && (str[path_len] == '/' || str[path_len] == '\\')){
			char file_path[GN_MAXPATH];
			strncpy(file_path, str, path_len); 
			file_path[path_len] = 0;
			if(GN_StrcmpPath(app_path, file_path))return (char*)str + (path_len + 1);
		}
		return 0;
	}else return (char*)str;
}
/********************************************************************************************************************************/
GN_API char				    *GN_Words_No_Yes[2] = { "否", "是" };
GN_API P< P<char> >          GN_Words;

GN_API bool GN_InitWords(){
	/*if(GN_Words)return true;
	GN_Words = GN_StrLoadWordFromFile(GN_SysDir("language.txt"));

	if(!GN_Words){ //建立默认单词

	}
	*/
	unsigned int i, num;
	for(i = 0, num = GN_Words.Num(); i < num; ++i){

	}
	return true;
}
/********************************************************************************************************************************/
#if defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
extern char GT_CurAppDirW[];
#endif
GN_API GN::sLIST< P<char> >  GN_ErrorList;
GN_API P<char>               GN_ErrorLast;

GN_API void GN_ErrorInfoClear(){
	GN_ErrorLast.Delete();
	//if(!GN_ErrorList.empty())GN_ErrorList.clear();
}
GN_API bool GN_RunErrorOutputOn = true;

char GT_RunLogOpenMode[2] = "w";
GN_API void GN_Log(const char *fmt, ...){
	if(!GN_RunErrorOutputOn)return;

	static char run_filename[256] = {0};
	if(!run_filename[0]){
		GNf_DirStatePush( false );
		GN_StrToSysFilePath(run_filename,GN_RunErrorFileName,"w", false);
		GNf_DirStatePop();
	}
	FILE *fp = fopen(run_filename,GT_RunLogOpenMode);
	if(fp){
		va_list va;
	    va_start(va,fmt);
	    vfprintf(fp,fmt,va);
        va_end(va);
	    fclose(fp);

       #if defined GT_USING_ANDROID_JNI || defined GT_USING_ANDROID
	    fp = fopen(GN_DocDir(GN_RunErrorFileName),GT_RunLogOpenMode); //Android 多输出个文件到Sdcard 以方便查阅
	    if(fp){
            va_start(va,fmt);
            vfprintf(fp,fmt,va);
            va_end(va);
            fclose(fp);
	    }
       #endif
		GT_RunLogOpenMode[0] = 'a';
	}
}
GN_API char *GN_RunErrorFileName = "RunLog.txt";

/************************************************************ 插件加载 **********************************************************/

GN_API void GN_PluginFileImageAdd(int *module, GT_PFUNC_VOID create_func){
	(((GNg::iFILE_IMAGE*(*)())create_func)())->module = module;
}
GN_API void GN_PluginFileAudioAdd(int *module, GT_PFUNC_VOID create_func){
	(((GNa::iFILE_AUDIO*(*)())create_func)())->module = module;
}
GN_API int GN_PluginsLoad_(const char *class_name, void (*add_plugin_func)(int*,GT_PFUNC_VOID)){
	static char pluginsDir[128] = "Plugins/";
	char *pstr = pluginsDir + 8;
	strncpy(pstr, class_name, 128 - 8 - 1); pluginsDir[128 - 1] = 0; pstr += strlen(class_name);
	*pstr = '/'; ++pstr;
	strncpy(pstr, "*.*", 128 - (int)(pstr - pluginsDir) - 1); pluginsDir[128 - 1] = 0;
	GN::sLIST< P<GNf::sFILE_INFO> > files, dirs;
	if(GNf::FindFileDirInfoData(pluginsDir, false, &files, &dirs, 0) || dirs.empty())return 0;
	int *mod, mod_num = 0;
	char *v__, *version = GN_INTERFACE_VERSION, *version1, *del_plugin_str = " 如您不想要该插件 可以前往该插件目录把它删除即可!";
	char *const (*vfunc)();
	GT_PFUNC_VOID pfunc;
	GNf::sFILE_INFO *_dir;
	char *dir_name;
	GN::sLIST< P<GNf::sFILE_INFO> >::iterator itd = dirs.begin(), etd = dirs.end();
	for(; itd != etd; ++itd){
		_dir = *itd;
		dir_name = _dir->name;
		if(dir_name[0] == '.'){
			if(dir_name[1] == 0 || (dir_name[1] == '.' && dir_name[2] == 0))continue;
		}
		sprintf(pstr, "%s/%s%s", dir_name, dir_name, GN_StrGetModuleExtName(GN_eMODULE_DLL));
		mod = GT_ModuleOpen(pluginsDir);
		if(mod){
			vfunc = (char*const(*)())GT_ModuleGetFunction(mod, "Version");
			if(vfunc){
				version1 = vfunc();
				v__ = strchr(version, '.');
				if(strncmp(version1, version, v__ - version)){
					GN_Log("插件 %s 版本 %s 与引擎版本 %s 不兼容! 容易造成程序闪退 %s\n", pluginsDir, version1, version, del_plugin_str);
				}else if(atoi(strchr(version1, '.') + 1) > atoi(v__ + 1)){
					GN_Log("插件 %s 次版本 %s 大于引擎次版本 %s ! 容易造成程序闪退 %s\n", pluginsDir, version1, version, del_plugin_str);
				}
			}else{
				GN_Log("未能获取到插件 %s 版本! 容易造成程序闪退 %s\n", pluginsDir, del_plugin_str);
			}
			pfunc = GT_ModuleGetFunction(mod, "Create");
			if(pfunc || vfunc){
				add_plugin_func(mod, pfunc);
				++mod_num;
			}else{
				GT_ModuleClose(mod);
				GN_Log("加载插件 %s 失败! 插件不完整 缺少创建方法 %s\n", pluginsDir, del_plugin_str);
			}
		}else GN_Log("加载插件 %s 失败! 插件损坏或与引擎版本 %s 不兼容! %s\n", pluginsDir, version, del_plugin_str);
	}
	return mod_num;
}
GN_API int	GN_PluginsLoadAll(){
	return GN_PluginsLoad(FileImage)
		 + GN_PluginsLoad(FileAudio)
		 + GN_PluginsLoad(UIObject)
		 + GN_PluginsLoad(2DObject);
}
GN_API int	GN_PluginsRemoveAll(){
	return GN_PluginsFileImageRemove() + GN_PluginsFileAudioRemove() + GN_PluginsUIObjectRemove()
		+ GN_Plugins2DObjectRemove();
}
#ifdef _GN_UI_H_
extern GN::sLIST<int*> GT_UIObjectPlugins;
#endif
#ifdef _GN_2DSCENE_H_
extern GN::sLIST<int*> GT_2DObjectPlugins;
#endif
GN_API GN::sLIST<int*> *const GN_PluginsClassifyGet(uint8 obj_class){
	switch(obj_class){
   #ifdef _GN_UI_H_
	case 0: return GT_UIObjectPlugins.empty()? 0: &GT_UIObjectPlugins;
   #endif
   #ifdef _GN_2DSCENE_H_
	case 1: return GT_2DObjectPlugins.empty()? 0: &GT_2DObjectPlugins;
   #endif
	}
	return 0;
}
int GT_RemoveListPlugins(GN::sLIST<int*> &plugins){
	int count = plugins.size();
	if(count){
		GN::sLIST<int*>::iterator itp = plugins.begin(), etp = plugins.end();
		for(; itp != etp; ++itp)GT_ModuleClose(*itp);
		plugins.clear();
	}
	return count;
}
#endif //endif _GN_FILE_H_
