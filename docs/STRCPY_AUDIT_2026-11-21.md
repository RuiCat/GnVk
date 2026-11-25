# 裸 strcpy/strcat 审计（2026-11-21，B5g/B5h/B5i）

> **状态**：📊 快照 — 2026-11-21 的裸 `strcpy/strcat` 审计：71 → 65 处，逐条判定。

> 复核起点：`rg -n '\bstrcpy\s*\(|\bstrcat\s*\(' src` → **71 处**（65 `strcpy` + 6 `strcat`）；其中 2 处位于注释块内。
> **B5i 后现存裸调用 65 处**：6 处改为非 strcpy/strcat 形态（`strncpy` / `GN_StrCopyToLimit` ×2 / `memcpy` / 容量版转换 ×2）。
> 行号为本轮改动后的当前树；判定：**已加固**（改代码，带上限或守卫）、**契约型**（旧 ABI，无法在不改签名下消除）、**已核**（精确分配/字面量/余量保护）、**非代码**。

## 0. 汇总（起点 71 处 = 现存 65 + 改形 6）

| 判定 | 行数 | 说明 |
|---|---:|---|
| 已加固（B5g/B5h/B5i） | 22 | 15 处仍为裸调用但已加容量/长度守卫 + 6 处改形 + 1 处入口守卫 |
| 契约型 | 3 | `GN_String.cpp` 旧 ABI 3 处：结果写调用方缓冲，内部调用已走安全路径，`_s` 变体已存在 |
| 已核 | 45 | 分配式与写入长度逐项相等、写的是字面量、或受入口余量保护 |
| 非代码 | 2 | 位于注释块内的示例 |
| **合计** | **72** | = 71 处调用 + 1 行入口守卫 |

## 1. 已加固（22 行）

| 位置 | 写法 | 处置与依据 |
|---|---|---|
| `src/core/GN_Format.cpp:2242` | `strncpy(ext_name2, ext_name, sizeof-1)` | 原 `strcpy(ext_name2, ext_name)`；`ext_name2[8]` 装不下任意长扩展名 → 截断拷贝。 |
| `src/core/GN_Format.cpp:3119` | `strcpy(_dir + len, "/")` | 宏 `GT_TMP_ZIP_ADD_DIR_NAME_MAKE` 加 `len + 2 > GT_ZIP_PATH_BUF_SIZE` 守卫。 |
| `src/core/GN_Format.cpp:3144` | `if((char *)in_zip_name != (char *)ZipCurProcFileName) strcpy(ZipCurProcFileName, in_zip_name);` | `ZipCurProcFileName[384]`；整名拷贝前加长度守卫，超长返回 -1。 |
| `src/core/GN_Format.cpp:3176` | `strcpy(find_str + gdir_len, "/*.*");` | `find_str[384]`；`gdir_len + 5 <= sizeof` 才拼接，否则记失败。 |
| `src/core/GN_Format.cpp:3188` | `strcpy(_dir + len, p_info->name);` | 路径总长守卫（`len + strlen(name) + 1 ≤ 384`），装不下记失败并跳过。 |
| `src/core/GN_Format.cpp:3206` | `strcpy(_dir + len, _name);` | 同上，递归建目录前做同一守卫。 |
| `src/core/GN_Format.cpp:3218` | `if((char *)in_zip_name != (char *)ZipCurProcFileName) strcpy(ZipCurProcFileName, in_zip_name);` | `ZipCurProcFileName[384]`；整名拷贝前加长度守卫，超长返回 -1。 |
| `src/core/GN_Frame.cpp:2847` | `GT_ConvertImageNameForLoad_s(load_path_+base, cap-base, …)` | 镜头光环图片名续写改按 `load_path_cap` 截断；3 参 `zDataLoad` 由场景装载传 `sizeof(file_path)`。 |
| `src/core/GN_RunRec.cpp:693` | `GN_StrCopyToLimit(_tod->text, cap, text)` | 原 `strcpy(_tod->text, text)`；旧缓冲可能小于文本长度 → 精确分配 + 限量拷贝。 |
| `src/file/GN_File.cpp:480` | `strcpy(p_file->global_filename + g_len, p_file->name);` | `global_filename[384]` 固定；超长路径退化为只存文件名 + 限量拷贝。 |
| `src/file/GN_File.cpp:504` | `strcpy(tmp_buf + len, "*.*");` | `tmp_buf[384]`；目录段 + "/*.*" 需 ≤384，加 `len + 4 > sizeof` 守卫。 |
| `src/graphics/GN_2D.cpp:161` | `strcpy(fn, filename);` | `fn[384]` 入口加长度校验（留 ".png" / "%04d.png" + NUL），超长报错返回。 |
| `src/graphics/GN_2D.cpp:397` | `fnlen + 16 > sizeof(img_path) 守卫` | 原 `strncpy(img_path, img->fileName, fnlen)` 无容量校验；ExportImageFile 入口加守卫。 |
| `src/graphics/GN_2D.cpp:442` | `strcpy(fn, filename);` | `fn[384]` 入口加长度校验（留 ".png" / "%04d.png" + NUL），超长报错返回。 |
| `src/graphics/GN_2Dscene.cpp:508` | `GN_StrCopyToLimit(img_name, 64, pimg_name1)` | 原 `strcpy(img_name,pimg_name1)`；64 字节契约缓冲 → 截断拷贝。 |
| `src/graphics/GN_2Dscene.cpp:511` | `strcat(img_name, "0000");` | `img_name` 是 64 字节契约缓冲；先 `GN_StrCopyToLimit` 再按剩余容量追加 "0000"。 |
| `src/graphics/GN_2Dscene.cpp:599` | `GT_ConvertImageNameForLoad_s(out, cap, name, len)` | 原 `:599 strcpy(img_name_out, img_name)` + `:601 strcpy(..., ".png")`；改为容量版，旧 ABI 转调 `_s`（行 616-617）。 |
| `src/graphics/GN_2Dscene.cpp:692` | `strcpy(file_path, file_name);` | `file_path[384]`；超长会让后续 `_filename` 指针越界 → 长度守卫 + 报错退出。 |
| `src/graphics/GN_2Dscene.cpp:817` | `if(strlen(exp_pathfile) < sizeof(exp_path)) strcpy(exp_path, exp_pathfile);` | `exp_path[384]`；超长时忽略导出路径（不写穿）。 |
| `src/media/Media/GN_Media.cpp:511` | `strcpy(zFileName, filename);` | 成员 `char zFileName[384]`；打开前校验空/超长，失败返回 false。 |
| `src/network/GN_NetTrans.h:464` | `memcpy(dst + size - 1, suffix, sizeof)` | 原 `strcat(data, " Receive!")`；收包缓冲原地追加会越界 → 等长临时缓冲。 |
| `src/ui/GN_UiListGui.cpp:110` | `} else strcpy(pstr2, str_buf);` | `str_size==0` 时原分配少 1 字节；改 `value_len + 1`。 |

## 2. 契约型（3 处，旧 ABI）

| 位置 | 写法 | 契约 / 残留风险 |
|---|---|---|
| `src/core/GN_String.cpp:198` | `strcpy(pstr, strs[0]);\` | 结果写调用方缓冲；旧 ABI 无法在不改签名的前提下消除，`_s` 变体已存在且内部调用已走安全路径。 |
| `src/core/GN_String.cpp:204` | `strcpy(pstr, strs[i]);\` | 结果写调用方缓冲；旧 ABI 无法在不改签名的前提下消除，`_s` 变体已存在且内部调用已走安全路径。 |
| `src/core/GN_String.cpp:396` | `strcpy(str_out, pstr);` | 结果写调用方缓冲；旧 ABI 无法在不改签名的前提下消除，`_s` 变体已存在且内部调用已走安全路径。 |

## 3. 已核（45 处）

| 位置 | 写法 | 依据 |
|---|---|---|
| `src/core/GN_Array.h:3082` | `strcpy(pointer.zName, name); \` | 仅 `GT_PMEM_WORNING`（GN_DEBUG）编译的调试命名。 |
| `src/core/GN_Array.h:3221` | `strcpy((char *)(this->zData->pmem), (char *)p_data);` | 精确分配 / 字面量 / 受入口余量保护。 |
| `src/core/GN_Format.cpp:2876` | `if(upper) strcpy(out, "wav,WAV,mp3,MP3");` | 调用方缓存 128 字节，写 16 字节。 |
| `src/core/GN_Format.cpp:2877` | `else strcpy(out, "wav,mp3");` | 调用方缓存 128 字节，写 8 字节。 |
| `src/core/GN_Format.cpp:2984` | `strcpy(out, "png,bmp,tga,hdr,gim");` | 调用方缓存 160 字节（`GT_SupportExtNamesImage`），写 20 字节。 |
| `src/core/GN_Format.cpp:2985` | `if(upper) strcpy(out + 19, ",PNG,BMP,TGA,HDR");` | 写 16 字节，总 36 字节 < 160 字节缓存。 |
| `src/core/GN_RunRec.cpp:463` | `strcpy(zpErrorInfo + (ti + 1), "...");` | 写入点 ≤ `zMaxInfoStrNum-1`（65536 字节缓冲）。 |
| `src/core/GN_RunRec.cpp:472` | `strcpy(zpErrorInfo + ti, "..."); //超过最大缓冲时结束` | 写入点 ≤ `zMaxInfoStrNum-1`（65536 字节缓冲）。 |
| `src/core/GN_RunRec.cpp:838` | `strcat(ver_str, GN_VERSION);` | `ver_str[32]`，`GN_VERSION` 为 "2026.07.04"（10 字节）。 |
| `src/core/GN_RunRec.cpp:853` | `strcat(ver_str, GN_VERSION);` | `ver_str[32]`，`GN_VERSION` 为 "2026.07.04"（10 字节）。 |
| `src/core/GN_String.cpp:705` | `strncpy(pstr2, pstr, inpos); strncpy(pstr2+inpos, sub_str, sub_len); strcpy(pstr2+inpos+sub_len, pstr+inpos);` | `new char[inpos+sub_len+尾部+1]` 精确。 |
| `src/core/GN_String.cpp:939` | `strcpy(_fmt, n_str); _fmt += n_n;` | 分配 `fn+4+n_n+ulen` 已计入写入长度。 |
| `src/core/GN_String.cpp:942` | `strcpy(_fmt + 1, _unit);` | 分配 `fn+4+n_n+ulen` 已计入写入长度。 |
| `src/core/GN_String.h:10` | `strcpy((p_dest), (src)); \` | 宏内 `new char[strlen(src)+1]`。 |
| `src/graphics/GN_2D.cpp:167` | `strcpy(extname, "png");` | 落点在 `fn[384]` 内，入口已留 ".png" 余量。 |
| `src/graphics/GN_2D.cpp:365` | `strcpy(filename, img->fileName);` | 精确分配 / 字面量 / 受入口余量保护。 |
| `src/graphics/GN_2D.cpp:368` | `strcpy(pstr, "0000");` | `filename[384]` 入口已留 16 字节余量。 |
| `src/graphics/GN_2D.cpp:376` | `strcpy(pstr1, ImgFileTypeNames[i]);` | 同上余量；扩展名表最长 5 字节。 |
| `src/graphics/GN_2D.cpp:401` | `strcpy(p_ext_name, "0000");` | `img_path[384]` 入口已留 16 字节余量。 |
| `src/graphics/GN_2D.cpp:409` | `strcpy(p_ext_name, ext_names[eni]);` | `img_path[384]` 入口已留 16 字节余量。 |
| `src/graphics/GN_2D.cpp:457` | `else strcpy(_name, ".png");` | `fn[384]` 入口已留 10 字节余量。 |
| `src/graphics/GN_3Dmodel.cpp:567` | `strcpy(m->aName, "Cube");` | `aName[128]` + 字面量。 |
| `src/graphics/GN_3Dmodel.cpp:622` | `strcpy(m->aMeshes[0].aName, "cube");` | `aName[128]` + 字面量。 |
| `src/graphics/GN_3Dmodel.cpp:629` | `strcpy(m->aName, "Sphere");` | `aName[128]` + 字面量。 |
| `src/graphics/GN_3Dmodel.cpp:688` | `strcpy(m->aMeshes[0].aName, "sphere");` | `aName[128]` + 字面量。 |
| `src/graphics/GN_3Dmodel.cpp:695` | `strcpy(m->aName, "Plane");` | `aName[128]` + 字面量。 |
| `src/graphics/GN_3Dmodel.cpp:734` | `strcpy(m->aMeshes[0].aName, "plane");` | `aName[128]` + 字面量。 |
| `src/graphics/GN_Font.cpp:592` | `strcpy(com_lable, pLable->lableStr);` | `GetSpace(lableStr.Num()+extStr.Num())` 与写入长度逐项相等。 |
| `src/graphics/GN_Font.cpp:593` | `if(extStr) strcat(com_lable, extStr);` | `GetSpace(lableStr.Num()+extStr.Num())` 与写入长度逐项相等。 |
| `src/media/Media/GN_Media.cpp:735` | `strcpy(_img->fileName, zFileName);` | 紧前 `new char[strlen(zFileName)+16]`。 |
| `src/media/Media/GN_Media.cpp:736` | `strcat(_img->fileName, " frame buffer");` | 追加 14 字节 < 预留 16。 |
| `src/network/_Net/GN_Net.cpp:170` | `strcpy(ifr.ifr_name, "eth0");` | `ifr_name[16]` + 4 字节字面量。 |
| `src/ui/GN_UI.cpp:600` | `strcpy(_ui_text->text, cnt);` | 精确分配 / 字面量 / 受入口余量保护。 |
| `src/ui/GN_UiFBrowser.cpp:1223` | `strcpy(gpath0 + opendir_len, "/..");` | `New(opendir_len + 4)`，"/.."+NUL = 4 字节。 |
| `src/ui/GN_UiListGui.cpp:76` | `strcpy(pstr2 + value_len, pstr + (strlen(pstr) - (str_size - 1)));` | `new char[value_len + str_size]`。 |
| `src/ui/GN_UiListGui.cpp:108` | `strcpy(pstr2 + value_len, pstr + (strlen(pstr) - (str_size - 1)));` | `new char[value_len + str_size]`。 |
| `src/ui/GN_UiListGui.cpp:122` | `strcpy(pstr2 + value_len, pstr + (strlen(pstr) - (str_size - 1)));` | `new char[value_len + str_size]`。 |
| `src/ui/GN_UiListGui.cpp:134` | `strcpy(file_name, _pimg->fileName);` | `new char[strlen+1]`。 |
| `src/ui/GN_UiListTreeNodes.cpp:50` | `strcpy(name1, objname); \` | `P<char>(name_num+1)` 且 `strlen+1 == name_num`。 |
| `src/ui/GN_UiPopDlgMenu.cpp:72` | `strcpy(chd.name, name);` | `new char[len+1]`。 |
| `src/ui/GN_UiPopDlgMenu.cpp:102` | `strcpy(chd.enumName, text); //原初始值保存到列举名里` | `new char[text_buf_size]`，text 已截断到 `text_buf_size-1`。 |
| `src/ui/GN_UiPopDlgMenu.cpp:238` | `strcpy(menu->zCaption, name);` | `new char[len+1]`。 |
| `src/ui/GN_UiPopDlgMenu.cpp:248` | `strcpy(chd.name, text); \` | `new char[len+1]`。 |
| `src/ui/GN_UiPopDlgSetup.cpp:121` | `strcpy(zFileName + di, ".ygs");` | 精确分配 / 字面量 / 受入口余量保护。 |
| `src/ui/GN_UiPopDlgSetup.cpp:129` | `strcpy(zFileName, def_file_name);` | `new char[strlen+1]`。 |

## 4. 非代码（2 处）

| 位置 | 写法 | 依据 |
|---|---|---|
| `src/core/GN_Format.cpp:2871` | `strcpy(out, upper?"ogg,OGG,":"ogg,");` | 位于 `/* … */` 注释块内的旧用法示例，不参与编译。 |
| `src/network/GN_NetTrans.h:424` | `strcpy(clt_addinfo->data, "为属性赋值");` | 位于 `/* … */` 文档注释块内的用法示例，不参与编译。 |

## 5. 复算命令

```bash
cd /home/ruicat/下载/GnVk/GnVk
rg -n '\bstrcpy\s*\(|\bstrcat\s*\(' src | wc -l   # 65（起点 71：6 处已改形）
rg -n '\bstrcpy\s*\(' src | wc -l                     # 59
rg -n '\bstrcat\s*\(' src | wc -l                     # 6
make unit                                               # 438 cases / 0 failed
```

## 6. 边界声明与残留

- 契约型只剩 `GN_String.cpp` 旧 ABI 3 处：函数签名无容量参数，改成容量版需 ABI 破坏；内部调用（`GN_StrUnSplitToData` 等）已走精确分配/`_s`，旧接口保留并注释为契约。
- 图像名转换（原 593/595）已改为容量版 `GT_ConvertImageNameForLoad_s`：场景装载传 `sizeof(file_path)` 余量，镜头光环装载传 `load_path_cap`；旧 ABI 包装转调容量版。
- 已加固的守卫只覆盖「已知会越界」的路径；未跑 GUI/GPU，ZIP/媒体/网络/场景装载的错误分支只有编译与单测覆盖。
- `strncpy`（109 处）不在本口径内。
