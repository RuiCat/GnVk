// GN_mini 容器拆分片段：GN::sNODE 安全节点树。
// 2026-11-26 自 GN_Array.h 第 3672-4015 行 move-only 拆出（REFACTOR_PLAN §13 B3）。
// 由 GN_Array.h 按原顺序转发包含；单独包含需自行补齐前置依赖。
#ifndef GN_CONTAINERS_NODETREE_H
#define GN_CONTAINERS_NODETREE_H

// 自包含依赖（B3 拆分时补齐；正文仍为 move-only）
#include "baseTypes.h"
#include "list.h"
#include "listWheel.h"
#include "smartPtr.h"

namespace GN {
template <class T>
struct sNODE //★安全节点 用于方便构成子父嵌套关系的树形连锁数据结构 为数据元素动态删减添加分类等带来高效方便的处理 一般用法是定义个新类继承它 如: class 新类 :public GN::sNODE<新类>
{
  public:
    GN::sLIST<P<T>> zSub; //接入到它的子节点
    P<T> *zParentNode; //记下该节点在它父节点的子节点链表中的数据地址 以方便解开连接用
    T *zParent; //父节点指针
    P_ zPthis; //节点自身的安全指针
    P<char> aName; //名称

    sNODE() : zParent(0), zParentNode(0) {}

    virtual ~sNODE() {
        GT_LIST_iterator(P<T>) in = zSub.begin(), en = zSub.end();
        for(; in != en; ++in) {
            (*in)->zParent = 0; //先解除它和它的子节点连接关系
            (*in)->zParentNode = 0;
        }
        zSub.clear();
        //zParent = 0; zParentNode = 0;
        //zPthis.Delete();
        //aName.Delete();
    }
    //初始化节点(名称, 它的安全指针号) !!如果它是由用户手动在代码上创建的必需调用一下该函数
    void InitToData(char *name, P_ p_this) {
        GT_TMP_OBJ_RE_CONSTRUCTOR(sNODE)
        zPthis = p_this;
        aName(name, 0);
    }
    //解除该节点与父节点的连接 !!在对该节点的子节点历遍操作时请不要解除连接操作 否则会出轨
    void UnComment() {
        if(!zParentNode) return;
        zParent->zSub.del(zParentNode);
        zParentNode = 0;
        zParent = 0;
    }
    //接入其它节点(其它节点) !!在对该节点的子节点历遍操作时请不要连接操作 否则会出轨
    void Comment(P<T> node) {
        if(!node.Has()) return;
        node->UnComment(); //如这对象已经连接到其它节里就解除它
        node->zParentNode = zSub.add(node);
        node->zParent = (T *)this;
    }
    //复制出一个新副本(复制号, 父节点指针地址)
    void zClone(unsigned short id, P<T> *parent) {
#define GT_TMP_CLONE_NODE_NAME                                                            \
    P<T> new_node = this->zCloneOut(); /*调用不同类的特定复制虚函数来创建复制出专有数据*/ \
    char *pn = aName;                                                                     \
    while(*pn) {                                                                          \
        if(*pn == '#') break;                                                             \
        ++pn;                                                                             \
    }                                                                                     \
    unsigned int len = pn - (char *)aName;                                                \
    new_node->aName.New(len + 7);                                                         \
    pn = new_node->aName;                                                                 \
    strncpy(pn, aName, len);                                                              \
    pn[len] = '#';                                                                        \
    pn[len + 6] = 0;                                                                      \
    itoa(id, pn + len + 1, 10);

        GT_TMP_CLONE_NODE_NAME

        new_node->zParent = (T *)(*parent);
        new_node->zParentNode = (*parent)->zSub.add(new_node);

#define GT_TMP_SUB_NODE_CLONE                                              \
    GT_LIST_iterator(P<T>) it = this->zSub.begin(), et = this->zSub.end(); \
    for(; it != et; ++it) (*it)->zClone(id, &new_node); //复制出它的子节点

        GT_TMP_SUB_NODE_CLONE
    }
    virtual P<T> zCloneOut() { //复制出内部专有数据
        P<T> clone(1);
        clone->zPthis.zData = clone.zData;
        //...继承类必须重写这个虚函数 并在这补充复制新类的专有数据
        return clone;
    }
    //复制出该节点的一个新副本(复制ID号, 是否也复制它的所有子对象)返回新复制出来的节点 !!如果复制源有父节点 那么新复制出来的都挂在同一个父节点里 复制号:在原名称后面加上一个号码以示区分 该号码最好由 GN::sNUMBER 类生成
    P<T> Clone(unsigned short id, bool clone_sub = true) {
        GT_TMP_CLONE_NODE_NAME

        if(this->zParent) {
            new_node->zParent = this->zParent;
            new_node->zParentNode = this->zParent->zSub.add(new_node);
        } else {
            new_node->zParent = 0;
            new_node->zParentNode = 0;
        }
        if(clone_sub) {
            GT_TMP_SUB_NODE_CLONE
        }
        return new_node;
    }
    //获得该节点所在群组树的下一个检索节点(当节点已经是尽头时切换到的起始节点)
    P<T> GetNext(P<T> st) {
        if((CVP(T) zPthis).Us() > 1) {
            if(zSub.empty()) {
                T *_node = (T *)this;
            GT_GOTO_FindNextTreeNode:
                if(_node->zParent && _node->zParentNode) {
                    GT_LIST_iterator(P<T>) ito = _node->zParent->zSub.to_iterator(_node->zParentNode);
                    ++ito;
                    if(ito == _node->zParent->zSub.end()) {
                        _node = _node->zParent;
                        goto GT_GOTO_FindNextTreeNode;
                    }
                    return (*ito);
                } else return st;
            } else return zSub.front();
        } else return st;
    }
    //获得该节点所在群组树的跟节点
    P<T> GetRoot() {
        T *root = (T *)this;
        while(root->zParent) root = root->zParent;
        return CVP(T) root->zPthis;
    }
    //从该节点往前查找到首个符合条件的父节点( 包含的指定名字段, 比较条件函数(当前检索到的父节点) ) 返回首个满足条件的父节点 如果没有满足的返回空指针  第一第二参数是二选一的 另外一个应为0  如果两个参数都为0 即返回上一层的父节点
    P<T> GetParent(const char *name, bool (*compare)(T &first) = 0) {
#define GT_TMP_NODE_GET_PARENT(CMP_PROC)                                        \
    if(!name && !compare) {                                                     \
        if(zParent) return CVP(T) zParent->zPthis;                              \
        else return P<T>();                                                     \
    } else {                                                                    \
        T *parent = zParent;                                                    \
        if(name) {                                                              \
            size_t len = strlen(name);                                          \
            while(parent) {                                                     \
                if(strncmp(parent->aName, name, len)) parent = parent->zParent; \
                else return CVP(T) parent->zPthis;                              \
            }                                                                   \
            return P<T>();                                                      \
        } else {                                                                \
            while(parent) {                                                     \
                if(CMP_PROC) return CVP(T) parent->zPthis;                      \
                else parent = parent->zParent;                                  \
            }                                                                   \
            return P<T>();                                                      \
        }                                                                       \
    }

        GT_TMP_NODE_GET_PARENT(compare(*(parent)))
    }
    //从该节点往前查找到首个符合条件的父节点( 包含的指定名字段, 比较条件函数(当前检索到的父节点, 传回给你的参数), 传给执行函数用的参数) 返回首个满足条件的父节点 如果没有满足的返回空指针  第一第二参数是二选一的 另外一个应为0  如果两个参数都为0 即返回上一层的父节点
    P<T> GetParent(const char *name, bool (*compare)(T &first, void *us_arg), void *_us_arg) {
        GT_TMP_NODE_GET_PARENT(compare(*(parent), _us_arg))
    }
    //从该节点往前查找是否含有指定父节点
    bool HasParent(T *ptr) {
        T *root = (T *)this;
        while(root->zParent) {
            root = root->zParent;
            if(root == ptr) return true;
        }
        return false;
    }
    //从该节点群组树里查找到指定名的节点( 节点名 ) 返回查找到的节点 查找不到返回0
    T *zSearch(const char *name) {
        if(!strcmp(this->aName, name)) return (T *)this;

        GT_LIST_iterator(P<T>) in = zSub.begin(), en = zSub.end();
        for(; in != en; ++in) {
            T *ret = (*in)->zSearch(name);
            if(ret) return ret;
        }
        return 0;
    }
    //从该节点群组树里查找到符合条件的节点( 包含的指定名字段 , 比较条件函数(当前检索到的节点) )返回满足条件的节点 如果没有找到返回0
    T *zSearch_if(const char *name, bool (*compare)(T &first)) {
#define GT_TMP_NODE__SEARCH_IF(ADD_PROC, ADD_PROC2)                     \
    if(name) {                                                          \
        if(!strncmp(this->aName, name, strlen(name))) return (T *)this; \
    } else if(ADD_PROC) return (T *)this;                               \
                                                                        \
    GT_LIST_iterator(P<T>) in = zSub.begin(), en = zSub.end();          \
    for(; in != en; ++in) {                                             \
        T *ret = (*in)->ADD_PROC2;                                      \
        if(ret) return ret;                                             \
    }                                                                   \
    return 0;

        GT_TMP_NODE__SEARCH_IF(compare(*((T *)this)), zSearch_if(name, compare))
    }
    //从该节点群组树里查找到符合条件的节点( 包含的指定名字段 , 比较条件函数(当前检索到的节点, 传回给你的参数), 传给执行函数用的参数 )返回满足条件的节点 如果没有找到返回0
    T *zSearch_if(const char *name, bool (*compare)(T &first, void *us_arg), void *_us_arg){
        GT_TMP_NODE__SEARCH_IF(compare(*((T *)this), _us_arg), zSearch_if(name, compare, _us_arg))}
    //从该节点群组树里和调用节点自身查找指定名的节点( 节点名 , 是否查找整棵树) 返回查找到的节点 查找不到返回空指针
    P<T> Search(const char *name, bool full_tree = false) {
        T *search_node = (T *)this, *ret = 0;
        if(full_tree) search_node = GetRoot().Begin();
        ret = search_node->zSearch(name);
        if(ret) return CVP(T) ret->zPthis;
        else return P<T>();
    }
    //从该节点群组树里和调用节点自身查找到符合条件的首个节点( 包含的指定名字段, 比较条件函数(当前检索到的节点), 是否要查找整棵树) 返回满足条件的首个节点 如果没有满足的返回空指针 第一第二参数是分别是二选一的 另外一个应为0
    P<T> Search_if(const char *name, bool (*compare)(T &first), bool full_tree = false) {
#define GT_TMP_NODE_SEARCH_IF(ADDPROC)             \
    T *search_node = (T *)this, *ret = 0;          \
    if(full_tree) search_node = GetRoot().Begin(); \
    ret = search_node->ADDPROC;                    \
    if(ret) return CVP(T) ret->zPthis;             \
    else return P<T>();

        GT_TMP_NODE_SEARCH_IF(zSearch_if(name, compare))
    }
    //从该节点群组树里和调用节点自身查找到符合条件的首个节点( 包含的指定名字段, 比较条件函数(当前检索到的节点, 传回给你的参数), 传给执行函数用的参数, 是否要查找整棵树) 返回满足条件的首个节点 如果没有满足的返回空指针 第一第二参数是分别是二选一的 另外一个应为0
    P<T> Search_if(const char *name, bool (*compare)(T &first, void *us_arg), void *_us_arg, bool full_tree = false) {
        GT_TMP_NODE_SEARCH_IF(zSearch_if(name, compare, _us_arg))
    }
    //使该节点群组树里包含指定名的多个节点和调用节点自身都执行指定函数( 包含的指定名字段:0为全部对象, 返回是否要继续查找 执行函数(当前检索到的元素), 是否要查找整棵树) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
    bool Search_exec(const char *name, bool (*proc)(P<T> &first), bool full_tree = false) {
#define GT_TMP_NODE_SEARCH_EXEC(ADD_PROC, ADD_PROC2)                                         \
    T *search_node = (T *)this;                                                              \
    if(full_tree) search_node = GetRoot().Begin();                                           \
    bool proc_ing;                                                                           \
    if(name) {                                                                               \
        if(strncmp(search_node->aName, name, strlen(name))) proc_ing = true;                 \
        else proc_ing = ADD_PROC;                                                            \
    } else proc_ing = ADD_PROC;                                                              \
                                                                                             \
    if(proc_ing) {                                                                           \
        GT_LIST_iterator(P<T>) in = search_node->zSub.begin(), en = search_node->zSub.end(); \
        for(; in != en; ++in) {                                                              \
            if((*in)->ADD_PROC2) return true;                                                \
        }                                                                                    \
        return false;                                                                        \
    } else return true;

        GT_TMP_NODE_SEARCH_EXEC(proc(CVP(T) search_node->zPthis), Search_exec(name, proc, false))
    }
    //使该节点群组树里包含指定名的多个节点和调用节点自身都执行指定函数( 包含的指定名字段:0为全部对象, 返回是否要继续查找 执行函数(当前检索到的元素, 传回给你的参数), 传给执行函数用的参数, 是否要查找整棵树) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
    bool Search_exec(const char *name, bool (*proc)(P<T> &first, void *us_arg), void *_us_arg, bool full_tree = false) {
        GT_TMP_NODE_SEARCH_EXEC(proc(CVP(T) search_node->zPthis, _us_arg), Search_exec(name, proc, _us_arg, false))
    }
    //使该节点群组表层子对象包含指定名的多个节点都执行指定函数( 包含的指定名字段:0为表层的所有子对象, 返回是否要继续查找 执行函数(当前检索到的元素) ) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
    bool SearchSurface_exec(const char *name, bool (*proc)(P<T> &first)) {
#define GT_TMP_NODE_SEARCH_SURFACE_EXEC(ADD_PROC, ADD_PROC2)               \
    GT_LIST_iterator(P<T>) in = this->zSub.begin(), en = this->zSub.end(); \
    if(name) {                                                             \
        T *search_node;                                                    \
        for(; in != en; ++in) {                                            \
            search_node = (*in);                                           \
            if(!strncmp(search_node->aName, name, strlen(name))) {         \
                if(!ADD_PROC) return true;                                 \
            }                                                              \
        }                                                                  \
    } else {                                                               \
        for(; in != en; ++in) {                                            \
            if(!ADD_PROC2) return true;                                    \
        }                                                                  \
    }                                                                      \
    return false;

        GT_TMP_NODE_SEARCH_SURFACE_EXEC(proc(CVP(T) search_node->zPthis), proc(CVP(T)((*in)->zPthis)))
    }
    //使该节点群组表层子对象包含指定名的多个节点都执行指定函数( 包含的指定名字段:0为表层的所有子对象, 返回是否要继续查找 执行函数(当前检索到的元素, 传回给你的参数), 传给执行函数用的参数) ) 返回查找中途是否有被中断 !!注意尽量不要在执行回调内使当前检索到的元素调用UnComment来断开连接 这样会使查找树结构蹦掉!
    bool SearchSurface_exec(const char *name, bool (*proc)(P<T> &first, void *us_arg), void *_us_arg) {
        GT_TMP_NODE_SEARCH_SURFACE_EXEC(proc(CVP(T) search_node->zPthis, _us_arg), proc(CVP(T)((*in)->zPthis), _us_arg))
    }
    //收集该节点和它的所有子节点(返回节点列表)
    void Collect(sLIST<P<T>> *_subs_out) {
        this->Search_exec(0, GT_NodeCollect<P<T>>, _subs_out);
    }
    //获得该节点的子节点数(是否检索所有子节点里的子节点)
    int SubNum(bool all_sub = true) {
        if(all_sub) {
            int num = 0;
            this->Search_exec(0, GT_Add1Func<P<T> &, int>, &num);
            return num - 1;
        } else return zSub.size();
    }
    //确保子节点的父归属正确
    void SubParentEnsure() {
        GT_LIST_iterator(P<T>) in = zSub.begin(), en = zSub.end();
        for(; in != en; ++in) {
            (*in)->zParentNode = &(*in);
            (*in)->zParent = (T *)this;
        }
    }
    //打印该节点群组树里包含的所有子节点名称( 是否查找整棵树 )
    void PrintAllNodeName(bool full_tree = false) {
        T *search_node = (T *)this;
        if(full_tree) search_node = GetRoot().Begin();
        GN_Log("%s\n", (char *)search_node->aName);

        GT_LIST_iterator(P<T>) in = search_node->zSub.begin(), en = search_node->zSub.end();
        for(; in != en; ++in) (*in)->PrintAllNodeName(false);
    }
};
struct sNODE_ : public sNODE<sNODE_> {}; //★安全基础节点

template <class T>
//返回指定对象是否仅有一个引用
inline bool UsEq1(T &v) {
    return v.Us() == 1;
}

template <class T>
//返回指定对象是否少于或等于一个引用
inline bool UsLE1(T &v) {
    return v.Us() <= 1;
}

template <class T>
struct sNUMBER //编号分配器 帮你分配唯一的编号 由小到大分配
{
    T zMaxID; //当前的可用ID
    sLIST<T> zFreeIDs; //自由的可用ID

    sNUMBER() : zMaxID(0) {}
    ~sNUMBER() { Clear(); }
    void Clear() {
        if(zFreeIDs.size()) zFreeIDs.clear();
        zMaxID = 0;
    } //清除记录

    //获取唯一的号码
    T GetID() {
        T ret;
        if(zFreeIDs.empty()) {
            ret = zMaxID;
            zMaxID++;
        } else {
            ret = zFreeIDs.front();
            zFreeIDs.pop_front();
        }
        return ret;
    }
    //试探下一个号码 用来预先得知下个会分配什么号 但不实际分配
    T ProbeID() {
        T ret;
        if(zFreeIDs.empty()) ret = zMaxID;
        else ret = zFreeIDs.front();
        return ret;
    }
    //重设指定号码可用于下次再分配(号码) !一般在拥有指定ID的对象销毁时调用它以使这个ID可以重新分配 以免编号不断增长引起出轨
    void ReSetIDToData(T id) {
        if(id < zMaxID) zFreeIDs.push_back(id);
    }
    //设置分配的开始基数(基数)
    void SetStartNumber(T baseNum) { zMaxID = baseNum; }
};
}; //end namespace GN

#endif // GN_CONTAINERS_NODETREE_H
