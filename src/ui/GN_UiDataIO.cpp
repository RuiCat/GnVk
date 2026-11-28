// GN_UiDataIO.cpp — UI 场景/对象的序列化与共享对象读写（从 GN_UI.cpp 拆出，纯定义搬家）
#include "GN.h"
#include "GN_Def.h"
#include "GN_UI.h"
GN_API void GT_WriteFramesToFile(unsigned int *rc_pos, uint8 *buf, uint8 in_launc, GN::sLIST<GNg::cFRAME *> &list_frame, GNg::cFRAME_RC::sREF_OBJSW *_in_frame_objs, GNf::sFILE *fp);
GN_API uint8 *GT_ReadFrames(uint8 *rpos, P<GNg::cFRAME *> &p_frames, GNg::cFRAME_RC::sREF_OBJSR *_frame_ref_objs);

void GT_Scene2DLoadShareObjData(GT_sSCENE2D_SHARE_OBJS_R &ref_objs, unsigned int *data_pos, const char *file_name, char *fbuf, float &progress);
void GT_Scene2DSaveShareObjData(GT_sSCENE2D_SHARE_OBJS &share_objs, GN::sLIST<GNg::sIMAGE *> *, unsigned int *rc_pos /*[10]*/, const char *exp_pathfile, GNf::sFILE *fp);
uint8 *GT_Scene2DLoadObjNameTable(uint8 *rpos, char **pname_buf, P<char *> &name_table);
char *GT_Scene2DLoadFileHead(const char *func_name, const char *file_name, const char *ext_name1);
GNf::sFILE *GT_Scene2DSaveFileHead(const char *class_name, const char *filename, uint8 *file_head /*[4]*/, int name_pos, char *name_buf, P<char *> &name_table);

//读取UI组件数据(当前读取位置, 当前加入UI组件的组, 引用共享对象集)返回最后的读完位置
uint8 *GT_UiObjLoadData(uint8 *rpos, GN::sLIST<P<GNu::iUI_OBJECT>> *cur_group, GNu::cSHARE_DATA *_ref_objs) {
    unsigned int size = *((unsigned int *)rpos), utype = *((unsigned int *)(rpos + 5));
    if(utype == GNg::eOBJ_GROUP || utype == GNu::eUI_SCROLL_RECT) { //GN_Log("A:%d %d %d\n", size, (int)rpos[4], utype);
        GNu::cUI_GROUP *_ug;
        if(utype == GNg::eOBJ_GROUP) {
            P<GNu::cUI_GROUP> ug(1);
            cur_group->push_back(ug);
            _ug = ug;
        } else {
            P<GNu::cSCROLL_RECT> ug2(1);
            cur_group->push_back(ug2);
            _ug = ug2;
        }
        _ug->DataLoad(_ref_objs, rpos + 4);
        rpos += size;
        while(*((unsigned int *)rpos) != 0xffffffff) rpos = GT_UiObjLoadData(rpos, &(_ug->aSubs), _ref_objs);
        rpos += 4;
    } else { //GN_Log("a:%d %d %d\n", size, (int)(rpos[4]), utype);
        P<GNu::iUI_OBJECT> node = GNu::UiObjectCreate(utype); //GN_Log("b\n");
        if(!node || !node->DataLoad(_ref_objs, rpos + 4)) { //读取失败 没装插件或插件版本太老不支持
            P<GNu::sUI_IMAGE> ui_img(1); //指定一个小白图来占位
            ui_img->Init(0, 0, 28, 28, 0xffffffff);
            ui_img->bmode = 1;
            node = ui_img;
        } //GN_Log("c\n");
        cur_group->push_back(node); //GN_Log("d\n");
        rpos += size;
    } //GN_Log("e\n");
    return rpos;
}
//写入UI组件数据到文件(UI组件, 文件, 临时缓冲, 缓冲大小, 收集共享对象集)
void GT_UiObjSaveData(GNu::iUI_OBJECT *node, GNf::sFILE *_fp, uint8 *buf, unsigned int buf_size, GNu::cSHARE_DATA *_share_objs) {
    unsigned int size, utype = node->aType;
    if(utype == GNg::eOBJ_GROUP || utype == GNu::eUI_SCROLL_RECT) {
        size = node->DataSave(_share_objs, buf + 4) + 4; //GN_Log("A:%d %d\n", size,  utype);
        *((unsigned int *)buf) = size;
        GNf::Write((char *)buf, 1, size, _fp);
        GN::sLIST<P<GNu::iUI_OBJECT>>::iterator ito = ((GNu::cUI_GROUP *)node)->aSubs.begin(), eto = ((GNu::cUI_GROUP *)node)->aSubs.end();
        for(; ito != eto; ++ito) GT_UiObjSaveData(*ito, _fp, buf, buf_size, _share_objs);
        unsigned int endg = 0xffffffff;
        GNf::Write((char *)&endg, 1, 4, _fp); //写入结束标记
    } else {
        if(utype == GNu::eUI_TEXT_ART || utype == GNu::eUI_TEXT_IN || utype == GNu::eUI_PALETTE || utype >= GNu::eUI_TYPE_NUM) {
            size = node->DataSave(_share_objs, 0) + 4;
            if(size > buf_size) {
                uint8 *buf_ = new uint8[size];
                node->DataSave(_share_objs, buf_ + 4);
                *((unsigned int *)buf_) = size;
                GNf::Write((char *)buf_, 1, size, _fp);
                delete[] buf_;
                return;
            }
        }
        size = node->DataSave(_share_objs, buf + 4) + 4; //GN_Log("a:%d %d\n", size, utype);
        *((unsigned int *)buf) = size;
        GNf::Write((char *)buf, 1, size, _fp);
    }
}
bool GNu::cUI_SCENE::InitLoadToData(const char *file_name, float &progress) {
    progress = 0.0f;
    char *fbuf = GT_Scene2DLoadFileHead("cUI_SCENE::InitLoadToData()", file_name, "gui");
    this->Release(); //GN_Log("2\n");
    progress = 0.1f;
    uint8 *rpos = (uint8 *)fbuf + 3;
    (void)*rpos; // B9-3: version 未使用
    ++rpos;
    unsigned int *data_pos = (unsigned int *)rpos;
    rpos += (18 << 2); //GN_Log("3\n");
    rpos = GT_Scene2DLoadObjNameTable(rpos, &zObjNameBuf, zObjNameTable); //GN_Log("4\n");
    progress = 0.11f;
    if(*((unsigned int *)rpos) == 0 || *((unsigned int *)(rpos + 5)) != GNg::eOBJ_GROUP) {
        delete[] fbuf;
        GN_Log("读取 %s 失败 文件可能损坏!\n", file_name);
        return false;
    }
    uint8 *r_node_pos = rpos;
    GT_cUI_SHARE_DATA_R share_objs; //GN_Log("5\n");
    GT_Scene2DLoadShareObjData(share_objs, data_pos, file_name, fbuf, progress); //GN_Log("6\n");
    if(data_pos[17] & 0x0000ffff) {
        unsigned int fmi = 0, fmn = data_pos[17] & 0x0000ffff;
        share_objs.image_refs.New(fmn);
        GNg::sIMAGE **_pimgs = share_objs.frame_ref_objs.pimgs, **_image_refs = share_objs.image_refs;
        // 初始化为NULL，防止野指针
        for(fmi = 0; fmi < fmn; ++fmi) _image_refs[fmi] = nullptr;
        fmi = 0;
        _pimgs += share_objs.frame_ref_objs.pimgs.Num() - fmn;
        for(; fmi < fmn; ++fmi) { //复制出各个字体图
            if(!_pimgs[fmi]) continue; // 跳过加载失败的字体(如被移除的GB2312)
            P<GNg::sIMAGE> ft_img(1);
            *ft_img = *(_pimgs[fmi]);
            ft_img->cloneImg = false;
            _pimgs[fmi]->cloneImg = true;
            aFontMapList.push_back(ft_img);
            _image_refs[fmi] = ft_img;
        }
    } //GN_Log("7\n");
    GT_ReadFrames((uint8 *)fbuf + data_pos[16], share_objs.pframes_rc, &(share_objs.frame_ref_objs)); //GN_Log("8\n");//读取独立的正规动画图筐
    progress = 0.79f;
    unsigned int si = (data_pos[17] & 0xffff0000) >> 16, num;
    if(si < 0xffff) {
        delete(share_objs.pframes[si]);
        share_objs.pframes[si] = GNu::cSCROLL_LIST::CheckInitDefaultFrameAct(); //替换成系统默认的效果筐
    }
    rpos = (uint8 *)fbuf + data_pos[15]; /*跳转到读取各个其它自定义共享对象*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.sobj_refs.New(num);
        P<uint8> *_sobj_refs = share_objs.sobj_refs;
        for(si = 0; si < num; ++si) {
            UiShareObjLoad(_sobj_refs[si], rpos + 4);
            rpos += *((unsigned int *)rpos);
        }
    }
    progress = 0.795f;
    rpos = (uint8 *)fbuf + data_pos[14]; /*跳转到读取各个独立的声音对象*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.psounds.New(num);
        P<GNa::sSOUND> *_sounds = share_objs.psounds;
        for(si = 0; si < num; ++si) {
            if(rpos[0]) {
                _sounds[si].New(1);
                if(!GNa::SoundLoad((char *)rpos, _sounds[si])) _sounds[si].Delete();
            }
            rpos += strlen((char *)rpos) + 1;
        }
    } //GN_Log("9\n");
    progress = 0.798f;
    rpos = (uint8 *)fbuf + data_pos[13]; /*跳转到读取各个文本*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.text_refs.New(num);
        P<char> *_text_refs = share_objs.text_refs;
        for(si = 0; si < num; ++si) {
            if(rpos[0]) _text_refs[si]((char *)rpos, 0);
            rpos += strlen((char *)rpos) + 1;
        }
    }
    progress = 0.799f;
    rpos = (uint8 *)fbuf + data_pos[12]; /*跳转到读取各个文本数组*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.ptexts_s.New(num);
        P<P<char>> *_ptexts_s = share_objs.ptexts_s;
        for(si = 0; si < num; ++si) rpos = GN::ObjectRefIdLoad(rpos, _ptexts_s[si], share_objs.text_refs);
    } //GN_Log("10\n");
    rpos = (uint8 *)fbuf + data_pos[11]; /*跳转到读取各个文本式样*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        share_objs.text_style_refs.New(num);
        GNg::cTEXT_ART **_text_style_refs = share_objs.text_style_refs;
        for(si = 0; si < num; ++si) {
            P<GNg::cTEXT_ART> tstyle(1);
            rpos = tstyle->zDataLoad(rpos, share_objs.image_refs);
            aTextStyleList.push_back(tstyle);
            _text_style_refs[si] = tstyle;
        }
    }
    progress = 0.8f;
    rpos = (uint8 *)fbuf + data_pos[10]; /*跳转到读取各个内嵌UI子组件*/
    memcpy(&num, rpos, 4);
    rpos += 4;
    if(num) {
        GN::sLIST<P<GNu::iUI_OBJECT>> uobj_list;
        for(si = 0; si < num; ++si) rpos = GT_UiObjLoadData(rpos, &uobj_list, &share_objs);
        share_objs.puobjs.New(uobj_list);
    } //GN_Log("11\n");
    progress = 0.82f;
    rpos = r_node_pos;
    this->DataLoad(&share_objs, rpos + 4);
    rpos += *((unsigned int *)rpos); //GN_Log("12\n");		//跟节点
    {
        int loop_safety = 10000;
        while(*((unsigned int *)rpos) != 0xffffffff && loop_safety-- > 0) {
            rpos = GT_UiObjLoadData(rpos, &aSubs, &share_objs);
        }
    }
    //rpos += 4;
    delete[] fbuf; //GN_Log("13\n");
    num = share_objs.ppframes.Num();
    if(num) {
        GNg::cFRAME **_pframes = share_objs.pframes;
        P<GNg::cFRAME> *_ppframes = share_objs.ppframes;
        for(si = 0; si < num; ++si) {
            if(_ppframes[si]) delete _pframes[si]; //删除已转为安全指针的原指针数据
        }
    }
    progress = 1.0f;
    return true;
}
bool GNu::cUI_SCENE::SaveToFile(const char *filename, uint8 version, bool exp_img, cUI_GROUP *node) {
    uint8 file_head[4] = {'g', 'u', 'i', version};
    GNf::sFILE *fp = GT_Scene2DSaveFileHead("cUI_SCENE", filename, file_head, (18 << 2), zObjNameBuf, zObjNameTable);
    if(!fp) return false;

    GT_cUI_SHARE_DATA_W share_objs;
    if(aTextStyleList.size()) GN::ListSpliceList(share_objs.text_style_list, aTextStyleList);
    if(aFontMapList.size()) GN::ListSpliceList(share_objs.image_list, aFontMapList);
    if(!node) node = this;
    const unsigned int buf_size1 = sizeof(GNg::cFRAME) << 3;
    const unsigned int buf_size = buf_size1 + (sizeof(GNg::sAM_NODE) << 5);
    uint8 buf[buf_size];
    //GN_Log("s1\n");
    GT_UiObjSaveData(node, fp, buf, buf_size, &share_objs); //GN_Log("s2\n");
    unsigned int rc_pos[18];
    uint8 *wpos;
    rc_pos[10] = (unsigned int)GNf::Tell(fp);
    unsigned int num = share_objs.list_uobj.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个内嵌UI子组件
        GN::sLIST<P<GNu::iUI_OBJECT>>::iterator iu = share_objs.list_uobj.begin(), eu = share_objs.list_uobj.end();
        for(; iu != eu; ++iu) GT_UiObjSaveData(*iu, fp, buf, buf_size, &share_objs);
    } //GN_Log("s2_1\n");
    rc_pos[11] = (unsigned int)GNf::Tell(fp);
    num = share_objs.text_style_list.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个文本式样
        GN::sLIST<GNg::cTEXT_ART *>::iterator itx = share_objs.text_style_list.begin(), etx = share_objs.text_style_list.end();
        for(; itx != etx; ++itx) {
            wpos = (*itx)->zDataSave(buf, share_objs.image_list);
            GNf::Write((char *)buf, 1, wpos - buf, fp);
        }
    } //GN_Log("s2_2\n");
    rc_pos[12] = (unsigned int)GNf::Tell(fp);
    num = share_objs.list_texts.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个文本数组
        GN::sLIST<P<P<char>>>::iterator istrs = share_objs.list_texts.begin(), estrs = share_objs.list_texts.end();
        for(; istrs != estrs; ++istrs) {
            if(((*istrs).Num() + 1) << 2 > buf_size) {
                uint8 *buf_ = new uint8[((*istrs).Num() + 1) << 2];
                wpos = GN::ObjectRefIdSave(buf_, *istrs, share_objs.text_list);
                GNf::Write((char *)buf_, 1, wpos - buf_, fp);
                delete[] buf_;
            } else {
                wpos = GN::ObjectRefIdSave(buf, *istrs, share_objs.text_list);
                GNf::Write((char *)buf, 1, wpos - buf, fp);
            }
        }
    } //GN_Log("s2_3\n");
    rc_pos[13] = (unsigned int)GNf::Tell(fp);
    num = share_objs.text_list.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个文本
        GN::sLIST<P<char>>::iterator istr = share_objs.text_list.begin(), estr = share_objs.text_list.end();
        unsigned int num1;
        for(num = 0; istr != estr; ++istr) {
            num1 = (*istr).Num();
            if(num + num1 > buf_size) {
                if(num) {
                    GNf::Write((char *)buf, 1, num, fp);
                    num = 0;
                }
                if(num1 > buf_size) {
                    GNf::Write((*istr), 1, num1, fp);
                    continue;
                }
            }
            strncpy((char *)buf + num, (*istr), num1);
            num += num1;
        }
        if(num) GNf::Write((char *)buf, 1, num, fp);
    } //GN_Log("s2_4\n");
    rc_pos[14] = (unsigned int)GNf::Tell(fp);
    num = share_objs.list_sound.size();
    GNf::Write((char *)&num, 4, 1, fp);
    if(num) { //写入各个声音斋文件名
        GN::sLIST<P<GNa::sSOUND>>::iterator isu = share_objs.list_sound.begin(), esu = share_objs.list_sound.end();
        char exp_path[384], *_filename;
        if(exp_img) {
            strncpy(exp_path, filename, sizeof(exp_path) - 1);
            exp_path[sizeof(exp_path) - 1] = 0;
            _filename = GN_StrGetFileName(exp_path);
        }
        GNf::sFILE *fp1;
        int fsize;
        unsigned int num1;
        char *soud_filename, *fbuf;
        for(num = 0; isu != esu; ++isu) {
            soud_filename = GN_StrGetFileName((*isu)->fileName);
            if(exp_img) { //导出声音文件
                // B5f：_filename 指向 exp_path[384] 内部，余量 = 384 - 偏移，按上限截断。
                GN_StrCopyToLimit(_filename, (int)(sizeof(exp_path) - (_filename - exp_path)), soud_filename);
                fbuf = GNf::FileFullLoadToMemory((*isu)->fileName, &fsize);
                if(fbuf) {
                    fp1 = GNf::Open(exp_path, "wb");
                    if(fp1) {
                        GNf::Write(fbuf, 1, fsize, fp1);
                        GNf::Close(fp1);
                    }
                    delete[] fbuf;
                }
            }
            num1 = strlen(soud_filename) + 1; //写斋文件
            if(num + num1 > buf_size) {
                GNf::Write((char *)buf, 1, num, fp);
                num = 0;
            }
            strncpy((char *)buf + num, soud_filename, num1);
            num += num1;
        }
        if(num) GNf::Write((char *)buf, 1, num, fp);
    }
    rc_pos[15] = (unsigned int)GNf::Tell(fp);
    num = share_objs.sobj_type_list.size();
    GNf::Write((char *)&num, 4, 1, fp); //GN_Log("s2_5:%d\n", num);
    if(num) { //写入各个其它自定义共享对象
        GN::sLIST<unsigned int>::iterator iot = share_objs.sobj_type_list.begin(), eot = share_objs.sobj_type_list.end();
        GN::sLIST<P<uint8>>::iterator isob = share_objs.sobj_list.begin();
        for(; iot != eot; ++iot, ++isob) {
            num = UiShareObjSave(*iot, *isob, 0) + 4;
            if(num <= buf_size) {
                UiShareObjSave(*iot, *isob, buf + 4);
                *((unsigned int *)buf) = num;
                GNf::Write((char *)buf, 1, num, fp);
            } else {
                uint8 *buf_ = new uint8[num];
                UiShareObjSave(*iot, *isob, buf_ + 4);
                *((unsigned int *)buf_) = num;
                GNf::Write((char *)buf_, 1, num, fp);
                delete[] buf_;
            }
        }
    } //GN_Log("s3\n");
    GT_WriteFramesToFile(rc_pos + 16, buf, 0, share_objs.list_frame_rc, &(share_objs.in_frame_objs), fp); //GN_Log("s4\n"); //写入独立的正规动画图筐
    rc_pos[17] = share_objs.image_list.size(); //记录字图数
    GT_Scene2DSaveShareObjData(share_objs, &(share_objs.image_list), rc_pos, exp_img ? filename : 0, fp); //GN_Log("s6\n");
    if(share_objs.list_fileicon.size()) {
        GN::sLIST<GNg::cFRAME *>::iterator itf = share_objs.list_fileicon.begin(), etf = share_objs.list_fileicon.end();
        for(; itf != etf; ++itf) (*itf)->zPlay_obj = GNu::cFILE_BROWSER::GetIcon();
    }
    rc_pos[17] |= share_objs.scroll_act_frame_id << 16; //并入默认点中演变效果筐所在ID

    GNf::Seek(fp, 4, GNf::eSEEK_START); //跳到文件开始的第4个字节
    GNf::Write((char *)rc_pos, 4, 18, fp); //写入各种资源存放在文件的偏移位置信息
    GNf::Close(fp); //GN_Log("s7\n");
    return true;
}
//打印出UI组里的UI组件代码类名和名称(组, 空格跳过段, 当前组件ID, 当前历遍到的名)返回当前历遍到的名
static char **GT_PrintUiGroupNodeClassName(GNu::cUI_GROUP *node, char *space_seg, unsigned int &id, char **ppname) {
    unsigned short utype;
    GN::sLIST<P<GNu::iUI_OBJECT>>::iterator it = node->aSubs.begin(), et = node->aSubs.end();
    for(; it != et; ++it) {
        utype = (*it)->aType;
        GN_Log("%s%s    %s    %d\n", space_seg, *ppname, utype >= GNu::eUI_TYPE_NUM ? "?UNKNOWN" : GNu::UiObjCodeClassNames[utype], id);
        ++ppname;
        ++id;
        if(utype == GNg::eOBJ_GROUP || utype == GNu::eUI_SCROLL_RECT)
            ppname = GT_PrintUiGroupNodeClassName(CVP(GNu::cUI_GROUP)(*it), space_seg - 4, id, ppname);
    }
    return ppname;
}
void GNu::cUI_SCENE::PrintAllNodeClassName() {
    unsigned int id = 0;
    char **ppname = zObjNameTable;
    GN_Log("%s    %s    %d\n", *ppname, UiObjCodeClassNames[aType], id);
    ++ppname;
    ++id;
    char space_buf[2049];
    memset(space_buf, ' ', 2048);
    space_buf[2048] = 0;
    GT_PrintUiGroupNodeClassName(this, space_buf + 2044, id, ppname);
}
GN_API P<GNu::iUI_OBJECT> GNu::UiObjectSearch(P<iUI_OBJECT> &ug, unsigned int &id) {
    if(!id) return ug;
    --id;
    P<iUI_OBJECT> ret;
    if(ug->aType == GNg::eOBJ_GROUP || ug->aType == GNu::eUI_SCROLL_RECT) {
        cUI_GROUP *_ug = CVP(cUI_GROUP) ug;
        GN::sLIST<P<iUI_OBJECT>>::iterator it = _ug->aSubs.begin(), et = _ug->aSubs.end();
        for(; !ret && it != et; ++it) ret = UiObjectSearch(*it, id);
    }
    return ret;
}
bool GT_UiObjectIdSearch(GNu::iUI_OBJECT *uobj, GNu::iUI_OBJECT *tg, unsigned int &id) {
    GN::sLIST<P<GNu::iUI_OBJECT>>::iterator it = ((GNu::cUI_GROUP *)uobj)->aSubs.begin(), et = ((GNu::cUI_GROUP *)uobj)->aSubs.end();
    for(; it != et; ++it) {
        ++id;
        uobj = (*it);
        if(tg == uobj) return true;
        if(uobj->aType == GNg::eOBJ_GROUP || uobj->aType == GNu::eUI_SCROLL_RECT) {
            if(GT_UiObjectIdSearch(uobj, tg, id)) return true;
        }
    }
    return false;
}
GN_API unsigned int GNu::UiObjectIdSearch(iUI_OBJECT *ug, iUI_OBJECT *tg) {
    if(tg == ug) return 0;
    unsigned int id = 0;
    if(ug->aType == GNg::eOBJ_GROUP || ug->aType == GNu::eUI_SCROLL_RECT) GT_UiObjectIdSearch(ug, tg, id);
    return id;
}
GN_API unsigned short *GNu::UiObjectsTypeCollect(iUI_OBJECT *uobj, unsigned short *type) {
    *type = uobj->aType;
    ++type;
    if(uobj->aType == GNg::eOBJ_GROUP || uobj->aType == GNu::eUI_SCROLL_RECT) {
        GN::sLIST<P<iUI_OBJECT>>::iterator it = ((cUI_GROUP *)uobj)->aSubs.begin(), et = ((cUI_GROUP *)uobj)->aSubs.end();
        for(; it != et; ++it) type = UiObjectsTypeCollect(*it, type);
    }
    return type;
}
/*****************************************************************************************************************/
uint8 *GNu::cUI_NEAR_OBJECT::zNearAreaDataSave(uint8 *wpos) {
    *((unsigned int *)wpos) = aType;
    wpos += 4;
    memcpy(wpos, &x, 2);
    wpos += 2;
    memcpy(wpos, &y, 2);
    wpos += 2;
    memcpy(wpos, &w, 2);
    wpos += 2;
    memcpy(wpos, &h, 2);
    wpos += 2;
    memcpy(wpos, &c, 4);
    wpos += 4;
    memcpy(wpos, &nmin_x, 2);
    wpos += 2;
    memcpy(wpos, &nmin_y, 2);
    wpos += 2;
    memcpy(wpos, &ntype_x, 1);
    wpos += 1;
    memcpy(wpos, &ntype_y, 1);
    wpos += 1;
    return wpos;
}
uint8 *GNu::cUI_NEAR_OBJECT::zNearAreaDataLoad(uint8 *rpos) {
    aType = *((unsigned int *)rpos);
    rpos += 4;
    memcpy(&x, rpos, 2);
    rpos += 2;
    memcpy(&y, rpos, 2);
    rpos += 2;
    memcpy(&w, rpos, 2);
    rpos += 2;
    memcpy(&h, rpos, 2);
    rpos += 2;
    memcpy(&c, rpos, 4);
    rpos += 4;
    memcpy(&nmin_x, rpos, 2);
    rpos += 2;
    memcpy(&nmin_y, rpos, 2);
    rpos += 2;
    memcpy(&ntype_x, rpos, 1);
    rpos += 1;
    memcpy(&ntype_y, rpos, 1);
    rpos += 1;
    return rpos;
}
unsigned int GNu::cUI_GROUP::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    w = lw;
    h = lh;
    wpos = ((cUI_NEAR_OBJECT *)((GNg::sDRAW_IN_AREA *)this))->zNearAreaDataSave(wpos);
    memcpy(wpos, &aUsScissor, 1);
    wpos += 1;
    return wpos - data_out;
}
unsigned int GNu::cUI_GROUP::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = ((cUI_NEAR_OBJECT *)((GNg::sDRAW_IN_AREA *)this))->zNearAreaDataLoad(rpos);
    memcpy(&aUsScissor, rpos, 1);
    rpos += 1;
    lw = w;
    lh = h;
    return rpos - data;
}
unsigned int GNu::sUI_IMAGE::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    *((unsigned int *)wpos) = aType;
    wpos += 4;
    memcpy(wpos, &x, 2);
    wpos += 2;
    memcpy(wpos, &y, 2);
    wpos += 2;
    memcpy(wpos, &w, 2);
    wpos += 2;
    memcpy(wpos, &h, 2);
    wpos += 2;
    memcpy(wpos, &c, 4);
    wpos += 4;
    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), img);
    memcpy(wpos, &bmode, 2);
    wpos += 2;
    return wpos - data_out;
}
unsigned int GNu::sUI_IMAGE::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    aType = *((unsigned int *)rpos);
    rpos += 4;
    memcpy(&x, rpos, 2);
    rpos += 2;
    memcpy(&y, rpos, 2);
    rpos += 2;
    memcpy(&w, rpos, 2);
    rpos += 2;
    memcpy(&h, rpos, 2);
    rpos += 2;
    memcpy(&c, rpos, 4);
    rpos += 4;
    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) img = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&bmode, rpos, 2);
    rpos += 2;
    return rpos - data;
}
unsigned int GNu::cUI_IMAGE::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);

    unsigned int pointer_id;
    GT_TMP_OBJECT_WRITE_INDEX((*share_objs->pimage_list), img);
    memcpy(wpos, &bmode, 2);
    wpos += 2;
    return wpos - data_out;
}
unsigned int GNu::cUI_IMAGE::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);

    unsigned int pointer_id;
    memcpy(&pointer_id, rpos, 4);
    rpos += 4;
    if(pointer_id != 0xffffffff) img = (*(share_objs->pimage_refs))[pointer_id];
    memcpy(&bmode, rpos, 2);
    rpos += 2;
    return rpos - data;
}
unsigned int GNu::cUI_BACK_FRAME::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    unsigned int gsize = sUI_IMAGE::DataSave(share_objs, data_out);
    uint8 loc_buf[16];
    if(!data_out) data_out = loc_buf;
    else data_out += gsize;
    uint8 *wpos = data_out;
    memcpy(wpos, &aBackRepeat, 1);
    wpos += 1;
    memcpy(wpos, &aBackBlurLv, 1);
    wpos += 1;
    memcpy(wpos, &aZoom, 1);
    wpos += 1;
    memcpy(wpos, &aDragBarH, 1);
    wpos += 1;
    return (wpos - data_out) + gsize;
}
unsigned int GNu::cUI_BACK_FRAME::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    if(rpos[0] > 1) return 0;
    rpos += sUI_IMAGE::DataLoad(share_objs, rpos);
    memcpy(&aBackRepeat, rpos, 1);
    rpos += 1;
    memcpy(&aBackBlurLv, rpos, 1);
    rpos += 1;
    memcpy(&aZoom, rpos, 1);
    rpos += 1;
    memcpy(&aDragBarH, rpos, 1);
    rpos += 1;
    return rpos - data;
}
unsigned int GNu::sUI_FRAME_RC::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[640];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    *((unsigned int *)wpos) = aType;
    wpos += 4;
    wpos = zDataSave(wpos, &(((GT_cUI_SHARE_DATA_W *)share_objs)->in_frame_objs), 0);
    return wpos - data_out;
}
unsigned int GNu::sUI_FRAME_RC::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    aType = *((unsigned int *)rpos);
    rpos += 4;
    rpos = zDataLoad(rpos, &(((GT_cUI_SHARE_DATA_R *)share_objs)->frame_ref_objs));
    return rpos - data;
}
unsigned int GNu::cUI_FRAME_RC::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[640];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = zDataSave(wpos, &(((GT_cUI_SHARE_DATA_W *)share_objs)->in_frame_objs), 0);
    return wpos - data_out;
}
unsigned int GNu::cUI_FRAME_RC::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    rpos = zDataLoad(rpos, &(((GT_cUI_SHARE_DATA_R *)share_objs)->frame_ref_objs));
    return rpos - data;
}
unsigned int GNu::cUI_FRAME::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    return ((sUI_FRAME_RC *)this)->sUI_FRAME_RC::DataSave(share_objs, data_out);
}
unsigned int GNu::cUI_FRAME::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    return ((sUI_FRAME_RC *)this)->sUI_FRAME_RC::DataLoad(share_objs, data);
}
#define GT_TMP_UI_FLAUNCHER_DATA_SAVE_DEF(UCLASS)                                                                                    \
    unsigned int GNu::UCLASS::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {                                                   \
        uint8 loc_buf[640];                                                                                                          \
        if(!data_out) data_out = loc_buf;                                                                                            \
        uint8 *wpos = data_out;                                                                                                      \
        *wpos = 1;                                                                                                                   \
        ++wpos; /*版本*/                                                                                                             \
        *((unsigned int *)wpos) = aType;                                                                                             \
        wpos += 4;                                                                                                                   \
        wpos = zDataSave(wpos, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_frame, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_keep_obj); \
        return wpos - data_out;                                                                                                      \
    }                                                                                                                                \
    unsigned int GNu::UCLASS::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {                                                 \
        uint8 *rpos = (uint8 *)data;                                                                                                 \
        uint8 version = *rpos;                                                                                                       \
        ++rpos;                                                                                                                      \
        if(version > 1) return 0; /*版本*/                                                                                           \
        aType = *((unsigned int *)rpos);                                                                                             \
        rpos += 4;                                                                                                                   \
        rpos = zDataLoad(rpos, ((GT_cUI_SHARE_DATA_R *)share_objs)->pframes, ((GT_cUI_SHARE_DATA_R *)share_objs)->pkeep_objs);       \
        return rpos - data;                                                                                                          \
    }
GT_TMP_UI_FLAUNCHER_DATA_SAVE_DEF(cUI_FRAME_LAUNCHER)
GT_TMP_UI_FLAUNCHER_DATA_SAVE_DEF(cUI_FRAME_LAUNCHER2D)

unsigned int GNu::cUI_FRAME_LAUNCHER_FONT::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[640];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);
    wpos = zDataSave(wpos, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_frame, ((GT_cUI_SHARE_DATA_W *)share_objs)->list_keep_obj);
    return wpos - data_out;
}
unsigned int GNu::cUI_FRAME_LAUNCHER_FONT::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);
    rpos = zDataLoad(rpos, ((GT_cUI_SHARE_DATA_R *)share_objs)->pframes, ((GT_cUI_SHARE_DATA_R *)share_objs)->pkeep_objs);
    return rpos - data;
}
unsigned int GNu::cUI_TEXT_ART::DataSave(cSHARE_DATA *share_objs, uint8 *data_out) {
    uint8 loc_buf[65536 + 64];
    if(!data_out) data_out = loc_buf;
    uint8 *wpos = data_out;
    *wpos = 1;
    ++wpos; //版本
    wpos = zNearAreaDataSave(wpos);

    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_WRITE_INDEX(share_objs->text_style_list, textstyle)
    memcpy(wpos, &blendmode, 2);
    wpos += 2;
    memcpy(wpos, &offsetx, 2);
    wpos += 2;
    memcpy(wpos, &offsety, 2);
    wpos += 2;
    memcpy(wpos, &fw, 2);
    wpos += 2;
    memcpy(wpos, &fh, 2);
    wpos += 2;
    unsigned int tsize = 0;
    if(text) {
        tsize = strlen(text) + 1;
        memcpy(wpos, &tsize, 4);
        wpos += 4;
        memcpy(wpos, text, tsize);
        wpos += tsize;
    } else
        memcpy(wpos, &tsize, 4);
    wpos += 4;
    return wpos - data_out;
}
unsigned int GNu::cUI_TEXT_ART::DataLoad(cSHARE_DATA *share_objs, const uint8 *data) {
    uint8 *rpos = (uint8 *)data;
    uint8 version = *rpos;
    ++rpos;
    if(version > 1) return 0; //版本
    rpos = zNearAreaDataLoad(rpos);

    unsigned int pointer_id;
    GT_TMP_TEXTSTYLE_READ_INDEX(share_objs->text_style_refs, textstyle)
    if(!textstyle) textstyle = &GN::RunRec.aFont; // 字体缺失时用默认
    memcpy(&blendmode, rpos, 2);
    rpos += 2;
    memcpy(&offsetx, rpos, 2);
    rpos += 2;
    memcpy(&offsety, rpos, 2);
    rpos += 2;
    memcpy(&fw, rpos, 2);
    rpos += 2;
    memcpy(&fh, rpos, 2);
    rpos += 2;
    unsigned int tsize = *((unsigned int *)rpos);
    rpos += 4;
    GN_DeleteArray(text);
    if(tsize) {
        text = new char[tsize];
        memcpy(text, rpos, tsize);
        rpos += tsize;
    }
    return rpos - data;
}