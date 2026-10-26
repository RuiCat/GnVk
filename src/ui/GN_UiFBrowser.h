#ifndef _GN_UIFBROWSER_H_
#define _GN_UIFBROWSER_H_

void GT_FILE_BROWSER_FindProc(P<GN_sTHREAD>, void *, void *);
void GT_FILE_BROWSER_FindDir(P<GN_sTHREAD>, void *, void *);
void GT_FILE_BROWSER_FindCopy(P<GN_sTHREAD>, void *, void *);
void GT_FILE_BROWSER_FindDel(P<GN_sTHREAD>, void *, void *);

namespace GNu {
enum eFILE_SORT_TYPE { //文件排序方式
    eFILE_SORT_NAME, //按名称排列
    eFILE_SORT_SIZE, //按大小排列
    eFILE_SORT_CLASS, //按类型排列
    eFILE_SORT_DATE //按修改时间排列
};
class GN_API cFILE_BROWSER : public cUI_NEAR_OBJECT, public cSCROLL_CTRL // ★文件浏缆  这类对象一般定以一个就够用了
{
    friend void ::GT_FILE_BROWSER_FindProc(P<GN_sTHREAD>, void *, void *);
    friend void ::GT_FILE_BROWSER_FindDir(P<GN_sTHREAD>, void *, void *);
    friend void ::GT_FILE_BROWSER_FindCopy(P<GN_sTHREAD>, void *, void *);
    friend void ::GT_FILE_BROWSER_FindDel(P<GN_sTHREAD>, void *, void *);
#ifndef _APPLE_
    cSCROLL_CTRL::RePos;
#endif
  protected:
    P<GNg::cFRAME_LAUNCHER_FONT> zFFrames; //图标发射器
    GNg::cTEXT_ART zTextOut; //文本输出器

    int oW, oH; //矫正筐范围位置以弥补字的中心点不在左上角的其他位置上
    short zTextW; //文件名显示筐宽
    short zTextH; //文件名显示筐高
    int zOffsetAdd; //偏移增加量 该值用于在查找到该图标时拖动屏幕量
    unsigned int zSeekSelectID; //要求立即去跳转选定的文件ID
    unsigned int zCurSelectID; //当前最近一次选中的图标是该目录的第几个图标ID
    unsigned int zLastSelectFID; //当前最近一次选中的图标是当前浏缆筐内的第几个文件图标ID
    unsigned short zLastStartPos; //当zFileNum超出排版限制数量时 记录从第几个文件开始送进排版处理的
    eNEAR_TYPE zNearPos; //靠位方向

    bool zShowReady; //标记准备好可显示没
    bool zReArrange; //是否要重新排列图标
    bool zReArrangeParame; //是否需重设置一些排版参数
    bool zReLoadIcon; //是否要重新读取图标
    bool zUsMultiSelect; //是否使用多选 这个会结合快捷键用来作最终判断
    uint8 zReGenIcon; //当前是否需重新生成文件图标
    uint8 zIsNoFileAndDir; //当前打开目录是否没有文件和目录
    bool zIsFindIng; //是否正在搜索文件和目录以准备在视图中显示
    bool zCurHasReArrange; //当前帧是否有重排位过图标
    bool zClickLastFile; //标记是否有点击图标提示
    bool zIsDirDelOrCopy; //标记要求执行目录查找处理是否删除目录或复制目录提出的
    int8 zLastClickType; //最近是否有触发双击点中东西 1:文件 -1:文件夹 0:没点中东西 2:该层文件夹的内容已经消失了
    eFILE_SORT_TYPE zSortType; //文件排列方式

    void *zRelate; //关联对象 在用其他程序生成动态预览图标时用到
    void *zActIconTmpData; //处理动态图标的临时数据
    P<P<char>> zGlobalPaths; //全局路径数组 记录当前视图里的所有图标对应的路径 主要用来给回调aOnIconGenCallBack传值等...
    P<char *> zGlobalPaths_; //全局路径数组 指向 zGlobalPaths 里的 P<char> 的以便于向没有安全指针的接口传值

    unsigned int zFileNum; //当前打开目录文件数
    P<P<char>> zFileNames; //文件名组 收集到当前打开目录的所有斋文件名 用于显示用
    P<P<GNf::sFILE_INFO> *> zInfoP; //文件信息指针数组
    P<char> zPicID; //图标图序号数组 记录当前打开目录的所有文件和文件夹的图标ID
    P<char> zSelected; //当前打开目录的所有文件和文件夹的选中状态

    GN::sLIST<P<GNf::sFILE_INFO>> zFileList; //临时收集文件信息用
    GN::sLIST<P<GNf::sFILE_INFO>> zDirList; //临时收集目录信息用
    GN::sLIST<P<GNf::sFILE_INFO>> *zpSearchFileList; //在查找时指向用那个列表来装查找到的文件
    GN::sLIST<P<GNf::sFILE_INFO>> *zpSearchDirList; //在查找时指向用那个列表来装查找到的目录
    GN::sLIST<P<char>> zUsedFileNameList; //标记还有那些名称已经使用了 不能在当前打开目录下再使用 该变量主要供 GetNoUsedCloneName 使用

    P<GNg::sIMAGE> zFocusImg; //选中焦点底图
    P<GNg::cFRAME> zFrameIcon; //默认动画效果筐

    char zLastDir[GN_MAXFILEPATH]; //上次成功打开过的目录

    //初始化内部对象(默认显示图标的范围X, Y, 宽, 高, 文件名显示的文本筐宽 0:自动适应为文件图标宽度) 只有在没设置图标弹出效果筐时该范围才有作用
    void zInitInternalObj(short fx, short fy, short fw, short fh, short tw = 0);
    //查找文件或目录
    void zSearch(P<P<char>> &sm, const char *cur_dir, bool search_sub_dir, void *ph);
    //查找文件或目录__
    void zSearch__(const char *dir, const char *search_mode, bool search_sub_dir, void (*proc)(GNf::sFILE_INFO *file_info), void *ph);

    void zArrangeFileIcons(); //排列图标
    void zGenerateFileIcons(); //重新生成图标()
    void zSelectSegment(unsigned int fframe_id, unsigned int pid); //选择段
    int8 zDoubleClickOpen(unsigned int fframe_id); //处理双击打开由字筐ID所代表的东西(字筐ID)返回点中的是否文件:1还是文件夹:-1
    void zSetIconChange(unsigned int fframe_id, unsigned int pid); //设置图标更改显示状态
    void zScrollToSelectIcon(unsigned int pid); //滚动到指定图标,使它选中
    void zScrollProc(unsigned int updatatime); //处理滚动(每帧间隔时间)
    void zSpecClone(); //分离克隆副本不能共享的属性
    uint8 *zDataSave(uint8 *wpos, cSHARE_DATA *share_objs); //保存该对象的2进制数据(当前写入位置, 收集共享对象集)返回最后的写完位置
    uint8 *zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs); //读取该对象的2进制数据(当前读取位置, 引用共享对象集)返回最后的读完位置

    static unsigned short zCreateTotalNum; //记录文件浏览的总创建数
  public:
    P<GN_sTHREAD> zTkThread; //处理任务线程
    P<char> zTkArgSMode; //任务线程用的传递参数
    P<char> zTkArgSDir; //任务线程用的传递参数
    void *zTkArgPH; //任务线程用的传递参数
    void *zTkArgPH2; //任务线程用的传递参数
    short zTkArgPathLen; //任务线程用的传递参数
    short zTkArgPathLen2; //任务线程用的传递参数
    bool zTkArgSSubDir; //任务线程用的传递参数
    void (*zTkArgFunc)(GNf::sFILE_INFO *); //用于传递任务线程用的参数

    // ★用户接口 --------------------------------------------------------------------------------------------------------------------------------------------------------------
    //	unsigned int					aFocusColor;        //焦点颜色		!已在基类有定义
    //	unsigned short                  aFocusBlend;        //焦点混合模式  !已在基类有定义
    unsigned int aTextColor; //文本颜色
    unsigned short aTextBlend; //文本混合模式
    short aShowOffsetX; //在文件图标筐内的显示偏移量X
    short aShowOffsetY; //在文件图标筐内的显示偏移量Y
    bool aUsMultiSelect; //是否启用多选
    bool aIgnoreSED; //查找过程是否忽略文件夹不作处理! 忽略这个并不代表不查找子目录
    bool aDisableControl; //是否禁用操作控制 如果是就只能显示不能对它操作

    P<char> aFileTypeFilterStr; //类型文件显示过滤描述 如"png,jpg,txt" 中间用逗号隔开每种文件扩展名 该值默认为空 不进行过滤
    char aOpenDir[384]; //记录当前要打开的目录

    int aSearchTotalFileN; //统计查找总文件数 经过一次查找后会把所查到的文件数一直累加到这个值中
    int aSearchTotalDirN; //统计查找总目录数 经过一次查找后会把所查到的目录数一直累加到这个值中
    int aSearchTotalSizeGB; //统计查找总大小   总大小超1GB的用该值来记录
    int aSearchTotalSizeB; //统计查找总大小   经过一次查找后会把所查到的文件大小一直累加到这个值中 当大小超过1GB将进位到aSearchTotalSizeGB值中保存

    cFILE_BROWSER();
    ~cFILE_BROWSER();

    //设置图标弹出动画效果  这个需在 InitStyleToData 之前调用
    void SetEffect(
        short focus_flash_time, //焦点闪动间隔时间 0为不闪动
        P<GNg::cFRAME_LAUNCHER_FONT> effect = P_(), //图标弹出动画发射器 这个在运行后将会受到修改
        P<GNg::sIMAGE> focus_style = P_(), //闪动焦点底图
        P<GNg::sIMAGE> offset_arrow = P_(), //内容拖动箭头拦式样图
        int8 repeat_num = 0 //拖动拦图案重复铺盖数 值0:就拉伸到边沿
    );

    //初始化设置显示式样 !!必需项
    bool InitStyleToData(
        const GNg::cTEXT_ART *text_style = 0, //文字输出式样
        const char *open_dir = 0, //初始打开目录 如"E:/myfile" 值0:为打开当前运行应用程序所在的目录 !注意需指定全局路径 但想要在不同的系统平台下都得到兼容 那最好就是调用GNf::EnumSystemDirectory()获得系统列举几个常用目录来打开
        short x = NearLx(0), //浏缆筐位置X 如有调用SetEffect指定生成效果筐范围时该值被忽略
        short y = NearTy(0), //浏缆筐位置Y 如有调用SetEffect指定生成效果筐范围时该值被忽略
        short w = GNg::ViewCur->orthoW, //浏缆筐宽度  如有调用SetEffect指定生成效果筐范围时该值被忽略
        short h = GNg::ViewCur->orthoH, //浏缆筐高度  如有调用SetEffect指定生成效果筐范围时该值被忽略
        short tw = 90, //文件名显示筐宽 0:自动适应为文件图标宽度
        short th = 52, //文件名显示筐高 0:自动适应为文件图标搞度
        eNEAR_TYPE near_pos = eNEAR_BOTTOM, //文件名靠图标方向
        unsigned int text_color = 0, //文本颜色
        unsigned int focus_color = 0xffff3434, //焦点颜色
        unsigned short text_blend = GNg::eBLEND_NORMAL, //文本混合模式
        unsigned short focus_blend = GNg::eBLEND_BRIGHT //焦点混合模式
    ); //返回是否成功

    //设置内容在筐内偏移效果(偏移速度:0.05f至1.0f) !如果该值被设为0.0f 它将不能滚动 !已在基类有定义
    //void  SetOffsetEF(float sheep = 0.1f );

    //获得系统的文件类型图标图象  通常用该方法获得图标图象来传给动画筐对象再调用 SetEffect 设置图标弹出动画效果
    static GNg::sIMAGE *GetIcon();

    //更新运行文件浏缆(帧间隔时间, 如有指定返回双击点中的图象指针, 如有指定返回双击点中的图象显示范围, 实时总体色调) 返回是否有触发双击点中东西 1:文件 -1:文件夹 0:没点中东西 2:该层文件夹的内容已经消失了 !通常如果遇到返回有触发就调用下面的两个函数来取得当前选中的文件名来打开处理 或 下面的GetSelectFileAndDirInfoData 函数来取得当前选中的文件和目录详细信息
    int8 UpdateRun(unsigned int updatetime, GNg::sIMAGE **img_out = 0, GNg::sDRAW_RANGE *click_icon_info = 0, unsigned int color = 0xffffffff);

    //获取当前选到的文件和目录信息列表(获取分类选项)返回文件和目录信息列表 用之前最好检测一下返回值是否有
    P<P<GNf::sFILE_INFO>> GetSelectFileAndDirInfoData(GNf::ePATH_TYPE get_op = GNf::ePATH_UNKNOWN);

    P<P<char>> GetSelectFileNameData(); //获取当前选择到的文件名列表 返回为全局路径的 用之前最好检测一下返回值是否有
    P<P<char>> GetSelectDirNameData(); //获取当前选择到的目录名列表 返回为全局路径的 用之前最好检测一下返回值是否有

    char const *GetNoUsedCloneName(const char *name, bool name_set = true); //获取当前打开目录里还未被使用指定文件或文件夹名的一个副本名(指定文件或文件夹名, 是否顺便将返回的名标记为已使用)如参数的名未被使用则直接返回它 如使用了就返回一个未被使用的指定文件或文件夹名的副本名 格式规律 00001#name.txt 编号由1开始
    void GetShowIconPosInfo(unsigned int icon_sid, GNg::sDRAW_RANGE *info_out); //获得第N个可视图标的绘制范围(第几个可视图标:0开始, 输出图标的绘制范围)
    P<unsigned int> GetSelectVIDs(); //获取当前选择到的可视图标全部顺序号
    short GetParalleShowCount(); //获取浏缆筐内的最大并排显示数
    inline unsigned int GetCurIconCount() { return zFileNum; } //获得当前目录层下共要显示的文件和文件夹图标数 这可能会包含系统自动生成的一个返回文件夹
    inline unsigned int GetCurIconVisualCount() { return zFFrames->zMax_num; } //获得当前目录层下显示了的文件和文件夹图标数
    inline short GetIconW() { return zFFrames->zParame.width; } //获得图标排列宽度
    inline short GetIconH() { return zFFrames->zParame.height; } //获得图标排列高度
    inline short GetIconRangeX() { return zFFrames->zParame.i_compartment; } //获得图标排列的水平间距
    inline short GetIconRangeY() { return zFFrames->zParame.j_compartment; } //获得图标排列的垂直间距
    //	inline unsigned int  GetFocusChangeColor(){ return zArrowAm.aLevel_color; }     //获得焦点动画当前变化的颜色 !已在基类有定义
    inline eFILE_SORT_TYPE GetSortMode() { return zSortType; } //获得当前图标排列方式类型
    inline GNg::cTEXT_ART *const GetTextStyle() { return &zTextOut; } //获得文本输出式样
    inline int8 GetLastClickType() { return zLastClickType; } //返回最近是否有触发双击点中东西 1:文件 -1:文件夹 0:没点中东西 2:该层文件夹的内容已经消失了 !当调用无返回值的UpdateRun时就会用这个来缓返回值
    inline bool IsBusy() { return zTkThread ? true : false; } //返回当前是否正在忙碌中
    inline bool IsFindIng() { return zIsFindIng; } //返回当前是否正在检索文件和目录以准备在视图中显示
    inline bool IsIconPosChange() { return (zReArrange || zReGenIcon); } //返回当前文件图标排位是否有变动过
    inline bool IsCanAccess() { return zIsNoFileAndDir != 2; } //返回当前打开的目录是否能访问
    //		   bool  IsMouseFocus();													//返回鼠标当前是否指到该对象 !已在基类有定义

    GNg::sVEC2 GetPosXY(); //获得显示位置的左上角坐标
    void SetPos(short x, short y, short w, short h); //设置显示位置和大小(左上角坐标X, 左上角坐标Y, 文本筐宽度, 文本筐高度)
    void GetPos(short *x, short *y, short *w, short *h); //获得显示位置和大小(左上角X输出, 左上角Y输出, 宽度输出, 高度输出)
    void SetPos(GNg::sRECT2 &range); //设置显示位置和大小(矩型范围)
    void GetPos(GNg::sRECT2 &range_out); //获得显示位置和大小(矩型范围输出)

    bool SelectIconFromName(const char *name); //选上一个和指定名相同的文件或文件夹并滚动图标使选到的图标能够出现在视图中(名) 返回当前打开目录是否有该名的内容
    void SelectIconFromRect(const GNg::sRECT *rect); //选上指定范围的相关的图标
    void SelectClear(); //清除选择 使当前目录下的文件都为未选状态
    inline void ClickLastFile() { zClickLastFile = true; } //提示上次选到的文件为被双击点中 这样在更新运行时就算未使用鼠标双击图标都当上次选到的图标触发双击事件 !!如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙

    void ReturnToLastDir(); //返回到上一级目录
    void OkHide(); //使有动画效果的筐内图标隐藏 如果打开文件图标有动画 那么这样隐藏也有相应的动画
    void RePop(); //重弹出排列文件图标 通常来恢复 OkHide 的调用 !如果文件浏缆筐正在忙会忽略这调用
    void ClearCurIconData(); //提示清除当前所显示的图标相关内部数据以减少内存开销 这个通常用在当用户选好文件后 有一段时间将不会再用到文件浏览 但又不想把整个文件浏览删除时用 !如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙

    void SetSortMode(eFILE_SORT_TYPE sort_type); //设置图标排列方式类型  !!如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙
    void SetOpenDirectory(const char *open_dir); //设置打开目录(目录路径 需全局目录路径) 如:"D:/myfile" 值0:打开当前运行应用程序所在目录 !想要在不同的系统平台下都得到兼容 那最好就是调用GNf::EnumSystemDirectory()获得系统列举几个常用目录来打开 !如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙

    void InitNoUiMode(); //初始化文件浏览为无界面简易模式 这样就可以快捷地用文件浏览来作一些目录查找 复制 删除等操作 而无需调用繁琐的界面初始化工作 但该模式下所有文件浏览有关界面的操作方法都不能调用 只能使用以下的一些函数和 IsBusy UpdateRun 函数

    void TotalFileSizeAdd(size_t add_size); //使统计总大小和文件数的值按1GB进位的格式增加一个指定大小的文件数到值里

    //清除所有之前查找统计到文件与文件夹总数和文件大小总数()
    inline void SearchStatisticsNumClear() {
        aSearchTotalDirN = 0;
        aSearchTotalFileN = 0;
        aSearchTotalSizeGB = 0;
        aSearchTotalSizeB = 0;
    }

    //通知查找文件和文件夹(是否等到处理完才返回, 查找目录 需全局路径默认0:为当前打开目录, 查找模式字符, 是否连所有子目录都查找, 查找到的目录和文件都执行的处理) !!这里查找后重刷新图标不会列出文件夹来 如果有指定最后的参数就不会刷新显示图标 如果文件浏览被指定为无界面模式你必须提供最后个参数  !如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙
    void Search(bool wait, char *dir = 0, const char *search_mode = 0, bool search_sub_dir = 0, void (*proc)(GNf::sFILE_INFO *file_info) = 0);

    //通知在指定文件夹里查找所有子文件夹和文件都执行指定处理(是否等到处理完才返回, 查找目录 需全局路径默认0:为当前打开目录, 执行处理函数) 想在执行函数传来的参数里区分文件夹可用 if(file_info->attrib & _A_SUBDIR) 来区分 !如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙
    void Search_exec(bool wait, char *dir, void (*proc)(GNf::sFILE_INFO *file_info));

    //复制源位置指定目录里的所有内容到目标位置目录里(是否等到处理完才返回, 目标位置目录名 需全局路径, 源位置目录名 需全局路径, 源目录所在主机) 如果中途有错会把错误信息记录到GN_ErrorLast变量中 !注意 源主机和当前处理主机相同时源目录不能和目标目录相同 !如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙
    void CopyDir(bool wait, const char *dir_dst, const char *dir_src, void *ph_src = 0);

    //删除指定目录(是否等到处理完才返回, 目录名 需全局路径, 删除的目录所在主机) 如果中途有错会把错误信息记录到GN_ErrorLast变量中 !如果浏缆筐正在忙会忽略这调用 所以调用前先用 cFILE_BROWSER::IsBusy 检查是否正在忙
    void DelDir(bool wait, const char *dir, void *ph = GNf::sFILE::DefaultH);

    //如果有处理线程就通知立即返回并等返回完成为止 这个一般用来中断以上4个调用的持久处理
    void WaitProcFlushReturn();

    //文件复制( 目标位置文件名 需全局路径, 源位置文件名 需全局路径, 目标文件所在主机, 源文件所在主机) 成功返回0 返回其它 -1:打开源文件有错 -2:打开目的文件有错 -3:读取源有错, -4:写入目的文件有错
    static int FileCopy(const char *fname_dst, const char *fname_src, void *ph_dst, void *ph_src);

    //当筐内图标被重新排列过时的处理(是否查找结果, 当前所有排列好的图标名称:其中也包含返回目录的名称, 文件图标的起始ID;如没文件该值为0xffffffff) 用户可以继承修改该方法用来得知查找结果数或做其它处理
    virtual void OnReArrangeIcon(bool is_search_result, P<P<char>> names, unsigned int file_start_id) {}

    //当查找执行处理完成时的处理(成功执行文件和目录总数) 用户可以继承修改该方法用来得知查找处理完成
    virtual void OnSearchProcFinish(unsigned int proc_count) {}

    virtual P<iUI_OBJECT> Clone(); //复制出该UI组件的一个新副本
    virtual void OnRePos(sUI_LEVEL *parent); //当父层UI组大小变更了要求组件成员重置位置时的处理(父层级信息链)
    virtual void OnMove(short mx, short my); //当父层UI组平移拖动了要求组件成员偏移位置时的处理(X移动量, Y移动量)
    virtual void UpdateRun(unsigned int updatetime, sUI_LEVEL *parent); //更新运行(帧间隔时间, 父层级信息链)
    virtual void GetDrawArea(GNg::sDRAW_AREA2 &out, cUI_GROUP *parent); //获得UI组件的实际绘制区域(返回区域, 父层组 您可以不指定它) !需在 UpdateRun UpdatePlay 执行后再调用该方法才有正确值输出

    //把内部数据储存为一整段数据时的处理(收集共享对象集, 返回整段数据 首个字节为版本号之后4个字节为aType 当该值为0时仅返回数据段需要大小)返回数据段大小字节 !用户自定义UI组件的版本号应由1开始
    virtual unsigned int DataSave(cSHARE_DATA *share_objs, uint8 *data_out = 0);
    //从文件数据段里读入数据时的处理(引用共享对象集, 数据段地址 首个字节为版本号之后4个字节为aType)返回已读取字节数 失败返回0 !用户自定义UI组件的版本号应由1开始
    virtual unsigned int DataLoad(cSHARE_DATA *share_objs, const uint8 *data);

    static char *aCurProcFileName; //指向当前正在执行 FileCopy 或 CopyDir 或 DelDir 函数时正在处理的文件名
    static float aCurProcFileProgress; //记录在执行 FileCopy 或 CopyDir 函数时当前正在复制文件的进度 0.0f 至 1.0f

    //指向当文件复制将要开始时的处理函数(复制目标文件名, 复制源文件名, 目的文件所在主机, 源文件所在主机) 要求返回复制源文件的句柄将继续处理该文件 返回0表示跳过该文件不处理 !这里告诉你的两个文件名参数已经帮你设置好全局读写目录状态 直接拿来用即可
    static GNf::sFILE *(*aOnFileCopyBeginCallBack)(const char *dst, const char *src, void *ph_dst, void *ph_src);

    //指向当文件复制完成结束时的处理函数(复制目标文件名, 复制源文件名, 目的文件所在主机, 源文件所在主机) !这里告诉你的两个文件名参数已经帮你设置好全局读写目录状态 直接拿来用即可
    static void (*aOnFileCopyEndCallBack)(const char *dst, const char *src, void *ph_dst, void *ph_src);

    //设置文件图标显示是否使用缩图预览(是否)
    void UsShowPreview(bool yes);

    //设置所有该类对象文件图标显示是否使用缩图预览(是否) 默认是不使用预览的 !但您依然可以调用 UsShowPreview 为特定的文件浏览启用缩图预览
    static void SetShowPreview(bool yes);

    /*----------------------------------------------- 以下这几个回调函数指针常用于使所有的文件浏览增加对特殊文件的显示与缩图预览用 ---------------------------------------------------------------------------------------------------------------------------------------*/

    //当该类对象创建时的处理函数指针(文件浏览对象)要求返回一个关联对象
    static void *(*aOnCreateCallBack)(void *fb);
    //当该类对象被销毁时的处理函数指针(关联对象)
    static void (*aOnDestroyCallBack)(void *relate);

    //当有文件或目录图标要生成时的处理函数指针(关联对象, 当前要准备显示的文件夹和文件总数, 所包含的文件夹和文件全局路径数据, 对应每个文件夹和文件所分配到的图标号数据, 是否查找结果, 显示图标宽度, 高度) !这个通常在打开一个目录显示目录里的内容前和查找文件显示结果前被调用到 这里分配到的图标号数据你可修改它来实现文件类型图标更变
    static void (*aOnIconGenCallBack)(void *relate, int fn, const char **paths, char *icon_ids, bool is_search_result, int icon_w, int icon_h);
    //当文件或目录图标要清除时的处理函数指针(关联对象) !这个通常在新一批图标要准备生成时或该类对象销毁前被调用到
    static void (*aOnIconClearCallBack)(void *relate);

    //当图标被重新排列时的处理函数指针(关联对象, 当前打开目录里包含的文件夹和文件总数, 首个图标相当于文件夹和文件总数的第几个, 末尾图标相当于文件夹和文件总数的第几个)
    static void (*aOnIconsReArrangeCallBack)(void *relate, int fn, int first, int last);

    //当该类对象更新显示时的处理函数指针(关联对象, 每帧间隔时间)
    static void (*aOnUpdateCallBack)(void *relate, unsigned int updatetime);

    /*返回通知状态
	 0:该图标没有数据不需更变图标  
	-1:数据还没准备好
	 1:数据好了 通知原图标消失 新图标原位出现  
	 2:数据好了 通知原图标消失 新图标带弹出动画出现
	 3:数据好了 通知原图标消失 新图标原位出现		并提示接下来不用再检测该图标数据了 因为图标内容不会动画更新
	 4:数据好了 通知原图标消失 新图标带弹出动画出现 并提示接下来不用再检测该图标数据了 因为图标内容不会动画更新
	 以上的正值如再组合|8 那么您可返回一个图像指针sIMAGE* 通知文件浏览这图标可以直接用了 无需再传入数据给图标
	*/
    //当图标更变数据要检测是否准备好时的处理函数指针(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 显示图标宽度, 高度, 当前图标对应文件或目录的全局路径)要求返回一个通知状态
    static int (*aOnIconDataOkCheckCallBack)(void *relate, int fi, int is_sed, int icon_w, int icon_h, const char *path);
    //当图标更变需要获取数据时的处理函数指针(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 当前图标对应文件或目录的全局路径)要求返回图标的图象数据 !必需为RGBA格式数据, 这个通常是aOnIconDataOkCheckCallBack被调用后得知图标更变数据准备好了才被调用的, 用以来更新图标数据内容
    static char *(*aOnIconDataGetCallBack)(void *relate, int fi, int is_sed, const char *path);
    //当图标获取到数据后更变了时的处理函数指针(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 当前图标对应文件或目录的全局路径) 这个通常是aOnIconDataGetCallBack被调用后获得数据更改了图标后就被调用, 这里的数据被传达后文件浏览会另外拷贝副本, 你可以在这就把数据删掉以节省空间, 除非你想实现图标帧动画
    static void (*aOnIconDataChangedCallBack)(void *relate, int fi, int is_sed, const char *path);

    //当图标正在绘制时的处理函数指针(关联对象, 当前图标属于第几个文件夹或文件, 是否被选中, 图标显示左上角坐标X, Y, 图标显示宽度,高度, 图标显示色调, 图标图层ID, 当前图标对应文件或目录的全局路径)返回是否要隐藏对应的原图标
    static bool (*aOnIconDrawCallBack)(void *relate, int fi, int is_sed, int x, int y, int w, int h, unsigned int color, unsigned short tid, const char *path);
};
}; // namespace GNu
#endif //_GN_UIFBROWSER_H_
