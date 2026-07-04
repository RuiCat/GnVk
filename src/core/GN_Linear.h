#ifndef _GN_LINEAR_H_
#define _GN_LINEAR_H_

/************************************************************ 快速插值 ***************************************************************/
template<class T>
//混合插值( 目标值 , 混合值 , 混合浓度比率 )
inline T GN_Mix(T dst, T src, float a){
    return (dst * (1.0f - a) + src * a);
}
//线性插值        ( 开始值, 结束值, 现在时间, 结束总时间 )
#define GN_Linear(start, end, nowtime, endtime) (((end)-(start))*(float)(nowtime)/(float)(endtime) + (start))
//减速缓动线性插值( 开始值, 结束值, 现在时间, 结束总时间 )
#define GN_SLinearStart(start, end, nowtime, endtime)  (((end)-(start))*(float)(nowtime)/(float)(endtime)*(2.0f-(float)(nowtime)/(float)(endtime)) + (start))
//加速缓动线性插值( 开始值, 结束值, 现在时间, 结束总时间 )
#define GN_SLinearEnd(start, end, nowtime, endtime)    (((end)-(start))*(float)(nowtime)/(float)(endtime)*((float)(nowtime)/(float)(endtime)) + (start))
//加减速缓动线性插值( 开始值, 结束值, 现在时间, 结束总时间 ,结果转换类型, 结果输出)
#define GN_SLinearCentre(start, end, nowtime, endtime, class_type, v_out) {\
    float tm_t=(float)(nowtime), end_t=(float)(endtime),SE=(float)(end)-(float)(start), tm_tt=0.0f;\
	float en2 = end_t*0.5f;\
	if(tm_t>en2){ tm_tt = tm_t-en2; tm_t = en2; }\
	(v_out) = (class_type)(SE*tm_t/end_t *tm_t/end_t*2.0f  +  SE*tm_tt/end_t*(2.0f-tm_tt/end_t*2.0f) + (float)(start));\
}
//整型类型缓动减速插值( 数据类型 , 要插值变动的变量 , 变动目标值 , 插值变动倍速:一般用0.1f-0.05f值越大越快变成目标值 , 插值完成的附加处理代码 ) !!必须每帧调用 这个不会乘上时间比率, 想使插值不会受卡机影响将sheep参数 乘上 GN::TimeRatio 即可
#define GN_ValueChange( class_type , value , value_tg , sheep , end_proc ){ \
	class_type v_ = ( class_type )((float)( (value_tg) - (value) ) * (sheep));\
	if( v_ ) (value) += v_;\
	else{\
        if( (value) != (value_tg) )(value) += (value_tg) > (value) ? 1 : -1;\
		else { end_proc ; }\
    }\
}
/* ------------------------------------------------------- 变量值插值更变 -------------------------------------------------------------*/
struct GN_API GT_sVALUE_CHANGE_BASE{
	static GN::sLIST<GT_sVALUE_CHANGE_BASE*> valueProcList; //变量插值处理列表 用来记录所有的变量插值任务

	virtual  bool ValueChange(){ return true; }    //变量插值更变  返回插值是否完成
};
struct GN_API GT_sVALUE_CHANGE_CR  :public GT_sVALUE_CHANGE_BASE    //其它类型插值结构
{
	void          (* endPorc)(void*);						   //记录插值完成后的处理函数指针
	void           * usParame;								   //回调传回给用户的参数
    unsigned int   * pValue;								   //记录需要插值的变量指针
    unsigned int     tgValue;								   //插值目标值
	float            tgValueR,tgValueG,tgValueB,tgValueA;      //插值目标值
	float            lsValueR,lsValueG,lsValueB,lsValueA;      //记录上次值
	float            linearSheep;							   //插值变化倍速
	float            crossRange;							   //误差范围内视为完成插值

	GT_sVALUE_CHANGE_CR(){}
	GT_sVALUE_CHANGE_CR(unsigned int  *change_value, unsigned int  target_value, float cross_range, float linear_sheep, void (* end_porc)(void*), void *us_parame);

    virtual bool ValueChange();    //变量插值更变  返回插值是否完成
};
template<class T>
struct  GT_sVALUE_CHANGE  :public GT_sVALUE_CHANGE_BASE    //其它类型插值结构
{
	void      (* endPorc)(void*);			 //记录插值完成后的处理函数指针
	void       * usParame;					 //回调传回给用户的参数
    T          * pValue;					 //记录需要插值的变量指针
	float        tgValue;					 //插值目标值
	float        lsValue;					 //记录上次值
	float        linearSheep;				 //插值变化倍速
	float        crossRange;				 //误差范围内视为完成插值

	GT_sVALUE_CHANGE(){}
	GT_sVALUE_CHANGE(T *change_value, T target_value, float cross_range, float linear_sheep, void (* end_porc)(void*), void *us_parame)
		:endPorc(end_porc),usParame(us_parame),pValue(change_value),tgValue((float)target_value),lsValue((float)(*change_value)),crossRange(cross_range),linearSheep(linear_sheep)
	{
	    if(!endPorc && usParame)memset(usParame,1,1);
	}
    virtual bool ValueChange(){    //变量插值更变  返回插值是否完成
		float lnrsp = linearSheep * GN::TimeRatioL;
		if(lnrsp > 1.0f)lnrsp = 1.0f;

		lsValue += (tgValue - lsValue) * lnrsp;
        *pValue = (T)lsValue;

        float _val = tgValue - lsValue;
		if((_val < 0.0f ? - _val : _val) < crossRange){
            *pValue = ( T )tgValue;
		    if(endPorc)endPorc(usParame);
			else if(usParame)memset(usParame,0,1);
			return true;
		}else return false;
	}
};
//更新所有要插值的变量内容 这必需要放在帧循环里挂起不段调用 有加入GN_Input摸块 系统会帮你自动调用该函数 用户没必要手动调用
GN_API void GN_ValueChangeUpdateAll();
//清除全部正在处理的变量插值项 应用程序结束前也最好调用一下这个  有加入GN_Input摸块 系统会帮你自动调用该函数 用户没必要手动调用
GN_API void GN_ValueChangeClearAll();

namespace GN {
//★颜色变量更变插值(要修改的变量指针, 变量想要修改成的目标值, 完成插值时的处理函数, 完成插值回调时的用户参数, 两值之间误差范围内视为完成插值, 插值变化倍速:一般用0.1f-0.05f值越大越快变成目标值) !注意第一个参数的变量不能为局部临时变量 而且要保证在为变量插值完成后该变量都为有效的 如果插值结束回调为0 那时用户参数可以传入一个bool类型或单字节的变量指针 那么在插值结束后引擎会自动设置该变量为0以告诉用户插值完成
inline void ValueChangeColor(unsigned int  *change_value, unsigned int  target_value, void (* end_porc)(void*)= 0, void *us_parame=0, float cross_range=0.5f, float linear_sheep=0.08f){
	GT_sVALUE_CHANGE_BASE::valueProcList.push_back( new GT_sVALUE_CHANGE_CR(change_value,target_value,cross_range,linear_sheep,end_porc,us_parame) );
}
//★颜色变量更变插值(要修改的变量指针, 变量想要修改成的目标值, 完成插值时的处理函数, 完成插值回调时的用户参数, 两值之间误差范围内视为完成插值, 插值变化倍速:一般用0.1f-0.05f值越大越快变成目标值) !注意第一个参数的变量不能为局部临时变量 而且要保证在为变量插值完成后该变量都为有效的 如果插值结束回调为0 那时用户参数可以传入一个bool类型或单字节的变量指针 那么在插值结束后引擎会自动设置该变量为0以告诉用户插值完成
inline void ValueChangeColor(unsigned long *change_value, unsigned int  target_value, void (* end_porc)(void*)= 0, void *us_parame=0, float cross_range=0.5f, float linear_sheep=0.08f){
	GT_sVALUE_CHANGE_BASE::valueProcList.push_back( new GT_sVALUE_CHANGE_CR((unsigned int*)change_value,target_value,cross_range,linear_sheep,end_porc,us_parame) );
}
//!!这只能支持C++标准默认类型而非结构体类型
template<class T>
//★    变量更变插值(要修改的变量指针, 变量想要修改成的目标值, 完成插值时的处理函数, 完成插值回调时的用户参数, 两值之间误差范围内视为完成插值, 插值变化倍速:一般用0.1f-0.05f值越大越快变成目标值) !注意第一个参数的变量不能为局部临时变量 而且要保证在为变量插值完成后该变量都为有效的 如果插值结束回调为0 那时用户参数可以传入一个bool类型或单字节的变量指针 那么在插值结束后引擎会自动设置该变量为0以告诉用户插值完成
inline void ValueChange(  T *change_value,     T target_value,      void (* end_porc)(void*)= 0, void *us_parame=0, float cross_range=0.5f, float linear_sheep=0.08f){
	GT_sVALUE_CHANGE_BASE::valueProcList.push_back( new GT_sVALUE_CHANGE< T >(change_value,target_value,cross_range,linear_sheep,end_porc,us_parame) );
}
};
namespace GNg{

enum LINEAR_TYPE{  //带有缓动的插值方式标志
	L_NONE    = 0,   //没有缓动
	L_CENTRE  = 1,   //最大居中 加减速
	L_START   = 2,   //最大开始 减速
	L_END     = 4,   //最大结尾 加速
	L_DOUBLE  = 8    //双倍来回运动
};
//返回线性颜色插值并一起调和后的新颜色(开始颜色值, 目标颜色值, 现在进度时间, 整个变化过程的所需时间, 目标调和颜色)返回新颜色
GN_API unsigned int  LinearColorMultiplyRatio(unsigned int  start, unsigned int  end, unsigned int  tmtime, unsigned int  endtime, unsigned int  ratio_color);

//返回线性颜色插值并一起调至特定不透明度时的新颜色(开始颜色值, 目标颜色值, 现在进度时间, 整个变化过程的所需时间, 不透明度0.0f-1.0f)返回新颜色
GN_API unsigned int  LinearColorMultiplyAlphaRatio(unsigned int  start, unsigned int  end, unsigned int  tmtime, unsigned int  endtime, float ratio);

//线性颜色插值(					    开始颜色值,  目标颜色值,   现在进度时间,  整个变化过程的所需时间)返回颜色
GN_API unsigned int  LinearColor(unsigned int  start, unsigned int  end, unsigned int  tmtime, unsigned int  endtime);

//线性插值(                 开始值,       目标值,     现在进度时间,    整个变化过程的所需时间)返回插值后的值
inline         float Linear(float start, float end, unsigned int  tmtime, unsigned int  endtime){ return (end-start) * ((float)tmtime/(float)endtime) + start; }
//线性插值(                 开始值,       目标值,     现在进度时间,    整个变化过程的所需时间)返回插值后的值
inline unsigned int  Linear(unsigned int  start, unsigned int  end, unsigned int  tmtime, unsigned int  endtime){ return ((end-start) * tmtime/endtime + start); }
//线性插值(                 开始值,       目标值,     现在进度时间,    整个变化过程的所需时间)返回插值后的值
inline           int Linear(int   start, int   end, unsigned int  tmtime, unsigned int  endtime){ return ((end-start) * (int)tmtime/(int)endtime + start); }
//线性插值(                 开始值,       目标值,     现在进度时间,    整个变化过程的所需时间)返回插值后的值
inline		   sVEC2 Linear(sVEC2 start, sVEC2 end, unsigned int  tmtime, unsigned int  endtime){ return (end-start) * ((float)tmtime/(float)endtime) + start; }

//带有缓动加减速的线性插值(               开始值,          目标值,          现在进度时间,		 整个变化过程的所需时间,   缓动值0.0f-1.0f,       缓动模式 )返回插值后的值
GN_API         float SLinear(             float start,         float end, unsigned int  tmtime, unsigned int  endtime, float value=0.0f, unsigned short mode=L_CENTRE);
//带有缓动加减速的线性插值(               开始值,          目标值,          现在进度时间,		 整个变化过程的所需时间,   缓动值0.0f-1.0f,       缓动模式 )返回插值后的值
GN_API unsigned int  SLinearColor(unsigned int  start, unsigned int  end, unsigned int  tmtime, unsigned int  endtime, float value=0.0f, unsigned short mode=L_CENTRE);
//带有缓动加减速的线性插值(               开始值,          目标值,          现在进度时间,		 整个变化过程的所需时间,   缓动值0.0f-1.0f,       缓动模式 )返回插值后的值
GN_API		   sVEC2 SLinear(			  sVEC2 start,		   sVEC2 end, unsigned int  tmtime, unsigned int  endtime, float value=0.0f, unsigned short mode=L_CENTRE);

//从直线列表评估生成平滑曲线列表的缓动表示数据(直线点数据, 缓动表示数据输出, 直线点个数, 是否断开头尾)
GN_API void GenSLinearTypeData(const float *line_list, unsigned char *sl_type_list_out, int count, bool off_end=true);

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------*/
template<class T>
struct sBEZIER      //★贝兹曲线
{
    T        p0, p1;     //贝兹曲线的两个端点
	T        v0, v1;     //贝兹曲线的两个方向点

	inline sBEZIER(){}
	inline sBEZIER  (const T *_p0, const T *_p1, const T *_v0, const T *_v1):p0(*_p0),p1(*_p1),v0(*_v0),v1(*_v1){}
	inline void Init(const T *_p0, const T *_p1, const T *_v0, const T *_v1){ p0 = *_p0;  p1 = *_p1; v0 = *_v0;  v1 = *_v1; }

	//获取在曲线上的一点(返回点, 位置在曲线的范围比率0.0f - 1.0f)
	void GetPoint(T *pout, float ratio){
		float r1 = 1.0f - ratio;
		T a = p0 * pow(ratio, 3);
        T b = v0 *  (3 * pow(ratio, 2) * r1);
		T c = v1 * ((3 * ratio) * pow(r1, 2));
        T d = p1 * pow(r1, 3);
		*pout = a + b + c + d;
	}
};
//获得波形线在指定长度位置上的高度(特定长度位置, 每个波浪的间距) 返回高度 该高度是一个比率值 范围在 0.0-1.0f
inline float GetWaveLineHeight(unsigned int _l, unsigned int wc){
    unsigned int _wc = wc >> 1;
    unsigned int rn = _l / _wc;
	unsigned int l_ = _l - (rn * _wc);
	return (float)((rn & 1)?_wc - l_ : l_) / (float)_wc;
}
//获得指定时间里淡入淡出渐变的不透明度(指定时间, 淡入淡出整个过程时间, 渐变段的时间)返回不透明度
inline uint8 GetInOutLinearAlpha(unsigned int tmtime, unsigned int endtime, unsigned int segtime){
	unsigned int a = 255;
	if(tmtime < segtime)a = (255 * tmtime) / segtime;
	else{
		unsigned int time2 = endtime - segtime;
		if(tmtime > time2)a = 255 - (tmtime - time2) * 255 / segtime;
	}
	return a;
}
}; //end namespace GNg
#endif //_GN_LINEAR_H_
