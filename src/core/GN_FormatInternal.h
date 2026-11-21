// GN_FormatInternal.h — 文件格式模块内部共享宏（插件链摘除/插件表清空），不对外暴露。
#ifndef _GN_FORMAT_INTERNAL_H_
#define _GN_FORMAT_INTERNAL_H_

// 共享的错误模板（定义在 GN_FormatImgFile.cpp，图像/音频保存共用）。
extern const char *GT_ERR_FileCreateFail;

#define GT_TMP_REMOVE_PLUGINS_NODE(TYPE, LIST)                 \
    if(module) {                                               \
        GT_ModuleClose(module);                                \
        module = 0;                                            \
    }                                                          \
    if(LIST == this) LIST = this->next_node;                   \
    else {                                                     \
        TYPE *last = LIST;                                     \
        while(last->next_node != this) last = last->next_node; \
        last->next_node = this->next_node;                     \
    }

#define GT_TMP_REMOVE_PLUGIN(_PLG, PLG) \
    int count = 0;                      \
    while(_PLG != &PLG) {               \
        delete _PLG;                    \
        ++count;                        \
    }                                   \
    return count;

#endif //_GN_FORMAT_INTERNAL_H_
