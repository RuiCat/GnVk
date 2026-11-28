// GN_FormatAudio.cpp — 音频格式：WAV/MP3 读取与保存、音频流实现、音频文件插件链（原 GN_Format.cpp 拆分）。
#include "GN.h"
#include "GN_FormatInternal.h"
#include "GN_CoreContext.h" // B7-3：音频扩展名缓存收敛进 GNg::GN_Core
#ifdef _GN_FORMAT_H_
    /*-----------------------------------------------------------------------------------------------------------------------------------------*/
    GN_API bool GNa::CheckAudioFmtSupportFromFileName(const char *filename) {
        char *ext_name = GN_StrGetExtName(filename);
        if(ext_name) return FileAudio->CheckFmtSupportFromExtName(ext_name);
        return false;
    }
    // B7-3：扩展名缓存（原 static GT_SupportExtNamesAudio[128]）已并入 GNg::GN_Core.aAudioExtNames。
    GN_API char * GNa::GetSupportAudioFmtFileExtName(bool upper) {
        if(!GNg::GN_Core.aAudioExtNames[0]) FileAudio->GetSupportFmtFileExtName(GNg::GN_Core.aAudioExtNames, upper);
        return (char *)GNg::GN_Core.aAudioExtNames;
    }
    GN_API unsigned int GNa::LoadWavFileInfo(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize) {
#define GT_TMP_SET_WAV_INFO                              \
    *format = *(unsigned short *)(td + 20); /*格式*/     \
    *channels = *(unsigned short *)(td + 22); /*声道数*/ \
    *bit = *(unsigned short *)(td + 34); /*声道位*/      \
    *freq = *(unsigned int *)(td + 24); /*频率*/         \
    *size = *(unsigned int *)(td + 40); /*音频数据的大小*/

#define GT_TMP_LOAD_WAV_HEAD                                                                       \
    GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize);                                       \
    if(!fp) return 0; /*文件打开失败返回0*/                                                        \
    char td[44];                                                                                   \
    GNf::Read(td, 1, 44, fp); /*读取wav头信息共44个字节*/                                          \
    if(td[0] != 'R' || td[1] != 'I' || td[2] != 'F' || td[3] != 'F') { /*判断是否不是wav格式文件*/ \
        GNf::Close(fp);                                                                            \
        return 0;                                                                                  \
    }                                                                                              \
    GT_TMP_SET_WAV_INFO

        GT_TMP_LOAD_WAV_HEAD;
        GNf::Close(fp); //关闭文件
        return GNa::ComputePlayTime(*channels, *bit, *freq, *size); //计算返回音频总播放时间 单位毫秒
    }
    GN_API uint8 *GNa::LoadWavFileData(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize) {
        GT_TMP_LOAD_WAV_HEAD; //读取wav 文件头信息
        uint8 *data = new uint8[*size]; //分配数据内存空间
        GNf::Read((char *)data, 1, *size, fp); //一次过读取音频数据 !wav无压缩数据比较简单
        GNf::Close(fp); //关闭文件
        return data; //返回数据
    }
    GN_API bool GNa::SaveWavFile(char *file_name, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq, unsigned int size, uint8 *data) {
#define GT_TMP_SAVE_WAV_HEAD                                                                                                                      \
    GNf::sFILE *fp = GNf::Open((char *)file_name, "wb", GNf::eFILE_STD); /*创建并打开文件(路径文件名, 以写入二进制方式, 创建于标准本地磁盘文件)*/ \
    if(!fp) {                                                                                                                                     \
        GN_Log(GT_ERR_FileCreateFail, "wav", file_name);                                                                                          \
        return 0;                                                                                                                                 \
    } /*文件创建失败就打印失败原因到 RunLog.txt 文件里*/                                                                                          \
    char td[44]; /*44字节临时变量空间*/                                                                                                           \
    strncpy(td, "RIFF", 4); /*首4个字节为wav的文件效验标签*/                                                                                      \
    memset(td + 4, 0, 40); /*其它40个字节先全设为0*/                                                                                              \
    *(unsigned short *)(td + 20) = format; /*格式*/                                                                                               \
    *(unsigned short *)(td + 22) = channels; /*声道数*/                                                                                           \
    *(unsigned short *)(td + 34) = bit; /*声道位*/                                                                                                \
    *(unsigned int *)(td + 24) = freq; /*频率*/                                                                                                   \
    *(unsigned int *)(td + 40) = size; /*音频数据的大小*/                                                                                         \
    GNf::Write(td, 1, 44, fp); //写入文件头信息

        GT_TMP_SAVE_WAV_HEAD //先保存文件头信息
            GNf::Write((char *)data, 1, size, fp); //再一次过写入音频数据
        GNf::Close(fp); //关闭文件
        return true; //返回成功
    }
    /*-----------------------------------------------------------------------------------------------------------------------------------------*/
    const char *GT_ERR_ReadMp3FileFail = "读取 mp3 文件失败! 解释数据有错!\n";

    //获得Mp3标签内容大小
    inline int GT_Mp3GetTagSize(uint8 *size) {
        return size[0] * 2097152 + size[1] * 16384 + size[2] * 128 + size[3];
    }
    //获得Mp3的音频数据帧开始指针(Mp3文件数据, 数据大小)
    static char *GT_Mp3GetFramesStartPtr(char *_data, int data_size) {
        if(_data[0] == 'I' && _data[1] == 'D' && _data[2] == '3') {
            return _data + (GT_Mp3GetTagSize((uint8 *)_data + 6) + 10); //跳过文件标签内容和标签头
        } else {
            unsigned short *_data2 = (unsigned short *)_data, *_end = (unsigned short *)(_data + data_size);
            while(_data2 < _end && (_data2[0] & 0xF0FF) != 0xF0FF) ++_data2; //跳过无效数据
            return (char *)(_data2 < _end ? _data2 : 0);
        }
    }
    inline unsigned short GT_Mp3TranslateBitrate(unsigned short ver, unsigned short layer, uint8 y) {
        unsigned short x;
        switch(ver) {
            case 1:
                x = layer - 1;
                break;
            case 2:
                x = layer == 1 ? 3 : 4;
            default: return 0xffff;
        }
        static const unsigned short bitrateIndex[16][5] = {
            {0, 0, 0, 0, 0},
            {32, 32, 32, 32, 8},
            {64, 48, 40, 48, 16},
            {96, 56, 48, 56, 24},
            {128, 64, 56, 64, 32},
            {160, 80, 64, 80, 40},
            {192, 96, 80, 96, 48},
            {224, 112, 96, 112, 56},
            {256, 128, 112, 128, 64},
            {288, 160, 128, 144, 80},
            {320, 192, 160, 160, 96},
            {352, 224, 192, 176, 112},
            {384, 256, 224, 192, 128},
            {416, 320, 256, 224, 144},
            {448, 384, 320, 256, 160},
            {0, 0, 0, 0, 0}};
        return bitrateIndex[y][x];
    }
    GN_API unsigned int GNa::LoadMp3FileInfo(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize) {
        int flen = memsize;
        char *data = filename_mem;
        if(memsize < GNf::eFILE_MEM || GNf::sFILE::DefaultH) {
            data = GNf::FileFullLoadToMemory(filename_mem, &flen, memsize);
            if(!data) return 0; /*文件不正确返回*/
        }
        uint8 *_data = (uint8 *)GT_Mp3GetFramesStartPtr(data, flen);
        if(!_data) return 0;

        uint8 ch_mk; //以下开始便是数据帧了 --------------------------------------------
        const unsigned int sample_rate[4][3] = {
            {44100, 22050, 11025},
            {48000, 24000, 12000},
            {32000, 16000, 8000},
            {0, 0, 0}};
        unsigned short version, layer, bi, *info2 = (unsigned short *)_data;
        unsigned int freq1 = 0, frame_n = 0;
        for(; (info2[0] & 0xF0FF) == 0xF0FF; ++frame_n) { //统计音频总帧数和获取信息
            version = (info2[0] & 0xF00) >> 11;
            layer = 4 - ((info2[0] & 0x600) >> 9);
            bi = GT_Mp3TranslateBitrate(version, layer, (_data[2] & 0xF0) >> 4);
            if(bi == 0xffff) {
                if(data != filename_mem) delete[] data;
                GN_Log(GT_ERR_ReadMp3FileFail);
                return 0;
            }
            ch_mk = _data[3];
            freq1 = sample_rate[(_data[2] & 0x0F) >> 2][version - 1];
            uint8 padding = (_data[2] & 0x0F) >> 1;

            _data += layer == 1 ? (12000 * (unsigned int)bi / freq1 + padding) << 2 : 144000 * (unsigned int)bi / freq1 + padding;
            info2 = (unsigned short *)_data;
        }
        //GN_Log("frame_n=%d\n", frame_n);
        if(data != filename_mem) delete[] data;
        *format = 0; //WAVE_FORMAT_UNKNOWN;
        *channels = (ch_mk & 0xc0) == 0xc0 ? 1 : 2;
        *bit = 16;
        *freq = freq1;
        unsigned int smpln = (layer == 1 ? 384 : 1152) * frame_n;
        *size = smpln * (*channels) << 1; //16位2个字节
        return smpln / freq1 * 1000;
    }
#define MINIMP3_IMPLEMENTATION
#ifdef _MAC_
#include <xmmintrin.h>
#endif
#include <minimp3.h>
    GN_API uint8 *GNa::LoadMp3FileData(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize) {
        int flen;
        char *data = GNf::FileFullLoadToMemory(filename_mem, &flen, memsize);
        if(!data) return 0; /*文件不正确返回*/
        uint8_t *data1 = (uint8_t *)data;

        mp3dec_t decer;
        mp3dec_t *_decer = &decer;
        mp3dec_frame_info_t finfo;
        mp3dec_frame_info_t *_finfo = &finfo;
        mp3dec_init(_decer);

        const unsigned int pcm_size = flen * 12;
        uint8 *pcm_data = new uint8[pcm_size];
        mp3d_sample_t *pcmd = (mp3d_sample_t *)pcm_data;
        int smpln = mp3dec_decode_frame(_decer, data1, flen, pcmd, _finfo);
        if(!smpln) {
            GN_Log(GT_ERR_ReadMp3FileFail);
            delete[] pcm_data;
            delete[] data;
            return 0;
        }
        *format = 0; //WAVE_FORMAT_UNKNOWN;
        *channels = _finfo->channels;
        *bit = 16;
        *freq = _finfo->hz;
        unsigned int tot_smpln = 0;
        if(_finfo->channels == 2) {
            for(;;) {
                tot_smpln += smpln << 1;
                data1 += _finfo->frame_bytes;
                flen -= _finfo->frame_bytes;
                if(flen <= 0) break;
                smpln = mp3dec_decode_frame(_decer, data1, flen, pcmd + tot_smpln, _finfo);
                //GN_Log("%d, %d %d %d %d %d %d, %d\n", smpln, _finfo->frame_bytes, _finfo->frame_offset, _finfo->channels, _finfo->hz, _finfo->layer, _finfo->bitrate_kbps, flen);
            }
        } else {
            for(;;) {
                tot_smpln += smpln;
                data1 += _finfo->frame_bytes;
                flen -= _finfo->frame_bytes;
                if(flen <= 0) break;
                smpln = mp3dec_decode_frame(_decer, data1, flen, pcmd + tot_smpln, _finfo);
            }
        }
        delete[] data;
        *size = tot_smpln << 1; //16位2个字节

        if(pcm_size - (*size) > 524288) { //缓冲多出512K就裁掉空的部分
            uint8 *pcm_data2 = new uint8[*size];
            memcpy(pcm_data2, pcm_data, *size);
            delete[] pcm_data;
            return pcm_data2;
        }
        return pcm_data;
    }
    /*---------------------------------------------------- 默认音频文件插件功能 ----------------------------------------------------------------*/

    struct GT_sAUDID_STREAM_WAV : public GNa::iFILE_AUDIO::cSTREAM {
        GNf::sFILE *fp; //文件

        GT_sAUDID_STREAM_WAV(GNf::sFILE *_fp = 0) : fp(_fp) {}
        virtual void Seek(float progre);
        virtual unsigned int Read(char *buf, unsigned int count);
        virtual unsigned int Write(const char *buf, int count);
        virtual void Close();
    };
    struct GT_sAUDID_STREAM_MP3 : public GNa::iFILE_AUDIO::cSTREAM {
        char *file_data; //mp3文件数据 !该值为0时 表示数据不是它拥有的
        uint8_t *audio_data; //指向mp3音频数据的开始
        int audio_size; //mp3音频数据大小
        int offset; //当前音频数据读取位置
        mp3dec_t decer; //mp3解码器

        GT_sAUDID_STREAM_MP3(char *fdata = 0, uint8_t *adata = 0, int asize = 0) : file_data(fdata), audio_data(adata), audio_size(asize), offset(0) { mp3dec_init(&decer); }
        virtual void Seek(float progre);
        virtual unsigned int Read(char *buf, unsigned int count);
        virtual unsigned int Write(const char *buf, int count);
        virtual void Close();
    };
    void GT_sAUDID_STREAM_WAV::Close() {
        if(!fp->buf_size) { //文件流是以写方式打开的
            unsigned int size = GNf::Tell(fp) - 44; //写完获得当前文件写位置偏移减去文件头大小就是wav音频数据大小
            GNf::Seek(fp, 40, GNf::eSEEK_START); //跳回文件第40个字节处为wav文件储存音频数据大小值的位置
            GNf::Write((char *)&size, 1, 4, fp); //写入数据大小的值
        }
        GNf::Close(fp);
        delete this; //完成写入关闭文件 并删除自己的内存空间
    }
    void GT_sAUDID_STREAM_MP3::Close() {
        GN_DeleteArray(file_data);
        delete this;
    }

    void GT_sAUDID_STREAM_WAV::Seek(float progre) {
        GNf::Seek(fp, 44 + (int)(fp->buf_size * progre), GNf::eSEEK_START); //使文件读取跳转到音频播放进度对应的文件位置 44表示文件头所占字节
    }
    unsigned int GT_sAUDID_STREAM_WAV::Read(char *buf, unsigned int count) {
        return GNf::Read(buf, 1, count, fp); //在当前文件位置读出指定大小的数据
    }
    unsigned int GT_sAUDID_STREAM_WAV::Write(const char *buf, int count) {
        count = GNf::Write(buf, 1, count, fp); //在当前文件位置写入指定大小的数据
        return count > 0 ? count : 0; //使返回值最小为0
    }
    void GT_sAUDID_STREAM_MP3::Seek(float progre) {
        offset = (int)(audio_size * progre);
    }
    unsigned int GT_sAUDID_STREAM_MP3::Read(char *buf, unsigned int count) {
        mp3dec_t *_decer = &decer;
        mp3dec_frame_info_t finfo;
        mp3dec_frame_info_t *_finfo = &finfo;
        mp3d_sample_t *pcmd = (mp3d_sample_t *)buf;
        unsigned int read_size = 0, smpln2;
        count >>= 1;
        do {
            smpln2 = mp3dec_decode_frame(_decer, audio_data + offset, audio_size - offset, pcmd + read_size, _finfo) * _finfo->channels;
            offset += _finfo->frame_bytes;
            read_size += smpln2;
        } while(_finfo->frame_bytes && read_size + smpln2 <= count);
        return read_size << 1;
    }
    unsigned int GT_sAUDID_STREAM_MP3::Write(const char *buf, int count) {
        return 0;
    }
    GNa::iFILE_AUDIO::cSTREAM *GNa::iFILE_AUDIO::Open(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize) {
        GNf::sFILE *fp = GNf::Open(filename_mem, "rb", memsize); //打开文件 filename_mem 和 memsize 可以是磁盘文件 或压缩包 或 内存数据块 具体看 memsize 怎么表示
        if(!fp) return 0; //打开失败返回0
        char td[44];
        GNf::Read(td, 1, 44, fp); //读取wav文件头信息
        if(!strncmp(td, "RIFF", 4)) { //校验文件头信息 判断是否wav格式文件
            GT_TMP_SET_WAV_INFO //返回文件头的信息到输出参数
                fp->buf_size = *size; //文件该属性指定为数据大小 以便之后读取跳转 计算出文件读取位置
            return new GT_sAUDID_STREAM_WAV(fp); //创建文件流对象并返回
        } else { // Mp3
            if(GNf::sFILE::DefaultH || memsize == GNf::eFILE_STD || memsize == GNf::eFILE_SAFE) { //网络文件或本地文件
                memsize = GNf::GetSize(fp);
                filename_mem = new char[memsize];
                memcpy(filename_mem, td, 44);
                GNf::Read(filename_mem + 44, 1, memsize - 44, fp); //把整个Mp3度到缓存
                GNf::Close(fp);
                fp = GNf::Open(filename_mem, "rb", memsize);
                fp->special_mark = -1; //视这段内存是压缩包解压的 这样在关闭文件时就能一并删除
            }
            if(!LoadMp3FileInfo(fp->handle_buf, format, channels, bit, freq, size, fp->buf_size)) {
                GNf::Close(fp);
                return 0;
            }
            char *frame_st = GT_Mp3GetFramesStartPtr(fp->handle_buf, fp->buf_size);
            GT_sAUDID_STREAM_MP3 *fmp3 = new GT_sAUDID_STREAM_MP3(fp->special_mark ? fp->handle_buf : 0, (uint8_t *)frame_st, fp->buf_size - (frame_st - fp->handle_buf));
            fp->special_mark = 0; //数据以被所管 提示之后无需删除
            GNf::Close(fp);
            return fmp3;
        }
    }
    GNa::iFILE_AUDIO::cSTREAM *GNa::iFILE_AUDIO::Create(const char *file_name, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq) {
        char *ext_name = GN_StrGetExtName(file_name); //获得文件扩展名
        if(ext_name && !strcmp(ext_name, ext_name[0] > 'Z' ? "mp3" : "MP3")) return 0; /*模本代码:暂时不支持保存成Mp3*/
        unsigned int size = 0; //初始音频数据大小为0
        GT_TMP_SAVE_WAV_HEAD //先保存wav文件头信息
            fp->buf_size = 0; //文件该属性设为0 标记是写文件
        return new GT_sAUDID_STREAM_WAV(fp); //创建文件流对象并返回
    }
    GNa::iFILE_AUDIO::iFILE_AUDIO() : module(0) {
        GT_MX_PushFront(FileAudio, this);
        GNg::GN_Core.aAudioExtNames[0] = 0;
    }
    GNa::iFILE_AUDIO::~iFILE_AUDIO() {
        GT_TMP_REMOVE_PLUGINS_NODE(iFILE_AUDIO, FileAudio)
        GNg::GN_Core.aAudioExtNames[0] = 0;
    }
    unsigned int GNa::iFILE_AUDIO::LoadInfo(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize) {
        int yes = GNf::IsTypeFile(filename_mem, *format, "wav", "RIFF", 4, memsize); //判断提供的文件名或文件内容是否指定类型的文件
        if(yes) { //如是 或不确定
            unsigned int play_time = LoadWavFileInfo(filename_mem, format, channels, bit, freq, size, memsize); //尝试读取wav音频文件信息
            if(play_time || yes == 1) return play_time; //如调佣成功 或 已确定是wav文件 就返回音频播放时间 无论有否
        }
        return LoadMp3FileInfo(filename_mem, format, channels, bit, freq, size, memsize);
    }
    uint8 *GNa::iFILE_AUDIO::LoadData(char *filename_mem, unsigned short *format, unsigned short *channels, unsigned short *bit, unsigned int *freq, unsigned int *size, int memsize) {
        int yes = GNf::IsTypeFile(filename_mem, *format, "wav", "RIFF", 4, memsize); //判断提供的文件名或文件内容是否指定类型的文件
        if(yes) { //如是 或不确定
            uint8 *data = LoadWavFileData(filename_mem, format, channels, bit, freq, size, memsize); //尝试读取wav音频文件数据
            if(data || yes == 1) return data; //如调佣成功 或 已确定是wav文件 就返回音频数据 无论有否
        }
        return LoadMp3FileData(filename_mem, format, channels, bit, freq, size, memsize);
    }
    bool GNa::iFILE_AUDIO::Save(const char *file_name, unsigned short format, unsigned short channels, unsigned short bit, unsigned int freq, unsigned int size, uint8 *data) {
        char *ext_name = GN_StrGetExtName(file_name); /*模本代码:暂时不支持保存成Mp3*/
        if(ext_name && !strcmp(ext_name, "mp3")) return false;
        return SaveWavFile((char *)file_name, format, channels, bit, freq, size, data);
    }
    bool GNa::iFILE_AUDIO::CheckFmtSupportFromExtName(const char *extname) {
        /*
	if(!strcmp(extname,"ogg") || !strcmp(extname,"OGG"))return true;
	return next_node->CheckFmtSupportFromExtName(extname); //调用之前默认的方法
*/
        if(extname[0] > 'Z') {
            if(!strcmp(extname, "wav") || !strcmp(extname, "mp3")) return true;
        } else if(!strcmp(extname, "WAV") || !strcmp(extname, "MP3")) return true;
        return false;
    }
    void GNa::iFILE_AUDIO::GetSupportFmtFileExtName(char *out, bool upper) {
        /*  
	strcpy(out, upper?"ogg,OGG,":"ogg,");
	next_node->GetSupportFmtFileExtName(out+(upper?8:4), upper); //调用之前默认的方法
*/
        // 已核（STRCPY 审计）：调用方缓存 128 字节（GNg::GN_Core.aAudioExtNames），本函数只写 16 或 8 字节；
        // 该接口无容量参数，插件链扩展时须自行遵守调用方容量（见 docs/STRCPY_AUDIT_2026-11-21.md）。
        if(upper) strcpy(out, "wav,WAV,mp3,MP3");
        else strcpy(out, "wav,mp3");
    }
    GNa::iFILE_AUDIO GT_FileAudio;
    GN_API GNa::iFILE_AUDIO *GNa::FileAudio = &GT_FileAudio;

    GN_API int GN_PluginsFileAudioRemove() {
        GT_TMP_REMOVE_PLUGIN(GNa::FileAudio, GT_FileAudio)
    }
#endif //_GN_FORMAT_H_
