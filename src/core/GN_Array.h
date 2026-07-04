#ifndef _GN_ARRAY_H_
#define _GN_ARRAY_H_
/************************************************普通指针内存操作**************************************************************************/
//安全释放指针
#define GN_MemFree(p)     { if(p){ free     (p);   (p)=0; } }
//安全删除指针
#define GN_Delete(p)      { if(p){ delete   (p);   (p)=0; } }
//安全删除指针数组
#define GN_DeleteArray(p) { if(p){ delete[] (p);   (p)=0; } }

//安全释放指针表所指的内存(指针表数组地址,指针表指针的个数)
#define GN_FreePPMem(pp , n) {\
	if(pp){\
	    unsigned int di = 0, dn = (unsigned int)(n);\
		for(; di < dn ; ++di){ if((pp)[di])free((pp)[di]); }\
        free(pp);\
        (pp) = 0;\
	}\
}
//安全删除指针表所指的数据(指针表数组地址,指针表指针的个数)
#define GN_DeletePPData(pp , n) {\
	if(pp){\
	    unsigned int di = 0, dn = (unsigned int)(n);\
		for(; di < dn ; ++di){ if((pp)[di])delete ((pp)[di]); }\
        delete [] (pp);\
        (pp) = 0;\
	}\
}
//安全删除指针表所指的数组数据(指针表数组地址,指针表指针的个数)
#define GN_DeletePPArray(pp , n) {\
	if(pp){\
	    unsigned int di = 0, dn = (unsigned int)(n);\
		for(; di < dn ; ++di){ if((pp)[di])delete [] ((pp)[di]); }\
        delete [] (pp);\
        (pp) = 0;\
	}\
}
//安全删除安全指针表所指的数据(安全指针表)
#define GN_DelPPData( pp ) {\
	if(pp){\
	    unsigned int di = 0, dn = (pp).Num();\
		for(; di < dn ; ++di){ if((pp)[di])delete ((pp)[di]); }\
        (pp).Delete();\
	}\
}
//安全释放指针资源
#define GN_Release(p)     { if(p) { (p)->Release(); (p)=0; } }

//清除连表指针内容(指针数据类型 , 连表)
#define GN_DeleteListP( p_class_type , p_list ) \
   if(! (p_list) .empty()){\
		GN::sLIST< p_class_type *>::iterator it = (p_list) .begin(), et = (p_list) .end();\
		for(;it != et ; ++it)delete (*it);\
		(p_list) .clear();\
   }
//清除连表指针数组内容(指针数据类型 , 连表)
#define GN_DeleteListPArray( p_class_type , p_list ) \
   if(! (p_list) .empty()){\
		GN::sLIST< p_class_type *>::iterator it = (p_list) .begin(), et = (p_list) .end();\
		for(;it != et ; ++it)delete [] (*it);\
		(p_list) .clear();\
   }
//安全复制指针数据一个( 接收目标指针-未分配的 , 复制源指针  , 数据类型)
#define GN_CopyOnce( desc , src , DATA_TYPE) \
	if( desc ) delete  (desc);\
    if( src ){\
        (desc) = new DATA_TYPE;\
        * (desc) = * (src);\
	}else (desc) = 0

//安全复制指针数组( 接收目标指针-未分配的 , 复制源指针 , 数据数 , 数据类型)
#define GN_CopyArray( desc , src , num , DATA_TYPE) \
    if( desc ) delete [] (desc);\
    if( src ){\
        (desc) = new DATA_TYPE [ num ];\
	    unsigned int ci = 0, ce = (unsigned int)(num);\
        for( ; ci < ce; ci++) (desc) [ci] = (src) [ci];\
	}else (desc) = 0

//重分配数组大小(数组, 元素个数, 新元素个数, 数据类型)
#define GN_ReSizeArray( data , num , num_ , DATA_TYPE ) \
	if(data){\
	    if(num_){\
			DATA_TYPE *data_1 = data;\
			data = new DATA_TYPE[ num_ ];\
			memcpy(data, data_1, sizeof(DATA_TYPE) * (num_ < num? num_ : num));\
			  delete [] data_1;\
		}else{\
			delete [] data;\
			data = 0;\
		}\
	}\
	num = num_

//保留数组前指定个数的数据 裁掉后面的(数组 , 保留个数 , 数据类型 )
#define GN_KeepArray( data , num , DATA_TYPE ) {\
	DATA_TYPE *data_1 = data;\
	data = new DATA_TYPE[ num ];\
	memcpy(data, data_1, sizeof(DATA_TYPE) * num);\
	delete [] data_1;\
}
//设置数组里所有元素为指定值( 数组 , 元素个数 , 值 )
#define GN_SetArray( data , num , value ) {\
	unsigned int _di = 0, _dn = (unsigned int)(num);\
	for(; _di < _dn; ++_di)(data)[_di] = (value);\
}
//设置数组里所有元素与源数组里相等( 数组 , 源数组 , 数组元素个数 )
#define GN_SetArrayFromArray( dst , src , num ) {\
    unsigned int _di = 0, _dn = (unsigned int)(num);\
	for(; _di < _dn; ++_di)(dst)[_di] = (src)[_di];\
}
template<class T, class T2>
//设置数组里所有元素所有属性值均为0(数组 , 元素个数)
inline void GN_SetArray0(T *data, T2 num){ memset(data, 0, sizeof(T) * num); }

//以数组里前段值来填满整个数组( 数组 , 数组总大小字节 , 前段值大小字节 )
inline void GN_SetArraySelf(uint8 *data, unsigned int tot_size , unsigned int value_size ){\
	for(; (value_size << 1) <= tot_size ; value_size <<= 1)memcpy(data + value_size,  data, value_size);\
	if(tot_size > value_size)memcpy(data + value_size,  data, tot_size - value_size);\
}
//交换两数组里的数据并保持原顺序不变( 数组1 , 数组2 , 交换个数 , 数据类型 )
#define GN_SwapArrayKeepOrder( data , data2 , num , DATA_TYPE ) \
    DATA_TYPE _tmp ;\
    unsigned int _di = 0, _dn = (unsigned int)(num);\
	for(; _di < _dn; ++_di){\
      _tmp = data2[_di];\
      data2[_di] = data[_di];\
      data[_di] = _tmp;\
	}

template<class T, class T2>
//抽取源数组里特定间距的元素数据来构建另一数组(另一数组 需先分配好足够空间, 源数组, 源数组结束位置, 抽取间距, 抽取元素放到另一数组里的复制副本数)
void GN_MakeArray(T *dst, T *src, T *end, T2 span, T2 cn){
	switch(cn){
	case 1: for(; src < end; src += span, dst += cn)  dst[0] = *src; break;
	case 2: for(; src < end; src += span, dst += cn){ dst[0] = *src; dst[1] = *src; } break;
	case 3: for(; src < end; src += span, dst += cn){ dst[0] = *src; dst[1] = *src; dst[2] = *src; } break;
	case 4: for(; src < end; src += span, dst += cn){ dst[0] = *src; dst[1] = *src; dst[2] = *src; dst[3] = *src; } break;
	default:{
		T2 ci;
		for(; src < end; src += span, dst += cn){
			for(ci = 0; ci < cn; ++ci)dst[ci] = *src;
		}
		}break;
	}
}
template<class T , class T2 >
//移除数组里指定范围特定值的元素并保持原顺序不变( 数组, 值 , 范围起点索引, 范围结束索引) !如果有元素被移除了 参数end会变小
void GN_RemoveArrayElementKeepOrder(T * data , T em, T2 st, T2 &end){
	if(st >= end)return;
    T2 e = end - 1;
	while(st < e && data[st] != em)++st;
	if(st == e){
		if(data[st] == em)end = st;
		return;
	}
	T2 i_ = st + 1;
	for(;;){
		while(data[i_] == em){
            if(i_ < e)++i_;
			else { end = st; return; }
		}
        data[st] = data[i_];
		if(i_ < e){ ++i_; ++st; }
		else { end = st + 1; return; }
	}
}
template<class T , class T2 >
//逆向移除数组里指定范围特定值的元素并保持原顺序不变( 数组, 值 , 范围起点索引, 范围结束索引) !如果有元素被移除了 参数st会变大
void GN_RemoveArrayElementKeepOrderReverse(T * data , T em, T2 &st, T2 end){
	if(st >= end)return;
    T2 i = end - 1, s = st;
	while(i > s && data[i] != em)--i;
	if(i == s){
		if(data[i] == em)st = i + 1;
		return;
	}
	T2 i_ = i - 1;
	for(;;){
		while(data[i_] == em){
            if(i_ > s)--i_;
			else { st = i + 1; return; }
		}
        data[i] = data[i_];
		if(i_ > s){ --i_; --i; }
		else { st = i; return; }
	}
}
//过滤保留数组里指定范围达到指定条件的所有元素并保持原顺序不变( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被过滤掉 参数end会变小  !适合保留数比移除数小的处理
#define GN_FilterArrayElementIfKeepOrder( data , add_proc , cmp , st , end ) {\
    unsigned int di = ( st ), di2 = ( end );\
    if(di < di2){\
		for(; di < di2; ++di){\
		    add_proc ;\
			if(!( cmp ))break;\
		}\
		if(di < di2){\
			unsigned int di1 = di;\
			++di;\
			for(; di < di2; ++di){\
			    add_proc ;\
			    if( cmp )(data)[di1++] = (data)[di];\
			}\
            ( end ) = di1;\
	}	}\
}
//逆向过滤保留数组里指定范围达到指定条件的所有元素并保持原顺序不变( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被过滤掉 参数st会变大 !适合保留数比移除数小的处理
#define GN_FilterArrayElementReverseIfKeepOrder( data , add_proc , cmp , st , end ) {\
	if((st) < (end)){\
        unsigned int di2 = ( st ), di = ( end ) - 1;\
		for(; di > di2; --di){\
		    add_proc ;\
			if(!( cmp ))break;\
		}\
		unsigned int di1 = di;\
		if(di > di2){\
			--di;\
			for(; di > di2; --di){\
			    add_proc ;\
			    if( cmp )(data)[di1--] = (data)[di];\
			}\
		}\
        add_proc ;\
        if( cmp )(data)[di1] = (data)[di]; /*最后一个*/\
		else ++di1;\
        ( st ) = di1;\
	}\
}
//快速移除数组里指定范围达到指定条件的所有元素( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被移除了 参数end会变小 !适合移除数比保留数小的处理
#define GN_RemoveArrayElementIf( data , add_proc , cmp , st , end ) {\
	if((st) < (end)){\
        unsigned int di = ( st ), di2 = ( end ) - 1;\
		for(; di < di2; ){\
		    add_proc ;\
			if( cmp )(data)[di] = (data)[di2--];\
		    else ++di;\
		}\
		add_proc ;\
		if( cmp )(end) = di; /*最后一个*/\
        else     (end) = di + 1;\
	}\
}
//快速逆向移除数组里指定范围达到指定条件的所有元素( 数组 , 比较前处理 , 条件 如 data[di]->x > 0.3f , 范围起点索引, 范围结束索引 ) !如果有元素被移除了 参数st会变大 !适合移除数比保留数小的处理
#define GN_RemoveArrayElementReverseIf( data , add_proc , cmp , st , end ) {\
	if((st) < (end)){\
        unsigned int di2 = ( st ), di = ( end ) - 1;\
		for(; di > di2; ){\
		    add_proc ;\
		    if( cmp )(data)[di] = (data)[di2++];\
		    else --di;\
		}\
		add_proc ;\
		if( cmp )(st) = di + 1; /*最后一个*/\
		else     (st) = di;\
	}\
}
template<class T , class T2 >
//加入一个在数组里不相同的元素到尾部( 数组 , 元素个数 如成功加入该值会+1, 加入元素 )返回数组里与加入元素值相同的首个ID 如果成功加入返回加入后元素个数
T2 GN_ArrayAddNotEqual( T *data, T2 &num , T elm ){
	T2 di = 0;
	for(; di < num; ++di){
		if(data[di] == elm)return di;
	}
	data[di] = elm; ++num;
	return num;
}
template<class T , class T2 >
//合并数组2里不同于数组1里的元素到数组1的尾部( 数组1 , 数组1元素个数 如成功加入该值会增大, 数组2 , 数组2元素个数 )返回共加入了多少个元素 !数组一需先分配好足够多的空间
T2 GN_ArrayMergeNotEqual( T *data, T2 &num , T *data2, T2 num2 ){
	T2 num1 = num;
	T2 i, i2 = 0;
	for(; i2 < num2; ++i2){
		for(i = 0; i < num; ++i){
			if(data2[i2] == data[i])break;
		}
		if(i >= num)data[num++] = data2[i2];
	}
	return num - num1;
}
template<class T , class T2 >
//合并数组2里对应分量比数组1大的值到数组1里(数组1, 数组2, 数组元素个数)
void GN_ArrayMagerMax( T *data, T *data2, T2 num ){
	T2 i = 0;
	for( ; i < num; ++i){
		if(data2[i] > data[i])data[i] = data2[i];
	}
}
template<class T , class T2 >
//合并数组2里对应分量比数组1小的值到数组1里(数组1, 数组2, 数组元素个数)
void GN_ArrayMagerMin( T *data, T *data2, T2 num ){
	T2 i = 0;
	for( ; i < num; ++i){
		if(data2[i] < data[i])data[i] = data2[i];
	}
}
//去掉数组里值相同的元素使数据内每个数据都是不同的( 数组 ,  元素个数 ) !!如果有相同内容去掉后数组会自动缩紧,元素数将会减少
#define GN_UniqueArray( data , num ) {\
	if(num){\
        unsigned int di, dj, dn = (unsigned int)( num ) - 1;\
		for(di = 0; di < dn; ++di){\
		    for(dj = di+1; dj <= dn; ++dj){\
				while( (data) [di] == (data) [dj] && dj <= dn){\
					(data) [dj] = (data) [dn];\
                    dn--;\
				}\
		    }\
		}\
        (num) = dn+1;\
	}\
}
//交换反转数组里的元素次序(  数组 ,  元素个数 , 数据类型)
#define GN_SwapArray( data , num , DATA_TYPE) {\
    DATA_TYPE tmp;\
    unsigned int di1 = 0, di2 = (unsigned int)(num) - 1;\
	for(; di1 < di2; ++di1, --di2){\
	    tmp       = (data)[di1];\
        (data)[di1] = (data)[di2];\
		(data)[di2] = tmp;\
	}\
}

//交换三角型相关数据的绕序 即点a - c值交换( 三角点数据类型 , 三角型相关数据 , 三角型数 )
#define GN_SwapTriDataCCW( _DATA_TYPE , tri_data , tri_num ) {\
    _DATA_TYPE _v_; \
    int _fi_  = 0, _fn_ = (int)(tri_num);\
    for(; _fi_ < _fn_; ++_fi_){\
        _v_ = (tri_data) [_fi_].a;\
        (tri_data) [_fi_].a = (tri_data) [_fi_].c;\
        (tri_data) [_fi_].c = _v_;\
    }\
}
//用快速2分查找法搜索有序数组里最接近条件的一个元素索引( 比较条件 , 查找首元素, 查找末元素 , 元素索引结果输出)  !!注意用2分快速查找法的数组必须是按从小到大或从大到小地有序排列好, 数据从小到大排列比较条件符号必需为 (特定量 < 数据[ID_OUT]), 数据从大到小排列比较条件符号比需为 (特定量 > 数据[ID_OUT]) !!处理完之后参数START_ID和END_ID也将受到改变
#define GN_FindNearDataID( COMPARE , START_ID , END_ID , ID_OUT ) \
	for(;;){\
	    (ID_OUT) = ( (END_ID)  -  (START_ID) )>>1;\
	    if( ID_OUT ){\
		    (ID_OUT) += (START_ID) ;\
		    if( COMPARE ) (END_ID) = (ID_OUT) ;\
		    else (START_ID) = (ID_OUT) ;\
	    }else{/*已经不能再分了*/\
		    (ID_OUT) = (END_ID) ;\
	        if( COMPARE ) (ID_OUT)  = (START_ID) ;\
			break;\
	    }\
	}

//用2分查找法搜索有序数组里最接近条件的临近两个元素索引( 比较条件 , 查找首元素, 查找末元素, 临时元素索引缓冲变量)  !!注意用2分快速查找法的数组必须是按从小到大或从大到小地有序排列好, 数据从小到大排列比较条件符号必需为 (特定量 < 数据[ID]), 数据从大到小排列比较条件符号比需为 (特定量 > 数据[ID]) !!处理完之后 临近两个元素索引也由参数START_ID和END_ID来返回
#define GN_FindNearDataID2( COMPARE , START_ID , END_ID , ID ) \
	for(;;){\
	    (ID) = ( (END_ID)  -  (START_ID) )>>1;\
	    if( ID ){\
		    (ID) += (START_ID) ;\
		    if( COMPARE ) (END_ID) = (ID) ;\
		    else (START_ID) = (ID) ;\
	    }else break; /*已经不能再分了*/\
	}

#if !defined(_ALGORITHM_) && !defined(_GLIBCXX_ALGORITHM) && !defined(_LIBCPP_ALGORITHM)
//数组排序(数据类型,数组指针,元数个数,比较方法,比较的成员) 这个比较合适简单数据的数组
#define GN_Sort(class_type,p_array,count,compare,member) {\
    unsigned int i,j,n=(count)-1;\
    bool exc;\
	class_type tmp;\
    for(i=0;i<n;++i){\
        exc = true;\
	    for(j=n;j>i;--j){\
		    if((p_array)[j] member compare (p_array)[j-1] member){\
			    tmp          = (p_array)[j];\
			    (p_array)[j]   = (p_array)[j-1];\
			    (p_array)[j-1] = tmp;\
			    exc = false;\
		    }\
        }\
	    if(exc)break;\
    }\
}
//数组排序复制(数据类型,储存排好数据的指针,要整理的数组指针,元数个数,比较方法,比较的指向成员)这个比较合适结构体数据的数组
#define GN_SortP(class_type,dp_array,p_array,count,compare,member) {\
    unsigned int i,j,n=(count)-1;\
    bool exc;\
	class_type **pdata=0;\
    pdata = new class_type *[count];\
	for(i=0;i<=n;++i)pdata[i] = &(p_array)[i];\
	class_type *tmp;\
    for(i=0;i<n;++i){\
        exc = true;\
	    for(j=n;j>i;--j){\
		    if(pdata[j] member compare pdata[j-1] member){\
			    tmp        = pdata[j];\
			    pdata[j]   = pdata[j-1];\
			    pdata[j-1] = tmp;\
			    exc = false;\
		    }\
        }\
	    if(exc)break;\
    }\
    for(i=0;i<=n;++i)(dp_array)[i] = *(pdata[i]);\
	if(pdata)delete []pdata;\
}
#else
//数组排序(数据类型,数组指针,元数个数,比较方法函数)
#define GN_Sort(class_type,p_array,count,comparefunc) std::sort<class_type *>((p_array),(p_array)+(count),comparefunc)
#endif
//采用二分数组排序(数据类型,数组指针,元数个数,比较方法,比较的成员) 这个比较合适简单数据的数组 !速度较快 但仅适合判断同一个值的线性大小排序, 和 std::sort 性质差不多
#define GN_Sort2(class_type,p_array,count,compare,emg) {\
  if((count) > 1){\
    unsigned int j, j_, n = (count)-1;\
	unsigned int oid = n, type_size = sizeof(class_type);\
	class_type tmp;\
	j = n;\
	j_ = j-1;\
	if( compare( emg ((p_array)[j]), emg ((p_array)[j_]))){\
		GN_Swap( class_type , (p_array)[j], (p_array)[j_]);\
	}\
	for(--j; j > 0; --j){\
	    j_ = j-1;\
	    if( compare( emg ((p_array)[j]), emg ((p_array)[j_]))){\
		    tmp = (p_array)[j_];\
		    unsigned int sid = j, eid = n;\
		    GN_FindNearDataID( compare(emg tmp, emg ((p_array)[oid])) , sid , eid , oid ) \
            memmove((p_array)+j_, (p_array)+j, (oid-j+1) * type_size);\
            (p_array)[oid] = tmp;\
	    }\
    }\
  }\
}
template<class T>
//默认排序比较方法 由小到大( 比较的左值 , 比较的右值 )返回是否符合条件 符合条件的值排在前
inline bool GN_SortFuncDef(T left, T right){
    return left < right;  //!您可以复制这个模板函数 修改这里自定义自己的排序方式
}
template<class T>
//    排序比较方法 由大到小( 比较的左值 , 比较的右值 )返回是否符合条件 符合条件的值排在前
inline bool GN_SortFuncBigToSmall(T left, T right){
    return left > right;
}
//    排序比较方法 由小到大
#define     GN_SortFuncSmallToBig GN_SortFuncDef

/*-----------------------------------------------------------------------------------------------------------------------*/
template<class T, class T2, class T3, class F>
//按指定方法分类排列数据为达成条件|和未达成条件的两边(数据, 数据个数, 比较方法参考参数, 比较方法) 返回达成条件的数据个数
T2 GN_ArrayClassify(T *p_array, T2 count, T3 user_arg, F cmp_func){
	if(count){
		T tmp;
		T2 st = 0, end = count-1;
		for(; st < end; ){
            if(cmp_func(p_array[st], user_arg))++st;
			else{
                tmp = p_array[st];
                p_array[st] = p_array[end];
                p_array[end] = tmp;
                --end;
		}	}
        if(cmp_func(p_array[st], user_arg))++st;
		return st;
	}else return 0;
}
/*////////////////////////////////////////////////////////////////
比较方法函数参考(要分类数据里的其中一个元素, 传回给您的用户参数)
inline int Compare(T em, T3 user_arg){
    return em.member < user_arg.member;
}
*/////////////////////////////////////////////////////////////////

template<class T , class T2 >
//获得两数组值的总差距(数组1, 数组2, 比较数组个数) 返回总差距量
T GN_ArrayTotalGapGet(T * data, T * data2, T2 dn){
    T2 di = 0, n = dn;
	T  gt = 0;
	for( ; di < n; ++di)gt += (data)[di] > (data2)[di] ? (data)[di] - (data2)[di] : (data2)[di] - (data)[di];
    return gt;
}

template<class T , class T2 >
//获得两数组里不同值的平均差距(数组1, 数组2, 比较数组个数) 返回差距量
T GN_ArrayAverageGapGet(T * data, T * data2, T2 dn){
    T2 di = 0, n = dn, gn = 0;
	T g, gt = 0;
	for( ; di < n; ++di){
		g = (data)[di] > (data2)[di] ? (data)[di] - (data2)[di] : (data2)[di] - (data)[di];
		if(g){
            gt += g;
            ++gn;
	}	}
	if(gn)return gt / ( T )gn;
	else  return 0;
}
//使浮点类型数组里每个元素归一化 即>0为1 <0为-1 0不变(数组, 数据个数)
inline void GN_ArrayNormalizeFloat(float *p_array, int count){
    int i = 0;
	for(; i < count; ++i){
        if     (p_array[i] > 0.0f)p_array[i] =  1.0f;
		else if(p_array[i] < 0.0f)p_array[i] = -1.0f;
	}
}
//整片内存数据乘比率值(逻辑数据类型,内存片地址,内存片大小,相乘的比值)
#define GN_MemRatio(class_type,p_memory,uint_len,ratio) {\
    class_type  * pTarget;\
	uint8 *pmm = (uint8*)(p_memory);\
    unsigned int TargetSize = sizeof( class_type ), leni = 0, ulen = uint_len;\
	for(;leni < ulen ; leni += TargetSize){\
	    pTarget = ( class_type * )( & pmm[leni] );\
        * pTarget = * pTarget  *  (ratio);\
	}\
}
//比较两内存片里的数值是否相同(内存地址1, 内存地址2, 大小字节 )
GN_API inline bool GN_MemCompare( void *p_memory1 , void *p_memory2 , unsigned int size ){
    unsigned char *pm1 = (unsigned char *)p_memory1, *pm2 =  (unsigned char *)p_memory2;
    unsigned char *pme = pm1 + size;

	while(pm1 < pme){
	    if(*pm1 != *pm2)return false;
	    pm1++; pm2++;
	}
	return true;
}
template<class T , class T2 >
//比较两个数组里的数值是否近似(数组指针1, 数组指针2, 元数个数 , 容许误差值:不能为负数)
bool GN_MemCompareSimilar(T *fd1, T *fd2, T2 count, T v){
    T * fe = fd1 + count;
	for(; fd1 < fe; ++fd1,++fd2){
	    if((*fd1 > *fd2 ? *fd1 - *fd2 : *fd2 - *fd1) > v )return false;
	}
	return true;
}
//比较两个浮点数组里的数值是否近似(数组指针1, 数组指针2, 元数个数 , 容许误差值:不能为负数)
#define GN_MemCompareFloat GN_MemCompareSimilar

template<class T , class T2 >
//在数组里查找与指定值相近的值位置指针(数组指针 , 元数个数 , 值 , 容许误差值:不能为负数)返回位置指针 返回0表示数组里无相近的值
T *GN_ArrayGetSimilarElementId(T *fd, T2 count, T f, T v){
	T * fe = fd + count;
	for(; fd < fe; ++fd){
	    if((*fd > f ? *fd - f : f - *fd) < v)return fd;
	}
	return 0;
}
/********************************************************* 使数组整理为数组与索引 *************************************************************/

template<class T , class T2 , class T3>
//按现有数据来创建它对应的索引和数据(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数不能少于 data_num
T3 GN_IndexAndDataCreate(T *data_out, T2 *index_out, const T *data_in, T3 data_num){
	//数据生成索引
   #define GT_TMP_DATA_GEN_INDEXS( CMP ) \
	T3 i, i2, n2 = 0;\
    for(i = 0; i < data_num; ++i){\
	    for(i2 = 0; i2 < n2; ++i2){\
		    if( CMP ){\
		        index_out[i] = (T2)i2;\
			    break;\
		    }\
	    }\
	    if(i2 >= n2){\
	        data_out[n2] = data_in[i];\
		    index_out[i] = (T2)n2;\
		    ++n2;\
	    }\
    }\
    return n2;

	GT_TMP_DATA_GEN_INDEXS( data_in[i] == data_out[i2] )
}

template<class T , class T2 , class T3>
//按现有数据来创建它对应的索引和数据-小数类型数据(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数不能少于 data_num
T3 GN_IndexAndDataCreateFloat(T *data_out, T2 *index_out, const T *data_in, T3 data_num){
    unsigned int t_size = sizeof( T );
	GT_TMP_DATA_GEN_INDEXS( GN_MemCompare( (void*)(&data_in[i]) , (void*)(&data_out[i2]) , t_size ) )
}

template<class T , class T2 , class T3>
//集中紧揍整理好数据和对应的数据索引 使数据不存在相同的值(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数, 要整理索引数据输入, 索引数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数分别不能少于 data_num 和 index_num
T3 GN_IndexAndDataConcentrate(T *data_out, T2 *index_out, const T *data_in, T3 data_num, const T2 *index_in, T3 index_num){
   #define GT_TMP_INDEX_AND_DATA_CONCENTRATE( PROC ) \
	T2 *em_refs = new T2[data_num];\
    T3 i = 0, n = PROC (data_out, em_refs, data_in, data_num);\
	for(i = 0; i < index_num; ++i)index_out[i] = em_refs[ index_in[i] ];\
	delete [] em_refs;\
    return n;

    GT_TMP_INDEX_AND_DATA_CONCENTRATE( GN_IndexAndDataCreate )
}

template<class T , class T2 , class T3>
//集中紧揍整理好数据和对应的数据索引 使数据不存在相同的值-小数类型数据(整里好不相同的数据输出, 整理好的对应索引输出, 要整理数据输入, 数据数, 要整理索引数据输入, 索引数)返回整理后不相同的数据输出有多少个 !参数 data_out 和 index_out 需预先分配好空间 元素个数分别不能少于 data_num 和 index_num
T3 GN_IndexAndDataConcentrateFloat(T *data_out, T2 *index_out, const T *data_in, T3 data_num, const T2 *index_in, T3 index_num){
	GT_TMP_INDEX_AND_DATA_CONCENTRATE( GN_IndexAndDataCreateFloat )
}

template<class T , class T2 , class T3>
//整理移去特定段落未被引用的数据(数据首地址, 数据整理段落开始位置, 段落结束位置, 索引数据, 索引数)返回整理后数据段落新结束位置 !!适合用于简单数据类型
T3 GN_IndexAndDataRemoveNoUsed(T *data, T3 st, T3 end, T2 *index_s, T3 index_num){
   #define GT_TMP_INDEX_AND_DATA_REMOVE_NO_USED( COPY_PROC ) \
	T2 idx_buf[65536];\
	T2 *idxs = end > 65536 ? new T2 [end] : idx_buf;\
	memset(idxs+st, 0xff, sizeof(T2) * (end - st));\
    T3 ii;\
	for(ii = 0; ii < index_num; ++ii)idxs[index_s[ii]] = index_s[ii];\
	T3 st2 = end - 1;\
	for( ; st < st2; ){\
		if(idxs[st] == 0xffff){ /*没有引用到的索引*/\
			if(idxs[st2] != 0xffff){\
                COPY_PROC ;\
				idxs[st2] = st;\
			}\
            idxs[st] = idxs[st2--];\
		}else ++st;\
	}\
	if(idxs[st] != 0xffff)++st;\
	for(ii = 0; ii < index_num; ++ii)index_s[ii] = idxs[index_s[ii]];\
    if(idxs != idx_buf)delete [] idxs;\
    return st;

    GT_TMP_INDEX_AND_DATA_REMOVE_NO_USED( data[st] = data[st2] )
}

template<class T , class T2 , class T3>
//整理移去特定段落未被引用的数据(数据移动复制处理回调(传回给您的数据首地址, 当前要复制到的目标数据位置, 当前复制数据源位置), 数据首地址, 数据整理段落开始位置, 段落结束位置, 索引数据, 索引数)返回整理后数据段落新结束位置 !!适合用于复杂数据类型 或类对象
T3 GN_IndexAndDataRemoveNoUsed(void (*on_data_cpy_proc)(T *data, T3 dst_i, T3 src_i), T *data, T3 st, T3 end, T2 *index_s, T3 index_num){
    GT_TMP_INDEX_AND_DATA_REMOVE_NO_USED( on_data_cpy_proc(data, st , st2) )
}
/*  历遍搜索方法 模拟普通连表那样操作 ------------------------------------------------------------------------------------------------ */
template<class TW , class T , class T2 >
struct GT_sIterator       //顺向搜索迭代器
{
    TW              *lw;    //指向关联列表轮
    T2               id;    //当前位置

	GT_sIterator(){}
	GT_sIterator(TW *_lw, T2 _id):lw(_lw), id(_id){}

	inline bool operator ==(const GT_sIterator & it)const{ return (this->lw == it.lw && this->id == it.id); }
	inline bool operator !=(const GT_sIterator & it)const{ return (this->lw != it.lw || this->id != it.id); }
    inline T & operator*() { return lw-> zElement(id); }
    inline T * operator->(){ return lw->zpElement(id); }

	GT_sIterator& operator++()   { ++id; return (*this); } /*模板代码:迭代器增减*/
	GT_sIterator  operator++(int){ GT_sIterator tmp = (*this); ++id; return tmp; }
	GT_sIterator& operator--()   { --id; return (*this); }
	GT_sIterator  operator--(int){ GT_sIterator tmp = (*this); --id; return tmp; }
};
template< class ITER >
struct GT_sReverseIterator :public ITER  //逆向搜索迭代器
{
    ITER& operator++()   { --(this->id); return (*this); } /*模板代码:迭代器增减*/
	ITER  operator++(int){ ITER tmp = (*this); --(this->id); return tmp; }
	ITER& operator--()   { ++(this->id); return (*this); }
	ITER  operator--(int){ ITER tmp = (*this); ++(this->id); return tmp; }
};
template<class TW , class T, class T2>
struct GT_sIteratorExt :public GT_sIterator< TW , T , T2 >   //顺向搜索迭代器扩展功能
{
	GT_sIteratorExt(TW *_lw){ this->lw = _lw; }

	//下标引用数据
	inline T &operator [](T2 _id){ return this->lw-> zElement(this->id + _id); }

	//转换为普通指针 这个一般在参数传递和赋值给普通指针会自动调用 连 if( P ) 也会调用的
	inline operator const T* () const{
		if(this->lw)return (T*)this->lw->zpElement(this->id);
		else return 0;
	}
	inline operator  T* () {
	    if(this->lw)return (T*)this->lw->zpElement(this->id);
		else return 0;
	}
};
namespace GN{
/*------------------------------------------------------ sARRAY -------------------------------------------------------------------*/
template<class T, class T2 = unsigned int>
struct sARRAY                               //通用简单的数组
{
    T               *data;                      //数据指针
    T2               num;                       //数据数
};
template<class T, class T2 = unsigned int>
struct sARRAY_POSE :public sARRAY<T, T2>     //通用简单的数组与处理点结构
{
    T2               pos;                       //当前位置
};
template<class T, class T2 = unsigned int>
struct sARRAY_BUF :public sARRAY<T, T2>     //数组缓冲 可不断随存储增长的数组
{
    T2               buf_len;                   //数据空间可容纳数据个数

	inline sARRAY_BUF():buf_len(0){ this->data = 0; this->num = 0; }
	inline sARRAY_BUF(T2 _len){ this->data = 0; this->New(_len); }      //分配数据(预分配多少个数据空间)
	inline ~sARRAY_BUF(){ this->Delete(); }
	
	//删除数据
	void Delete(){
		GN_DeleteArray(this->data);
		this->buf_len = 0;
		this->num = 0;
	}
	//分配数据(预分配多少个数据空间)
	void New(T2 _len){
	   #define GT_TMP_ARRAY_VEC_NEW \
		this->Delete();\
	    this->data = new T [_len];\
		this->buf_len = _len

		GT_TMP_ARRAY_VEC_NEW;
		this->num = 0;
	}
	//在缓冲末端存入一组数据(数据, 数据数)
	void Push(T *_array, T2 _num);

	T2   BufSize()const{ return sizeof(T) * buf_len; }  //获得数据空间所占大小字节
	T2   Count()const{ return this->num; }			    //获得实存数据个数
};
template<class T, class T2>
void sARRAY_BUF<T, T2>::Push(T *_array, T2 _num){
	if(this->num + _num > buf_len){
		buf_len = this->num + _num;
		buf_len += buf_len >> 1;
		T *data1 = new T[buf_len];
		memcpy(data1, this->data, sizeof(T) * this->num);
		delete [] this->data; this->data = data1;
	}
	memcpy(this->data + this->num, _array, sizeof(T) * _num);
	this->num += _num;
}
template<class T , class T2 = unsigned int>
struct sARRAY_VEC  :public sARRAY<T , T2> //用普通数组来模拟向量容器 std::vector 并且数据空间会自动增长
{
    T2               buf_len;                      //数据空间可容纳数据个数
    void           (*on_del_proc)(T *data,T2 num); //当内部数据要清理前的处理

   #define GT_TMP_ARRAY_ELEMENT_GET_DEF \
	inline T* zpElement(T2 id){ return this->data + id;  }  /*获得指定存取位置的数据元素指针*/\
	inline T& zElement (T2 id){ return this->data  [id]; }  /*获得指定存取位置的数据元素*/

	GT_TMP_ARRAY_ELEMENT_GET_DEF

	inline  sARRAY_VEC():buf_len(0),on_del_proc(0){ this->data = 0; this->num = 0; }
	inline  sARRAY_VEC(T2 _len):on_del_proc(0){ this->data = 0; this->New(_len); }                                    //分配数据(预分配多少个数据空间)
	inline  sARRAY_VEC(T2 _len, T2 _num, T & em_all):on_del_proc(0){ this->data = 0; this->New(_len, _num, em_all); } //分配数据并设置一些初值(预分配多少个数据空间, 从首部起设置多少个初值,  默认初始值)
	inline  sARRAY_VEC(T2 _len, T2 _num, T * p_data):on_del_proc(0){ this->data = 0; this->New(_len, _num, p_data); } //分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间, 设置数据个数, 普通指针)
//	inline  sARRAY_VEC(T2 _len, sLIST< T > &list):on_del_proc(0){ data = 0; this->New(_len, list); }			//分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间, 连表 )
	inline ~sARRAY_VEC(){ this->Delete(); }

	void Delete(); //删除数据

	//分配数据(预分配多少个数据空间)
	void New(T2 _len);

	//分配数据并设置一些初值(预分配多少个数据空间, 从首部起设置多少个初值,  默认初始值)
	void New(T2 _len, T2 _num, T & em_all);

	//分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间, 设置数据个数, 普通指针)
	void New(T2 _len, T2 _num, T * p_data);

    //分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间, 连表 )
//	void New(T2 _len, GN::sLIST< T > &list);

	T2   BufNum()const{ return buf_len; }       //获得数据空间个数
	T2   BufSize()const;                        //获得数据空间所占大小字节
	bool Has()const{ return this->num?1:0; }    //是否有实存数据
	bool empty()const{return this->num == 0; }  //是否无实存数据
	T2   size()const{ return this->num; }       //获得实存数据个数
	T2   Count()const{ return this->num; }      //获得实存数据个数

    void reserve(T2 _len);                      //预留新的数据空间 原有数据保持不变 新多出的空间数据可能为未知值
	inline void reset(){ this->num = 0; }       //重设数据存取位置为初始状态, 以便覆盖原有数据 并保留数据空间
	inline void clear(){ Delete(); }            //删除数据

	T&   front(){ return *(this->data); }       //获得首个数据
    T&   back(){ return this->data[this->num-1]; }	//获得尾个数据

	void push_back(const T &v);                 //在尾部存入一个数据( 值 )
	T2 push_back_neq(const T &v);               //在尾部存入一个不同于现存数据的值( 值 )返回插入值的位置ID 如已有相同的值将取消存入那么返回首个相同值的位置ID
	void pop_back(T &v){ --(this->num); v = this->data[this->num]; } //在尾部取出一个数据( 值输出 )

	T& operator [](T2 id);				        //安全引用数组元数(索引号)

	#define GT_TMP_ITERATOR_OPT_DEF( A , TYPE , B , E , RB , RE ) \
    typedef GT_sIterator##A < TYPE <T,T2> , T ,  T2 > iterator;          /*定义一个属于该类内的顺向迭代器*/\
    typedef GT_sReverseIterator< iterator >           reverse_iterator;  /*定义一个属于该类内的逆向迭代器*/\
    \
	iterator begin (){ return iterator(this, B ); }   /*获得开始端搜索迭代器*/\
	iterator end   (){ return iterator(this, E ); }   /*获得结尾端搜索迭代器*/\
	iterator rbegin(){ return iterator(this, RB );}   /*获得逆向开始端搜索迭代器*/\
	iterator rend  (){ return iterator(this, RE );}   /*获得逆向结尾端搜索迭代器*/
	GT_TMP_ITERATOR_OPT_DEF(   , sARRAY_VEC , 0 , this->num , this->num-1 , 0-1 )  //定义迭代器操作

	void insert(iterator &pos, const T &v);				  //在指定位置插入一个值(位置, 值)
	void insert(iterator &pos, T2 count, const T &v);	  //在指定位置插入多个相同值(位置, 插入个数, 值)
	void insert(iterator &pos, const T *data2, T2 count); //在指定位置插入一段数据(位置, 数据, 插入个数)
};
template<class T, class T2>
void sARRAY_VEC<T, T2>::Delete(){
	if(this->data){
		if(on_del_proc)on_del_proc(this->data, buf_len);
		delete [] (this->data); this->data = 0;
	}
	buf_len = 0;
	reset();
}
template<class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len){
	GT_TMP_ARRAY_VEC_NEW;
    this->reset();
}
template<class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len, T2 _num, T & em_all){
    GT_TMP_ARRAY_VEC_NEW;
	this->num = _num;
	GN_SetArray( (this->data) , _num , em_all );
}
template<class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len, T2 _num, T * p_data){
	GT_TMP_ARRAY_VEC_NEW;
	this->num = _num;
	GN_SetArrayFromArray( (this->data) , p_data , _num );
}
/*
template<class T, class T2>
void sARRAY_VEC<T, T2>::New(T2 _len, GN::sLIST< T > &list){
    num = (T2)list.size();
	GT_TMP_ARRAY_VEC_NEW;
	GT_LIST_iterator( T ) it_t = list.begin();
	T2 i = 0;
	for(; i < num; ++i, ++it_t)data[i] = (*it_t);
}*/
template<class T, class T2>
T2 sARRAY_VEC<T, T2>::BufSize()const{ return sizeof(T) * buf_len; }

template<class T, class T2>
void sARRAY_VEC<T, T2>::reserve(T2 _len){
	if(_len == buf_len)return;
    T *data2 = new T [_len];
	if(_len < this->num)this->num = _len;
	if(this->data){
		T2 ci = 0;
	    for( ; ci < this->num; ++ci)data2[ci] = this->data[ci];
		if(on_del_proc)on_del_proc(this->data, buf_len);
		delete [] (this->data);
	}
    this->data = data2;
	buf_len = _len;
}
template<class T, class T2>
void sARRAY_VEC<T, T2>::push_back(const T &v){
	if(this->num >= buf_len)reserve(this->num < 4 ? 4 : (this->num + (this->num >> 1)));
    this->data[this->num++] = v;
}
template<class T, class T2>
T2 sARRAY_VEC<T, T2>::push_back_neq(const T &v){
	if(this->num >= buf_len)reserve(this->num < 4 ? 4 : (this->num + (this->num >> 1)));
	T2 i = 0;
	for(; i < this->num; ++i)
		if(v == this->data[i])return i;

    this->data[this->num++] = v;
	return i;
}
template<class T, class T2>
T& sARRAY_VEC<T, T2>::operator [](T2 id){
	if(id >= buf_len)reserve(id + 1);
	return this->data[id];
}
template<class T, class T2>
void sARRAY_VEC<T, T2>::insert(iterator &pos, const T &v){
	if(this->num >= buf_len)reserve(this->num < 4 ? 4 : (this->num + (this->num >> 1)));
	T2 j = this->num;
	++(this->num);
	T2 id = pos.id;
	for(; j > id; --j)this->data[j] = this->data[j-1];
	this->data[id] = v;
}
template<class T, class T2>
void sARRAY_VEC<T, T2>::insert(iterator &pos, T2 count, const T &v){
   #define GT_TMP_ARRAY_VEC_INSERT \
    T2 count2 = this->num + count;\
	if(count2 > buf_len)reserve(count2 < 4 ? 4 : (count2 + (count2 >> 1)));\
	this->num += count;\
	T2 id = pos.id;\
	T2 j = this->num - 1, id2 = id + count - 1;\
	for(; j > id2; --j)this->data[j] = this->data[j-count];

	GT_TMP_ARRAY_VEC_INSERT
	for(; id <= id2; ++id)this->data[id] = v;
}
template<class T, class T2>
void sARRAY_VEC<T, T2>::insert(iterator &pos, const T *data2, T2 count){
	GT_TMP_ARRAY_VEC_INSERT
	T2 i = 0;
	for(; id <= id2; ++id, ++i)this->data[id] = data2[i];
}
};
/*--------------------------------------------------- 模拟STL的内存块分配器 -----------------------------------------------------------*/
struct GN_API GT_sBUF_MAPPING    //自动增长的缓冲映射
{
	GN::sARRAY_VEC<uint8*, unsigned int>    zBufs;                //一组缓冲
	unsigned int                            zBufLen;              //单个缓冲大小字节
	unsigned int                            zUnitSize;            //单个数据大小字节
    unsigned int                            zBufElmNum;           //单个缓冲可存多少个数据

	inline  GT_sBUF_MAPPING(unsigned int unit_size=1):zUnitSize(unit_size),zBufLen(0){}
    inline ~GT_sBUF_MAPPING(){ this->Release(); }

    //释放内部数据
    void Release();

    //初始化(预分配多少个缓冲, 单个缓冲可存多少个数据)
    inline void InitToData(unsigned int count, unsigned int em_num){
        zBufElmNum = em_num;
		zBufLen = zBufElmNum * zUnitSize;
        zBufs.New(count);
    }
	//加入一块缓冲
	inline void PushBuf(){
		zBufs.push_back((uint8*)malloc(zBufLen));
	}
	//获得数据存放空间指针(数据存放位置)
	inline uint8 *GetPtr(unsigned int pos){
        unsigned int bufid = pos ? pos / zBufElmNum : 0;
        return zBufs.data[bufid] + ((pos - (bufid * zBufElmNum)) * zUnitSize);
	}
	//获得数据存放位置(数据存放空间指针)
	unsigned int GetPos(const uint8 *p);
};
/* ----------------------------------------------------------------------------------------------------------------------------*/
namespace GN{
class GN_API cVOLUME_MAG1                           //用量管理器 - 仅能分配每份相等的用量 分配速度快
{
	unsigned int (*zpfOnFull)(unsigned int tot_size, void *us_arg);    //当容量满时的回调
	void                                          *zUsArg;             //用户参数
	unsigned int                                   zTotalSize;         //总空间大小个数
	unsigned int                                   zUsedSize;          //已使用大小个数
	unsigned int                                  *zPosData;           //可用位置数据
	unsigned int                                   zPosNum;            //可用位置数
   public:
    cVOLUME_MAG1():zTotalSize(0),zPosData(0){}
   ~cVOLUME_MAG1(){ this->Release(); }

    //释放内部资源
    void Release();

	//初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数)
	void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, void *us_arg), void *_us_arg);

    //空间申请()返回空间的位置
	unsigned int Alloc();

	//归还空间(空间位置)
	inline void Free(unsigned int pos){
		zPosData[zPosNum++] = pos;
	}
	//获得当前总容量大小
	unsigned int GetTotalSize(){ return zTotalSize; }

	//获得最尾空段范围(是否精确检测 这稍慢一点)
	sSEGMENT<unsigned int> GetEndSegRange(bool accur=true);
};
/* ------------------------------------------------------------------------------------------------------------------*/
class GN_API cMEM_POOL1 :public cVOLUME_MAG1 , public GT_sBUF_MAPPING //内存池 - 仅能分配每份相等的空间 分配速度快
{public:
   #define GT_TMP_MEM_POOL_BASE_FUNC_DEF( N ) \
    /*构造对象(单个数据大小)*/\
    cMEM_POOL##N(unsigned int unit_size=1):GT_sBUF_MAPPING(unit_size){}\
   ~cMEM_POOL##N(){}\
    \
    /*释放内部资源*/\
    void Release(){\
		GT_sBUF_MAPPING::Release();\
		cVOLUME_MAG##N::Release();\
    }
    GT_TMP_MEM_POOL_BASE_FUNC_DEF( 1 )

    //初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数) !!后两个参数在该类中无效
	void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, void *us_arg)=0, void *_us_arg=0);

    //空间申请(参数无效为兼容其它接口而设)返回空间指针
    uint8 * Alloc(unsigned int size=1){
		return GetPtr(cVOLUME_MAG1::Alloc());
	}
	//释放归还空间(空间指针, 此参数无效为兼容其它接口而设)
	void Free(const uint8 *p, unsigned int size=1){
		cVOLUME_MAG1::Free(GetPos(p));
	}
};
template<class T, class T2 = size_t, class T3 = cMEM_POOL1>
class cALLOC_POOL                       //空间分配器 使用内存池 并兼容STD标准
{
	T3                          zVmem;       //模拟内存

   public:     //为兼容STD标准
	typedef T                   value_type;
	typedef T2                  size_type;
    typedef std::ptrdiff_t      difference_type;
    typedef       value_type&   reference;
    typedef const value_type&   const_reference;
    typedef       value_type  * pointer;
    typedef const value_type  * const_pointer;

	template<typename U>
	struct rebind {  typedef cALLOC_POOL<U, T2, T3> other; };

    inline cALLOC_POOL(){ }

	template<class U>
	cALLOC_POOL(const cALLOC_POOL<U, T2, T3>& al){ }
/*
    cALLOC_POOL(const cALLOC_POOL<T>&){ }

	template<class _Other>
	cALLOC_POOL<T>& operator=(const cALLOC_POOL<_Other>&){
		return (*this);
	}
*/
	//初始化
	void InitToData(){
		zVmem.zUnitSize = sizeof(value_type);
        zVmem.InitToData(256);
	}
    //分配空间(多少个数据空间, 接收数据空间的指针)返回空间指针
	inline pointer allocate(size_type n, pointer **p=0){
        if(!zVmem.zBufLen)InitToData();
	    return (pointer)zVmem.Alloc(n);
	}
    //释放空间(空间指针, 释放多少个数据空间)
	inline void deallocate(pointer p, size_type n = 1){
		zVmem.Free((uint8*)p, n);
	}
    //为指定的空间指针调用构造函数(空间指针, 初始值)
    inline void construct(pointer p, const value_type & v){
        new(p) value_type(v);
    }
    //销毁空间指针指向的对象(空间指针) 即调用对象的析构函数
    inline void destroy(pointer p) {
        p->~value_type();
    }
    //可分配最大大小个空间
	size_type max_size() const {
		size_type size = (size_type)(-1) / sizeof(value_type);
		return (0 < size ? size : 1);
	}
};
};
/*********************************************************** 单向链表 ***********************************************************************/

//简单单向连表加入一个节点到队列的首部( 队列 , 要加进去的节点 )
#define GT_MX_PushFront( FLIST ,  F ) { ( F ) ->next_node = ( FLIST ); ( FLIST ) = ( F ); }

//简单单向连表历遍开始( 连表类型 , 连表 )
#define GT_MX_FindBegin( TYPE , LIST ) {\
	GT_s_LITMK_FACE *_last = LIST;\
	GT_s_LITMK_FACE *_cur = _last;\
	for(; _cur; ){

//简单单向连表历遍时移除当前检索节点( 连表 )
#define GT_MX_FindRemoveCur( LIST ) {\
		if(_cur == _last){\
			_cur = _cur->next_node;\
			LIST = _last = _cur;\
		}else{\
			_cur = _cur->next_node;\
			_last->next_node = _cur;\
		}\
		continue;\
	}

//简单单向连表历遍时跳转继续检索下个节点( 连表 )
#define GT_MX_FindContinue() {\
		_last = _cur;\
		_cur = _cur->next_node;\
		continue;\
	}

//简单单向连表历遍结束
#define GT_MX_FindEnd() \
		_last = _cur;\
		_cur = _cur->next_node;\
	}}

template<class T>
//连接合并两个简单单向连表(首个连表, 尾个连表)返回合并后的连表首部
T GT_MX_Merge(T frist , T last){
	if(frist){
		if(last){
			T  _fmk = frist;
			for(; _fmk->next_node; _fmk = _fmk->next_node);
			_fmk->next_node = last;
		}
		return frist;
	}else return last;
}

template<class T>
struct GT_sNODE_BASE				//基础简单单向连表节点
{
	GT_sNODE_BASE<T>   *next_node;		//下一节点
	T					v;				//值
};
/*********************************************************** 双向链表相关 *****************************************************************/
template<class T>
struct GT_sLNODE;

template<class T>
struct GT_sLNODE_		 //链表端点
{
    GT_sLNODE<T>		*last;   //上一个数据节点
	GT_sLNODE<T>		*next;   //下一个数据节点
};
template<class T>
struct GT_sLNODE :public GT_sLNODE_<T>	//链表数据节点
{
	T                    em;     //值元素
};
/* 链表 历遍搜索方法  -------------------------------------------------------------------------------------------------------- */
template<class TW , class T >
struct GT_sIterator1     //顺向搜索迭代器
{
    TW					*lw;     //指向关联节点

	inline GT_sIterator1(){}
	inline GT_sIterator1(TW *_lw):lw(_lw){}

	inline bool operator ==(const GT_sIterator1 & it)const{ return (this->lw == it.lw); }
	inline bool operator !=(const GT_sIterator1 & it)const{ return (this->lw != it.lw); }
    inline T & operator*() { return lw->em; }
    inline T * operator->(){ return &(lw->em); }

	inline GT_sIterator1& operator++()   { lw = lw->next; return (*this); } /*模板代码:迭代器增减*/
	inline GT_sIterator1  operator++(int){ GT_sIterator1 tmp = (*this); lw = lw->next; return tmp; }
	inline GT_sIterator1& operator--()   { lw = lw->last; return (*this); }
	inline GT_sIterator1  operator--(int){ GT_sIterator1 tmp = (*this); lw = lw->last; return tmp; }
};
template< class ITER >
struct GT_sReverseIterator1 :public ITER  //逆向搜索迭代器
{
    inline GT_sReverseIterator1& operator++()   { this->lw = this->lw->last; return (*this); } /*模板代码:迭代器增减*/
	inline GT_sReverseIterator1  operator++(int){ GT_sReverseIterator1 tmp = (*this); this->lw = this->lw->last; return tmp; }
	inline GT_sReverseIterator1& operator--()   { this->lw = this->lw->next; return (*this); }
	inline GT_sReverseIterator1  operator--(int){ GT_sReverseIterator1 tmp = (*this); this->lw = this->lw->next; return tmp; }
};
/* --------------------------------------------------------- sLIST ----------------------------------------------------------- */
#define GT_US_STDLIST  //使继承自stl的链表
#ifdef  GT_US_STDLIST
#ifdef _MSC_VER
  #if _MSC_VER < 1600 || _MSC_VER >= 1900
	#if _MSC_VER >= 1900
	#define GT_TMP_ITORID 0
	#else
	#define GT_TMP_ITORID 1
	#endif
	/* !以下这个方法比较硬核 如果其它平台的C标准库有些差异可能会影响到这里导致出轨 可以用以下方法来输出 历遍游标和数据地址看看它们的关系再作更改
	  GN_Log("%08X, %08X, %08X\n",((unsigned int **)(&itor_))[0],((unsigned int **)(&itor_))[1], &(*itor_) );
	*/
	//把连表里指定数据的地址指针转回历遍迭代器(数据类型, 分配器类型, 对应连表, 连表里指定数据的地址指针)
	#define GT_ReListIterator( list_type , alloc_type , list_ , p_tg ) \
		GN::sLIST< list_type alloc_type >::iterator itor_ =  (list_) .begin();\
		void **pptr = (void**)(& itor_ ); \
	    pptr[ GT_TMP_ITORID ] = ((void**)(p_tg)) - 2; /*用数据指针计算回连表内相对应的位置*/\
		return itor_;
  #else
   #define GT_ReListIterator( list_type , alloc_type , list_ , p_tg ) \
	GN::sLIST< list_type alloc_type >::iterator itor_ =  (list_) .begin(), end_ = (list_) .end();\
	for( ; itor_ != end_; ++itor_){\
        if( &(*itor_) == (p_tg) )return itor_;\
	}
  #endif
#else
//把连表里指定数据的地址指针转回历遍迭代器(数据类型, 分配器类型, 对应连表, 连表里指定数据的地址指针)
#ifdef __LP64__
#define GT_ITER_PTR_OFS 16
#else
#define GT_ITER_PTR_OFS 8
#endif
#ifdef _LINUX_
#define GT_ReListIterator( list_type , alloc_type , list_ , p_tg ) \
     unsigned char * pdr = ((unsigned char*) (p_tg) ) - GT_ITER_PTR_OFS;\
	 return *((std::_List_iterator< list_type > *)&pdr);
#else
#define GT_ReListIterator( list_type , alloc_type , list_ , p_tg ) \
unsigned char * pdr = ((unsigned char*) (p_tg) ) - GT_ITER_PTR_OFS;\
return *((typename std::list< list_type >::iterator *)&pdr);
#endif
#endif
#endif
namespace GN {
template<class T , class A = std::allocator<T> >
#ifdef GT_US_STDLIST
struct  sLIST :public std::list<T , A>
#else
struct  sLIST	   //★双向连表数据结构 实现高效动态增加删减数据元素 扩展了标准C的一些功能
#endif
{
	//以下是模拟STL list 的一些定义 --------------------------------------------------
	#ifdef  _MSC_VER
     #define GT_LIST_iterator( _T_ )  GN::sLIST< _T_ >::iterator
    #else
    #ifdef _LINUX_
     #define GT_LIST_iterator( _T_ )  std::_List_iterator< _T_ >
     #ifdef GT_US_STDLIST
     typedef std::_List_iterator<T>              iterator;
     #endif
    #else
     #define GT_LIST_iterator( _T_ ) typename GN::sLIST< _T_ >::iterator
     #ifdef GT_US_STDLIST
     typedef typename std::list<T , A>::iterator iterator;
     #endif
    #endif
    #endif
   #ifndef GT_US_STDLIST
	typedef typename A::template rebind< GT_sLNODE<T> >::other zAltype;

	GT_sLNODE_<T>		 z_Start;		//头端点
	GT_sLNODE_<T>		 z_End;			//末端点
	unsigned int		 zSize;         //连表节点数
	zAltype				 zAlnod;		//节点分配器

	//#define GT_TMP_ITERATOR_OPT_DEF( A , TYPE , B , E , RB , RE )

    typedef GT_sIterator1< GT_sLNODE<T> , T > iterator;          /*该类内的顺向迭代器*/
    typedef GT_sReverseIterator1< iterator >  reverse_iterator;  /*该类内的逆向迭代器*/
    friend struct iterator;
	friend struct reverse_iterator;

	//获得开始端搜索迭代器
	inline iterator begin (){ return iterator(z_Start.next); }

	//获得结尾端搜索迭代器
	inline iterator end   (){
		return iterator((GT_sLNODE<T>*)&z_End);
	}
	//获得逆向开始端搜索迭代器
	inline reverse_iterator rbegin(){ reverse_iterator rit; rit.lw = z_End.last; return rit; }

	//获得逆向结尾端搜索迭代器
	inline reverse_iterator rend  (){
		reverse_iterator rit;
		rit.lw = (GT_sLNODE<T>*)&z_Start;
		return rit;
	}
	//重设内部状态
	inline void zReset(){ zSize = 0; z_Start.next = (GT_sLNODE<T>*)&z_End; z_End.last = (GT_sLNODE<T>*)&z_Start; }

	//在指定位置前插入一个数据元素(位置, 值)
	void zInsert(iterator pos, const T &v){
		GT_sLNODE<T> *new_node = zAlnod.allocate(1);
		new(&(new_node->em)) T(v);
		new(&(new_node->last)) GT_sLNODE<T>*();
		new(&(new_node->next)) GT_sLNODE<T>*();

		GT_sLNODE<T> *f = pos.lw->last;
		new_node->last = f;
		new_node->next = pos.lw;
		pos.lw->last = new_node;
		f->next = new_node;
		++zSize;
	}
	//转移指定链表里一段范围的数据元素到该链表里(插入进该链表那个位置之前, 指定链表, 指定链表的范围段的首个位置, 范围段的尾个位置, 范围段的数据元素个数)
	void zSplice(iterator pos, sLIST &ls, iterator first, iterator last, unsigned int count){
		if(&zAlnod == &(ls.zAlnod)){
			if (this != &ls){
				zSize	 += count;
				ls.zSize -= count;
			} //在之前的拿出
			GT_sLNODE<T> *__l = last.lw->last; //段的尾节点
			last.lw->last = first.lw->last;    //断开就及时封口
			first.lw->last->next = last.lw;

			//放入到当前的
			GT_sLNODE<T> *l1 = pos.lw->last;
			l1->next = first.lw; first.lw->last = l1; //绑定段的首连
			pos.lw->last = __l; __l->next = pos.lw;   //绑定段的尾连
		}else{
			insert(pos, first, last);
			ls.erase(first, last);
		}
    }
	//默认构造
	inline  sLIST(){ zReset(); }

	//构造指定数量的默认值元素(数量)
	inline  sLIST(unsigned int count){
		zReset();
		T v;
		unsigned int i = 0;
		for(; i < count; ++i)push_back(v);
	}
	//构造指定数量的指定值元素(数量, 指定值)
	inline  sLIST(unsigned int count, const T &v){
		zReset();
		unsigned int i = 0;
		for(; i < count; ++i)push_back(v);
	}
	//构造并插入一段范围的数据元素(范围段的首个位置, 范围段的尾个位置)
	inline  sLIST(iterator frist, iterator last){
		zReset();
		insert(begin(), frist, last);
	}
	//构造与指定链表拥有相同数据的链表(指定链表)
	inline  sLIST(const sLIST &ls){
		zReset();
		insert(begin(), ((sLIST&)ls).begin(), ((sLIST&)ls).end());
	}
	//析构
    inline ~sLIST(){ this->clear(); }

	//使内部数据等同于指定链表(指定链表)
	inline sLIST & operator = (const sLIST &ls){
		this->clear();
		insert(begin(), ((sLIST&)ls).begin(), ((sLIST&)ls).end());
		return *this;
	}
    //清除原有数据并用指定链表的值来重新赋值(链表)
    inline void assign(sLIST ls){
		this->assign(ls.begin(), ls.end());
	}
    //清除原有数据重新赋值(范围段的首个位置, 范围段的尾个位置)
    inline void assign(iterator frist, iterator last){
		this->clear();
		for(; frist != last; ++frist)push_back(*frist);
	}
	//清除原有数据重新赋值(数据个数, 值)
    inline void assign(unsigned int count, const T &v){
		this->clear();
		for(; count; --count)push_back(v);
	}
	//反转链表中的数据元素顺序
	void reverse(){
		if(zSize > 1){
			iterator last = end();
			for (iterator cur = ++begin(); cur != last; ){
				iterator before = cur;
				zSplice(begin(), *this, before, ++cur, 1);
		}	}
	}
	//重设链表当前容纳数据个数(个数 这如小于之前的会自动移除尾部的一些数据元素)
	inline void resize(unsigned int new_size){
		resize(new_size, T());
	}
	//重设链表当前容纳数据个数(个数 这如小于之前的会自动移除尾部的一些数据元素, 新增数据的值)
	inline void resize(unsigned int new_size, T v){
		if(zSize < new_size)insert(end(), new_size - zSize, v);
		else{
			while(new_size < zSize)pop_back();
		}
	}
	//在指定位置前插入一个数据元素(位置, 值)返回元素插入的位置
	inline iterator insert(iterator pos, const T &v){
		zInsert(pos, v);
		return (--pos);
	}
	//在指定位置前插入数据元素多次(位置, 次数, 值)
	inline void insert(iterator pos, unsigned int count, const T &v){
		for(;count; --count)zInsert(pos, v);
	}
	//在指定位置前插入一段范围的数据元素(位置, 范围段的首个位置, 范围段的尾个位置)
	inline void insert(iterator pos, iterator frist, iterator last){
		for(;frist != last; ++frist)zInsert(pos, *frist);
	}
	//在指定位置前插入另一个链表的所有数据元素 (位置, 另一个链表)
	inline void insert(iterator pos, sLIST &ls){
		insert(pos, ls.begin(), ls.end());
	}
	//在尾部添加一个数据元素
	void push_back(const T & val){
		zInsert(end(), val);
	}
	//在首部添加一个数据元素
	void push_front(const T & val){
	    zInsert(begin(), val);
	}
	//删除最头的一个数据元素
	void pop_front(){
		erase(begin());
	}
	//删除最尾的一个数据元素
	void pop_back(){
	    erase(--end());
	}
	//删除指定位置指向的数据元素(位置) 返回迭代器的下个
	inline iterator erase(iterator _it){
		GT_sLNODE<T> *tmp = _it.lw;
		if(tmp == (GT_sLNODE<T>*)&z_End || tmp == (GT_sLNODE<T>*)&z_Start)return _it;
		--zSize;
		_it.lw = tmp->next;
		tmp->last->next = _it.lw;
		_it.lw->last = tmp->last;

		tmp->~GT_sLNODE<T>();
		zAlnod.deallocate(tmp,1);
		return _it;
	}
	//删除指定范围段的数据元素(范围段的首个位置, 范围段的尾个位置) 返回迭代器的下个
	inline iterator erase(iterator frist, iterator last){
		for(; frist != last; )frist = erase(frist);
		return last;
	}
	//移除与指定值相等的所有数据元素(值)
	inline void remove(const T &v){
		iterator last = end();
		for (iterator first = begin(); first != last; ){
			if(*first == v)first = erase(first);
			else ++first;
		}
	}
	template<class F>
	//移除所有符合条件的数据元素(条件方法)
	void remove_if(F cmpfunc){
		iterator last = end();
		for (iterator first = begin(); first != last; ){
			if (cmpfunc(*first))first = erase(first);
			else ++first;
		}
	}
	//删除连表中邻近相同值的数据元素
	void unique(){
		#define GT_TMP_LIST_UNIQUE( CMP ) \
		if(zSize > 1){\
			iterator first = begin(), last = end();\
			iterator after = first;\
			for (++after; after != last; ) \
				if (*first == *after)after = erase(after);\
				else first = after++;\
		}
		GT_TMP_LIST_UNIQUE( *first == *after )
	}
	template<class F>
	//删除连表中邻近且符合条件的数据元素(条件方法)
	void unique(F cmpfunc){
		GT_TMP_LIST_UNIQUE( cmpfunc(*_First, *_After) )
	}
	//清除全部数据元素
	void clear(){
		if(zSize){
			/*GT_sLNODE<T> *_cur1, *_cur = z_Start.next, *_end = (GT_sLNODE<T>*)&(z_End);
			zReset();
			for(; _cur != _end; _cur = _cur1){
				_cur1 = _cur->next;
				_cur->~GT_sLNODE<T>();
				zAlnod.deallocate(_cur,1);
			}*/
			erase(begin(), end());
		}
	}
	//交换两链表所有内容(另一链表)
	void swap(sLIST &ls){
		z_End.last->next = (GT_sLNODE<T>*)&(ls.z_End);
		ls.z_End.last->next = (GT_sLNODE<T>*)&(z_End);
		z_Start.next->last = (GT_sLNODE<T>*)&(ls.z_Start);
		ls.z_Start.next->last = (GT_sLNODE<T>*)&(z_Start);

		GN_Swap( GT_sLNODE_<T> , z_Start , ls.z_Start);
		GN_Swap( GT_sLNODE_<T> , z_End   , ls.z_End  );
		GN_Swap(unsigned int   , zSize   , ls.zSize  );
		GN_Swap(zAltype		   , zAlnod  , ls.zAlnod );
		/*
		sLIST tmp;
		unsigned int tsize = sizeof(tmp);
		memcpy(&tmp, this, tsize);
		memcpy(this, &ls, tsize);
		memcpy(&ls, &tmp, tsize);*/
	}
	//转移指定链表里的所有数据元素到该链表里(插入进该链表那个位置之前, 指定链表)
	inline void splice(iterator pos, sLIST& ls){
		if(this != &ls && ls.zSize)
			zSplice(pos, ls, ls.begin(), ls.end(), ls.zSize);
	}
	//转移指定链表里一个数据元素到该链表里(插入进该链表那个位置之前, 指定链表, 指定链表的数据源位置)
	void splice(iterator pos, sLIST& ls, iterator first){
		if (first != ls.end()){
			iterator last = first;
			++last;
			if (this != &ls || (pos != first && pos != last))
				zSplice(pos, ls, first, last, 1);
		}
	}
	//转移指定链表里一段范围的数据元素到该链表里(插入进该链表那个位置之前, 指定链表, 指定链表的范围段的首个位置, 范围段的尾个位置)
	void splice(iterator pos, sLIST& ls, iterator first, iterator last){
		if (first != last && (this != &ls || pos != last)){
			unsigned int count = 0;
			if(this == &ls);
			else if (first == ls.begin() && last == ls.end())count = ls.zSize;
			else{
				iterator cur = first;
				for(; cur != last; ++cur)++count; //统计范围段长度
			}
			zSplice(pos, ls, first, last, count);
		}
	}
	//合并两个链表 并使值小的排在前(另一链表) !这个排序不是完全排序 它仅考虑相邻的值
	void merge(sLIST& ls){
	   #define GT_TMP_LIST_MERGE( CMP ) \
		if (&ls != this){\
			iterator first1 = begin(),	  last1 = end();\
			iterator first2 = ls.begin(), last2 = ls.end();\
			\
			while (first1 != last1 && first2 != last2) \
				if ( CMP ){\
					iterator last = first2;\
					zSplice(first1, ls, first2, ++last, 1);\
					first2 = last;\
				}else ++first1;\
			\
			if (first2 != last2)zSplice(last1, ls, first2, last2, ls.zSize);\
		}
		GT_TMP_LIST_MERGE( (*first2)<(*first1) )
	}
	template<class F>
	//合并两个链表 并按指定方法来定那边的值排在前(另一链表, 判定方法) !这个排序不是完全排序 它仅考虑相邻的值
	void merge(sLIST& ls, F cmpfunc){
		GT_TMP_LIST_MERGE( cmpfunc(*first2, *first1) )
	}
	//排序数据
	inline void sort(){
		sort( GN_SortFuncDef<T> );
	}
	template<class F>
	//按指定方法排序数据(方法)
	void sort(F cmpfunc){
	  if(zSize > 1){
		uint8 buf[131072]; //128KB
		sTBUF_INT tbuf(buf, 131072);
		T** ptrs = (T**)tbuf.GetSpace(sizeof(T*) * zSize);
		get_parray(ptrs);
		GN_Sort2( T* , ptrs, zSize, cmpfunc , * ); //排序指向值的指针

		T **ptr_end = ptrs + zSize;
		unsigned int node_shell_size = ((uint8*)&(((GT_sLNODE<T>*)buf)->em) - buf);
		GT_sLNODE<T> *cur1 = (GT_sLNODE<T>*)(((uint8*)(*ptrs)) - node_shell_size);
		GT_sLNODE<T> *cur;
		cur1->last = (GT_sLNODE<T>*)&z_Start;
		z_Start.next = cur1;
		for(++ptrs; ptrs < ptr_end; ++ptrs){ //重链路各个节点
			cur = (GT_sLNODE<T>*)(((uint8*)(*ptrs)) - node_shell_size);
			cur1->next = cur;
			cur->last = cur1;
			cur1 = cur;
		}
		cur1->next = (GT_sLNODE<T>*)&z_End;
		z_End.last = cur1;
	  }
	}
	//获得链表首个数据元素
	inline T & front(){ return z_Start.next->em; }
	//获得链表首个数据元素
	inline T & front()const{ return z_Start.next->em; }

	//获得链表尾个数据元素
	inline T & back() { return z_End.last->em; }
	//获得链表尾个数据元素
	inline T & back() const{ return z_End.last->em; }

	//连表是否空的
	inline bool empty()const{ return (zSize == 0); }

	//获得连表数据元素个数
	inline unsigned int size()const{ return zSize; }

	//获得链表能容纳数据的最大个数
	inline unsigned int max_size()const{ return zAlnod.max_size(); }
   #endif
	//以下是引擎扩展的一些定义 --------------------------------------------------

	//添加一个数据元素( 数据元素 )返回该元素被添加后的位置指针
	inline T *add(const T & v){
		this->push_back(v);
       #ifdef GT_US_STDLIST
		return &(*(--(this->end())));
	   #else
		return &(z_End.last->em);
	   #endif
	}
    //在指定位置添加一个数据元素(指定位置, 数据元素)返回该元素被添加后的位置指针
	inline T *add(iterator _it, const T & v){
		return &(* ( this->insert(_it, v) ) );
	}
	//添加数组里的所有元素(数组, 数组元素个数)
	inline void add(const T *data, unsigned int num){
		unsigned int i = 0;
		for(; i < num; ++i)this->push_back( data[i] );
	}
	//在该连表首部添加一个数据元素(数据元素)返回该元素被添加后的位置指针
	inline T *add_front(const T & v){
		this->push_front(v);
	   #ifdef GT_US_STDLIST
		return &(*(this->begin()));
	   #else
		return &(z_Start.next->em);
	   #endif
	}
    //添加一个在连表里没有的数据元素(数据元素) 如果成功添加返回该元素被添加后的位置指针 不成功返回0
	inline T *add_no_eq(const T & v){
		if(this->size()){
            iterator _et = this->end();
			for (iterator _it = this->begin(); _it != _et; ++_it){
				if((*_it) == v )return 0;
			}
		}
        this->push_back(v);
	   #ifdef GT_US_STDLIST
		return &(*(--(this->end())));
	   #else
		return &(z_End.last->em);
	   #endif
	}
	//添加一个在连表里没有的数据元素(数据元素) 返回添加后元素在连表里的序号 如连表已有该数据元素值则返回这个值的序号
	inline unsigned int add_no_eq2(const T & v){
		unsigned int id = 0, ln = this->size();
		if(ln){
			for (iterator _it = this->begin(); id < ln; ++id, ++_it){
				if((*_it) == v )return id;
			}
		}
        this->push_back(v);
		return id;
	}
	//添加数组里在连表里没有的数据元素(数组, 数组元素个数)
	inline void add_no_eq(const T *data, unsigned int num){
		unsigned int i = 0;
		for(; i < num; ++i){
			if(this->size()){
                iterator _et = this->end();
				for (iterator _it = this->begin(); _it != _et; ){
					if((*_it) == data[i] ){
						++i;
						if(i == num)return;
						_it = this->begin();
					}else ++_it;
				}
			}
            this->push_back( data[i] );
		}
	}
	//在连表中删除(指定位置的数据元素)
	inline void del(T *p_em){
		if(!p_em){
           #ifdef GT_PMEM_WORNING
			GN_Log("连表删除的数据元素 无效!!\n");
           #endif
			return;
		}
		this->erase(to_iterator(p_em));
	}
	//把指定数据元素位置转回历遍迭代器(数据元素位置)
	inline iterator to_iterator(T *p_em){
	   #ifdef GT_US_STDLIST
		#define GT_TMP_AT ,A
		GT_ReListIterator( T , GT_TMP_AT , (*this) , p_em )
	   #else
		uint8 buf[32];
		return iterator( (GT_sLNODE<T>*)((uint8*)p_em - ((uint8*)&(((GT_sLNODE<T>*)buf)->em) - buf)) );
	   #endif
	}
	 //查找指定数据元素的位置指针(数据元素)返回该数据元素的位置指针 返回0表示没找到
    inline T* search(const T & v){
		iterator _et = this->end();
		for (iterator _it = this->begin(); _it != _et; ++_it)
			if ((*_it) == v)return &(*_it);
		return 0;
	}
	//查找指定位置序号的数据元素位置指针(位置序号)返回该数据元素的位置指针 返回0表示没找到
	inline T* search(unsigned int id){
		if(id >= this->size())return 0;
		iterator _it = this->begin();
		unsigned int i_ = 0;
		for(; i_ < id; ++i_)++_it;
		return &(*_it);
	}
	 //查找指定数据元素的位置序号(数据元素)返回该数据元素的位置序号 返回0xffffffff表示没找到
    inline unsigned int search_id(const T & v){
		unsigned int id = 0, num = this->size();
		for (iterator _it = this->begin(); id < num; ++id, ++_it)
			if ((*_it) == v)return id;
		return id;
	}
    //按条件查找得到一个数据元素的指针( 返回是否 比较条件函数(告诉你当前检索到的元素) )返回满足条件的一个元素的指针 返回0表示没找到
    inline T* search_if( bool (*compare)(T &cur) ){
		iterator _et = this->end();
		for (iterator _it = this->begin(); _it != _et; ++_it)
			if (compare( (*_it) ))return &(*_it);
		return 0;
	}
	//使连表里的所有数据元素都执行指定函数( 返回是否需继续查找 执行函数(告诉你当前检索到的元素) ) 返回查找中途是否有中断
	inline bool search_exec( bool (*proc)(T &cur) ){
	    iterator _et = this->end();
		for (iterator _it = this->begin(); _it != _et; ++_it){ if(! proc( (*_it) ))return true; }
		return false;
	}
	//删除连表中全部相同值的数据元素  系统默认的unique只删除隔离值相同的数据元素
	inline void unique_all(){
       #define GT_LIST_unique_all( CMP ) \
		if (2 <= this->size() ){\
			iterator _it = this->begin(), _et = this->end(), _cur;\
			for( ; _it != _et; ++_it){\
                _cur = _it; ++_cur;\
				for ( ; _cur != _et; ){\
					if ( CMP )_cur = this->erase(_cur);\
					else    ++_cur;\
				}\
			}\
		}\

        GT_LIST_unique_all( *_it == *_cur )
	}
	//按指定比较条件来删除连表中全部值相同的数据元素(要求返回是否要删除 比较条件(告诉你被比较的数据元素, 告诉你正在比较的数据元素)要求返回是否删除正在比较的元素)  系统默认的unique只删除隔离值相同的数据元素
	inline void unique_all(bool (*compaer)(T &left, T &right)){
		GT_LIST_unique_all( compaer( *_it , *_cur ) )
	}
	//生成一个和连表数据一一对应的数组() 返回数组指针 如果连表里没有数据返回0 这会帮您自动分配好数据空间 用完记得 delete[] 释放指针内容
	inline T* to_array(){
		unsigned int n = this->size();
		if(!n)return 0;
	    T* data = new T[ n ];
		iterator _it = this->begin(), _et = this->end();
		for(n = 0; _it != _et; ++_it)data[n++] = (*_it);
		return data;
	}
	//获得连表的所有数据并装填到指定缓冲里(缓冲 需预先分配足够空间)
	inline void get_array(T *data){
		if(this->empty())return;
		unsigned int i = 0;
		iterator _it = this->begin(), _et = this->end();
		for(; _it != _et; ++_it, ++i)data[i] = (*_it);
	}
	//获得连表的所有数据指针并装填到指定缓冲里(缓冲 需预先分配足够空间) !如果数据元素为大型结构或类对象 这个更有效率
	inline void get_parray(T **ptr_data){
	 #ifdef GT_US_STDLIST

	 #else
	  if(zSize){
		GT_sLNODE<T> *cur = z_Start.next, *_end1 = z_End.last;
		for(;;cur = cur->next, ++ptr_data){
			*ptr_data = &(cur->em);
			if(cur == _end1)break;
	  } }
	 #endif
	}
	//移动数据元素位置顺序(数据元素位置的指针, 向后移动次数 负值就向前移动)返回实际移动了的次数 当移到最前或最后时会提前返回
	int order(T **pp_em, int fn){
		int i = 0;
		iterator ito = to_iterator(*pp_em), eto;
		if(fn > 0){
			iterator ito1 = ito;
			++ito1;
			eto = this->end();
			if(ito1 == eto)return 0;
			
		}else{
			eto = this->begin();
			if(ito == eto)return 0;
		}
		T tv = (*(*pp_em));
		ito = this->erase(ito);
		if(fn > 0){
			for(; i != fn && ito != eto; ++i)++ito;
		}else{
			for(; i != fn && ito != eto; --i)--ito;
		}
		*pp_em = this->add(ito, tv);
		return i;
	}
};
template<class T , class T2 >
//转移源连表的所有元素到目标连表里(目标连表, 源连表)
void ListSpliceList(sLIST< T > &tg, sLIST< T2 > &src){
	GT_LIST_iterator( T2 ) _it = src.begin(), _et = src.end();
	for(; _it != _et; ++_it)tg.push_back(*_it);
}
template<class T , class ITER >
//获得连表指定历遍迭代器的下个位置迭代器(连表, 指定迭代器)返回下个位置迭代器 !当迭代器已到结尾时就轮回到开始
ITER *ListGetNextIter(T &list, ITER *iter){
	if(list.size()){
		if(!iter){
			iter = new ITER;
			*iter = list.begin();
		}else{
			if(*iter == list.end())*iter = list.begin();
			else{
				++(*iter);
				if(*iter == list.end())*iter = list.begin();
			}
		}
	}else{ GN_Delete( iter ); }
	return iter;
}
/* ------------------------------------------------------ sLIST_WHEEL --------------------------------------------------------------------- */
template<class T, class T2 >
class  sLIST_WHEEL  //★列表轮 首尾相连的数据结构  !用固定的容器空间轮寻来模拟连表操作 优点:与普通数组操作效率相当 缺点:没真正连表的动态增长删减
{protected:
    T               *data;    //内部数据
	T2               end_id;  //内部数据最大索引ID
    T2               fid;     //前向存取位置
	T2               bid;     //后向存取位置

	static T2        sid;     //起点位置
	static void zComputeSP(); //计算起点位置 为该类型最大值的一半

	virtual void zInit(){ if(!sid)zComputeSP(); }
   public:
	inline T* zpElement(T2 id){ return data + (id & end_id); }  //获得指定存取位置的数据元素指针
	inline T& zElement (T2 id){ return data   [id & end_id]; }  //获得指定存取位置的数据元素

	inline  sLIST_WHEEL():data(0){ this->reset(); }
	inline  sLIST_WHEEL(T2 _len):data(0){ this->New(_len); }                                    //分配数据(预分配多少个数据空间 为提高处理速度实际分配可能大于该数)
	inline  sLIST_WHEEL(T2 _len, T2 _num, T & em_all):data(0){ this->New(_len, _num, em_all); } //分配数据并设置一些初值(预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 从首部起设置多少个初值,  默认初始值)
	inline  sLIST_WHEEL(T2 _len, T2 _num, T * p_data):data(0){ this->New(_len, _num, p_data); } //分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 设置数据个数, 普通指针)
	inline  sLIST_WHEEL(T2 _len, GN::sLIST< T > &list):data(0){ this->New(_len, list); }        //分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 连表 )
	inline ~sLIST_WHEEL(){ this->Delete(); }

	void Delete(); //删除数据

	//分配数据(预分配多少个数据空间 为提高处理速度实际分配可能大于该数)
	void New(T2 _len);

	//分配数据并设置一些初值(预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 从首部起设置多少个初值, 默认初始值)
	void New(T2 _len, T2 _num, T & em_all);

	//分配数据并从复制普通指针数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 设置数据个数, 普通指针)
	void New(T2 _len, T2 _num, T * p_data);

    //分配数据并从复制连表里的数据来设置一段初始值( 预分配多少个数据空间 为提高处理速度实际分配可能大于该数, 连表 )
	void New(T2 _len, GN::sLIST< T > &list);

	T2   BufNum();                        //获得数据空间个数
	T2   BufSize();                       //获得数据空间所占大小字节
	bool Has();                           //是否有实存数据
	bool empty();                         //是否无实存数据
	T2   size();                          //获得实存数据个数
	T2   Count();                         //获得实存数据个数
	T2   GetArrays(T* a[2][2]);           //获得内部实存数据段的首尾指针(返回数据段的首尾指针) 返回共有多少段数据 !调用这后您就可以像普通数组那样操作里面的数据了 这个的用法您可以参考 Search 的实现
    T*   ToArray(T2 &n);                  //生成一个数据一一对应的数组(返回数据个数) 返回数组指针 返回0表示内部没有数据 这会帮您自动分配好空间 用完记得释放指针内容
	T*   Search(const T &v);              //查找指定数据元素的位置指针 返回该数据元素的位置指针 返回0表示没找到
	T*   Search(const T &v, T2 &idout);   //查找指定数据元素的位置指针(元素值, 返回该数据元素的位置ID)返回该数据元素的位置指针 返回0表示没找到

    T*   Begin ();                        //快速直接获得开始端数据指针 不作范围安全检测
	T*   End   ();                        //快速直接获得结尾端数据指针 不作范围安全检测
	T*   RBegin();                        //快速直接获得逆向开始端数据指针 不作范围安全检测
	T*   REnd  ();                        //快速直接获得逆向结尾端数据指针 不作范围安全检测

	bool Front(T &v);                     //获得首个数据( 值输出 ) 返回是否成功
	bool Back(T &v);                      //获得尾个数据( 值输出 ) 返回是否成功
    T&   front();                         //快速直接获得首个数据 不作范围安全检测
    T&   back();						  //快速直接获得尾个数据 不作范围安全检测

	void reset();                         //重设数据存取位置为初始状态, 以便覆盖原有数据  并保留数据空间

	bool can_push();					  //检测是否还可以存入数据

	bool push_front (const T &v);         //在首部存入一个数据( 值 ) 返回是否成功
	bool push_back  (const T &v);         //在尾部存入一个数据( 值 ) 返回是否成功
	bool pop_front  (T &v);               //在首部取出一个数据( 值输出 ) 返回是否成功
	bool pop_back   (T &v);               //在尾部取出一个数据( 值输出 ) 返回是否成功
    T*   pop_begin  ();                   //在首部取出一个数据 成功返回它的指针 失败返回0
	T*   pop_rbegin ();                   //在尾部取出一个数据 成功返回它的指针 失败返回0

	void pop_front__(T &v);               //在首部直接盲取一个数据( 值输出 ) 不作范围安全检测
	void pop_back__ (T &v);               //在尾部直接盲取一个数据( 值输出 ) 不作范围安全检测
	void discd_front();                   //快速直接丢弃首个数据 不作范围安全检测
	void discd_back ();                   //快速直接丢弃尾个数据 不作范围安全检测
	void discd_front(T2 c);               //快速直接丢弃首部几个数据(丢弃个数) 不作范围安全检测
	void discd_back (T2 c);               //快速直接丢弃尾部几个数据(丢弃个数) 不作范围安全检测

	GT_TMP_ITERATOR_OPT_DEF(  ,  sLIST_WHEEL , fid , bid , bid-1 , fid - 1 )

	//快速历遍查找列表轮里的所有实存数据开始( 数据类型 , 计数类型 )  得到 (*p) 为当前数据元素
   #define GN_LIST_WHEEL_SEARCH_BEGIN( T , T2 ) {\
    T *as[2][2], *p;\
    T2 i = 0, n = GetArrays(as);\
	for(; i < n; ++i)\
	    for(p = as[i][0]; p < as[i][1]; ++p){

    //快速历遍查找列表轮里的所有实存数据结束
   #define GN_LIST_WHEEL_SEARCH_END }}
};
template<class T, class T2>
T2 sLIST_WHEEL<T, T2>::sid = 0;

template<class T, class T2>
void sLIST_WHEEL<T, T2>::zComputeSP(){
	T2 sp = 1;
	T2 i  = 1, n = (T2)sizeof(T2);
	for(; i < n; ++i)sp *= 256;
    sid = sp * 128;
}
template<class T, class T2>
void sLIST_WHEEL<T, T2>::Delete(){ GN_DeleteArray(data); reset(); }

template<class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len){
   #define GT_TMP_LIST_WHEEL_NEW \
    this->Delete();\
	zInit();\
	_len = GN_NumGet2n(_len);\
    data = new T [_len];\
    end_id = _len - 1

   #define GT_TMP_LIST_WHEEL_SET_RANGE \
    if(_num >= _len)_num = _len;\
	fid = sid;\
	bid = sid + _num;

	GT_TMP_LIST_WHEEL_NEW;
    this->reset();
}
template<class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len, T2 _num, T & em_all){
    GT_TMP_LIST_WHEEL_NEW;
	GT_TMP_LIST_WHEEL_SET_RANGE
	GN_SetArray( data , _num , em_all );
}
template<class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len, T2 _num, T * p_data){
	GT_TMP_LIST_WHEEL_NEW;
	GT_TMP_LIST_WHEEL_SET_RANGE
	GN_SetArrayFromArray( data , p_data , _num );
}
template<class T, class T2>
void sLIST_WHEEL<T, T2>::New(T2 _len, GN::sLIST< T > &list){
    T2 _num = (T2)list.size();
	GT_TMP_LIST_WHEEL_NEW;
	GT_TMP_LIST_WHEEL_SET_RANGE
	GT_LIST_iterator( T ) it_t = list.begin();
	T2 i = 0;
	for(; i < _num; ++i, ++it_t)data[i] = (*it_t);
}
template<class T, class T2>
T2 sLIST_WHEEL<T, T2>::BufNum(){ return data ? end_id + 1: 0; }

template<class T, class T2>
T2 sLIST_WHEEL<T, T2>::BufSize(){ return data ? (end_id + 1) * sizeof( T ) : 0; }

template<class T, class T2>
bool sLIST_WHEEL<T, T2>::Has(){ return fid < bid; }

template<class T, class T2>
bool sLIST_WHEEL<T, T2>::empty(){ return fid >= bid; }

template<class T, class T2>
T2 sLIST_WHEEL<T, T2>::size(){ return fid < bid ? bid - fid : 0; }

template<class T, class T2>
T2 sLIST_WHEEL<T, T2>::Count(){ return bid - fid; }

template<class T, class T2>
T2 sLIST_WHEEL<T, T2>::GetArrays(T* a[2][2]){
	if(bid > fid){
        T2 _fid = (fid & end_id), _bid = (bid & end_id);
		if(_fid < _bid){
            a[0][0] = data + _fid;  a[0][1] = data + _bid;
			return 1;
		}else{
			a[0][0] = data + _fid;  a[0][1] = data + (end_id + 1);
			if(_bid){
                a[1][0] = data;  a[1][1] = data + _bid;
				return 2;
			}
			return 1;
		}
	}
	return 0;
}
template<class T, class T2>
T* sLIST_WHEEL<T, T2>::ToArray(T2 &n){
    if(bid > fid){
		T *r_data = new T[bid - fid];
		T2 j = 0;
		GN_LIST_WHEEL_SEARCH_BEGIN( T , T2 )
			r_data[j++] = *p;
        GN_LIST_WHEEL_SEARCH_END
		return r_data;
	}
	return 0;
}
template<class T, class T2>
T* sLIST_WHEEL<T, T2>::Search(const T &v){
	GN_LIST_WHEEL_SEARCH_BEGIN( T , T2 )
		if((*p) == v)return p;
    GN_LIST_WHEEL_SEARCH_END
	return 0;
}
template<class T, class T2>
T* sLIST_WHEEL<T, T2>::Search(const T &v, T2 &idout){
	T2 did = 0;
	GN_LIST_WHEEL_SEARCH_BEGIN( T , T2 )
		if((*p) == v){ idout = did; return p; }
		++did;
    GN_LIST_WHEEL_SEARCH_END
	return 0;
}
template<class T, class T2>
T* sLIST_WHEEL<T, T2>::Begin (){ return data + (fid & end_id); }

template<class T, class T2>
T* sLIST_WHEEL<T, T2>::End   (){ return data + (bid & end_id); }

template<class T, class T2>
T* sLIST_WHEEL<T, T2>::RBegin(){ return data + ((bid-1) & end_id); }

template<class T, class T2>
T* sLIST_WHEEL<T, T2>::REnd  (){ return data + ((fid-1) & end_id); }

template<class T, class T2>
bool sLIST_WHEEL<T, T2>::Front(T &v){
	if(bid > fid){
		v = data[fid & end_id];
	    return true;
	}else return false;
}
template<class T, class T2>
bool sLIST_WHEEL<T, T2>::Back(T &v){
	if(bid > fid){
		v = data[(bid - 1) & end_id];
	    return true;
	}else return false;
}
template<class T, class T2>
T&   sLIST_WHEEL<T, T2>::front(){ return data[fid & end_id]; }

template<class T, class T2>
T&   sLIST_WHEEL<T, T2>::back (){ return data[(bid - 1) & end_id]; }

template<class T, class T2>
void sLIST_WHEEL<T, T2>::reset(){ bid = sid; fid = bid; }

template<class T, class T2>
bool sLIST_WHEEL<T, T2>::can_push(){
	return (bid <= fid + end_id);
}
template<class T, class T2>
bool sLIST_WHEEL<T, T2>::push_front(const T &v){
    if(bid > fid + end_id)return false;
	T2 _fid = (bid < fid ? bid : fid) - 1;
	data[_fid & end_id] = v;
	fid = _fid;
	return true;
}
template<class T, class T2>
bool sLIST_WHEEL<T, T2>::push_back(const T &v){
    if(bid > fid + end_id)return false;
	if(bid < fid)bid = fid;
	data[bid & end_id] = v;
	++bid;
	return true;
}
template<class T, class T2>
bool sLIST_WHEEL<T, T2>::pop_front(T &v){
	if(bid > fid){
		++fid;
		v = data[(fid-1) & end_id];
	    return true;
	}else return false;
}
template<class T, class T2>
bool sLIST_WHEEL<T, T2>::pop_back(T &v){
	if(bid > fid){
		--bid;
		v = data[bid & end_id];
	    return true;
	}else return false;
}
template<class T, class T2>
T*   sLIST_WHEEL<T, T2>::pop_begin (){
    if(bid > fid){
		++fid;
		return data + ((fid-1) & end_id);
	}else return 0;
}
template<class T, class T2>
T*   sLIST_WHEEL<T, T2>::pop_rbegin(){
    if(bid > fid){
		--bid;
		return data + (bid & end_id);
	}else return 0;
}
template<class T, class T2>
void sLIST_WHEEL<T, T2>::pop_front__(T &v){
	    ++fid;
		v = data[(fid-1) & end_id];
}
template<class T, class T2>
void sLIST_WHEEL<T, T2>::pop_back__(T &v){
		--bid;
		v = data[bid & end_id];
}
template<class T, class T2>
void sLIST_WHEEL<T, T2>::discd_front(){ ++fid; }

template<class T, class T2>
void sLIST_WHEEL<T, T2>::discd_back() { --bid; }

template<class T, class T2>
void sLIST_WHEEL<T, T2>::discd_front(T2 c){ fid += c; }

template<class T, class T2>
void sLIST_WHEEL<T, T2>::discd_back (T2 c){ bid -= c; }
}; //end namespace GN
/* ----------------------------------------------------- GT_sLIST_WHEEL_AT -----------------------------------------------------------------*/
template<class T>
void GT_EmptyFunc(void){}

template<class T>
void GT_EmptyFunc(T arg){}

template<class T , class T2 >
bool GT_Add1Func(T , void *arg){ ++(*(((T2*)arg))); return true; }

template<class T, class T2 >
class GT_sLIST_WHEEL_AT :public GN::sLIST_WHEEL<T, T2>  //列表轮 首尾相连的数据结构 附加操作版本
{protected:
    virtual void zInit(){ GN::sLIST_WHEEL<T, T2>::zInit(); zpAddOperate = GT_EmptyFunc<void>; }
    void (*zpAddOperate)(void);       //附加动作函数指针
   public:
    void reset();                     //重设数据存取位置为初始状态  以便覆盖原有数据  并保留数据空间

	bool push_front (const T &v,unsigned int wt); //允许等待在首部存入一个数据( 值 , 等待时间) 返回是否成功 !由于空间可能会满 就等待能存入时存入
	bool push_back  (const T &v,unsigned int wt); //允许等待在尾部存入一个数据( 值 , 等待时间) 返回是否成功 !由于空间可能会满 就等待能存入时存入
	bool push_front (const T &v);     //在首部存入一个数据( 值 ) 返回是否成功
	bool push_back  (const T &v);     //在尾部存入一个数据( 值 ) 返回是否成功
	bool pop_front  (T &v);           //在首部取出一个数据( 值输出 ) 返回是否成功
	bool pop_back   (T &v);           //在尾部取出一个数据( 值输出 ) 返回是否成功
    T*   pop_begin  ();               //在首部取出一个数据 成功返回它的指针 失败返回0
	T*   pop_rbegin ();               //在尾部取出一个数据 成功返回它的指针 失败返回0

	void pop_front__(T &v);           //在首部直接盲取一个数据( 值输出 ) 不作范围安全检测
	void pop_back__ (T &v);           //在尾部直接盲取一个数据( 值输出 ) 不作范围安全检测
	void discd_front();               //快速直接丢弃首个数据 不作范围安全检测
	void discd_back ();               //快速直接丢弃尾个数据 不作范围安全检测
	void discd_front(T2 c);           //快速直接丢弃首部几个数据(丢弃个数) 不作范围安全检测
	void discd_back (T2 c);           //快速直接丢弃尾部几个数据(丢弃个数) 不作范围安全检测
};
#ifndef _MSC_VER
extern unsigned int  GT_RunTotalTimeApp;
void GN_TimeSleep(unsigned int);
#endif
#if !defined _WIN32_ && !defined _WIN32_WCE
char *itoa(int, char*, int);
#endif
template<class T, class T2 >
void GT_sLIST_WHEEL_AT< T , T2 >::reset(){ this->bid = this->sid;  this->fid = this->bid; zpAddOperate(); }

template<class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_front(const T &v, unsigned int wt){
	if(push_front(v))return true;
	unsigned int t = GT_RunTotalTimeApp;
	for(;;){
		GN_TimeSleep(1);
		if(push_front(v))return true;
		else if((volatile unsigned int)GT_RunTotalTimeApp - t > wt)return false;
	}
	return false;
}
template<class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_back(const T &v, unsigned int wt){
    if(push_back(v))return true;
	unsigned int t = GT_RunTotalTimeApp;
	for(;;){
		GN_TimeSleep(1);
		if(push_back(v))return true;
		else if((volatile unsigned int)GT_RunTotalTimeApp - t > wt)return false;
	}
	return false;
}
template<class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_front(const T &v){
    if(this->bid > this->fid + this->end_id)return false;
	T2 _fid = (this->bid < this->fid ? this->bid : this->fid) - 1;
	this->data[_fid & this->end_id] = v;
	this->fid = _fid;              zpAddOperate();
	return true;
}
template<class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::push_back(const T &v){
    if(this->bid > this->fid + this->end_id)return false;
	if(this->bid < this->fid)this->bid = this->fid;
	this->data[this->bid & this->end_id] = v;
	++(this->bid);                   zpAddOperate();
	return true;
}
template<class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::pop_front(T &v){
	if(this->bid > this->fid){
		++(this->fid);                       zpAddOperate();
		v = this->data[(this->fid-1) & this->end_id];
	    return true;
	}else return false;
}
template<class T, class T2>
bool GT_sLIST_WHEEL_AT<T, T2>::pop_back(T &v){
	if(this->bid > this->fid){
		--(this->bid);                       zpAddOperate();
		v = this->data[this->bid & this->end_id];
	    return true;
	}else return false;
}
template<class T, class T2>
T*   GT_sLIST_WHEEL_AT<T, T2>::pop_begin (){
    if(this->bid > this->fid){
		++(this->fid);                       zpAddOperate();
		return this->data + ((this->fid-1) & this->end_id);
	}else return 0;
}
template<class T, class T2>
T*   GT_sLIST_WHEEL_AT<T, T2>::pop_rbegin(){
    if(this->bid > this->fid){
		--(this->bid);                       zpAddOperate();
		return this->data + (this->bid & this->end_id);
	}else return 0;
}
template<class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::pop_front__(T &v){
	    ++(this->fid);                      zpAddOperate();
		v = this->data[(this->fid-1) & this->end_id];
}
template<class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::pop_back__(T &v){
		--(this->bid);                      zpAddOperate();
		v = this->data[this->bid & this->end_id];
}
template<class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_front(){ ++(this->fid); zpAddOperate(); }

template<class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_back() { --(this->bid); zpAddOperate(); }

template<class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_front(T2 c){ this->fid += c; zpAddOperate(); }

template<class T, class T2>
void GT_sLIST_WHEEL_AT<T, T2>::discd_back (T2 c){ this->bid -= c; zpAddOperate(); }

/***************************************************************** 内存管理 ************************************************************/
template<class T>
struct GT_sTBUF
{
    T                      _size;		 //存量字节数
	uint8                 *p__;			 //空间首地址

    void *GetSpace( T _size_); //获取指定大小的数据空间(大小字节) 返回数据空间指针 失败返回0
};
template<class T>
void *GT_sTBUF<T>::GetSpace( T _size_){
    if(_size_ > _size)return 0;
	else{
        _size -= _size_;
        return p__ + _size;
	}
}
namespace GN{
template<class T>
struct sTBUF :public GT_sTBUF<T>   //★局部临时高速处理缓冲 适用于在一个功能函数处理时需要临时分配大量(不定量)的临时空间时用 !不适合比较看重构造函数分配的对象数据
{
    T                          size;     //缓冲大小
	GN::sLIST< GT_sTBUF<T> >   add_bufs; //在局部缓冲不够用时来分配的堆缓冲

	sTBUF():size(131072/*128k*/){ this->p__ = 0; this->_size = 0; }
	sTBUF(uint8 *loc_buf, T buf_size);   //创建(指定一个局部缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节) 局部缓冲例如 uint8 loc_buf[100000]; !局部缓冲的大小您应该适当设为稍大于当前需要处理数据的评估最大临时空间, 如果实际使用大于这个设定大小 系统会自动new出适当的其它空间, 但使用到new出来的空间会比较慢
   ~sTBUF(){ this->Clear(); }

    void  Clear();						 //清除一下额外增长多分配出来的内存空间
    void *GetSpace( T _size_);           //获取指定大小的数据空间(大小字节) 返回数据空间指针
};
template<class T>
sTBUF<T>::sTBUF(uint8 *loc_buf, T buf_size){
    #define GT_TMP_INIT_TBUF( BUF ) \
    this->p__ = BUF ;\
	size = buf_size;\
	this->_size = BUF ? buf_size : 0;

	GT_TMP_INIT_TBUF( loc_buf )
}
template<class T>
void sTBUF<T>::Clear(){
	if(add_bufs.size()){
		GT_LIST_iterator(GT_sTBUF<T>) it_b = add_bufs.begin(), et_b = add_bufs.end();
		for(; it_b != et_b; ++it_b)free(it_b->p__);
		add_bufs.clear();
	}
}
template<class T>
void *sTBUF<T>::GetSpace( T _size_){
    T _size4 = _size_ & (~3);
    if(_size_ > _size4)_size_ = _size4 + 4;
	void *rp = GT_sTBUF<T>::GetSpace(_size_);
	if(rp)return rp;
	if(add_bufs.size()){
        GT_LIST_iterator(GT_sTBUF<T>) it_b = add_bufs.begin(), et_b = add_bufs.end();
		for(; it_b != et_b; ++it_b){
			rp = it_b->GetSpace(_size_);
			if(rp)return rp;
		}
	}
	GT_sTBUF<T> * tbf = add_bufs.add(GT_sTBUF<T>());
	if(_size_ > size){
		tbf->p__ = (uint8*)malloc(_size_);
		tbf->_size  = - _size_;
		if(tbf->_size > 0)tbf->_size = 0;
		return tbf->p__;
	}else{
		tbf->p__ = (uint8*)malloc(size);
		tbf->_size  = size - _size_;
		return tbf->p__ + tbf->_size;
	}
}
typedef sTBUF<int> sTBUF_INT;  //定义整形计数的局部缓冲类型

struct GN_API sTBUF_MT :public sTBUF<unsigned int>   //★局部临时高速处理缓冲 能在各线程了安全分配空间 适用于在一个功能函数处理时需要临时分配大量(不定量)的临时空间时用 !不适合比较看重构造函数分配的对象数据
{
	GT_sATOMIC       is_busy;				 //标记是否正在忙

	sTBUF_MT():is_busy(0){}
	sTBUF_MT(uint8 *loc_buf, unsigned int buf_size):sTBUF<unsigned int>(loc_buf,buf_size),is_busy(0){} //创建(指定一个局部缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节) 局部缓冲例如 uint8 loc_buf[100000]; !局部缓冲的大小您应该适当设为稍大于当前需要处理数据的评估最大临时空间, 如果实际使用大于这个设定大小 系统会自动new出适当的其它空间, 但使用到new出来的空间会比较慢
   ~sTBUF_MT(){ this->Clear(); }

    volatile long IsBusy();				 //检测是否正在忙
    void  Clear();                       //清除一下额外增长多分配出来的内存空间
    void *GetSpace(unsigned int _size_); //获取指定大小的数据空间(大小字节) 返回数据空间指针
};
struct GN_API sTBUF_STATE	 //★高速处理缓冲使用状态
{
	GT_sTBUF<int>	*alloc_buf;			 //记录当分配区块
	int				 m_size;			 //记录当时可用空间大小
};
struct GN_API sTBUF_MAG :public sTBUF_INT	  //★高速处理缓冲管理器 适用于处理需要临时分配大量(不定量)的临时空间时用 这个比内存池更有效率 !但不适合比较看重构造函数分配的对象数据 !这个不局限于在一个功能函数内 配合 sTBUF_STATE 和 SaveUsState 可以快速腾出不再需要的空间给下个处理反复用
{
	GN::sLIST<unsigned int>     buf_idle_times;		 //各个附加缓冲的空闲时间
	int							last_reset_max_bid;	 //记录上次重设或恢复空间用量状况时最大使用到的附加缓冲ID号
	unsigned int				remove_time;		 //移除空闲附加缓冲超时时间

	sTBUF_MAG(){}
	//构造  (借用共享缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节, 使用到内存堆上的空闲空间移除超时时间) !该管理器不负责借用的共享缓冲的空间创建和删除
	sTBUF_MAG(uint8 *share_buf, int buf_size, unsigned int out_time):sTBUF_INT(share_buf, buf_size),remove_time(out_time){}

	//初始化(借用共享缓冲 0为不指定那么之后所获取的空间总是在内存堆上的, 缓冲大小字节, 使用到内存堆上的空闲空间移除超时时间) !该管理器不负责借用的共享缓冲的空间创建和删除
	inline void Init(uint8 *share_buf, int buf_size, unsigned int out_time){
		GT_TMP_INIT_TBUF( share_buf )
		remove_time = out_time;
	}
	void Clear();  //清除一下额外增长多分配出来的内存空间

	//重设使各个空间区块存量恢复为之前指定的缓冲大小 !调用这个你需确保之前分配的空间数据可丢去失效的
	void ResetSpace();

	//重设数据缓冲(缓冲, 首部以使用大小) !调用这个你需确保之前分配的空间数据可丢去失效的
	void ResetSpace(uint8 *buf, int used_size);

	//获得当前空间用量状况(状况输出)
	void SaveUsState(sTBUF_STATE &tst);

	//恢复之前调用 SaveUsState 时的空间用量状况(用量状况)
	void RestoreUsState(const sTBUF_STATE &tst);

	//检测并清除空闲未被使用的空间(更新时间) !这个应该放在每帧都被调用的地方
	void CheckAndRemoveNoUseMem(unsigned int up_time);
};
class GN_API cBUF_WHEEL		  //缓冲轮 首尾相通的缓冲空间 !适合边写数据边读数据的应用场合 在尾写入在头取出 数据写到末尾之后如前段数据以被取出就轮回到开头段继续写入 这样就可以用小缓冲来处理大数据流 !当写满时将会自动扩充内部空间以保证所有数据都能成功写入
{
	uint8			 *zBuf;		  //数据缓冲
	unsigned int	  zBufSize;	  //缓冲大小字节
	unsigned int	  zReadPos;	  //当前取出位置
	unsigned int	  zWritePos;  //当前写入位置
   public:
	cBUF_WHEEL():zBuf(0){}
	cBUF_WHEEL(unsigned int size):zBufSize(size){ zBuf = (uint8*)malloc(size); zReadPos = 0; zWritePos = 0; } //分配缓冲空间(初始缓冲大小字节)
   ~cBUF_WHEEL(){ GN_MemFree(zBuf); }

    void Delete(){ GN_MemFree(zBuf); }			  //删除缓冲数据
	void Clear(){ zReadPos = 0; zWritePos = 0; }  //清空以存数据

	//分配缓冲空间(初始缓冲大小字节)
	void New(unsigned int size){ this->Delete(); zBufSize = size; zBuf = (uint8*)malloc(size); zReadPos = 0; zWritePos = 0; }
	
	//写入数据(数据, 数据大小字节)返回内部是否有扩充过空间
	bool Write(const uint8 *data, unsigned int size);

	//读取数据(装填数据的空间, 需读取大小字节)返回取出数据大小字节 返回0表示里面没数据了
	unsigned int Read(uint8 *data_out, unsigned int size);

	//取出内部数据(输入需取的大小字节返回取出数据大小字节)返回内部数据指针 !如果内部数据在指定大小不连续时将会自动整理为连续一段 调用后被取出的数据地址段 会被 Write 写入的数据覆盖
	uint8 *GetOut(unsigned int &size_in_out);

	//获得以存数据大小字节
	inline unsigned int DataSize() { return (zWritePos < zReadPos? zBufSize-zReadPos+zWritePos: zWritePos-zReadPos); }    
	
	//数据缓冲大小字节
	inline unsigned int BufSize(){ return zBufSize; }
};
/*--------------------------------------------------------------------------------------------------------------------------------------*/
template<class T, class T2 >
struct cDEPOT                 //数据仓库  是一种封闭的能定点快速移除特定数据 并且容量可自动扩充的数组 访问里面数据可像数组一样快速历遍 但放入数据最好用 insert 拿到数据的存放位置 !注意不适合比较看重构造函数分配的对象数据
{
    T2             ** opps;       //对方联系号码缓冲
    T               * data;       //数据缓冲
	T2                num;        //实存数据个数
    T2                buf_len;    //数据空间个数
	GN::sTBUF<T2>   * tbuf;       //临时缓冲分配器
   public:
	GT_TMP_ARRAY_ELEMENT_GET_DEF

   ~cDEPOT(){ this->clear(); tbuf = 0; }
	cDEPOT():data(0),num(0),buf_len(0),tbuf(0){}

	//构造(预留多少个数据空间, 临时缓冲分配器 如果放到里面的数据只是临时处理用给定这个性能更好)
	cDEPOT(T2 count, GN::sTBUF<T2> *_tbuf=0):data(0),num(0),buf_len(0),tbuf(0){
	    this->reserve(count, _tbuf);
	}
	T   *Data() { return data; }                   //获得数据首地址  !!注意返回的地址您最好只用在临时处理的时候 因为如果您有新的数据放入后 该地址可能会变为无效
	T2   BufNum() { return buf_len; }              //获得数据空间个数
	T2   BufSize(){ return sizeof(T) * buf_len; }  //获得数据空间所占大小字节
	bool Has()  { return num ?true:false; }        //是否有实存数据
	bool empty(){ return num ?false:true; }        //是否无实存数据
	T2   size() { return num; }                    //获得实存数据个数

	//清除内部所有数据
	void clear();

	//重分配预留一定的空间(预留多少个数据空间, 临时缓冲分配器 如果放到里面的数据只是临时处理用给定这个性能更好)
	void reserve(T2 count, GN::sTBUF<T2> *_tbuf=0);

	//在尾部插入一个数据(返回装数据的位置ID, 数据)               !!! id_out 收到ID之后需确保 该ID的数据空间和数据仓库并存 因为之后如有移除过数据为了压缩储存空间 数据仓库会搬动数据从而更新这个ID
	void insert(T2 *id_out, const T &v);

    //在尾部插入空数据位置(返回装数据的位置ID)返回该数据位置地址 !!! id_out 收到ID之后需确保 该ID的数据空间和数据仓库并存 因为之后如有移除过数据为了压缩储存空间 数据仓库会搬动数据从而更新这个ID
	T* insert(T2 *id_out);

	//移除指定数据(数据位置ID)
	void remove(T2 id);

	//重设数据存取位置为初始状态, 以便覆盖原有数据 并保留数据空间
	void reset(){ num = 0; }

    //向所有拥有数据者通告数据无效(无效值) !调用后拥有者的数据位置ID变为指定值
	void notify_inv(T2 inv);

	GT_TMP_ITERATOR_OPT_DEF( Ext ,  cDEPOT , 0 , num , num-1 , 0 - 1 )
};
template<class T, class T2 >
void cDEPOT<T, T2>::clear(){
    if(data){
		if(!tbuf){
            free(opps);
			free(data);
		}
		opps = 0;
		data = 0;
	}
    num = 0;
    buf_len = 0;
}
template<class T, class T2 >
void cDEPOT<T, T2>::reserve(T2 count, GN::sTBUF<T2> *_tbuf){
	T   *data2 = (T   *)(_tbuf ? _tbuf->GetSpace(sizeof( T ) * count) : malloc(sizeof( T ) * count));
    T2 **opps2 = (T2 **)(_tbuf ? _tbuf->GetSpace(sizeof(T2*) * count) : malloc(sizeof(T2*) * count));
	if(data){ //原本有数据
		if(num){
		    T2 cpy_n = (num < count ? num : count);
		    memcpy(data2, data, sizeof( T ) * cpy_n);
			memcpy(opps2, opps, sizeof(T2*) * cpy_n);
		}
		if(!tbuf){
			free(opps);
			free(data);
		}
	}
    data    = data2;
    opps    = opps2;
    buf_len = count;
    tbuf    = _tbuf;
}
template<class T, class T2 >
void cDEPOT<T, T2>::insert(T2 *id_out, const T &v){
	if(num>=buf_len)reserve(buf_len < 8 ? 8 : buf_len + (buf_len >> 1), tbuf);
    data[num] = v;
    opps[num] = id_out;
	*id_out = num++;
}
template<class T, class T2 >
T* cDEPOT<T, T2>::insert(T2 *id_out){
    if(num>=buf_len)reserve(buf_len < 8 ? 8 : buf_len + (buf_len >> 1), tbuf);
//  data[num] = v;
    opps[num] = id_out;
	*id_out = num;
	return data + num++;
}
template<class T, class T2 >
void cDEPOT<T, T2>::remove(T2 id){
	--num;
	if(id >= num)return;
    data[id]  = data[num];
	opps[id]  = opps[num];
  *(opps[id]) = id;
}
template<class T, class T2 >
void cDEPOT<T, T2>::notify_inv(T2 inv){
    T2 i = 0;
	for(; i < num; ++i)*(opps[i]) = inv;
}
}; //endif namespace GN
/*-------------------------------------------------------------------------------------------------------------*/
template<class T>
inline int GT_CompareNodeNumber(T &cur_v, T &v){
	return cur_v == v ? 0 : (v < cur_v ? -1 : 1);
}
template<class T>
inline int GT_CompareImgHeightMin(T cur_v, T v){
	return cur_v->h == v->h ? 0 : (v->h < cur_v->h ? -1 : 1);
}
template<class T>
inline int GT_CompareImgHeightMin1(T cur_v, T v){
	return v->h > cur_v->h ? 1 : -1;
}
template<class T>
int GT_CompareSegPosMin(T &cur_v, T &v){
	return cur_v.pos == v.pos ? 0 : (v.pos < cur_v.pos ? -1 : 1);
}
template<class T>
int GT_CompareSegLenMin(T &cur_v, T &v){
	return cur_v.len == v.len ? 0 : (v.len < cur_v.len ? -1 : 1);
}
template<class T>
int GT_CompareSegLenMin1(T &cur_v, T &v){
	return v.len > cur_v.len ?  1 : -1;
}
template<class T>
int GT_CompareSegLenMin2(T &cur_v, T &v){
	return cur_v.len == v.len ? (cur_v.pos == v.pos ? 0 : -1) : (v.len < cur_v.len ? -1 : 1);
}
namespace GN{
template<class T, class A = std::allocator<T> >
class cTREE2                          //二分查找树
{
	template<class T_ , class A_ >
    struct s_TREE2_NODE                  //二分查找树节点
    {
		typedef typename A_::template rebind<s_TREE2_NODE>::other::pointer TNP;

		T_                         zData;        //数据
		TNP                        zLeft;        //左子节点
		TNP                        zRight;       //右子节点

		s_TREE2_NODE(T_ &v):zData(v), zLeft(0), zRight(0){}
    };
	typedef typename A::template rebind< s_TREE2_NODE<T,A> >::other ALLOC;
   public:
    typedef typename A::template rebind< s_TREE2_NODE<T,A> >::other::pointer TP;
   private:
    TP                 zRoot;            //跟节点
	ALLOC              zAlloc;           //节点空间分配器
   public:
	cTREE2():zRoot(0){}
   ~cTREE2(){ this->clear(); }

    //清除指定节点(节点)
	void zClear(TP &p);

    //清除树所有节点
	inline void clear(){
        zClear(zRoot);
        zRoot = 0;
	}
    //获得根节点指针
	inline TP *GetRoot(){ return &zRoot; }

	//采用二分查找内容符合条件的节点(比较条件(当前节点内容,比较内容)要求返回查找方向值, 传入的比较内容, 返回符合条件的节点或比较接近条件的终止节点指针)返回最后的查找方向值 值0:找到符合条件的节点  -1:表示在接近条件的左侧终结了 1:表示在接近条件的右侧终结了
	int search2_if( int (*compare)(T &cur_v, T &v) , T &cmp_v, TP **node_out);

    //采用二分查找内容符合条件的节点(比较条件(当前节点内容,比较内容)要求返回查找方向值, 传入的比较内容, 返回符合条件的节点或比较接近条件的终止节点指针, 返回比node_out大最小的节点指针 如果无返回0)返回最后的查找方向值 值0:找到符合条件的节点  -1:表示在接近条件的左侧终结了 1:表示在接近条件的右侧终结了
    int search2_if( int (*compare)(T &cur_v, T &v) , T &cmp_v, TP **node_out, TP **up_node_out);

	//采用二分查找内容符合条件的节点(比较条件(当前节点内容,比较内容)要求返回查找方向值, 传入的比较内容, 返回符合条件的节点或比较接近条件的终止节点指针, 返回比node_out大最小的节点指针 如果无返回0, 返回比node_out小最小的节点指针 如果无返回0)返回最后的查找方向值 值0:找到符合条件的节点  -1:表示在接近条件的左侧终结了 1:表示在接近条件的右侧终结了
	int search2ff_if( int (*compare)(T &cur_v, T &v) , T &cmp_v, TP **node_out, TP **up_node_out, TP **down_node_out);

	//在指定节点里插入一个内容(节点指针, 内容)
	inline void insert(TP *node, T &v){
       (*node) = zAlloc.allocate(1, &node);
      *(*node) = s_TREE2_NODE<T,A>(v);
	}
    //插入一个内容值(内容, 插入排位条件(当前节点内容, 插入内容)要求返回排位查找方向值)返回插入值存放节点指针 !排位查找方向值 值0:查找方向终止直接覆盖当前节点内容 -1:再往当前节点左侧排 1:再往当前节点右侧排
	TP * insert(T &v, int (*compare)(T &cur_v, T &v) = GT_CompareNodeNumber){
	    TP *find_node;
	    if(search2_if( compare , v, &find_node) == 0){
            (*find_node)->zData = v;
	    }else insert(find_node, v);
	    return find_node;
    }
	//查找删除符合条件内容的节点(条件内容, 比较条件(当前节点内容, 条件内容)要求返回查找方向值) !查找方向值 值0:查找方向终止删除当前节点内容 -1:再往当前节点左侧查 1:再往当前节点右侧查
	bool erase(T &v, int (*compare)(T &cur_v, T &v) = GT_CompareNodeNumber);

    //删除指定节点(节点指针)
	void erase(TP *node);

	//获得指定节点最靠左的结尾节点(指定节点, 返回结尾节点指针)返回该路径有多少个有效节点
	int zGetLeftEndNode(TP p, TP **node_out);

	//获得指定节点最靠右的结尾节点(指定节点, 返回结尾节点指针)返回该路径有多少个有效节点
	int zGetRightEndNode(TP p, TP **node_out);
};
template<class T, class A>
void cTREE2<T, A>::zClear(TP &p){
	if(!p)return;
    zClear(p->zLeft );
	zClear(p->zRight);
    zAlloc.destroy(p);
    zAlloc.deallocate(p, 1);
}
template<class T, class A>
int cTREE2<T, A>::search2_if( int (*compare)(T &cur_v, T &v) , T &cmp_v, TP **node_out){
   #define GT_TREE2_SEARCH_IF( ADD_PROC , ADD_PROC2 ) \
	int ret = -1;\
    TP *find_node = &zRoot;\
	for(;;){\
		if(!(*find_node)){ *node_out = find_node; return ret; }\
		ret = compare( (*find_node)->zData , cmp_v );\
		if(!ret){ *node_out = find_node; return 0; }\
		else if(ret < 0){ ADD_PROC  find_node = &((*find_node)->zLeft);  }\
		else            { ADD_PROC2 find_node = &((*find_node)->zRight); }\
	}
    GT_TREE2_SEARCH_IF(   ,  )
}
template<class T, class A>
int cTREE2<T, A>::search2_if( int (*compare)(T &cur_v, T &v) , T &cmp_v, TP **node_out, TP **up_node_out){
	*up_node_out = 0;
    GT_TREE2_SEARCH_IF( *up_node_out = find_node; ,   )
}
template<class T, class A>
int cTREE2<T, A>::search2ff_if( int (*compare)(T &cur_v, T &v) , T &cmp_v, TP **node_out, TP **up_node_out, TP **down_node_out){
	*up_node_out   = 0;
	*down_node_out = 0;
    GT_TREE2_SEARCH_IF( *up_node_out = find_node;  , *down_node_out = find_node; )
}
template<class T, class A>
bool cTREE2<T, A>::erase(T &v, int (*compare)(T &cur_v, T &v)){
    TP *find_node;
	if(search2_if( compare , v, &find_node) == 0){
		erase(find_node);
		return true;
	}
	return false;
}
template<class T, class A>
void cTREE2<T, A>::erase(TP *node){
    TP *l_node, *r_node;
	TP del_node = (*node);
	if(     !(del_node->zRight))*node = del_node->zLeft;
	else if(!(del_node->zLeft ))*node = del_node->zRight;
	else{
		if(zGetLeftEndNode(del_node->zRight, &l_node) < zGetRightEndNode(del_node->zLeft, &r_node)){
		    *l_node = del_node->zLeft;
            *node   = del_node->zRight;
	    }else{
            *r_node = del_node->zRight;
            *node   = del_node->zLeft;
	    }
	}
	zAlloc.destroy(del_node);
    zAlloc.deallocate(del_node, 1);
}
template<class T, class A>
int cTREE2<T, A>::zGetLeftEndNode(TP p, TP **node_out){
   #define GT_TMP_TREE2_GET_END_NODE( L ) \
	int i = 0;\
	for(;;++i){\
		*node_out = &(p-> L );\
		p = p-> L ;\
		if(!p)return i;\
	}
	GT_TMP_TREE2_GET_END_NODE( zLeft )
}
template<class T, class A>
int cTREE2<T, A>::zGetRightEndNode(TP p, TP **node_out){
	GT_TMP_TREE2_GET_END_NODE( zRight )
}
/* ------------------------------------------------------------------------------------------------------------------*/
class GN_API cVOLUME_MAG         //用量管理器
{
	typedef GN::cTREE2<sSEGMENT<unsigned int> , GN::cALLOC_POOL<sSEGMENT<unsigned int> > > cTREE2_UINTx2;
	unsigned int (*zpfOnFull)(unsigned int tot_size, unsigned int need_size, void *us_arg);  //当容量满时的回调
    void                                         *zUsArg;             //用户参数
	unsigned int                                  zTotalSize;         //总空间大小个数
    cTREE2_UINTx2                                 zSizeTable;         //按大小的2叉查找表
	cTREE2_UINTx2                                 zPosTable;          //按存储位置的2叉查找表

   public:
	cVOLUME_MAG():zTotalSize(0){}
   ~cVOLUME_MAG(){ this->Release(); }

    //释放内部资源
	void Release();

	//初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 当前需要分配的最小大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数)
	void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, unsigned int need_size, void *us_arg), void *_us_arg);

    //用量空间申请(空间大小个数)返回空间的位置
	unsigned int Alloc(unsigned int size);

    //归还空间(空间位置, 空间大小个数, 归还的空间可否与前面剩余空间合并, 归还的空间可否与后面剩余空间合并)
	void Free(unsigned int pos, unsigned int size, bool merge_front=true, bool merge_back=true);

	//获得当前总容量大小
	unsigned int GetTotalSize(){ return zTotalSize; }

    //获得最尾空段范围
	sSEGMENT<unsigned int> GetEndSegRange();
};
/*----------------------------------------------------------------------------------------------------------------------------*/
class GN_API cMEM_POOL  :public cVOLUME_MAG , public GT_sBUF_MAPPING        //内存池
{public:
    GT_TMP_MEM_POOL_BASE_FUNC_DEF(  )

    //初始化(容量总大小个数, 当容量满时的回调(当前总大小个数, 当前需要分配的最小大小个数, 传回给您用户参数)要求返回新分配大小个数, 用户参数) !!后两个参数在该类中无效
	void InitToData(unsigned int total_size, unsigned int (*full_proc)(unsigned int tot_size, unsigned int need_size, void *us_arg)=0, void *_us_arg=0);

    //空间申请(多少个数据空间)返回空间指针
    uint8 * Alloc(unsigned int size=1){
		return GetPtr(cVOLUME_MAG::Alloc(size));
	}
	//释放归还空间(空间指针, 释放多少个数据空间)
	void Free(const uint8 *p, unsigned int size=1){
		unsigned int pos = GetPos(p);
		cVOLUME_MAG::Free(pos, size, (pos % zBufElmNum) != 0, ((pos+size) % zBufElmNum) != 0);
	}
	// ------- 以下两个函数用于模拟常规的内存分配 malloc 和 free  数据单位视为字节 使用这两个函数 该对象构造时单个数据大小必须为1 --------------------------

    //通用字节空间申请(多少个字节空间)返回空间指针
    uint8 * AllocByte(unsigned int size=1){
       #ifdef GN_DEBUG
		if(zUnitSize > 1)GN_Log("调用 cMEM_POOL::AllocByte 存在问题!! 该对象目前管理的数据单个大小为:%d 超过1字节!\n", zUnitSize);
       #endif
		size += 4;
		unsigned int *p = (unsigned int*)GetPtr(cVOLUME_MAG::Alloc(size));
        *p = size;
		return (uint8*)(p+1);
	}
    //释放归还通用字节空间(空间指针)
	inline void FreeByte(const uint8 *p){
        unsigned int *p1 = ((unsigned int*)p) - 1;
        Free((uint8*)p1, *p1);
	}
};
};
/*********************************************** 安全指针 **************************************************************************/
//#define GT_CUSTODY_PTR
#ifdef GT_CUSTODY_PTR
#define GT_P_ACTUAL( DNUM ) (DNUM == 0xffffffff ? 1 : DNUM)
#else
#define GT_P_ACTUAL( DNUM ) DNUM
#endif
#ifdef GN_DEBUG
#define GT_PMEM_WORNING
#endif
#ifndef GT_PMEM_WORNING
#define P_SetName( pointer , name )
#define P_PrintName()
#define P_PrintUsRange()
#else
//设置安全指针的名称(安全指针, 名称)以方便跟踪识别
#define P_SetName( pointer , name ) \
	strcpy( pointer .zName , name );\
    if( pointer. zData)GN_Log("@@: %08x - %08x %s\n", pointer. zData->pmem, ( pointer. zData->pmem) + pointer. Size(), zName);

//打印安全指针的名称
#define P_PrintName() if(zName[0])GN_Log("@@: %08x - %08x %s\n",0x00000000, 0x00000000, zName);

//打印安全指针的内存范围
#define P_PrintUsRange() if(zName[0])GN_Log("@@: %08x - %08x %s\n",zData->pmem, ((T *)zData->pmem) + zData->num, zName);
#endif

//强制转换安全指针( 目标类型 )
#define CVP( CType ) *(P< CType >*)&

//强制转换表示类型 而不转换它在内存的值( 目标类型 )
#define CV( CType ) *( CType *)&

struct GT_sMEMORY{				  //内存块结构
    void					*pmem;     //内存地址
	volatile unsigned int	 us;       //使用者数
	unsigned int			 num;      //数据数

	inline bool operator == (const GT_sMEMORY &mem) const{ return this->pmem == mem.pmem; }
	inline bool operator <  (const GT_sMEMORY &mem) const{ return this->pmem  < mem.pmem; }
};
struct GN_API GT_sMEMORY_TABLE{   //内存表结构
    GN::sLIST<GT_sMEMORY>	 memList;  //内存使用表 记下那些内存正在被使用
	GT_sATOMIC		         isBusy;   //内存使用表是否被占用

	GT_sMEMORY_TABLE();
   ~GT_sMEMORY_TABLE();

    volatile GT_sMEMORY *Add(GT_sMEMORY &em);  //记录一个内存块(内存块) 返回内存块在这里的位置
	void Del(volatile GT_sMEMORY *mem);		   //移除一个内存块记录(内存块位置)
};
extern GN_API GT_sMEMORY_TABLE GT_UsMemTable; //定义一个全局内存表

//inline bool CompareRemoveifMemory(GT_sMEMORY &mem){ return (mem.us == 0); } //释放内存的比较条件(当前检索到的内存块)返回是否可移除

extern GN_API bool P_CopyModeAll; //用于设置所有类型的安全指针遇到 " = " 和 " P<T> pointer(P) " 时是否为复制副本数据模式

struct  P_            //★所有安全指针基类 支持线程安全 一般用作不同类型安全指针的便捷传值 !注意它不能拥有和操作数据
{
  	volatile GT_sMEMORY       * zData; //指向内存使用表的元素位置

   #ifdef GT_PMEM_WORNING
    char						zName[64];

	inline  P_():zData(0){ zName[0] = 0; }
   #else
    inline  P_():zData(0){}
   #endif
    inline ~P_(){ /*zData = 0;*/ }
	inline void Delete(){ zData = 0; }

	//比较是否指向相同数据内存
	inline bool operator == (const P_ & p_)const{ return (this->zData == p_.zData); }
	//比较是否指向不同数据内存
	inline bool operator != (const P_ & p_)const{ return (this->zData != p_.zData); }

	//返回指针是否含有数据
	inline bool Has(){ return zData ? true : false; }

	//逆向从普通指针找回它的安全指针( 普通指针 )  !这需要一些处理时间 这普通指针必需是用安全方式创建来的 被安全指针包含 否侧该调用被忽略
	void Re(void *p);
};
/*------------------------------------------- 安全指针原理 ----------------------------------------------------*\

	安全指针主要用引用计数来管理内存片的生命存活周期 刚分配空间时使用者数为1 把它传给其它安全指针使用者数加1
	当删除安全指针时 仅在被使用者数为1的才会切底删除数据空间 否则记录使用者数减1 这样就可确保数据安全有效

	一般在对象里或局部函数范围内的安全指针都无需手动删除释放 引擎会在它没用时自动为您清除那些内存空间
	只有静态类型的或在全局区域的安全指针 如您不想一些数据内存被长期闲置占用空间即可手动清除一下

\*-------------------------------------------------------------------------------------------------------------*/
template<class T>
struct  P : public P_  //★★★安全指针  支持线程安全 预防内存涉漏 防重复删除造成崩溃 完美解决多对象同时引用同一指针删除问题 和提供更多数据操作方法
{
    static bool			copyMode;      //用于设置该类型指针遇到 " = " 和 " P<T> pointer(P) " 时是否为复制副本数据模式

    inline  P(){}
	inline ~P(){ this->Delete(); }

	//删除数据 ------------------------------------------------------------------------------------------------
	inline void Delete(){
		if( zData ){
			if(zData->us > 1 ){ zData->us--; } //如果这快内存被多个指针共享的话就记下它的共享数少1
			else if(zData->us == 1){ //仅有一个指针共享它时就切底删除它
               #ifdef GT_CUSTODY_PTR //删除托管的普通指针数据
				if(zData->num == 0xffffffff)delete (T*)zData->pmem;
				else
               #endif
                    delete [] (T*)zData->pmem;
				GT_UsMemTable.Del(zData);
			}
			zData = 0;
			P_PrintName();
		}
	}
	//分配数据(多少个元素) ------------------------------------------------------------------------------------
	inline void New(unsigned int count){
		this->Delete();
		#ifdef GT_PMEM_WORNING
		if(!count)GN_Log("P %s 分配数据空间为: 0 !!\n", zName[0]?zName:"");
        #endif
		GT_sMEMORY em;
        em.us = 1;
		em.num = count;
		em.pmem = new T [count];			   //分配一个内存块
		zData = GT_UsMemTable.Add(em);
		P_PrintUsRange();
	}
	inline P(unsigned int count){ this->New(count); }

	inline void operator ()(unsigned int count){ this->New(count); }

    //分配数据(多少个元素, 全部的默认初始值) ------------------------------------------------------------------
	inline void New(unsigned int count, T em_all){
		this->New(count);
		T *it = (T*)(zData->pmem), *et = ((T*)zData->pmem)+ zData->num ;
		for(; it < et; ++it)*it = em_all;
	}
    inline P(unsigned int count, T em_all){ this->New(count, em_all); }

	inline void operator ()(unsigned int count, T em_all){ this->New(count, em_all); }

	//从复制普通指针数据来初始分配数据( 普通指针, 数据个数) 如数据个数为0就当它是字符串来处理 -----------------
	inline void New(const T *p_data, unsigned int count){
		#ifdef GT_PMEM_WORNING
		if(!p_data)GN_Log("P %s 分配空间时复制数据源为无效的?!!\n", zName[0]?zName:"");
        #endif
        if(p_data){
			if(count){
			    this->New(count);
			    T *it = (T*)(zData->pmem), *et = ((T*)zData->pmem)+ count ,*id = (T*)(p_data);
			    for(; it < et; ++it, ++id)*it = *id;
			}else if(sizeof(T)==sizeof(char)){ //是字符类型时
				this->New( (unsigned int)strlen((char*)p_data) + 1);
				strcpy((char*)(this->zData->pmem), (char*)p_data);
				return;
			}
			return;
        }
        this->Delete();
	}
	inline P(const T *p_data, unsigned int count){ this->New(p_data,count); }

	inline void operator ()(const T *p_data, unsigned int count){ this->New(p_data,count); }

    //从复制连表里的数据来初始化分配数据( 连表 ) --------------------------------------------------------------
	inline void New(GN::sLIST< T > &list){
        if(list.empty()){
		   #ifdef GT_PMEM_WORNING
			GN_Log("P %s 分配空间时复制链表源为空的?!!\n", zName[0]?zName:"");
           #endif
            this->Delete();
			return;
		}
		this->New((unsigned int)list.size());
		GT_LIST_iterator( T ) it_t = list.begin(), et_t = list.end();
		T *it = (T*)(zData->pmem);
		for(; it_t != et_t; ++it_t, ++it)*it = (*it_t);
	}
	inline P(GN::sLIST< T > &list){ this->New(list); }

    inline void operator ()(GN::sLIST< T > &list){ this->New(list); }
   #ifdef GT_CUSTODY_PTR
	//一般在分配数据并一起构造对象时用这个函数比较有效率一点 使用示范: P<int> pointer(new int(123));  new出来的不能是数组 如 new int[3]
	//托管新分配的普通指针来初始化(新 new 出来的数据指针) 这不会另行分配空间来复制数据  托管后new出来的指针不必释放调用C++的 delete 了
	inline void New(const T *new_p){
        #ifdef GT_PMEM_WORNING
		if(!new_p)GN_Log("P %s 初始化时托管的指针为无效的?!!\n", zName[0]?zName:"");
        #endif
        this->Delete();
		GT_sMEMORY em;
        em.us = 1;
		em.num = 0xffffffff;	//这个标记它是用单独的一个数据分配的
		em.pmem = (void*)new_p;
		zData = GT_UsMemTable.Add(em);
		P_PrintUsRange();
	}
	inline P(const T *new_p){  this->New(new_p); }

	inline void operator ()(const T *new_p){ this->New(new_p); }
   #endif
	//赋值复制副本 --------------------------------------------------------------------------------------------
	inline P(const P &pm){
		if(copyMode || P_CopyModeAll){
		    if(pm.zData){
				unsigned int data_num = GT_P_ACTUAL(pm.zData->num);
		        this->New( data_num ); //另外创建一份新内存
			    unsigned int ci = 0;
			    for(; ci < data_num; ++ci)((T*)(zData->pmem))[ci] = ((T*)(pm.zData->pmem))[ci];
		    }else this->zData = 0;
		}else{
		    if(pm.zData){
		        this->zData = pm.zData;
                ++(this->zData->us);
		    }else this->zData = 0;
		}
	}
	//赋值复制副本
	inline P(const P_ &p_){ *this = *(P*)&p_; }

	//等号复制副本
	inline P & operator = (const P &pm){
		if(copyMode || P_CopyModeAll){
		    if(pm.zData){
                unsigned int data_num = GT_P_ACTUAL(pm.zData->num);
		        this->New( data_num ); //另外创建一份新内存
			    unsigned int ci = 0;
				for(; ci < data_num; ++ci)((T*)(zData->pmem))[ci] = ((T*)(pm.zData->pmem))[ci];
		    }else this->Delete();
		}else{
		    this->Delete();
		    if(pm.zData){
		        this->zData = pm.zData;
                ++(this->zData->us);
				P_PrintUsRange();
		    }
		}
		return *this;
	}
	//等号复制副本
	inline P & operator = (const P_ &p_){ *this =  *(P*)&p_; return (*this); }

	//从其它安全指针复制一份相同的数据( 其它安全指针 )
	inline void Copy(const P &pm){
		if(pm.zData){
			unsigned int data_num = GT_P_ACTUAL(pm.zData->num);
		    this->New( data_num ); //另外创建一份新内存
			unsigned int ci = 0;
		    for(; ci < data_num; ++ci)((T*)(zData->pmem))[ci] = ((T*)(pm.zData->pmem))[ci];
		}else this->Delete();
	}
    //复制出内部数据到新的安全指针( 增补元素个数, 新增的全部元数的默认值 ) 返回装有复制数据的新安全指针 首个参数为负数可以裁短数据
	inline P  CopyOut(int add_count, T em_all){
		P pd;
		unsigned int data_num = GT_P_ACTUAL(zData->num);
		int ci, new_num = (int)data_num + add_count;
		if(new_num > 0){
			pd.New( new_num );
			if(new_num >= data_num){
			   for(ci = 0; ci < data_num; ++ci)((T*)(pd.zData->pmem))[ci] = ((T*)(zData->pmem))[ci];
               for(;       ci < new_num ; ++ci)((T*)(pd.zData->pmem))[ci] = em_all;
			}else{
			   for(ci = 0; ci < new_num ; ++ci)((T*)(pd.zData->pmem))[ci] = ((T*)(zData->pmem))[ci];
			}
		}
		return pd;
	}
	//与指定安全指针交换指向的数据内容(其它安全指针) !!如果有其它安全指针与两者共享同一片数据,这修改将会影响其它共享的安全指针
	inline void Swap(const P & pm){
		GN_Swap(void*, zData->pmem , pm.zData->pmem);
		GN_Swap(unsigned int, zData->num , pm.zData->num);
	}
    //替换内部一组元素值(开始替换的元素索引号, 共替换几个元素, 这些元素的替换值)
	inline void Replace(unsigned int id, unsigned int count, T em_all){
	   #ifdef GT_PMEM_WORNING
        if(!count){
			GN_Log("P %s Replace count = 0? !!\n", zName[0]?zName:"");
			return;
		}
       #endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
		if(id >= data_num)return; //判断处理可能用户乱填的参数范围
		count += id;
		if(count > data_num)count = data_num;
		T *it = (T*)(zData->pmem);
        for(; id < count; ++id)it[id] = em_all;
	}
    //在内部从新分配指定大小的空间( 多少个元素 ) !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针
	inline void ReSize(unsigned int new_num){
	   #ifdef GT_PMEM_WORNING
		if(!new_num){
			GN_Log("P %s ReSize new_num = 0? !!\n", zName[0]?zName:"");
			return;
		}
       #endif
		unsigned int  data_num = GT_P_ACTUAL(zData->num);
		if(new_num == data_num)return;
		unsigned int ci, min = data_num < new_num ? data_num : new_num;
		T * new_data = new T [new_num];
		for(ci = 0; ci < min; ++ci)new_data[ci] = ((T*)(zData->pmem))[ci];
       #ifdef GT_CUSTODY_PTR
       #define GT_P_MEM_RESET \
		if(zData->num == 0xffffffff)delete (T*)zData->pmem;\
		else delete [] (T*)zData->pmem;\
        zData->pmem = new_data;\
        zData->num  = new_num
       #else
       #define GT_P_MEM_RESET \
		delete [] (T*)zData->pmem;\
        zData->pmem = new_data;\
        zData->num  = new_num
       #endif
		GT_P_MEM_RESET;
        P_PrintUsRange();
	}
	//在内部从新分配足够空间插入元素( 增补元素个数, 新增的全部元数的默认值) !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针
	inline void Insert(unsigned int add_count, T em_all){
	   #ifdef GT_PMEM_WORNING
		if(!add_count){
			GN_Log("P %s Insert add_count = 0? !!!\n", zName[0]?zName:"");
			return;
		}
       #endif
		unsigned int  data_num = GT_P_ACTUAL(zData->num);
		unsigned int  ci, new_num = data_num + add_count;
		T * new_data = new T [ new_num ];
		for(ci = 0; ci<data_num; ++ci)new_data[ci] = ((T*)(zData->pmem))[ci];
        for(;       ci<new_num ; ++ci)new_data[ci] = em_all;
		GT_P_MEM_RESET;
        P_PrintUsRange();
	}
    //在内部从新分配足够空间插入另一个同类型安全指针里的所有元素( 另一个安全指针 )
	inline void Insert(const P &pm){
		if(!pm.zData){
		   #ifdef GT_PMEM_WORNING
			GN_Log("P %s Insert 安全指针pm为空的!\n", zName[0]?zName:"");
		   #endif
			return;
		}
		unsigned int data_num = GT_P_ACTUAL(zData->num), add_count = GT_P_ACTUAL(pm.zData->num);
		if(!add_count)return;
		unsigned int  ci, ci2, new_num = data_num + add_count;
		T * new_data = new T [new_num], *pdata1 = (T*)(zData->pmem), *pdata2 = (T*)(pm.zData->pmem);
		for(ci  = 0; ci<data_num; ++ci)new_data[ci] = pdata1[ci];
        for(ci2 = 0; ci<new_num ; ++ci, ++ci2)new_data[ci] = pdata2[ci2];
		GT_P_MEM_RESET;
        P_PrintUsRange();
	}
    //在内部从新分配足够空间插入另一个同类型安全指针里不同于当前指针里的所有元素( 另一个安全指针 ) 返回另一个安全指针里的数据对应放到当前指针里的第几个位置 该返回值的数据个数和另一安全指针的数据数对应
	inline P<unsigned int> InsertNEQ(const P &pm){
		if(!pm.zData){
		   #ifdef GT_PMEM_WORNING
			GN_Log("P %s InsertNEQ 安全指针pm为空的!\n", zName[0]?zName:"");
		   #endif
			return P<unsigned int>();
		}
		unsigned int ci, mi1, mi2, me1 = GT_P_ACTUAL(zData->num), me2 = GT_P_ACTUAL(pm.zData->num);
		unsigned int add_em_num = 0, new_num = me1;
		T *pdata1 = (T*)(zData->pmem), *pdata2 = (T*)(pm.zData->pmem);
		T * add_em = new T [me2];
		P<unsigned int> em_indexs(me2);
		for(mi2 = 0; mi2 < me2; ++mi2){
		    for(mi1 = 0; mi1 < me1; ++mi1){
				if(pdata2[mi2] == pdata1[mi1]){ //原元素已有另一个安全指针的这个元素
					em_indexs[mi2] = mi1;
					break;
				}
			}
			if(mi1 >= me1){
				add_em[add_em_num] = pdata2[mi2]; //原元素没有另一个安全指针的这个元素就加入它
                em_indexs[mi2] = new_num;
				add_em_num++;
				new_num++;
			}
		}
		if(new_num > me1){ //有不同的元素加入
			T * new_data = new T [new_num];
			for(ci = 0; ci < me1; ++ci)new_data[ci] = pdata1[ci];
			for(add_em_num = 0; ci < new_num ; ++ci, ++add_em_num)new_data[ci] = add_em[add_em_num];
			GT_P_MEM_RESET;
		}
		delete [] add_em;
        P_PrintUsRange();
		return em_indexs;
	}
	//在内部移除指定范围内的元素( 开始移除的元素索引号 , 共移除几个元素) !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针 如果指针指向的数据只有一个元素则忽略该处理
    inline void Remove(unsigned int id, unsigned int count){
		    #ifdef GT_PMEM_WORNING
        if(!count){
			GN_Log("P %s Remove count = 0? !!\n", zName[0]?zName:"");
			return;
		}
        #endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
		if(data_num < 2 || id >= data_num)return;  //判断处理可能用户乱填的参数范围
		if(id){
			if(  id +    count >= data_num)count = data_num - id;
		}else{
		    if(/*id + */ count >= data_num)count = data_num - 1;
		}
        unsigned int ci, ci2, new_num = data_num - count;
        T * new_data = new T [new_num];
        for(ci2 = ci = 0; ci < id; ++ci, ++ci2)new_data[ci2] = ((T*)(zData->pmem))[ci];
        for(ci = id + count; ci<data_num; ++ci, ++ci2)new_data[ci2] = ((T*)(zData->pmem))[ci];
        GT_P_MEM_RESET;
        P_PrintUsRange();
	}
	//在内部移除指定值的元素( 指定值 , 按顺序共移除几个指定值的元素)返回共移除了多少个元素 !!如果有其它安全指针和它共享同一片数据 这修改将会影响其它共享的安全指针 如果指针指向的数据只有一个元素则忽略该处理
    inline int RemoveV(T em, unsigned int count){
		#ifdef GT_PMEM_WORNING
        if(!count){
			GN_Log("P %s RemoveV count = 0? !!\n", zName[0]?zName:"");
			return 0;
		}
        #endif
        unsigned int data_num = GT_P_ACTUAL(zData->num);
		if(data_num < 2)return 0;
		T *pdata = (T*)(zData->pmem);
        unsigned int ci = 0, cn = 0;
		for(; ci < data_num && cn < count; ++ci){
            if( pdata[ci] == em)++cn;  //检查需要移除的个数
		}
        if(!cn)return 0;
        unsigned int ci2, new_num = data_num - cn;
        T * new_data = new T [ new_num ];
		for(ci = 0, ci2 = 0, cn = 0; ci < data_num; ++ci){
			if(cn < count && pdata[ci] == em){
				++cn;
			}else{
				new_data[ci2] = pdata[ci];
				++ci2;
			}
		}
		GT_P_MEM_RESET;
        P_PrintUsRange();
		return cn;
	}
	//逆向从普通指针找回它的安全指针( 普通指针 ) !!这需要一些处理时间 这普通指针必需是用安全方式创建的 被安全指针包含 否侧该指针会变为空的
	inline void Re(T *p){
		this->Delete();
		GN::sLIST<GT_sMEMORY>::iterator ir = GT_UsMemTable.memList.begin(), er = GT_UsMemTable.memList.end();
		for(; ir != er; ++ir){
			if(p == (*ir).pmem){
				zData = &(*ir);
				++(zData->us);
			    P_PrintUsRange();
				return;
		}	}
	}
	//比较是否指向相同的内存
	inline bool operator ==(const P & pm)const{ return this->zData == pm.zData; }

	//比较是否指向不同的内存
	inline bool operator !=(const P & pm)const{ return this->zData != pm.zData; }

	//获得这快内存的数据个数
	inline unsigned int Num(){ return zData ? GT_P_ACTUAL(zData->num) : 0; }

	//获得这快内存的大小字节
	inline unsigned int Size(){ return zData ? sizeof( T ) * (GT_P_ACTUAL(zData->num)) : 0; }

	//获得这快内存的共享者数
	inline unsigned int Us(){ return zData ? zData->us : 0; }
   #if defined _WIN32_ && defined __LP64__
    #define GT_PMEM_SIZE_T size_t
   #else
    #define GT_PMEM_SIZE_T uint64
   #endif
   #define GT_PMEM_INDEX_FUNC_DEF( RET ) \
    /*引用内存数据元数( 索引号 ) */\
	inline T &operator [](unsigned int   id){ RET }\
	inline T &operator [](int			 id){ RET }\
	inline T &operator [](unsigned long  id){ RET }\
	inline T &operator [](long			 id){ RET }\
	inline T &operator [](unsigned short id){ RET }\
	inline T &operator [](short			 id){ RET }\
	inline T &operator [](unsigned char  id){ RET }\
	inline T &operator [](char			 id){ RET }\
    inline T &operator [](int8			 id){ RET }\
	inline T &operator [](GT_PMEM_SIZE_T id){ RET }

   #ifdef GT_PMEM_WORNING
	//引用内存数据元数( 索引号 )
	inline T & operator [](unsigned int id){
		GT_P_MEM_NOINIT_TIPS1( "[]" ）
        unsigned int data_num = GT_P_ACTUAL(zData->num);
		if(id >= data_num){
			GN_Log("P %s 内存访问出轨!!\n", zName[0] ? zName : "");
			id = id % data_num; //防止索引出轨
		}
		return ((T*)(zData->pmem))[id];
	}
	GT_PMEM_INDEX_FUNC_DEF( return (*this)[(unsigned int)id]; )
   #else
	GT_PMEM_INDEX_FUNC_DEF( return ((T*)(zData->pmem))[id]; )
   #endif
	//从指针抽取数据
	inline T & operator * (){
	   #ifdef GT_PMEM_WORNING
	    #define GT_P_MEM_NOINIT_TIPS( F , PROC ) \
		if(!zData){\
		   GN_Log( "P %s ::%s 使用了未初始化的指针!\n" , zName[0] ? zName : ""， F );\
		   PROC \
		}
		#define GT_P_MEM_NOINIT_TIPS1( F ) GT_P_MEM_NOINIT_TIPS( F , this->New(1); )

		GT_P_MEM_NOINIT_TIPS1( "*()" )
       #endif
		return *((T*)zData->pmem);
	}
	//指向成员
	inline T * operator -> ()const{
	   #ifdef GT_PMEM_WORNING
		GT_P_MEM_NOINIT_TIPS( "->" ,  ）
	   #endif
		return (T*)(zData->pmem);
	}
	//指向成员
	inline T * operator-> (){
	   #ifdef GT_PMEM_WORNING
		GT_P_MEM_NOINIT_TIPS( "->" ,  ）
       #endif
		return (T*)(zData->pmem);
	}
	/*!以下这些把它的实际内存地址挖回出来 提供更好的性能 但不安全 最好在局部函数里用 尽量不要超出单个函数范围内!*/

	//转换为普通指针 这个一般在参数传递和赋值给普通指针时会自动调用 连 if( P ) 也会被调用的
	inline operator const T* () const{
		if(zData)return (T*)(zData->pmem);
		else return 0;
	}
	inline operator T* (){
	    if(zData)return (T*)(zData->pmem);
		else return 0;
	}
	//获得内存首地址
	inline T * Begin(){
	   #ifdef GT_PMEM_WORNING
		GT_P_MEM_NOINIT_TIPS1( "Begin" )
	   #endif
		return (T*)(zData->pmem);
	}
    //获得内存尾地址
	inline T * End(){
	   #ifdef GT_PMEM_WORNING
		GT_P_MEM_NOINIT_TIPS1( "End" )
	   #endif
		unsigned int data_num = GT_P_ACTUAL(zData->num);
		return ((T*)zData->pmem) + data_num;
	}
};
template<class T>
bool P<T>::copyMode = false;

/* ----------------------------------------------------------- sNODE ---------------------------------------------------------------------------------------------------------------------- */
template<class T>
bool GT_NodeCollect(T &node, void *arg){
	((GN::sLIST< T > *)arg)->push_back(node);
	return true;
}
//- 对象数据重构造 ------------------------------------
#ifdef _MSC_VER
   #define GT_TMP_OBJ_RE_CONSTRUCTOR( OBJ_TYPE ) \
	if(aName){ \
		this-> OBJ_TYPE ::~ OBJ_TYPE (); \
		this-> OBJ_TYPE ::  OBJ_TYPE (); \
	}
   #define GT_RE_CONSTRUCTOR( CTYPE ) \
	this-> CTYPE ::~ CTYPE (); \
	this-> CTYPE ::  CTYPE ();
#else
   #define GT_TMP_OBJ_RE_CONSTRUCTOR( OBJ_TYPE ) \
	if(aName){ \
		this-> OBJ_TYPE ::~ OBJ_TYPE (); \
		new(this) OBJ_TYPE();\
	}
   #define GT_RE_CONSTRUCTOR( CTYPE ) \
	this-> CTYPE ::~ CTYPE (); \
	new(this) CTYPE();
#endif //----------------------------------------------

namespace GN {
template<class T>
struct sNODE		//★安全节点 用于方便构成子父嵌套关系的树形连锁数据结构 为数据元素动态删减添加分类等带来高效方便的处理 一般用法是定义个新类继承它 如: class 新类 :public GN::sNODE<新类>
{public:
	GN::sLIST< P<T> >    zSub;         //接入到它的子节点
	P<T>                *zParentNode;  //记下该节点在它父节点的子节点链表中的数据地址 以方便解开连接用
	T                   *zParent;      //父节点指针
	P_                   zPthis;       //节点自身的安全指针
    P<char>              aName;        //名称

	sNODE():zParent(0),zParentNode(0){}

    virtual ~sNODE(){
		GT_LIST_iterator( P<T> ) in = zSub.begin(), en = zSub.end();
		for(; in != en; ++in){
			(*in)->zParent = 0;  //先解除它和它的子节点连接关系
            (*in)->zParentNode = 0;
		}
		zSub.clear();
		//zParent = 0; zParentNode = 0;
		//zPthis.Delete();
		//aName.Delete();
	}
	//初始化节点(名称, 它的安全指针号) !!如果它是由用户手动在代码上创建的必需调用一下该函数
	void InitToData(char *name,P_  p_this){
        GT_TMP_OBJ_RE_CONSTRUCTOR( sNODE )
		zPthis = p_this;
        aName(name, 0);
	}
	//解除该节点与父节点的连接 !!在对该节点的子节点历遍操作时请不要解除连接操作 否则会出轨
	void UnComment(){
	    if(!zParentNode)return;
		zParent->zSub.del(zParentNode);
	    zParentNode = 0;
        zParent = 0;
	}
	//接入其它节点(其它节点) !!在对该节点的子节点历遍操作时请不要连接操作 否则会出轨
	void Comment(P<T> node){
	    if(!node.Has())return;
	    node->UnComment(); //如这对象已经连接到其它节里就解除它
	    node->zParentNode = zSub.add( node );
        node->zParent = (T*)this;
	}
	//复制出一个新副本(复制号, 父节点指针地址)
	void zClone(unsigned short id, P<T> *parent){
	   #define GT_TMP_CLONE_NODE_NAME \
	    P<T> new_node = this->zCloneOut(); /*调用不同类的特定复制虚函数来创建复制出专有数据*/\
		char *pn = aName;\
		while(*pn){\
			if(*pn == '#')break;\
			++pn;\
		}\
		unsigned int len = pn - (char*)aName;\
		new_node->aName.New(len + 7);\
		pn = new_node->aName;\
		strncpy(pn, aName, len);\
		pn[len] = '#';\
		pn[len+6] = 0;\
		itoa(id, pn + len + 1, 10);

		GT_TMP_CLONE_NODE_NAME

		new_node->zParent = (T*)(*parent);
        new_node->zParentNode = (*parent)->zSub.add(new_node);

	   #define GT_TMP_SUB_NODE_CLONE \
		GT_LIST_iterator( P<T> )  it = this->zSub.begin(), et = this->zSub.end();\
		for(;it != et; ++it)(*it)->zClone( id, &new_node ); //复制出它的子节点

		GT_TMP_SUB_NODE_CLONE
	}
	virtual P<T> zCloneOut(){ //复制出内部专有数据
	    P< T > clone(1);
        clone->zPthis.zData = clone.zData;
		//...继承类必须重写这个虚函数 并在这补充复制新类的专有数据
		return clone;
	}
	//复制出该节点的一个新副本(复制ID号, 是否也复制它的所有子对象)返回新复制出来的节点 !!如果复制源有父节点 那么新复制出来的都挂在同一个父节点里 复制号:在原名称后面加上一个号码以示区分 该号码最好由 GN::sNUMBER 类生成
	P<T> Clone(unsigned short id, bool clone_sub=true){
		GT_TMP_CLONE_NODE_NAME

		if(this->zParent){
		    new_node->zParent = this->zParent;
            new_node->zParentNode = this->zParent->zSub.add(new_node);
		}else{
			new_node->zParent = 0;
		    new_node->zParentNode = 0;
		}
       	if(clone_sub){
			GT_TMP_SUB_NODE_CLONE
		}
        return new_node;
	}
	//获得该节点所在群组树的下一个检索节点(当节点已经是尽头时切换到的起始节点)
	P<T> GetNext(P<T> st){
		if((CVP(T)zPthis).Us() > 1){
			if(zSub.empty()){
				T *_node = (T*)this;
			    GT_GOTO_FindNextTreeNode:
				if(_node->zParent && _node->zParentNode){
					GT_LIST_iterator( P<T> ) ito = _node->zParent->zSub.to_iterator(_node->zParentNode);
					++ito;
					if(ito == _node->zParent->zSub.end()){
						_node = _node->zParent;
						goto GT_GOTO_FindNextTreeNode;
					}
					return (*ito);
				}else return st;
			}else return zSub.front();
		}else return st;
	}
    //获得该节点所在群组树的跟节点
	P<T> GetRoot(){
		T *root = (T*)this;
		while( root->zParent )root = root->zParent;
		return CVP( T )root->zPthis;
	}
	//从该节点往前查找到首个符合条件的父节点( 包含的指定名字段, 比较条件函数(当前检索到的父节点) ) 返回首个满足条件的父节点 如果没有满足的返回空指针  第一第二参数是二选一的 另外一个应为0  如果两个参数都为0 即返回上一层的父节点
	P<T> GetParent(const char *name, bool (*compare)(T &first) = 0){
	   #define GT_TMP_NODE_GET_PARENT( CMP_PROC ) \
		if( !name && !compare ){\
		    if(zParent)return CVP( T )zParent->zPthis;\
			else return P<T>();\
		}else{\
            T *parent = zParent; \
			if( name ){\
				size_t len = strlen( name );\
				while( parent ){\
				    if(strncmp(parent->aName, name, len))parent = parent->zParent;\
					else return CVP( T )parent->zPthis;\
				}\
				return P<T>();\
			}else{\
				while( parent ){\
				    if( CMP_PROC )return CVP( T )parent->zPthis;\
					else parent = parent->zParent;\
				}\
				return P<T>();\
			}\
		}

		GT_TMP_NODE_GET_PARENT( compare( *( parent ) ) )
	}
	//从该节点往前查找到首个符合条件的父节点( 包含的指定名字段, 比较条件函数(当前检索到的父节点, 传回给你的参数), 传给执行函数用的参数) 返回首个满足条件的父节点 如果没有满足的返回空指针  第一第二参数是二选一的 另外一个应为0  如果两个参数都为0 即返回上一层的父节点
    P<T> GetParent(const char *name, bool (*compare)(T &first, void *us_arg), void *_us_arg){
        GT_TMP_NODE_GET_PARENT( compare( *( parent ) , _us_arg) )
	}
	//从该节点往前查找是否含有指定父节点
	bool HasParent(T *ptr){
		T *root = (T*)this;
		while( root->zParent ){
			root = root->zParent;
			if(root == ptr)return true;
		}
		return false;
	}
	//从该节点群组树里查找到指定名的节点( 节点名 ) 返回查找到的节点 查找不到返回0
	T* zSearch(const char *name){
		if(!strcmp(this->aName,name))return (T*)this;

		GT_LIST_iterator( P<T> ) in = zSub.begin(), en = zSub.end();
		for(; in != en; ++in){
			T *ret = (*in)->zSearch( name );
			if(ret)return ret;
		}
		return 0;
	}
	//从该节点群组树里查找到符合条件的节点( 包含的指定名字段 , 比较条件函数(当前检索到的节点) )返回满足条件的节点 如果没有找到返回0
	T* zSearch_if(const char *name, bool (*compare)(T &first) ){
	   #define GT_TMP_NODE__SEARCH_IF( ADD_PROC , ADD_PROC2 ) \
		if(name){\
			if(!strncmp(this->aName, name, strlen(name)))return (T*)this;\
		}else if( ADD_PROC )return (T*)this;\
		\
		GT_LIST_iterator( P<T> ) in =  zSub.begin(), en = zSub.end();\
		for(; in != en; ++in){\
			T *ret = (*in)-> ADD_PROC2 ;\
			if(ret)return ret;\
		}\
		return 0;

        GT_TMP_NODE__SEARCH_IF( compare( *((T*)this) ) , zSearch_if( name ,compare ) )
	}
	//从该节点群组树里查找到符合条件的节点( 包含的指定名字段 , 比较条件函数(当前检索到的节点, 传回给你的参数), 传给执行函数用的参数 )返回满足条件的节点 如果没有找到返回0
	T* zSearch_if(const char *name, bool (*compare)(T &first, void *us_arg), void *_us_arg){
        GT_TMP_NODE__SEARCH_IF( compare( *((T*)this), _us_arg ) , zSearch_if( name , compare, _us_arg ) )
	}
	//从该节点群组树里和调用节点自身查找指定名的节点( 节点名 , 是否查找整棵树) 返回查找到的节点 查找不到返回空指针
    P<T> Search(const char *name, bool full_tree=false){
		T *search_node = (T*)this, *ret = 0;
		if( full_tree )search_node = GetRoot().Begin();
		ret = search_node->zSearch( name );
        if(ret)return CVP( T )ret->zPthis;
		else return P<T>();
	}
	//从该节点群组树里和调用节点自身查找到符合条件的首个节点( 包含的指定名字段, 比较条件函数(当前检索到的节点), 是否要查找整棵树) 返回满足条件的首个节点 如果没有满足的返回空指针 第一第二参数是分别是二选一的 另外一个应为0
    P<T> Search_if(const char *name, bool (*compare)(T &first), bool full_tree=false){
	   #define GT_TMP_NODE_SEARCH_IF( ADDPROC ) \
		T *search_node = (T*)this, *ret = 0; \
		if( full_tree )search_node = GetRoot().Begin();\
        ret = search_node-> ADDPROC ;\
        if(ret)return CVP( T )ret->zPthis;\
		else return P<T>();

		GT_TMP_NODE_SEARCH_IF( zSearch_if( name , compare ) )
	}
	//从该节点群组树里和调用节点自身查找到符合条件的首个节点( 包含的指定名字段, 比较条件函数(当前检索到的节点, 传回给你的参数), 传给执行函数用的参数, 是否要查找整棵树) 返回满足条件的首个节点 如果没有满足的返回空指针 第一第二参数是分别是二选一的 另外一个应为0
	P<T> Search_if(const char *name, bool (*compare)(T &first, void *us_arg), void *_us_arg, bool full_tree=false){
        GT_TMP_NODE_SEARCH_IF( zSearch_if( name , compare , _us_arg) )
	}
	//使该节点群组树里包含指定名的多个节点和调用节点自身都执行指定函数( 包含的指定名字段:0为全部对象, 返回是否要继续查找 执行函数(当前检索到的元素), 是否要查找整棵树) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
	bool Search_exec(const char *name, bool (*proc)(P<T> &first), bool full_tree=false){
       #define GT_TMP_NODE_SEARCH_EXEC( ADD_PROC , ADD_PROC2 ) \
		T *search_node = (T*)this;\
		if(full_tree)search_node = GetRoot().Begin();\
		bool proc_ing;\
		if( name ){\
			if(strncmp(search_node->aName, name, strlen(name)))proc_ing = true;\
			else proc_ing = ADD_PROC ;\
		}else proc_ing = ADD_PROC ;\
		\
		if(proc_ing){\
		    GT_LIST_iterator( P<T> ) in = search_node->zSub.begin(), en = search_node->zSub.end();\
			for(; in != en; ++in){ \
				if( (*in)-> ADD_PROC2 )return true;\
			} \
			  return false;\
		}else return true;

        GT_TMP_NODE_SEARCH_EXEC( proc ( CVP( T )search_node->zPthis ) , Search_exec( name , proc , false) )
	}
	//使该节点群组树里包含指定名的多个节点和调用节点自身都执行指定函数( 包含的指定名字段:0为全部对象, 返回是否要继续查找 执行函数(当前检索到的元素, 传回给你的参数), 传给执行函数用的参数, 是否要查找整棵树) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
	bool Search_exec(const char *name, bool (*proc)(P<T> &first, void *us_arg), void *_us_arg, bool full_tree=false){
        GT_TMP_NODE_SEARCH_EXEC( proc ( CVP( T )search_node->zPthis , _us_arg) , Search_exec( name , proc , _us_arg, false) )
	}
    //使该节点群组表层子对象包含指定名的多个节点都执行指定函数( 包含的指定名字段:0为表层的所有子对象, 返回是否要继续查找 执行函数(当前检索到的元素) ) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
	bool SearchSurface_exec(const char *name, bool (*proc)(P<T> &first) ){
       #define GT_TMP_NODE_SEARCH_SURFACE_EXEC( ADD_PROC , ADD_PROC2 ) \
		GT_LIST_iterator( P<T> ) in = this->zSub.begin(), en = this->zSub.end();\
		if( name ){\
			T *search_node;\
			for(; in != en; ++in){\
				search_node = (*in);\
				if(!strncmp(search_node->aName,name, strlen(name))){\
					if(! ADD_PROC )return true;\
			}	}\
		}else{\
		    for(; in != en; ++in){\
                if(! ADD_PROC2 )return true;\
		}	}\
		return false;

		GT_TMP_NODE_SEARCH_SURFACE_EXEC( proc( CVP( T )search_node->zPthis ) , proc( CVP( T )( (*in)->zPthis ) ) )
	}
	//使该节点群组表层子对象包含指定名的多个节点都执行指定函数( 包含的指定名字段:0为表层的所有子对象, 返回是否要继续查找 执行函数(当前检索到的元素, 传回给你的参数), 传给执行函数用的参数) ) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
	bool SearchSurface_exec(const char *name, bool (*proc)(P<T> &first, void *us_arg), void *_us_arg){
		GT_TMP_NODE_SEARCH_SURFACE_EXEC( proc( CVP( T )search_node->zPthis , _us_arg) , proc( CVP( T )((*in)->zPthis) , _us_arg) )
	}
	//收集该节点和它的所有子节点(返回节点列表)
	void Collect(sLIST< P<T> > *_subs_out){
		this->Search_exec(0, GT_NodeCollect< P<T> >, _subs_out);
	}
	//获得该节点的子节点数(是否检索所有子节点里的子节点)
	int SubNum(bool all_sub=true){
		if(all_sub){
			int num = 0;
			this->Search_exec(0, GT_Add1Func<P<T> &, int>, &num);
			return num - 1;
		}else return zSub.size();
	}
	//确保子节点的父归属正确
	void SubParentEnsure(){
		GT_LIST_iterator( P<T> ) in = zSub.begin(), en = zSub.end();
		for(; in != en; ++in){
			(*in)->zParentNode = &(*in);
			(*in)->zParent = (T*)this;
		}
	}
	//打印该节点群组树里包含的所有子节点名称( 是否查找整棵树 )
	void PrintAllNodeName(bool full_tree=false){
	    T *search_node = (T*)this;
		if(full_tree)search_node = GetRoot().Begin();
        GN_Log( "%s\n",(char*)search_node->aName);

		GT_LIST_iterator( P<T> ) in = search_node->zSub.begin(), en = search_node->zSub.end();
		for(; in != en; ++in)(*in)->PrintAllNodeName( false );
	}
};
struct sNODE_ :public sNODE<sNODE_>{}; //★安全基础节点

template<class T>
//返回指定对象是否仅有一个引用
inline bool UsEq1(T &v){ return v.Us() == 1; }

template<class T>
//返回指定对象是否少于或等于一个引用
inline bool UsLE1(T &v){ return v.Us() <= 1; }

template<class T>
struct  sNUMBER  //编号分配器 帮你分配唯一的编号 由小到大分配
{
	T                 zMaxID;    //当前的可用ID
	sLIST<T>		  zFreeIDs;  //自由的可用ID

	sNUMBER():zMaxID(0){ }
   ~sNUMBER(){ Clear(); }
   void Clear(){ if(zFreeIDs.size())zFreeIDs.clear(); zMaxID = 0; } //清除记录

	//获取唯一的号码
	T GetID(){
		T ret;
		if(zFreeIDs.empty()){
			ret = zMaxID;
            zMaxID++;
		}else{
			ret = zFreeIDs.front();
			zFreeIDs.pop_front();
		}
		return ret;
	}
	//试探下一个号码 用来预先得知下个会分配什么号 但不实际分配
	T ProbeID(){
		T ret;
		if(zFreeIDs.empty())ret = zMaxID;
		else ret = zFreeIDs.front();
		return ret;
	}
	//重设指定号码可用于下次再分配(号码) !一般在拥有指定ID的对象销毁时调用它以使这个ID可以重新分配 以免编号不断增长引起出轨
	void ReSetIDToData(T id){
	    if(id < zMaxID)zFreeIDs.push_back( id );
	}
	//设置分配的开始基数(基数)
	void SetStartNumber(T baseNum){ zMaxID = baseNum; }
};
}; //end namespace GN
#endif //_GN_ARRAY_H_
