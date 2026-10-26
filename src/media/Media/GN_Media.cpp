/****************************************************************************
Copyright (c) 2018-2025 gncreate Technologies Inc.

 http://www.gncreate.com
 http://www.img-power.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "GN_Media.h"
#ifdef _GN_MEDIA_H_

#ifdef _WIN32_WCE
GNm::cVIDEO::cVIDEO() : z__attrb(0) {}
GNm::cVIDEO::~cVIDEO() {}
void GNm::cVIDEO::Release() {}
bool GNm::cVIDEO::InitToData(const char *filename, int loop) {
    return false;
}
void GNm::cVIDEO::Stop() {}
void GNm::cVIDEO::Play() {}
void GNm::cVIDEO::Pause() {}
void GNm::cVIDEO::AddPlayCount(int loop) {}
bool GNm::cVIDEO::IsPlaying() {
    return false;
}
bool GNm::cVIDEO::IsBusy() {
    return false;
}
void GNm::cVIDEO::SetStage(float state) {}
void GNm::cVIDEO::SetSpeed(float speed) {}
unsigned int GNm::cVIDEO::GetEndTime() {
    return 0;
}
unsigned int GNm::cVIDEO::GetNowTime() {
    return 0;
}
P<GNg::sIMAGE> GNm::cVIDEO::GetImage() {
    return P_();
}
unsigned int GNm::cVIDEO::UpdataToImage(unsigned int updatatime) {
    return 0;
}
unsigned int GNm::cVIDEO::UpdataPlay(unsigned int updatatime, int x, int y, int w, int h, unsigned int color, unsigned short blendmode) {
    return 0;
}

#else /*****************************************************************************************************************************************/

#define GT_AVI_NET_BUF_SIZE 2097152 //2MB
#define GT_AVI_FILE_BUFFER_SIZE 8192 //8K
#define GT_AVI_PROBE_BUF_SIZE 2048 //2K
//typedef signed char  int8_t;

#ifndef _INC_MATH
#include <math.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#ifdef __cplusplus
}
#endif

// FFmpeg 6.x compatibility macros
#define US_FFMPEG4 6
#ifdef US_FFMPEG4
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#define PIX_FMT_RGB24 AV_PIX_FMT_RGB24
#define avcodec_open(arg1, arg2) avcodec_open2(arg1, arg2, 0)
#define av_close_input_stream(FC) avformat_close_input(&FC)
#define av_find_stream_info(CTX) avformat_find_stream_info(CTX, 0)
#define avcodec_alloc_frame av_frame_alloc
#define avcodec_free_frame(FRAME) av_frame_free(&FRAME)
#define av_free_packet av_packet_unref
#else
#define avcodec_free_frame av_free
#define AVPixelFormat int
#endif

struct GT_s_NET_BUF {
    uint8_t buf[GT_AVI_NET_BUF_SIZE]; //数据缓冲
    int buf_pos; //标记缓冲开头相对于文件的位置
    int readed_pos; //标记已经读好的文件位置
    int played_pos; //标记已经播放的文件位置
};
static void GT_AVI_NetBufRead(P<GN_sTHREAD> this_thread, void *arg, void *arg2);
static int GT_AVI_read(void *opaque, uint8_t *buf, int size);
static int64_t GT_AVI_seek(void *opaque, int64_t pos, int whence);

class GT_cVAUDIO : public GT_cSOUND_STREAM, public GN::cBUF_WHEEL {
  public:
#ifdef US_FFMPEG4
    AVFrame *zpAUFrame;
#else
    uint8 *zRecvBuf;
#endif
    AVCodecContext *zpCodecContextAudio;
    float zVolume;
    unsigned int zBufferSize;
    unsigned short zBit;

    virtual unsigned int zOnLoad(char *data);

  public:
    GN::cBUF_WHEEL aVBuf;
#define AUDIO_BUF_SIZE 25000

    GT_cVAUDIO() :
#ifdef US_FFMPEG4
                   zpAUFrame(0),
#else
                   zRecvBuf(0),
#endif
                   zpCodecContextAudio(0), zVolume(1.0f) {
    }
    ~GT_cVAUDIO() {
#ifdef US_FFMPEG4
        if(zpAUFrame) {
            avcodec_free_frame(zpAUFrame);
            zpAUFrame = NULL;
        }
#if US_FFMPEG4 > 4
        if(zpCodecContextAudio) {
            avcodec_free_context(&zpCodecContextAudio);
            zpCodecContextAudio = 0;
        }
#endif
#else
        if(zRecvBuf) {
            av_free(zRecvBuf);
            zRecvBuf = 0;
        }
#endif
        if(zpCodecContextAudio) {
            avcodec_free_context(&zpCodecContextAudio);
            zpCodecContextAudio = 0;
        }
    }
    void Init(AVCodecContext *ctx, unsigned short bit, unsigned int stream_lov_sec) {
        zBufferSize = 0;
        zBit = bit;
        //ctx->bit_rate = ctx->sample_rate * bit * ctx->channels;
        //GN_Log("%d %d %d %d %d\n", bit, ctx->bit_rate, ctx->sample_rate, ctx->ch_layout.nb_channels, ctx->sample_fmt);
        zpCodecContextAudio = ctx;
        New(ctx->sample_rate << 1);
        aVBuf.New(stream_lov_sec >> 6);
#ifdef US_FFMPEG4
        if(zpAUFrame == NULL) zpAUFrame = avcodec_alloc_frame();
#else
        if(!zRecvBuf) zRecvBuf = (uint8 *)av_malloc(192000 /* AVCODEC_MAX_AUDIO_FRAME_SIZE removed */);
#endif
    }
#ifdef US_FFMPEG4
    //解码音频数据(数据包)返回-1:失败 0:成功
    int DecodeAudio(AVPacket *pkt);
};
int GT_cVAUDIO::DecodeAudio(AVPacket *pkt) {
#else
    //解码音频数据(数据, 数据大小字节)返回-1:失败 0:成功
    int DecodeAudio(uint8 *data, int size);
};
int GT_cVAUDIO::DecodeAudio(uint8 *data, int size) {
#endif
#ifdef US_FFMPEG4
    if(avcodec_send_packet(zpCodecContextAudio, pkt) < 0) return -1;
    int ret, smi, smn;
    float *sdata;
    short tmp_buf[16384]; //16K
    for(;;) {
        ret = avcodec_receive_frame(zpCodecContextAudio, zpAUFrame);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        else if(ret < 0) return -1;
        if(zpCodecContextAudio->sample_fmt == AV_SAMPLE_FMT_FLTP && zpCodecContextAudio->ch_layout.nb_channels > 1) {
            float *sdata1 = (float *)zpAUFrame->data[1];
            sdata = (float *)zpAUFrame->data[0];
            for(smi = 0, smn = zpAUFrame->nb_samples; smi < smn; ++smi) {
                tmp_buf[(smi << 1)] = (short)(sdata[smi] * 32767.0f);
                tmp_buf[(smi << 1) + 1] = (short)(sdata1[smi] * 32767.0f);
            }
            Write((const uint8 *)tmp_buf, smn << 2);
        } else if(zpCodecContextAudio->sample_fmt == AV_SAMPLE_FMT_FLT) {
            sdata = (float *)zpAUFrame->data[0];
            for(smi = 0, smn = zpAUFrame->linesize[0] >> 2; smi < smn; ++smi) {
                tmp_buf[smi] = (short)(sdata[smi] * 32767.0f);
            }
            Write((const uint8 *)tmp_buf, smn << 1);
        } else Write(zpAUFrame->data[0], zpAUFrame->linesize[0]);
    }
#else
    int size_, tmp_size, seg_size, out_size;
    while(size > 0) {
        //解码
        tmp_size = 192000 /* AVCODEC_MAX_AUDIO_FRAME_SIZE removed */;
        // Modern: avcodec_decode_audio2 removed, using send/receive
        if(avcodec_send_packet(zpCodecContextAudio, pkt) == 0) {
            while(avcodec_receive_frame(zpCodecContextAudio, zpAUFrame) == 0) {
                int ch = zpCodecContextAudio->ch_layout.nb_channels;
                int bytes_per_sample = av_get_bytes_per_sample(zpCodecContextAudio->sample_fmt);
                tmp_size = zpAUFrame->nb_samples * ch * bytes_per_sample;
                if(tmp_size > 0 && tmp_size <= 192000) {
                    memcpy(zRecvBuf, zpAUFrame->data[0], tmp_size);
                    size_ = tmp_size;
                }
            }
        }
        //GN_Log("k:%d %d %d\n", size_, tmp_size, size);
        if(size_ < 0) break;
        size -= size_;
        data += size_;
        Write(zRecvBuf, tmp_size);
    }
#endif
    if(!zBufferSize && DataSize() >= (AUDIO_BUF_SIZE << 2)) {
        zBufferSize = AUDIO_BUF_SIZE;
        if(zInitStream(zBufferSize, WAVE_FORMAT_PCM, zpCodecContextAudio->ch_layout.nb_channels, zBit, zpCodecContextAudio->sample_rate, zVolume, 1.0f)) return 1;
        return -1;
    }
    return 0;
}
unsigned int GT_cVAUDIO::zOnLoad(char *data) { //GN_Log("pos:%d %d %d\n", zReadPos, zWritePos, zPcmBufSize);
    return Read((uint8 *)data, zBufferSize);
}
/***************************************************** 类GT_cVIDEO *******************************************************/
class GT_cVIDEO {
    friend void ::GT_AVI_NetBufRead(P<GN_sTHREAD>, void *, void *);
    friend int ::GT_AVI_read(void *, uint8_t *, int);
    friend int64_t GT_AVI_seek(void *, int64_t, int);

#ifdef US_FFMPEG4
    AVPacket *zPacket, *zPacket2;
#else
    ByteIOContext zIOContext;
#endif
    int zVideoIndex;
    int zAudioIndex;
    AVFormatContext *zpContext;
    SwsContext *zpSwScaleContext;
    AVCodecContext *zpCodecContext;

    AVCodec *zpCodec;
    AVInputFormat *zpFmt;
    AVFrame *zpAVFrame;
    AVFrame *zpAVFrameRGB;
    uint8_t *zpAVFrameRGBBuffer;
    uint8 zbyteFileBuffer[GT_AVI_FILE_BUFFER_SIZE + 1]; //缓冲区大小

    P<GNg::sIMAGE> zImage; //当前解码到的帧图象
    P<GT_cVAUDIO> zBgm; //声音
    GNf::sFILE *zpFile; //当前读取的文件
    unsigned int zdwEndTime; //视频的结束时间 单位:毫秒
    unsigned int zdwNowTime; //视频的行到时间 单位:毫秒
    float zfFrameDelay; //每帧停留时间
    float zfFrameRate; //每秒帧率
    float zSpeedRatio; //播放倍速
    unsigned int zMaxAccumFrameN; //最大统计帧数 由于画面变动少的视频可能几帧才合并一幅图像
    unsigned int zFrameNumber; //已经播放的帧数
    unsigned int zRecvFrameNum; //需处理音频时的收取播放帧数
    unsigned int zCrsFrameNum; //需处理音频时的视频延迟相差帧数
    bool zEndOfFilm; //当前是否结束
    bool zMediaOpen; //当前文件流是否打开
    bool zIsPlaying; //当前是否正在播放
    bool zIsPause; //当前是否暂停
    bool zSyncTime; //使时间尽量同步
    bool zBusy; //当前是否正在忙碌读取数据中
    int zLoop_v; //还剩播放次数
    char zFileName[384]; //当前打开的文件名
    P<GT_s_NET_BUF> zNetBuf; //网络文件流
    P<GN_sTHREAD> zUpdataThread; //更新播放线程
    static bool zAV_Register; //该类对象是否需要初始化注册AVCODE

    void zClose(); //关闭文件播放流
    int zReadPacket(unsigned int frame_id); //读数据包用(当前帧)
    int zDecodePacket(uint8 *data, int size); //解码数据包
    int zDecodeFrame(); //检测解码帧
    void zRecvDecodeFrame(); //读取缓冲数据并解码帧
    void zGetFrame(); //转换获取一帧图象用
  public:
    GT_cVIDEO();
    virtual ~GT_cVIDEO();
    void Release(); //释放内部资源
    bool InitToData(const char *filename, int loop, bool sync_time, bool audio); //初始化(加入文件名, 要播放多少次 负数为循环播放, 是否尽量使视频能同步上经历的时间 这会增加额外开销, 是偶处理视频里的声音)返回是否成功

    void Stop(); //停止
    void Play(); //播放
    void Pause(); //暂停或继续播放
    void AddPlayCount(int loop); //增加播放次数(次数)
    bool IsPlaying(); //是否正在播放中
    bool IsPause() { return zIsPause; } //判断当前是否暂停播放
    bool IsBusy() { return zBusy; } //返回是否正在忙碌读取数据中
    void SetStage(float state); //设置播放进度的 0.0f - 1.0f
    void SetSpeed(float speed); //设置播放倍速 !注意如果有指定播放陪速 那就不要再用以经乘了陪值的时间传给updatatime参数了
    void SetVolume(float vol); //设置音量大小(音量值)
    unsigned int GetEndTime(); //获得文件总播放时间 单位:毫秒
    unsigned int GetNowTime(); //获得文件现播放到的时间 单位:毫秒
    P<GNg::sIMAGE> &GetImage(); //获得视频解出来的帧图像

    inline int GetLoopNum() { return zLoop_v; } //获得还需要播放循环次数 <0:表示无限循环
    inline float GetVolume() { return zBgm ? zBgm->zVolume : 0.0f; } //获得当前音量大小
    inline unsigned int GetFreq() { return zBgm ? zBgm->zpCodecContextAudio->sample_rate : 0; } //获得音频采样率
    inline unsigned short GetBit() { return zBgm ? zBgm->zBit : 0; } //获得音频位数
    inline unsigned short GetChannel() { return zBgm ? zBgm->zpCodecContextAudio->ch_layout.nb_channels : 0; } //获得音频声道数
    inline char *GetFileName() { return zFileName; } //获得播放视频的路径文件名

    //更新文件流影像到帧图像(更新时间) 返回现在播放到的时间:0代表已经播放结束 这要在循环里面不断调用
    unsigned int UpdataToImage(unsigned int updatatime);

    //更新文件流播放(更新时间, 屏幕坐标x, y, 宽, 高, 颜色, 混合模式)返回现在播放到的时间:0代表已经播放结束 这要在循环里面不断调用
    unsigned int UpdataPlay(unsigned int updatatime, int x, int y, int w, int h, unsigned int color, unsigned short blendmode);
};
static void GT_AVI_NetBufRead(P<GN_sTHREAD> this_thread, void *arg, void *arg2) {
    GT_cVIDEO *av = (GT_cVIDEO *)arg;
    GT_s_NET_BUF *nb = av->zNetBuf;
    int size, _size, rsize, pos;
    /*if(this_thread)size = 1024 * 512; //512K  该值最大不能大于等于  GT_AVI_NET_BUF_SIZE>>1 大小
	else  */
    size = 1024 * 32; //32K
    pos = (nb->readed_pos - nb->buf_pos) % GT_AVI_NET_BUF_SIZE;
    _size = GT_AVI_NET_BUF_SIZE - pos;
    if(size > _size) size = _size;
    rsize = GNf::Read((char *)(nb->buf + pos), 1, size, av->zpFile);
    if(rsize > 0) nb->readed_pos += rsize;
}
static int GT_AVI_read(void *opaque, uint8_t *buf, int size) {
    GT_cVIDEO *av = (GT_cVIDEO *)opaque;
    GNf::sFILE *f = av->zpFile;
    GT_s_NET_BUF *nb = av->zNetBuf;
    if(nb) {
        int pos = (nb->played_pos - nb->buf_pos) % GT_AVI_NET_BUF_SIZE; //计算到数据位置
        int _size = GT_AVI_NET_BUF_SIZE - pos;
        int _size_ = size - (nb->readed_pos - nb->played_pos); //计算到差欠量
        if(_size_ > 0) {
            size -= _size_;
            if(size > _size) {
                memcpy(buf, nb->buf + pos, _size);
                memcpy(buf + _size, nb->buf, size - _size);
            } else memcpy(buf, nb->buf + pos, size);
            int rsize = GNf::Read((char *)(buf + size), 1, _size_, f);
            if(rsize > 0) {
                nb->readed_pos += rsize;
                size += rsize;
            }
        } else {
            if(size > _size) {
                memcpy(buf, nb->buf + pos, _size);
                memcpy(buf + _size, nb->buf, size - _size);
            } else memcpy(buf, nb->buf + pos, size);
        }
        nb->played_pos += size;
        return size;
    } else return GNf::Read((char *)buf, 1, size, f);
}
static int64_t GT_AVI_seek(void *opaque, int64_t pos, int whence) {
    if(whence != GNf::eSEEK_START && whence != GNf::eSEEK_CUR && whence != GNf::eSEEK_END) return -1;

    GT_cVIDEO *av = (GT_cVIDEO *)opaque;
    GNf::sFILE *f = av->zpFile; // GN_Log("seek: %d %d\n", pos, whence);
    int64_t ret = GNf::Seek(f, pos, whence);
    if(ret) return -1;
    ret = GNf::Tell(f);
    GT_s_NET_BUF *nb = av->zNetBuf;
    if(nb) {
        nb->buf_pos = ret;
        nb->readed_pos = ret;
        nb->played_pos = ret;
    }
    return ret;
}
bool GT_cVIDEO::zAV_Register = true;

GT_cVIDEO::GT_cVIDEO() :
#ifdef US_FFMPEG4
                         zPacket(0), zPacket2(0),
#endif
                         zpAVFrameRGBBuffer(0),
                         zpAVFrame(0),
                         zpAVFrameRGB(0),
                         zpContext(0),
                         zVideoIndex(-1),
                         zAudioIndex(-1),
                         zdwEndTime(0),
                         zdwNowTime(0),
                         zfFrameDelay(33.0f),
                         zfFrameRate(30.0f),
                         zSpeedRatio(1.0f),
                         zMaxAccumFrameN(13),
                         zEndOfFilm(0),
                         zpSwScaleContext(0),
                         zMediaOpen(0),
                         zIsPlaying(0),
                         zIsPause(0),
                         zBusy(0),
                         zpFile(0) {
    zFileName[0] = 0; //memset(&zPacket.data,0,4); //zPacket.data = 0;
}
GT_cVIDEO::~GT_cVIDEO() {
    this->Release();
}
void GT_cVIDEO::zClose() {
    if(!zMediaOpen) return;
    if(zIsPlaying) this->Stop();

    zMediaOpen = false;
    zBgm.Delete();
    if(zpSwScaleContext) {
        sws_freeContext(zpSwScaleContext);
        zpSwScaleContext = 0;
    }
    //if( zPacket.data )av_free_packet( &zPacket );
    avcodec_free_context(&zpCodecContext);
    zpCodecContext = 0;

    zImage.Delete();

    av_close_input_stream(zpContext);
#ifdef US_FFMPEG4
#if US_FFMPEG4 > 4
    if(zpCodecContext) {
        avcodec_free_context(&zpCodecContext);
        zpCodecContext = 0;
    }
#endif
    if(zpContext) {
        if(zpContext->pb) {
            av_freep(&(zpContext->pb->buffer));
            avio_context_free(&(zpContext->pb));
            zpContext->pb = 0;
        }
        avformat_free_context(zpContext);
    }
    if(zPacket2) {
        zPacket2->data = 0;
        zPacket2->size = 0;
        av_packet_free(&zPacket2);
        zPacket2 = 0;
    }
    if(zPacket) {
        av_packet_free(&zPacket);
        zPacket = 0;
    }
#endif
    if(zpFile) {
        GNf::Close(zpFile);
        zpFile = 0;
    }
    zpContext = 0;

    if(zpAVFrameRGBBuffer) {
        av_free(zpAVFrameRGBBuffer);
        zpAVFrameRGBBuffer = 0;
    }
    zNetBuf.Delete();
}
void GT_cVIDEO::Release() {
    this->zClose();
    if(zpAVFrameRGB) {
        avcodec_free_frame(zpAVFrameRGB);
        zpAVFrameRGB = 0;
    }
    if(zpAVFrame) {
        avcodec_free_frame(zpAVFrame);
        zpAVFrame = 0;
    }
}
static char *GT_GetAvVersionStr(unsigned ver) {
    static char avVersionStr[16];
    sprintf(avVersionStr, "%d.%d.%d", (ver & 0x00ff0000) >> 16, (ver & 0x0000ff00) >> 8, ver & 0xff);
    return avVersionStr;
}
bool GT_cVIDEO::InitToData(const char *filename, int loop, bool sync_time, bool audio) {
    if(zMediaOpen && GN_StrcmpPath(zFileName, filename)) {
        //再次打开相同的文件就忽略初始化
    } else {
#if 1 /* FFmpeg 6.x */
        GT_cVIDEO::zAV_Register = false;
#else
        if(GT_cVIDEO::zAV_Register) {
            /* av_register_all() removed in FFmpeg 4.0+ */
            GT_cVIDEO::zAV_Register = false;
        }
#endif
        zClose();
        strcpy(zFileName, filename);
        zpFile = GNf::Open(zFileName, "rb");
        if(!zpFile) {
            GN_Log("打开文件 %s 失败!\n", filename);
            return false;
        }
        int i, w, h, ret, numBytes;
        AVPixelFormat pix_fmt;
        zVideoIndex = -1;
        zAudioIndex = -1;
        zEndOfFilm = 0;
        zMediaOpen = 0;
        zBusy = 0;
        zLoop_v = 0;
#ifdef US_FFMPEG4
        zpContext = avformat_alloc_context();
        zpContext->pb = avio_alloc_context((uint8 *)av_malloc(GT_AVI_FILE_BUFFER_SIZE + 1), GT_AVI_FILE_BUFFER_SIZE, 0, this, GT_AVI_read, NULL, GT_AVI_seek);
        zpContext->flags |= AVFMT_FLAG_IGNIDX | AVFMT_FLAG_IGNDTS; // 忽略索引/DTS错误 (FFmpeg 8.x)
        //zpContext->flags = AVFMT_FLAG_CUSTOM_IO;
        GNf::Seek(zpFile, 0, GNf::eSEEK_START); //移动到文件开始
        { // 传递宽松选项给 demuxer
            AVDictionary *opts = NULL;
            av_dict_set(&opts, "fflags", "+ignidx+genpts", 0);
            av_dict_set(&opts, "max_analyze_duration", "10000000", 0);
            ret = avformat_open_input(&zpContext, 0, 0, &opts);
            av_dict_free(&opts);
        }
#else
        AVFormatParameters ap;
        uint8 pDataBuffer[GT_AVI_PROBE_BUF_SIZE + 1] = {0};
        long lSize = GT_AVI_PROBE_BUF_SIZE;
        AVProbeData pd;
        memset(&zIOContext, 0, sizeof(ByteIOContext));
        ret = init_put_byte(&zIOContext,
                            zbyteFileBuffer,
                            GT_AVI_FILE_BUFFER_SIZE,
                            0, //no write flag
                            (void *)this,
                            GT_AVI_read,
                            NULL, //can not write
                            GT_AVI_seek);

        if(ret < 0) {
            GN_Log("初始化视频流读写IO失败\n");
            return false;
        }

        zIOContext.is_streamed = false;
        zIOContext.max_packet_size = GT_AVI_FILE_BUFFER_SIZE;

        pd.buf = pDataBuffer;
        pd.buf_size = 0;
        pd.filename = "";
        pd.buf_size = get_buffer(&zIOContext, pDataBuffer, lSize);
        zpFmt = av_probe_input_format(&pd, 1);
        if(!zpFmt) {
            GN_Log("av_probe_input_format error:%s\n", zFileName);
            return false;
        }

        //移动到文件开始
        url_fseek(&zIOContext, 0, SEEK_SET);

        memset(&ap, 0, sizeof(AVFormatParameters));
        ret = av_open_input_stream(&zpContext, &zIOContext, "", zpFmt, &ap);
#endif
        if(ret < 0) {
            GN_Log("打开视频流失败 %s\nlib version %s\n", zFileName, GT_GetAvVersionStr(avcodec_version()));
            return false;
        }
        ret = av_find_stream_info(zpContext);
        if(ret < 0) {
            GN_Log("检索视频流信息失败!\n");
            return false;
        }

        zdwEndTime = (unsigned int)(zpContext->duration / (AV_TIME_BASE / 1000));

        AVStream *av_stream = 0;
        for(i = 0; i < (int)(zpContext->nb_streams); ++i) {
#if 1 /* FFmpeg 6.x */
            switch(zpContext->streams[i]->codecpar->codec_type)
#else
            switch(zpContext->streams[i]->codecpar->codec_type)
#endif
            {
                case CODEC_TYPE_VIDEO:
                    if(zVideoIndex == -1) {
                        zVideoIndex = i;
                        av_stream = zpContext->streams[i];
                    }
                    break;
                case CODEC_TYPE_AUDIO:
                    if(audio && zAudioIndex == -1) zAudioIndex = i;
                    break;
                default: break;
            }
        }
        if(!av_stream) {
            GN_Log("Couldn't find a video stream\n");
            return false;
        }

        //pCodecCtx = &pFormatCtx->streams[videoStream]->codec;
#if 1 /* FFmpeg 6.x */
        zpCodecContext = avcodec_alloc_context3(0);
        avcodec_parameters_to_context(zpCodecContext, zpContext->streams[zVideoIndex]->codecpar);
#else
        zpCodecContext = av_stream->codec;
#endif
        zpCodec = (AVCodec *)avcodec_find_decoder(zpCodecContext->codec_id);
        if(!zpCodec) {
            GN_Log("Couldn't find decoder %d\n", zpCodecContext->codec_id);
            return false;
        }

        //if(zpCodec->capabilities & CODEC_CAP_TRUNCATED)
        //zpCodecContext->flags |= CODEC_FLAG_TRUNCATED;
        ret = avcodec_open(zpCodecContext, zpCodec);
        if(ret < 0) {
            GN_Log("Couldn't open codec %s\n", zpCodec->name);
            return false;
        }

        if(!zpAVFrame) zpAVFrame = avcodec_alloc_frame();
        if(!zpAVFrameRGB) zpAVFrameRGB = avcodec_alloc_frame();
        w = zpCodecContext->width;
        h = zpCodecContext->height;
        /*-------------------------------- 分配用于存储转换后的象素数据空间 ----------------------------------*/
        pix_fmt = PIX_FMT_RGB24;
#if 1 /* FFmpeg 6.x */
        numBytes = av_image_get_buffer_size(pix_fmt, w, h, 1);
#else
        numBytes = av_image_get_buffer_size(pix_fmt, w, h);
#endif
        if(zpAVFrameRGBBuffer) {
            av_free(zpAVFrameRGBBuffer);
            zpAVFrameRGBBuffer = 0;
        }
        zpAVFrameRGBBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
#if 1 /* FFmpeg 6.x */
        av_image_fill_arrays(zpAVFrameRGB->data, zpAVFrameRGB->linesize, zpAVFrameRGBBuffer, pix_fmt, w, h, 1);
#else
        /* using av_image_fill_arrays above */
#endif
/*--------------------- 计算帧数和停留时间 -------------------------*/
#ifdef US_FFMPEG4
        zfFrameRate = av_q2d(av_stream->avg_frame_rate);
#else
        if(av_stream->r_frame_rate.den && av_stream->r_frame_rate.num)
            zfFrameRate = (float)av_q2d(av_stream->r_frame_rate);
        else
            zfFrameRate = (float)(1.0 / av_q2d(av_stream->codec->time_base));
#endif
        if(!zfFrameRate) zfFrameRate = 30.0f;
        zfFrameDelay = 1000.0f / zfFrameRate;

        if(zAudioIndex != -1) { // 初始化音频解码器相关 ----------------------------------------------------------
#if 1 /* FFmpeg 6.x */
            AVCodecContext *codec_ctx = avcodec_alloc_context3(0);
            avcodec_parameters_to_context(codec_ctx, zpContext->streams[zAudioIndex]->codecpar);
#else
            AVCodecContext *codec_ctx = zpContext->streams[zAudioIndex]->codec;
#endif
            AVCodec *audio_codec = (AVCodec *)avcodec_find_decoder(codec_ctx->codec_id);
            if(!audio_codec) {
                zAudioIndex = -1;
                GN_Log("没找到对应的音频解码器!\n");
            } else {
                if(avcodec_open(codec_ctx, audio_codec) < 0) {
                    zAudioIndex = -1;
                    GN_Log("音频解码器打开失败\n");
                } else {
                    unsigned short bit = 0xffff;
                    switch(codec_ctx->sample_fmt) {
                        case 0:
                        case 5:
                            bit = 8;
                            break;
                        case 1:
                        case 6:
#ifdef US_FFMPEG4
                        case AV_SAMPLE_FMT_FLT:
                        case AV_SAMPLE_FMT_FLTP:
#endif
                            bit = 16;
                            break;
                    }
                    if(bit > 16) {
                        GN_Log("%s 使用了还未支持的音频格式! 采样率:%d\n", filename, codec_ctx->sample_rate);
                        avcodec_free_context(&codec_ctx);
                        zAudioIndex = -1;
                    } else {
                        zBgm.New(1);
                        zBgm->Init(codec_ctx, bit, w * h * (int)zfFrameRate);
                        zCrsFrameNum = 1000000;
                    }
                }
            }
        }
        /*-----------------------------------------------------------------------------------------------------*/
        int cpu_flags = SWS_BICUBIC; //yuv_sws_get_cpu_flags();
        if(zpSwScaleContext) {
            sws_freeContext(zpSwScaleContext);
            zpSwScaleContext = 0;
        }
        zpSwScaleContext = sws_getContext(w, h, zpCodecContext->pix_fmt,
                                          w, h, PIX_FMT_RGB24,
                                          cpu_flags, //SWS_FAST_BILINEAR,
                                          0, 0, 0);

        if(!zpSwScaleContext) {
            GN_Log("像素颜色格式转换器初始化失败!\n");
            return false;
        }
#ifdef US_FFMPEG4
        zPacket = av_packet_alloc();
        zPacket2 = av_packet_alloc();
#endif
        /*------------------------------------------ 设置纹理 -------------------------------------------------*/
        if(!zImage) zImage.New(1);
        GNg::sIMAGE *_img = zImage;
        _img->CreateEmptyToData(w, h, 1, GNg::CR_RGB, 0);
        _img->fileName = new char[strlen(zFileName) + 16];
        strcpy(_img->fileName, zFileName);
        strcat(_img->fileName, " frame buffer");
        if(zAudioIndex != -1) {
            memset(zpAVFrameRGBBuffer, 0, numBytes);
            _img->DataFill(zpAVFrameRGBBuffer, GNg::CR_RGB, w, h, 0, 0);
        }
        zMediaOpen = true;
        char *file_ext_name = GN_StrGetExtName(zFileName); //如果是播放网络文件且是avi大数据文件就建立网络缓冲
        if(file_ext_name && strcmp(file_ext_name, "flv") && zpFile->offset < 0 && zpFile->offset > -1024) zNetBuf.New(1); //网络文件

    } //end 是否打开相同的文件检测
    Play();
    zLoop_v = loop;
    zSyncTime = sync_time;
    return true;
}
int GT_cVIDEO::zReadPacket(unsigned int frame_id) {
//if( zPacket.data )av_free_packet( &zPacket );
#ifdef US_FFMPEG4
    AVPacket *packet = zPacket;
#else
    AVPacket packet0;
    AVPacket *packet = &packet0;
#endif
    for(;;) { // ::GN_Log("av_read_frame\n");
        if(!av_read_frame(zpContext, packet)) { //返回0代表顺利读取数据包
            if(packet->stream_index == zVideoIndex) {
                int ret1;
                if(zSyncTime && frame_id > zFrameNumber + zMaxAccumFrameN) {
                    ++zFrameNumber;
                    ++zRecvFrameNum;
                    ret1 = 0;
                } else {
                    if(zAudioIndex == -1) {
                        ret1 = this->zDecodePacket(packet->data, packet->size);
                        zFrameNumber += ret1;
                    } else {
                        zBgm->aVBuf.Write((uint8 *)&(packet->size), 4);
                        zBgm->aVBuf.Write(packet->data, packet->size);
                        ++zFrameNumber;
                        ret1 = 2;
                    }
                }
                av_free_packet(packet);
                return ret1;
            } else {
                if(packet->stream_index == zAudioIndex && zSpeedRatio < 1.0001f) { //这一帧是音频
#ifdef US_FFMPEG4
                    switch(zBgm->DecodeAudio(packet)) {
#else
                    switch(zBgm->DecodeAudio(packet->data, packet->size)) {
#endif
                        case 1: zCrsFrameNum = zFrameNumber + 1; break;
                        case -1:
                            GN_Log("初始化音频设备有错!\n");
                            zBgm.Delete();
                            zAudioIndex = -1;
                            break;
                    }
                }
                av_free_packet(packet);
                continue;
            }
        }
        zEndOfFilm = true; //没有数据包或读错误 标记结束
        // 让 UpdataToImage 的循环检测来重启 (避免在 av_read_frame 失败循环中死锁)
        zIsPlaying = false;
        return -1;
    }
    return 0;
}
int GT_cVIDEO::zDecodePacket(uint8 *data, int size) {
    int igot_frame = 0;
#ifdef US_FFMPEG4
    zPacket2->data = data;
    zPacket2->size = size;
#if 1 /* FFmpeg 6.x */
    igot_frame = 0;
    int len = avcodec_send_packet(zpCodecContext, zPacket2);
    if(!len) {
        while(avcodec_receive_frame(zpCodecContext, zpAVFrame) == 0) {
            igot_frame = 1;
            break;
        }
    }
#else
    // Modern: avcodec_decode_video2 removed
    int len = 0;
    if(avcodec_send_packet(zpCodecContext, zPacket2) == 0) {
        if(avcodec_receive_frame(zpCodecContext, zpAVFrame) == 0) {
            igot_frame = 1;
            len = zPacket2->size;
        }
    }
#endif
#else
    int len = avcodec_decode_video(zpCodecContext, zpAVFrame, &igot_frame, data, size);
#endif
    if(len == -1) {
        GN_Log("解码视频帧 %d 失败\n", zFrameNumber);
        return -1;
    }
    if(!igot_frame) return 0;
    return 1;
}
int GT_cVIDEO::zDecodeFrame() {
    if(!zIsPlaying) return 0;
    int ret;
    unsigned int frameNum = (unsigned int)(zdwNowTime / zfFrameDelay);
    if(frameNum == zFrameNumber) return 0; //还没到时间播放下一帧
    else if((unsigned int)(frameNum - zFrameNumber) < (unsigned int)(zfFrameRate * 5)) {
        do { //GN_Log("zFrameNumber = %d\n", zFrameNumber);
            ret = this->zReadPacket(frameNum); //追赶回应该播放到的帧数
        } while(!ret);
        //GN_Log("      zFrameNumber = %d\n", zFrameNumber);
        return ret;
    } else {
        if(zdwNowTime >= zdwEndTime) zdwNowTime = (zdwEndTime - 8000); //以防跳文件指针出轨
        ret = av_seek_frame(zpContext, -1, zdwNowTime * (AV_TIME_BASE / 1000), 0); //AVSEEK_FLAG_BACKWARD); //时间距离太远跳转去另处
        if(ret < 0) {
            GN_Log("视频播放跳转失败\n");
            return -1;
        }
        zFrameNumber = frameNum;

        if(zAudioIndex != -1) {
            zRecvFrameNum = frameNum;
            zBgm->aVBuf.Clear();
        }
        return 0;
    }
}
void GT_cVIDEO::zRecvDecodeFrame() {
    if(zFrameNumber - zRecvFrameNum > zCrsFrameNum) {
        uint8 *pk_data;
        unsigned int pk_size;
        GT_cVAUDIO *_bgm = zBgm;
    GT_GOTO_ReadNextFrameData:
        if(_bgm->aVBuf.Read((uint8 *)&pk_size, 4)) {
            pk_data = _bgm->aVBuf.GetOut(pk_size);
            /*if(pk_size)*/ {
                ++zRecvFrameNum;
                switch(zDecodePacket(pk_data, pk_size)) {
                    case 1: zGetFrame(); break;
                    case 0: goto GT_GOTO_ReadNextFrameData;
                }
            }
        }
    }
}
void GT_cVIDEO::zGetFrame() {
    int w = zpCodecContext->width, h = zpCodecContext->height;
    //GN_RUNTSTART(1);
    int ret = sws_scale(zpSwScaleContext, zpAVFrame->data, zpAVFrame->linesize, 0, h, zpAVFrameRGB->data, zpAVFrameRGB->linesize); //转换传输象素格式
    //GN_RUNTEND;
    zImage->DataFill(zpAVFrameRGB->data[0], GNg::CR_RGB, w, h);
    //if(zPacket.data)av_free_packet( &zPacket );
}
void GT_cVIDEO::Stop() {
    zIsPlaying = 0;
    zLoop_v = 0;
}
void GT_cVIDEO::Play() {
    zIsPlaying = 1;
    if(zIsPause) zIsPause = false;
    else {
        zFrameNumber = 1000000;
        zdwNowTime = 0;
        zEndOfFilm = false;
        // 重置 IO 和解复用器状态 (支持 EOF 后循环重播)
        if(zpContext && zpContext->pb) {
            avio_seek(zpContext->pb, 0, SEEK_SET);
            avformat_flush(zpContext);
        }
    }
}
void GT_cVIDEO::Pause() {
    zIsPause = (!zIsPause);
}
void GT_cVIDEO::AddPlayCount(int loop) {
    if(zLoop_v) zLoop_v = zLoop_v + loop;
    else zLoop_v = zLoop_v + loop + 1;
}
bool GT_cVIDEO::IsPlaying() {
    return zIsPlaying;
}
void GT_cVIDEO::SetStage(float state) {
    zdwNowTime = (unsigned int)(zdwEndTime * state);
}
void GT_cVIDEO::SetSpeed(float speed) {
    zSpeedRatio = speed;
    zMaxAccumFrameN = (unsigned int)(13.0f * speed);
    GT_cVAUDIO *_bgm = zBgm;
    if(_bgm) {
        _bgm->zSetPitch(speed);
        _bgm->zSetVolume(speed > 1.0f ? 0.0f : _bgm->zVolume);
        zRecvFrameNum = zFrameNumber;
    }
}
void GT_cVIDEO::SetVolume(float vol) {
    if(zBgm) {
        zBgm->zVolume = vol;
        zBgm->zSetVolume(vol);
    }
}
unsigned int GT_cVIDEO::GetEndTime() {
    return zdwEndTime;
}
unsigned int GT_cVIDEO::GetNowTime() {
    return zdwNowTime;
}
P<GNg::sIMAGE> &GT_cVIDEO::GetImage() {
    return zImage;
}

unsigned int GT_cVIDEO::UpdataToImage(unsigned int updatatime) {
    if(zMediaOpen) {
        if(!zBusy) {
            if(zIsPlaying) {
                if(zAudioIndex != -1) zBgm->zUpdateStream(updatatime, zLoop_v ? true : false);
            } else { //非正在播放
                if(zLoop_v > 1 || zLoop_v < 0) {
                    --zLoop_v;
                    if(zLoop_v < -10000) zLoop_v = -10000;
                    Play();
                    if(zAudioIndex != -1) zBgm->zUpdateStream(updatatime, zLoop_v ? true : false);
                }
            }
            if(zDecodeFrame() == 1) zGetFrame(); //到时间获取帧了
            else if(zAudioIndex != -1) zRecvDecodeFrame();
            if(!zIsPause) {
                unsigned int fdelay = (unsigned int)zfFrameDelay;
                zdwNowTime += (unsigned int)((float)((zSyncTime || updatatime < fdelay) ? updatatime : fdelay) * zSpeedRatio);
            }
        }
        zBusy = false;
        if(zNetBuf) {
            GT_s_NET_BUF *_net_buf = zNetBuf;
            if(_net_buf->readed_pos < GNf::GetSize(zpFile)) {
                //GN_RunValueCheck2(_net_buf->readed_pos, 10);
                //GN_RunValueCheck2(_net_buf->played_pos, 11);
                //GN_RunValueCheck2(zdwNowTime,			  12);
                int _size_ = _net_buf->readed_pos - _net_buf->played_pos;
                if(_size_ < (GT_AVI_NET_BUF_SIZE >> 1)) {
                    GT_AVI_NetBufRead(P_(), this, 0); //提前预读数据
                    if(_size_ < 1024 * 512) zBusy = true; //缓存量不比播放量大512K的话就标记忙等 暂停一下 等读取多点数据才继续播放
                }
            }
        }
    }
    return (zIsPlaying || zLoop_v > 1 || zLoop_v < 0) ? zdwNowTime : 0;
}
unsigned int GT_cVIDEO::UpdataPlay(unsigned int updatatime, int x, int y, int w, int h, unsigned int color, unsigned short blendmode) {
    unsigned int ret_time = UpdataToImage(updatatime);
    if(zMediaOpen) {
        GNg::ImageDraw(zImage, x, y, w, h, color, 0, blendmode);
        if(zBusy) {
            int dw, dh;
            GN_RunProgressSizeGet(dw, dh);
            GN_RunProgressDraw((float)(GN::TimeGetRunCount() >> 4) * 0.007f, 0, true,
                               GNg::ViewCur->ortho_left + (dw >> 1) + 20,
                               GNg::ViewCur->ortho_top + (dh >> 1) + 10,
                               0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 1);
        }
    }
    return ret_time;
}
#endif //_WIN32_WCE
/****************************************************************************** cVIDEO ****************************************************************************/
GNm_API char *const GNm_GetVersion() {
    static const char GNm_Version[] = GNm_VERSION;
    return (char *const)GNm_Version;
}
GNm::cVIDEO::cVIDEO() {
    z__attrb = (void *)new GT_cVIDEO;
}
GNm::cVIDEO::~cVIDEO() {
    if(z__attrb) {
        delete((GT_cVIDEO *)z__attrb);
        z__attrb = 0;
    }
}
void GNm::cVIDEO::Release() {
    ((GT_cVIDEO *)z__attrb)->Release();
}
bool GNm::cVIDEO::InitToData(const char *filename, int loop, bool sync_time, bool audio) {
    return ((GT_cVIDEO *)z__attrb)->InitToData(filename, loop, sync_time, audio);
}
void GNm::cVIDEO::Stop() {
    ((GT_cVIDEO *)z__attrb)->Stop();
}
void GNm::cVIDEO::Play() {
    ((GT_cVIDEO *)z__attrb)->Play();
}
void GNm::cVIDEO::Pause() {
    ((GT_cVIDEO *)z__attrb)->Pause();
}
void GNm::cVIDEO::AddPlayCount(int loop) {
    ((GT_cVIDEO *)z__attrb)->AddPlayCount(loop);
}
bool GNm::cVIDEO::IsPlaying() {
    return ((GT_cVIDEO *)z__attrb)->IsPlaying();
}
bool GNm::cVIDEO::IsPause() {
    return ((GT_cVIDEO *)z__attrb)->IsPause();
}
bool GNm::cVIDEO::IsBusy() {
    return ((GT_cVIDEO *)z__attrb)->IsBusy();
}
void GNm::cVIDEO::SetStage(float state) {
    ((GT_cVIDEO *)z__attrb)->SetStage(state);
}
void GNm::cVIDEO::SetSpeed(float speed) {
    ((GT_cVIDEO *)z__attrb)->SetSpeed(speed);
}
void GNm::cVIDEO::SetVolume(float vol) {
    ((GT_cVIDEO *)z__attrb)->SetVolume(vol);
}
unsigned int GNm::cVIDEO::GetEndTime() {
    return ((GT_cVIDEO *)z__attrb)->GetEndTime();
}
unsigned int GNm::cVIDEO::GetNowTime() {
    return ((GT_cVIDEO *)z__attrb)->GetNowTime();
}
int GNm::cVIDEO::GetLoopNum() {
    return ((GT_cVIDEO *)z__attrb)->GetLoopNum();
}
P<GNg::sIMAGE> GNm::cVIDEO::GetImage() {
    return ((GT_cVIDEO *)z__attrb)->GetImage();
}
unsigned int GNm::cVIDEO::UpdataToImage(unsigned int updatatime) {
    return ((GT_cVIDEO *)z__attrb)->UpdataToImage(updatatime);
}
unsigned int GNm::cVIDEO::UpdataPlay(unsigned int updatatime, int x, int y, int w, int h, unsigned int color, unsigned short blendmode) {
    return ((GT_cVIDEO *)z__attrb)->UpdataPlay(updatatime, x, y, w, h, color, blendmode);
}
float GNm::cVIDEO::GetVolume() {
    return ((GT_cVIDEO *)z__attrb)->GetVolume();
}
unsigned int GNm::cVIDEO::GetFreq() {
    return ((GT_cVIDEO *)z__attrb)->GetFreq();
}
unsigned short GNm::cVIDEO::GetBit() {
    return ((GT_cVIDEO *)z__attrb)->GetBit();
}
unsigned short GNm::cVIDEO::GetChannel() {
    return ((GT_cVIDEO *)z__attrb)->GetChannel();
}
char *GNm::cVIDEO::GetFileName() {
    return ((GT_cVIDEO *)z__attrb)->GetFileName();
}
#endif // _GN_MEDIA_H_
