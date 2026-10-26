#ifndef _GN_ED_PLUGIN_H_
#define _GN_ED_PLUGIN_H_
/************************************************************************ 编辑器插件接口 ****************************************************************************************************************************************/
namespace GNu {
enum eCOPY_OPT { //复制副本操作方式
    eCOPY_DEL, //删除副本
    eCOPY_COPY, //复制副本
    eCOPY_PASTE, //粘贴副本
    eCOPY_PASTE_INST //粘贴副本实列
};
class GN_API iSETTING_MENU : public sSIM_MENU //参数设置菜单插件接口
{
  public:
    void *set_obj; //要设置的对象

    virtual ~iSETTING_MENU() {}
    virtual void Open() = 0; //打开菜单时的处理
    virtual void Change() {} //菜单值更改时的处理
    virtual void Close(int8 code) {} //关闭菜单时的处理(关闭触发码 值1:点确定关闭 0:点取消关闭 -1:其它情况强制关闭)
    virtual void ParameCopy(eCOPY_OPT op) {} //参数复制时的处理(复制副本操作方式)

    //获取复制栏显示状态(返回预览小图标色调, 返回是否显示粘贴图标, 返回是否需要显示粘贴实列图标, 返回首个复制属性起始ID 返回0xff不显示复制栏, 返回最末复制属性的ID)返回预览小图标 返回0用纯色显示
    virtual GNg::sIMAGE *GetCopyBarState(unsigned int &icon_color, bool &show_paste, bool &show_paste_inst, uint8 &first, uint8 &end) {
        show_paste = false;
        show_paste_inst = false;
        first = 0xff;
        end = 0;
        return 0;
    }
};
}; // namespace GNu

enum GP_eSETTING_CLASS { //设置类别
    GP_eSETTING_OBJUI, //设置UI组件
    GP_eSETTING_OBJ2D, //设置2D对象
    GP_eSETTING_OBJ3D //设置3D对象
};
//判断对话筐当前展现的是否为插件里的参数设置菜单(对话筐)
inline bool GP_CurProcIsPluginMenu(GNu::cSIM_DIALOG *dlg) {
    return dlg->GetCurProcCaption()[0] == '	' /*Tab键*/;
}

//插入自定义参数设置菜单到父菜单里的方法(设置对象的类型, 设置对象, 其它一起选到的对象, 父菜单) !插入的子选单名称首个字符必须为Tab键 !把该函数指针指向一个能插入派生自定义参数设置菜单的方法 便可以按您自定义的方法来设置对象参数
extern GN_API void (*GP_SettingMenuInsert)(GP_eSETTING_CLASS set_class, void *_set_obj, P<P_> &other_objs, GNu::sSIM_MENU *pmenu);

enum GP_eMENU_TYPE { //基础数据菜单类型
    GP_eMENU_IMAGE, //图像菜单
    GP_eMENU_ARFMT, //排列格式菜单
    GP_eMENU_TEXTSTYLE //文本输出式样菜单
};
struct GP_sMENU_ARGS_IMAGE //图像菜单初始化参数
{
    GP_eSETTING_CLASS set_class; //图像所在对象类型
    P<GNg::sIMAGE> &(*pfGetImgFunc)(void *); //取得图像的方法
    void (*pfChgImgProc)(void *, P<GNg::sIMAGE> &); //图像变更后的处理

    //初始化(图像所在对象类型, 取得图像的方法(传回给您的对象)要求返回图像的引用, 图像变更后的处理(传回给您的对象, 新选到的图像))
    inline GP_sMENU_ARGS_IMAGE(GP_eSETTING_CLASS _class, P<GNg::sIMAGE> &(*get_img_func)(void *), void (*chg_img_proc)(void *, P<GNg::sIMAGE> &)) : set_class(_class), pfGetImgFunc(get_img_func), pfChgImgProc(chg_img_proc) {}
};
struct GP_sMENU_ARGS_TEXTSTYLE //文本输出式样菜单初始化参数
{
    GNg::cTEXT_ART *set_text; //被设置的文本输出式样
    short *frame_wh; //指向文本筐宽高的指针
    short font_wh_min; //单个字或单格宽高的可设置最小值
    void (*change_proc)(); //选单值更变时的额外处理

    //初始化(被设置的文本输出式样, 指向文本筐宽高的指针 指定0则忽略限在筐内项的更变修改, 单个字宽高的可设置最小值, 选单值更变时的额外处理)
    inline GP_sMENU_ARGS_TEXTSTYLE(GNg::cTEXT_ART *_set_text, short *_fwh = 0, short wh_min = 0, void (*chg_proc)() = 0) : set_text(_set_text), frame_wh(_fwh), font_wh_min(wh_min), change_proc(chg_proc) {}
};
struct GP_sMENU_ARGS_ARFMT : public GP_sMENU_ARGS_TEXTSTYLE //排列格式菜单初始化参数
{
    //初始化(被设置的排列格式, 指向排列筐宽高的指针 指定0则忽略限在筐内项的更变修改, 单格宽高的可设置最小值, 选单值更变时的额外处理)
    inline GP_sMENU_ARGS_ARFMT(GNg::sARRANGE_FMT *_set_afmt, short *_fwh = 0, short wh_min = 0, void (*chg_proc)() = 0) : GP_sMENU_ARGS_TEXTSTYLE((GNg::cTEXT_ART *)_set_afmt, _fwh, wh_min, chg_proc) {}
};
//基础数据类型设置菜单创建(要创建的基础数据菜单类型, 对应的初始化参数结构)返回菜单 用完记得要 delete 菜单  !应用该菜单作为子选单时 名称首个字符必须为Tab键
extern GN_API GNu::iSETTING_MENU *(*GP_BaseSetMenuCreate)(GP_eMENU_TYPE menu_type, void *args);

#endif //_GN_ED_PLUGIN_H_