// GN_UiListTreeNodes.cpp — 树节点操作与 cTREE_VIEW 序列化实现（从 GN_UiListTree.cpp 拆出，纯定义搬家）
#include "GN.h"
#include "GN_Def.h"
#include "GN_UiListTree.h"
// 判断指定节点是否含展开标记（inline，两份 TU 同一定义，ODR 合法）。
inline uint8 GT_HasUnfold(P<char> &objname) {
    return objname[objname.Num() - 1] & 1;
}
P<GN::sNODE_> GNu::cTREE_NODE_P_OPTR::GetNodeAndSeekNext(short &level_n) {
    if(zCurLevelBk) {
        if(!zCurLevel->parent) return P<GN::sNODE_>();
    } else zCurLevelBk = zBackupLevels(zNextBk); //首个绘制节点

    level_n = zCurLevel->lid;
    P<GN::sNODE_> ret = zCurLevel->sobj;
    zSeekNextNode();
    return ret;
}
short GNu::cTREE_NODE_P_OPTR::GetNodeOnLevel() {
    return zCurLevel->lid;
}
void GNu::cTREE_NODE_P_OPTR::Init(P<GN::sNODE_> root, void (*comment_func)(GN::sNODE_ *, P<GN::sNODE_>)) {
    parent = 0;
    lid = 0;
    sobj = root;
    zCurLevel = this;
    zCurShowStartNid = 0;
    aCommentFunc = comment_func;
}
void GNu::cTREE_NODE_P_OPTR::Clear() {
    zNextBk.Delete();
    zNextBk2.Delete();
    zMsLastNode.Delete();
    next.Delete();
    sobj.Delete();
}
bool GNu::cTREE_NODE_P_OPTR::HasUnfold(P_ g) {
    if((CVP(GN::sNODE_) g)->zSub.empty()) return 0;
    P<char> &objname = (CVP(GN::sNODE_) g)->aName;
    return (objname[objname.Num() - 1] & 1);
}
void GNu::cTREE_NODE_P_OPTR::UnfoldTag(P_ g, uint8 on) {
    if((CVP(GN::sNODE_) g)->zSub.empty()) return;
    P<char> &objname = (CVP(GN::sNODE_) g)->aName;
#define GT_TMP_CHECK_AND_CREATE_OBJ_TAG_BUF                              \
    unsigned int name_num = objname.Num();                               \
    if((unsigned int)strlen(objname) + 1 == name_num) { /*创建标记空间*/ \
        P<char> name1(name_num + 1);                                     \
        /* 已核（B5f）：name1 = P<char>(name_num+1)，且 strlen(objname)+1 == name_num。 */ \
        strcpy(name1, objname);                                          \
        name1[name_num] = 0;                                             \
        objname = name1;                                                 \
    }
    GT_TMP_CHECK_AND_CREATE_OBJ_TAG_BUF
    if(on) objname[objname.Num() - 1] |= 1;
    else objname[objname.Num() - 1] &= 254; //~1
}
bool GNu::cTREE_NODE_P_OPTR::HasSelect(P_ node) {
    P<char> &objname = (CVP(GN::sNODE_) node)->aName;
    return (objname[objname.Num() - 1] & 2) ? true : false;
}
void GNu::cTREE_NODE_P_OPTR::SelectTag(P_ node, uint8 on, int8 mode) {
    P<char> &objname = (CVP(GN::sNODE_) node)->aName;
    GT_TMP_CHECK_AND_CREATE_OBJ_TAG_BUF

    if(on) {
        objname[objname.Num() - 1] |= 2;
        if(aSelectAddProc) aSelectAddProc(node, 1);
    } else {
        if(aSelectAddProc && (objname[objname.Num() - 1] & 2)) aSelectAddProc(node, 0);
        objname[objname.Num() - 1] &= 253; //~2
    }
    if(zMsLastNode == node) return;
    if(mode == -1) {
        if(zMsLastNode->zParent && GT_HasUnfold(zMsLastNode->zParent->aName)) { //段选
            sTREE_LEVEL_ *cur_level_bk = zBackupLevels(zNextBk2);
            if(zCurLevel->parent) { //不是末尾
                do {
                    if(zCurLevel->sobj == zMsLastNode) { //向下找到上次点选节点
                        zSeekLastNode();
                        while(zCurLevel->sobj != node) { //按原路返回不断设置标记到当前节点为止
                            SelectTag(zCurLevel->sobj, on, 1);
                            zSeekLastNode();
                        }
                        zSeekNextNode();
                        zMsLastNode = node;
                        return;
                    }
                } while(zSeekNextNode());
                GN_Swap(P<sTREE_LEVEL_>, next, zNextBk2);
                zCurLevel = cur_level_bk;
                cur_level_bk = zBackupLevels(zNextBk2);
            }
            P_ node2 = zMsLastNode;
            zSeekLastNode();
            zSeekLastNode();
            while(zCurLevel->sobj != node2) { //向上设置标记到上次点选节点为止
                SelectTag(zCurLevel->sobj, on, 1);
                zSeekLastNode();
            }
            GN_Swap(P<sTREE_LEVEL_>, next, zNextBk2);
            zCurLevel = cur_level_bk;
        }
    } else if(!mode && zMsLastNode) {
        P<char> &objname0 = zMsLastNode->aName;
        if(aSelectAddProc && (objname0[objname0.Num() - 1] & 2)) aSelectAddProc(zMsLastNode, 0);
        objname0[objname0.Num() - 1] &= 253; //~2
    }
    zMsLastNode = node;
}
GNu::sTREE_LEVEL_ *GNu::cTREE_NODE_P_OPTR::zBackupLevels(P<sTREE_LEVEL_> &next2) {
    if(zCurLevel == this) return this;
    else {
        if(!next2) {
            next2.New(1);
            next2->parent = this;
            next2->lid = 1;
        }
        sTREE_LEVEL_ *_lv = next, *_lv2 = next2;
        for(;;) {
            _lv2->sobj = _lv->sobj;
            _lv2->iter = _lv->iter;
            if(_lv == zCurLevel) break;
            if(!(_lv2->next)) {
                _lv2->next.New(1);
                _lv2->next->parent = _lv2;
                _lv2->next->lid = _lv2->lid + 1;
            }
            _lv2 = _lv2->next;
            _lv = _lv->next;
        }
        return _lv2;
    }
}
bool GNu::cTREE_NODE_P_OPTR::zSeekLastNode() {
    if(zCurLevel->parent) {
        if(zCurLevel->iter == zCurLevel->parent->sobj->zSub.begin()) {
            zCurLevel = zCurLevel->parent;
            return zCurLevel->parent ? true : false;
        } else {
            --(zCurLevel->iter);
            zCurLevel->sobj = *(zCurLevel->iter);
        }
    }
    while(HasUnfold(zCurLevel->sobj)) {
#define GT_TMP_TREE_NODE3D_TO_NEXT_LEVEL(ITER)     \
    if(!(zCurLevel->next)) {                       \
        zCurLevel->next.New(1);                    \
        zCurLevel->next->parent = zCurLevel;       \
        zCurLevel->next->lid = zCurLevel->lid + 1; \
    }                                              \
    zCurLevel = zCurLevel->next;                   \
    zCurLevel->iter = ITER;                        \
    zCurLevel->sobj = *(zCurLevel->iter)

        GT_TMP_TREE_NODE3D_TO_NEXT_LEVEL(--(zCurLevel->parent->sobj->zSub.end()));
    }
    return zCurLevel->parent ? true : false;
}
bool GNu::cTREE_NODE_P_OPTR::zSeekNextNode() {
    if(HasUnfold(zCurLevel->sobj)) {
        GT_TMP_TREE_NODE3D_TO_NEXT_LEVEL(zCurLevel->parent->sobj->zSub.begin());
        return true;
    } else {
        while(zCurLevel->parent) {
            ++(zCurLevel->iter);
            if(zCurLevel->iter == zCurLevel->parent->sobj->zSub.end()) zCurLevel = zCurLevel->parent;
            else {
                zCurLevel->sobj = *(zCurLevel->iter);
                return true;
            }
        }
        return false;
    }
}
void GNu::cTREE_NODE_P_OPTR::zSeekLastNode(int count) {
    while(count) {
        if(zSeekLastNode()) --count;
        else break;
    }
}
void GNu::cTREE_NODE_P_OPTR::zSeekNextNode(int count) {
    while(count) {
        if(zSeekNextNode()) --count;
        else break;
    }
}
int GNu::cTREE_NODE_P_OPTR::CanDrawLastNodeNum(int num) { //GN_RunValueCheck2(num, 31);
    if(zCurLevelBk) {
        GN_Swap(P<sTREE_LEVEL_>, next, zNextBk); //先恢复检索层级
        zCurLevel = zCurLevelBk;
    }
    zCurLevelBk = zBackupLevels(zNextBk); //再备份检索层级

    if(zCurLevel->parent) {
        int i = 1;
        for(; i < num && zSeekLastNode(); ++i);
        return i;
    }
    return 0;
}
int GNu::cTREE_NODE_P_OPTR::CanDrawNextNodeNum(int num) {
    if(zCurLevel->parent) {
        int i = 1;
        for(; i < num && zSeekNextNode(); ++i); //GN_RunValueCheck2(i, 32);
        return i;
    }
    return 0;
}
void GNu::cTREE_NODE_P_OPTR::OnUpdateBegin(unsigned int updatatime) {
    GNu::sTREE_LEVEL_ *p_level = zCurLevel->parent;
    if(p_level) { //检测并缝好树在外面被剪断删除的节点
        GN::sNODE_ *gp = p_level->sobj;
        if(gp == (GN::sNODE_ *)(this->sobj) || gp->HasParent(this->sobj)) {
            if(!zCurLevel->sobj->zParent) { //仅断开了与上一级的链接
                if(p_level->sobj->zSub.size()) {
                    zCurLevel->iter = p_level->sobj->zSub.begin();
                    zCurLevel->sobj = *(zCurLevel->iter);
                } else zCurLevel = zCurLevel->parent;
            }
        } else { //已经断开与主节点链接了
            zCurLevel = this;
            zSeekNextNode(zCurShowStartNid);
        }
    }
    zCurLevelBk = 0;
}
void GNu::cTREE_NODE_P_OPTR::OnUpdateEnd() { //GN_RunValueCheck2(zCurShowStartNid, 37); GN_RunValueCheck2(zCurLevel->sobj.Begin(), 36); GN_RunValueCheck2(zCurLevel->parent, 35);
    if(zCurLevelBk) {
        GN_Swap(P<sTREE_LEVEL_>, next, zNextBk);
        zCurLevel = zCurLevelBk;
    }
}
P<GNu::cTREE_NODE_OPTR> GNu::cTREE_NODE_P_OPTR::OnCloneOut() {
    P<GNu::cTREE_NODE_P_OPTR> optr(1);
    GNu::cTREE_NODE_P_OPTR *_optr = optr;
    _optr->parent = 0;
    _optr->lid = 0;
    _optr->sobj = this->sobj;
    _optr->iter = this->iter;
    _optr->zCurShowStartNid = this->zCurShowStartNid;
    _optr->zCurLevelBk = 0;
    _optr->zCurLevel = this->zBackupLevels(_optr->next);
    if(_optr->zCurLevel == this) _optr->zCurLevel = _optr;
    if(_optr->next) _optr->next->parent = _optr;
    return optr;
}
void GNu::cTREE_NODE_P_OPTR::OnReArranged(int id, float st_nid, unsigned int max_vnn) {
    int seek_n = (int)st_nid - zCurShowStartNid;
    if(seek_n) {
        if(seek_n < 0) zSeekLastNode(-seek_n);
        else zSeekNextNode(seek_n);
    }
    zCurShowStartNid = (int)st_nid;
}
/*static*/ void GT_TreeNodedDrawCall(int x, int y, int w, int h, unsigned int color, unsigned int zid, unsigned int vid, bool has_focus, void *arg) {
    GNu::cTREE_VIEW *sl = (GNu::cTREE_VIEW *)arg; /*模板代码:树节点绘制处理 ----------------------------------------------*/
    GNu::cTREE_NODE_P_OPTR *optr = (GNu::cTREE_NODE_P_OPTR *)((GNu::cTREE_NODE_OPTR *)(sl->GetNodeOptr()));
    short lv;
    P<GN::sNODE_> node = optr->GetNodeAndSeekNext(lv);
    GN::sNODE_ *_node = node;
    if(_node) {
        GNg::cTEXT_ART *_text_out = sl->GetTextStyle();
        short fw = _text_out->GetFontWidth();
        int ofs = fw + sl->aSpacing * lv;
        fw <<= 1;
        GNg::sARRANGE_FMT &tfmt = _text_out->zParame;
        GNg::sIMAGE *_icon = sl->GetIcons(), *_ufimg = sl->aUnfoldIcon;
        if(_node->zSub.size()) {
            unsigned short tid = GT_HasUnfold(_node->aName);
            if(has_focus && GN::MouseI[GM_LEFT]) { //点选展开或缩回节点
                int lx = tfmt.erect_print ? GNg::ViewCur->mouseMY - (ofs + y) : GNg::ViewCur->mouseMX - (ofs + x);
                if(lx > 0 && lx < _ufimg->height) {
                    optr->UnfoldTag(node, !tid);
                    if(tid) sl->AllSelect(0, node); //缩回事取消该节点里的所有选中标记
                    else sl->RefreshVisualNode(vid + 1, _node->zSub.size()); //通知刷新展开的节点弹出动画
                    has_focus = false;
                }
            } //绘制展开图标
            unsigned int arrw_color = sl->aAffectColor ? GNg_MultiplyColorRatio(sl->c, color) : sl->c;
            if(tfmt.erect_print) {
                if(tid && _ufimg->texnum == 1) {
                    int uf_ofs = ((_ufimg->height - _ufimg->width) >> 1), ufw = _ufimg->width;
                    if(sl->IsOverturnArrange()) {
                        uf_ofs += _ufimg->width;
                        ufw = -ufw;
                    }
                    GNg::ImageDraw(_ufimg, (int)((float)(w - _ufimg->height) * tfmt.near_pointU) + x + uf_ofs, ofs + y, ufw, 0, arrw_color, _ufimg->texnum == 1 ? 0 : tid);
                } else GNg::ImageDrawRot(_ufimg, (int)((float)(w - _ufimg->height) * tfmt.near_pointU) + x + (_ufimg->height >> 1), ofs + y + (_ufimg->height >> 1), 0, 0, 90.0f, 0.5f, 0.5f, arrw_color);
            } else {
                if(tid && _ufimg->texnum == 1) {
                    GNg::ImageDrawRot(_ufimg, (ofs + x) + (_ufimg->width >> 1), (int)((float)(h - _ufimg->height) * tfmt.near_pointV) + y + (_ufimg->height >> 1), 0, 0, 90.0f, 0.5f, 0.5f, arrw_color);
                } else GNg::ImageDraw(_ufimg, ofs + x, (int)((float)(h - _ufimg->height) * tfmt.near_pointV) + y, 0, 0, arrw_color, tid);
            }
            if(sl->aMergeIcon) {
                if(_icon) {
                    ofs += _icon->width + fw;
                    _icon = 0;
                }
            } else ofs += _ufimg->width + fw;
        } else if(!sl->aMergeIcon)
            ofs += _ufimg->width + fw;
        if(tfmt.erect_print) {
            if(_icon) { //绘制节点图标
                uint8 icid = *((uint8 *)_node + optr->aNodeIconIdAddrOffset);
                GNg::ImageDraw(_icon, (int)((float)(w - _icon->width) * tfmt.near_pointU) + x, ofs + y, 0, 0, color, icid >= (uint8)_icon->texnum ? 0 : icid);
                ofs += _icon->width + fw;
            }
            if(optr->aDrawNodeAddProc) ofs += optr->aDrawNodeAddProc(_node, x, ofs + y, w, h, color, has_focus, arg) + fw;
            y += ofs;
            h -= ofs;
        } else {
            if(_icon) { //绘制节点图标
                uint8 icid = *((uint8 *)_node + optr->aNodeIconIdAddrOffset);
                GNg::ImageDraw(_icon, ofs + x, (int)((float)(h - _icon->height) * tfmt.near_pointV) + y, 0, 0, color, icid >= (uint8)_icon->texnum ? 0 : icid);
                ofs += _icon->width + fw;
            }
            if(optr->aDrawNodeAddProc) ofs += optr->aDrawNodeAddProc(_node, ofs + x, y, w, h, color, has_focus, arg) + fw;
            x += ofs;
            w -= ofs;
        }
        GT_SELECT_LIST_DrawText(_text_out, _node->aName);
        GT_SELECT_LIST_ProcAndDrawFocus(zSelectProc(node, zid, has_focus), GetFocusChangeColor(), FocusImg())
    }
}
static void GT_TreeNode_DrawCall(int x, int y, int w, int h, unsigned int color, unsigned int zid, unsigned int vid, bool has_focus, void *arg) {
    GNu::cTREE_VIEW *sl = (GNu::cTREE_VIEW *)arg; /*模板代码:树节点绘制处理 ----------------------------------------------*/
    GNu::cTREE_NODE__OPTR *optr = (GNu::cTREE_NODE__OPTR *)((GNu::cTREE_NODE_OPTR *)(sl->GetNodeOptr()));
    short lv;
    P_ node = optr->GetNodeAndSeekNext(lv);
    //	GN::sNODE_  *_node = node;
    if(node.zData) {
        GNg::cTEXT_ART *_text_out = sl->GetTextStyle();
        short fw = _text_out->GetFontWidth();
        int ofs = fw + sl->aSpacing * lv;
        fw <<= 1;
        GNg::sARRANGE_FMT &tfmt = _text_out->zParame;
        GNg::sIMAGE *_icon = sl->GetIcons(), *_ufimg = sl->aUnfoldIcon;
        if(optr->GetNodeSubs(node)->size()) {
            unsigned short tid = optr->HasUnfold(node);
            if(has_focus && GN::MouseI[GM_LEFT]) { //点选展开或缩回节点
                int lx = tfmt.erect_print ? GNg::ViewCur->mouseMY - (ofs + y) : GNg::ViewCur->mouseMX - (ofs + x);
                if(lx > 0 && lx < _ufimg->height) {
                    optr->UnfoldTag(node, !tid);
                    if(tid) sl->AllSelect(0, node); //缩回事取消该节点里的所有选中标记
                    else sl->RefreshVisualNode(vid + 1, optr->GetNodeSubs(node)->size()); //通知刷新展开的节点弹出动画
                    has_focus = false;
                }
            } //绘制展开图标
            unsigned int arrw_color = sl->aAffectColor ? GNg_MultiplyColorRatio(sl->c, color) : sl->c;
            if(tfmt.erect_print) {
                if(tid && _ufimg->texnum == 1) {
                    int uf_ofs = ((_ufimg->height - _ufimg->width) >> 1), ufw = _ufimg->width;
                    if(sl->IsOverturnArrange()) {
                        uf_ofs += _ufimg->width;
                        ufw = -ufw;
                    }
                    GNg::ImageDraw(_ufimg, (int)((float)(w - _ufimg->height) * tfmt.near_pointU) + x + uf_ofs, ofs + y, ufw, 0, arrw_color, _ufimg->texnum == 1 ? 0 : tid);
                } else GNg::ImageDrawRot(_ufimg, (int)((float)(w - _ufimg->height) * tfmt.near_pointU) + x + (_ufimg->height >> 1), ofs + y + (_ufimg->height >> 1), 0, 0, 90.0f, 0.5f, 0.5f, arrw_color);
            } else {
                if(tid && _ufimg->texnum == 1) {
                    GNg::ImageDrawRot(_ufimg, (ofs + x) + (_ufimg->width >> 1), (int)((float)(h - _ufimg->height) * tfmt.near_pointV) + y + (_ufimg->height >> 1), 0, 0, 90.0f, 0.5f, 0.5f, arrw_color);
                } else GNg::ImageDraw(_ufimg, ofs + x, (int)((float)(h - _ufimg->height) * tfmt.near_pointV) + y, 0, 0, arrw_color, tid);
            }
            if(sl->aMergeIcon) {
                if(_icon) {
                    ofs += _icon->width + fw;
                    _icon = 0;
                }
            } else ofs += _ufimg->width + fw;
        } else if(!sl->aMergeIcon)
            ofs += _ufimg->width + fw;
        if(tfmt.erect_print) {
            if(_icon) { //绘制节点图标
                uint8 icid = *((uint8 *)node.zData->pmem + optr->aNodeIconIdAddrOffset);
                GNg::ImageDraw(_icon, (int)((float)(w - _icon->width) * tfmt.near_pointU) + x, ofs + y, 0, 0, color, icid >= (uint8)_icon->texnum ? 0 : icid);
                ofs += _icon->width + fw;
            }
            if(optr->aDrawNodeAddProc) ofs += optr->aDrawNodeAddProc(node, x, ofs + y, w, h, color, has_focus, arg) + fw;
            y += ofs;
            h -= ofs;
        } else {
            if(_icon) { //绘制节点图标
                uint8 icid = *((uint8 *)node.zData->pmem + optr->aNodeIconIdAddrOffset);
                GNg::ImageDraw(_icon, ofs + x, (int)((float)(h - _icon->height) * tfmt.near_pointV) + y, 0, 0, color, icid >= (uint8)_icon->texnum ? 0 : icid);
                ofs += _icon->width + fw;
            }
            if(optr->aDrawNodeAddProc) ofs += optr->aDrawNodeAddProc(node, ofs + x, y, w, h, color, has_focus, arg) + fw;
            x += ofs;
            w -= ofs;
        }
        GT_SELECT_LIST_DrawText(_text_out, optr->GetNodeName(node));
        GT_SELECT_LIST_ProcAndDrawFocus(zSelectProc(node, zid, has_focus), GetFocusChangeColor(), FocusImg())
    }
}
void *GNu::cTREE_NODE__OPTR::GetDrawNodeFunc() {
    return (void *)GT_TreeNode_DrawCall;
}

static bool GT_NodeP_Search_exec(P<GN::sNODE_> &node, bool (*proc)(P_ &node, void *us_arg), void *_us_arg) {
    if(proc(node, _us_arg)) {
        GN::sLIST<P<GN::sNODE_>>::iterator in = node->zSub.begin(), en = node->zSub.end();
        for(; in != en; ++in) {
            if(GT_NodeP_Search_exec(*in, proc, _us_arg)) return true;
        }
        return false;
    } else return true;
}
static bool GT_NodeP_UnfoldSearch_exec(P<GN::sNODE_> &node, bool (*proc)(P_ &node, void *us_arg), void *_us_arg) {
    if(proc(node, _us_arg)) {
        if(GT_HasUnfold(node->aName)) {
            GN::sLIST<P<GN::sNODE_>>::iterator in = node->zSub.begin(), en = node->zSub.end();
            for(; in != en; ++in) {
                if(GT_NodeP_UnfoldSearch_exec(*in, proc, _us_arg)) return true;
            }
        }
        return false;
    } else return true;
}
bool GNu::cTREE_NODE_P_OPTR::Search_exec(P_ g, bool unfold, bool (*proc)(P_ &node, void *us_arg), void *_us_arg) {
    if(unfold) return GT_NodeP_UnfoldSearch_exec(g.Has() ? (CVP(GN::sNODE_) g) : sobj, proc, _us_arg);
    else return GT_NodeP_Search_exec(g.Has() ? (CVP(GN::sNODE_) g) : sobj, proc, _us_arg);
}
void GNu::cTREE_NODE_P_OPTR::MoveNodes(P_ pos, bool pos_back, GN::sLIST<P_> *nodes) {
    P<GN::sNODE_> pos0 = CVP(GN::sNODE_) pos;
    GN::sNODE_ *tg_node = pos0;
    GN::sLIST<P<GN::sNODE_>> nodes1;
    {
        P<GN::sNODE_> tmp;
        GN::sLIST<P_>::iterator in = nodes->begin(), en = nodes->end();
        for(; in != en; ++in) {
            tmp = CVP(GN::sNODE_)(*in);
            if(!tg_node->HasParent(tmp)) {
                aCommentFunc(0, tmp);
                nodes1.push_back(tmp);
            }
        }
    }
    bool need_reseek = zCurLevel->parent && !(zCurLevel->sobj->HasParent(this->sobj));
    GN::sLIST<P<GN::sNODE_>> nodes_bk;
    if(GT_HasUnfold(tg_node->aName) && tg_node->zSub.empty())
        ;
    else if(sobj == pos) {
        tg_node = sobj;
        nodes_bk.splice(nodes_bk.end(), tg_node->zSub);
    } else {
        if(pos_back && HasUnfold(pos)) {
            nodes_bk.splice(nodes_bk.end(), tg_node->zSub);
        } else {
            GN::sNODE_ *pos1 = tg_node;
            tg_node = tg_node->zParent;
            GN::sLIST<P<GN::sNODE_>>::iterator inn = tg_node->zSub.begin(), enn = tg_node->zSub.end();
            for(; inn != enn; ++inn) {
                if((GN::sNODE_ *)(*inn) == pos1) {
                    if(pos_back) ++inn;
                    if(inn != enn) nodes_bk.splice(nodes_bk.end(), tg_node->zSub, inn, enn);
                    break;
                }
            }
        }
    }
    GN::sLIST<P<GN::sNODE_>>::iterator itn = nodes1.begin(), etn = nodes1.end();
    for(; itn != etn; ++itn) aCommentFunc(tg_node, (*itn));

    tg_node->zSub.splice(tg_node->zSub.end(), nodes_bk);
    if(need_reseek) {
        zCurLevel = this;
        zSeekNextNode(zCurShowStartNid);
    }
}
char *GNu::cTREE_NODE_P_OPTR::GetNodeName(P_ &node) {
    return (CVP(GN::sNODE_) node)->aName;
}
P_ GNu::cTREE_NODE_P_OPTR::GetCurFristNode() {
    return zCurLevel->sobj;
}
void *GNu::cTREE_NODE_P_OPTR::GetDrawNodeFunc() {
    return (void *)GT_TreeNodedDrawCall;
}
int GNu::cTREE_NODE_P_OPTR::GetVisualNodeNum() {
    return TreeVisualNodeNumStatist(sobj);
}

/*************************************************************************************************************************************************************************************************/
uint8 *GNu::cTREE_VIEW::zDataSave(uint8 *wpos, cSHARE_DATA *share_objs) {
    wpos = cSELECT_LIST_TEXT::zDataSave(wpos, share_objs);
    P<GNg::sIMAGE> uf_icon;
    if(aUnfoldIcon != aDefaultUnfoldImg) uf_icon = aUnfoldIcon;
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), uf_icon);
    memcpy(wpos, &aSpacing, 2);
    wpos += 2;
    memcpy(wpos, &aMergeIcon, 1);
    wpos += 1;
    return wpos;
}
uint8 *GNu::cTREE_VIEW::zDataLoad(uint8 *rpos, cSHARE_DATA *share_objs) {
    rpos = cSELECT_LIST_TEXT::zDataLoad(rpos, share_objs);
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) aUnfoldIcon = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&aSpacing, rpos, 2);
    rpos += 2;
    memcpy(&aMergeIcon, rpos, 1);
    rpos += 1;
    zArrowAm.aFrame_ts.sx = (float)zTextOut->zParame.height / (float)zOffsetArrow->width;
    return rpos;
}
unsigned int GNu::cTREE_VIEW::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    GT_TMP_PACK_DATA_SAVE(1260, cTREE_VIEW)
    return wpos - data_out;
}
unsigned int GNu::cTREE_VIEW::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    GT_TMP_PACK_DATA_LOAD(cTREE_VIEW)
    if(!zNodeOptr) {
        zZoningNum = 0;
        zFFrames->zMax_num = 0;
    }
    return rpos - data;
}