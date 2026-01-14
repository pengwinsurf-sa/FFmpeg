#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>

typedef struct {
    AVFormatContext *fmt;
    struct mp_decode {
        AVStream *stream;
        AVCodec *codec;
        AVFrame *sw_frame;
        AVFrame *hw_frame;
    } v, a;
} mp_media_t;

int Test_mp_decode_init() {
    // Begin function parameters
    enum AVMediaType type = AVMEDIA_TYPE_VIDEO; // Example initialization
    mp_media_t *m = av_mallocz(sizeof(mp_media_t)); // Allocate and zero-initialize
    // End function parameters

    // Initialize network components
    avformat_network_init();

    // Open input file and allocate format context
    if (avformat_open_input(&m->fmt, "input.mp4", NULL, NULL) < 0) {
        av_free(m);
        return -1; // Error opening input
    }

    // Determine the appropriate decode structure based on media type
    struct mp_decode *d = (type == AVMEDIA_TYPE_VIDEO) ? &m->v : &m->a;
    AVStream *stream;

    // Initialize the decode structure
    memset(d, 0, sizeof(*d));

    // Find the best stream for the given media type
    int ret = av_find_best_stream(m->fmt, type, -1, -1, NULL, 0);
    if (ret < 0) {
        avformat_close_input(&m->fmt);
        av_free(m);
        return -1; // Error finding stream
    }
    stream = d->stream = m->fmt->streams[ret];

    // Get the codec ID from the stream
    enum AVCodecID id = stream->codecpar->codec_id;

    // Check for alpha mode in stream metadata
    AVDictionaryEntry *tag =
        av_dict_get(stream->metadata, "alpha_mode", NULL, AV_DICT_IGNORE_SUFFIX);

    // Determine the codec based on codec ID
    const char *codec_name = (id == AV_CODEC_ID_VP8) ? "libvpx" : "libvpx-vp9";
    d->codec = avcodec_find_decoder_by_name(codec_name);

    // Fallback to finding decoder by codec ID
    if (!d->codec) {
        d->codec = avcodec_find_decoder(id);
    }

    // Allocate frames for software and hardware decoding
    d->sw_frame = av_frame_alloc();
    d->hw_frame = av_frame_alloc();

    // Log media type for debugging purposes
    av_log(NULL, AV_LOG_INFO, "Media type: %s\n", av_get_media_type_string(type));

    // Clean up
    av_frame_free(&d->sw_frame);
    av_frame_free(&d->hw_frame);
    avformat_close_input(&m->fmt);
    av_free(m);

    return 0;
}

int main() {
    return Test_mp_decode_init();
}
