#ifndef _GN_UILISTGUI_H_
#define _GN_UILISTGUI_H_
namespace GNu {
enum eSUBUI_FUNC { //子区块UI执行功能名
    eSUBUI_LOAD = 1, //加载数值
    eSUBUI_SAVE = 2, //保存数值
    eSUBUI_BEOPT = 3 //待操作
};
enum eLAYOUT_VALUE_TYPE { //布局值的类型
    eLAYOUT_VALUE_FLOAT = 5, //4字节浮点值
    eLAYOUT_VALUE_MIN_STR = 0, //小于等于256长度的小字符串 对应目标类型  char*
    eLAYOUT_VALUE_MIN_PSTR = -1, //小于等于256长度的小字符串 对应目标类型 P<char>
    eLAYOUT_VALUE_IMG0000 = -2, //要求图片路径简单接上"00000000_"序号后续名来访问的标记 对应目标类型  sIMAGE*		!需确保UI组件该指针指向数据不共享的
    eLAYOUT_VALUE_PIMG0000 = -3, //要求图片路径简单接上"00000000_"序号后续名来访问的标记 对应目标类型 P<sIMAGE>
    eLAYOUT_VALUE_FRAME_IMG0000 = -4, //要求图片路径简单接上"00000000_"序号后续名来访问的标记 对应目标类型 cFRAME* 里的 zPlay_obj !需确保UI组件该指针指向数据不共享的
    eLAYOUT_VALUE_PFFRAME_IMG0000 = -5, //要求图片路径简单接上"00000000_"序号后续名来访问的标记 对应目标类型 P<cFRAME_LAUNCHER_FONT> 里的zFontFrame->zPlay_obj

    eLAYOUT_VALUE_STR_POS = -6, //大字符串在文件里的位置偏移 4字节整型值
    eLAYOUT_VALUE_IMG_POS = -7, //图片路径在文件里的位置偏移 4字节整型值
    eLAYOUT_VALUE_TEXT_POS = -8, //文本路径在文件里的位置偏移 4字节整型值

    eLAYOUT_TARGET_STR_SIZE_BIT = 0x7f00 //目标char*字符串原大小掩码位 如&掩码再>>8位后有值(该值为字符串原大小含结束符) 就会强制数值转成字符串并接在目标字符串前面
};
class GN_API cVALUE_EXTRACT_FILE : public iUI_OBJECT //UI数值提取文件
{
  public:
    P<GN::sVALUE_LAYOUT2> extract_descs; //数值提取描述组 对应一组UI里的各个UI组件例如学生{刘同志,高二年级,180cm}这些数值所在UI组件里的内存位置偏移和文件位置 数值大小 加载方式等 每个描述对应一个UI组件 UI组也占一个 !如无需提取数值显示的UI组件 应把成员stride设为-1以示区分 成员size也可以是eLAYOUT_VALUE_TYPE里的值之一 !合理指定这些信息用于告知系统怎么从文件里加载数值给UI来显示
    P<char> filename; //文件路径或文件内存缓冲 可以通过判断 fp->type == GNf::eFILE_STD 来得知是否为路径
    GNf::sFILE *fp; //数值提取存储文件
    short data_pos; //文件数据区起始位置偏移

    cVALUE_EXTRACT_FILE() : fp(0) { aType = eUI_VALUE_EXTRACT; }
    virtual ~cVALUE_EXTRACT_FILE() {
        if(fp) {
            GNf::Close(fp);
            fp = 0;
        }
    }

    //当cSCROLL_LIST_GUI::SetSubUiCallBack函数被执行时会同时回调该方法来进行一些初始化统计 参数parent传来的也是cSCROLL_LIST_GUI类型的对象
    virtual void StatisticSize(cUI_GROUP *parent); //{ ((cSCROLL_LIST_GUI*)parent)->aSubUiCallBack = SubUiUpdateSyncProc; }

    //把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
    virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out = 0);

    //从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
    virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
//滚动列表子区块UI显示数值同步处理(执行功能名, 当前处理的子UI所属区块ID, 子UI, 给滚动列表UI数值提取的文件)  !该函数是 cSCROLL_LIST_GUI 类的默认回调处理
GN_API void SubUiUpdateSyncProc(eSUBUI_FUNC func, unsigned int zid, P<iUI_OBJECT> &sub_ui, P<iUI_OBJECT> &extract_file);

class GN_API cSCROLL_LIST_GUI : public cSCROLL_LIST //★滚动嵌套UI列表
{
    friend void ::GT_ScrollListGuiDrawCall(int, int, int, int, unsigned int, unsigned int, unsigned int, bool, void *);
    virtual void zOnReArranged(unsigned short sp, unsigned short sp_last, bool ret_am); //当滚动筐被重排列后的处理(当前排列起始区域号, 上次的排列起始区域号, 排列是否一起重设了所有区域的弹出效果)
#ifdef _MSC_VER
    cSCROLL_LIST::InitStyleToData;
#endif
#ifndef _APPLE_
    cSCROLL_LIST::SetZoningDrawCall; //使基类该函数不能用
#endif
  protected:
    uint8 *zDataSave(uint8 *wpos, cSHARE_DATA *share_objs); //保存该对象的2进制数据(当前写入位置, 收集共享对象集)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs); //读取该对象的2进制数据(当前读取位置, 引用共享对象集)返回最后的读完位置

    void zSpecClone(); //分离克隆副本不能共享的属性
    void zInitSubUis(P<iUI_OBJECT> sub_ui); //初始化列表子区块UI(嵌套到列表里的子UI模版)

    unsigned short zLastStartZID; //最近显示的起始区块ID
    unsigned short zPreLoadNum; //预加载缓冲区块个数
    P<P<iUI_OBJECT>> zUiGroups; //各个UI区块显示用的UI组
  public:
    unsigned int aCurDrawZID; //当前正在绘制的区块ID 一旦绘制完就会被重置为0xffffffff  这个通常给它自己的回调用的 例如在回调 UiActionCall 里可以通过判断 if(list_gui->aCurDrawZID != 0xffffffff && GNu::UiObjectSearch(list_gui->GetSubUi(list_gui->aCurDrawZID), uobj) == 您要检测触发动作的子UI所在顺序ID)执行您的消息处理
    P<iUI_OBJECT> aSubUiArg; //保存给子区块UI的用户参数

    //子区块UI处理回调(执行功能名, 当前处理的子UI所属区块ID, 子UI, 回传给子区块UI处理的用户参数)
    void (*aSubUiCallBack)(eSUBUI_FUNC func, unsigned int zid, P<iUI_OBJECT> &sub_ui, P<iUI_OBJECT> &args);

    cSCROLL_LIST_GUI();

    //初始化设置显示式样 !!必需
    bool InitStyleToData(
        unsigned short pre_loan_num = 0, //预加载缓冲区块个数 !指定这个可以提前为将要显示的区块里的图片或大文件提前加载 避免滚动时卡顿 但会占用更多内存空间
        short _x = NearLx(0), //滚动列表左上坐标X
        short _y = NearTy(0), //滚动列表左上坐标Y
        short _w = GNg::ViewCur->orthoW, //滚动列表宽度
        short _h = GNg::ViewCur->orthoH, //滚动列表高度
        bool default_am = true, //滚动列表默认动画效果 在没调用SetEffect指定生成效果筐或第一次调用该函数时才生这个默认效果 默认效果的区域底色是幻彩变化的
        short dw = 0, //嵌套UI区块宽度 默认0:跟筐的宽度一样
        short dh = 0, //嵌套UI区块高度 默认0:跟筐的高度度一半大小
        unsigned int zoning_num = 1, //嵌套UI区块个数
        unsigned int focus_color = GT_DEF_FOCUS_COLOR, //焦点颜色
        unsigned short focus_blend = GNg::eBLEND_BRIGHT //焦点混合模式
    ); //返回是否成功

    //设置子区块UI和回调(嵌套到列表里的子UI模版, 传给回调用的用户参数, 子区块UI处理回调(执行功能名, 当前处理的子UI所属区块ID, 子UI, 回传给子区块UI处理的用户参数)) !必需先调用InitStyleToData后再调用这个
    void SetSubUiCallBack(P<iUI_OBJECT> sub_ui, P<iUI_OBJECT> us_arg = P_(), void (*callback)(eSUBUI_FUNC func, unsigned int zid, P<iUI_OBJECT> &sub_ui, P<iUI_OBJECT> &args) = SubUiUpdateSyncProc);

    void SubUiExecCallBack(eSUBUI_FUNC func); //使子区块UI执行回调(执行功能名)
    void SetZoningCount(unsigned int zoning_num); //设置嵌套UI区块个数
    void SetPreLoadNum(unsigned short pre_loan_num); //设置预加载缓冲区块个数

    P<iUI_OBJECT> const GetSubUi() { return (zUiGroups ? zUiGroups[0] : P_()); } //获取首个子区块UI
    P<iUI_OBJECT> const GetSubUi(unsigned int zid) { return (zUiGroups ? zUiGroups[zid % zUiGroups.Num()] : P_()); } //获取指定区块ID对应的子区块UI(区块ID)返回子区块UI

    virtual P<iUI_OBJECT> Clone(); //复制出该UI组件的一个新副本

    //把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
    virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out = 0);

    //从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
    virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);
};
}; // namespace GNu
#endif //_GN_UILISTGUI_H_