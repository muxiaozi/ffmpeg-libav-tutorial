#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>

#include "video_debugging.h"

#define ERR_EXIT(ret, format, ...) \
    if (ret < 0) \
    { \
        logging(format##", err: %s", __VA_ARGS__, av_err2str(ret)); \
        exit(1); \
    }

typedef struct EncodeContext
{
    AVFormatContext *src_avfc;
    int src_video_stream_index;
    AVStream *src_video_stream;
    AVCodecContext *src_video_avcc;

    AVFormatContext *dst_avfc;
    AVCodecContext *dst_avcc;
} EncodeContext;

/**
 * 打开输入文件
 */
int open_input_media(EncodeContext *ctx, const char *url, const char *input_fmt)
{
    int ret;
    AVInputFormat *avif = av_find_input_format(input_fmt);
    if (!avif)
    {
        logging("input format not found: '%s'", input_fmt);
        return AVERROR_DEMUXER_NOT_FOUND;
    }

    ret = avformat_open_input(&ctx->src_avfc, url, avif, NULL);
    if (ret < 0)
        return ret;

    ret = avformat_find_stream_info(ctx->src_avfc, NULL);
    if (ret < 0)
        return ret;

    av_dump_format(ctx->src_avfc, 0, url, 0);

    ret = av_find_best_stream(ctx->src_avfc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0)
        return ret;
    ctx->src_video_stream_index = ret;
    ctx->src_video_stream = ctx->src_avfc->streams[ctx->src_video_stream_index];

    AVCodec *video_avc = avcodec_find_decoder(ctx->src_video_stream->codecpar->codec_id);
    ctx->src_video_avcc = avcodec_alloc_context3(video_avc);
    ret = avcodec_parameters_to_context(ctx->src_video_avcc, ctx->src_video_stream->codecpar);
    if (ret < 0)
        return ret;
    ret = avcodec_open2(ctx->src_video_avcc, video_avc, NULL);
    if (ret < 0)
        return ret;

    return 0;
}

int open_output_media(EncodeContext *ctx, const char *filename)
{
    int ret;
    ret = avformat_alloc_output_context2(&ctx->dst_avfc, NULL, NULL, filename);
    if (ret < 0)
        return ret;
    
    AVCodec *video_avc = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVStream *avs = avformat_new_stream(ctx->dst_avfc, video_avc);

    ctx->dst_avcc = avcodec_alloc_context3(video_avc);
    ctx->dst_avcc->width = ctx->src_video_stream->codecpar->width;
    ctx->dst_avcc->height = ctx->src_video_stream->codecpar->height;
    ctx->dst_avcc->bit_rate = ctx->src_video_stream->codecpar->bit_rate;
    ctx->dst_avcc->time_base = ctx->src_video_stream->time_base;
    if (video_avc->pix_fmts)
        ctx->dst_avcc->pix_fmt = video_avc->pix_fmts[0];
    else
        ctx->dst_avcc->pix_fmt = ctx->src_video_avcc->pix_fmt;

    avcodec_open2(ctx->dst_avcc, video_avc, NULL);
    
    if (ctx->dst_avfc->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->dst_avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(ctx->dst_avfc->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(ctx->dst_avfc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            logging("can't open file '%s'", filename);
            return ret;
        }
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    int ret;
    EncodeContext ctx;
    memset(&ctx, 0, sizeof(EncodeContext));

    avdevice_register_all();

    ret = open_input_media(&ctx, "video=XiaoMi USB 2.0 Webcam", "dshow");
    ERR_EXIT(ret, "open input media");

    ret = open_output_media(&ctx, "output.mp4");
    ERR_EXIT(ret, "open output media");

    return 0;
}