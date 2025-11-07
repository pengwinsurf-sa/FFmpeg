#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/buffer.h>

struct mp_decode {
    AVCodecContext *decoder;
    AVFrame *hw_frame;
    AVFrame *sw_frame;
    AVBufferRef *hw_ctx;
};

void mp_decode_clear_packets(struct mp_decode *d) {
    // Implementation to clear packets
}

int Test_mp_decode_free() {
    struct mp_decode *d = (struct mp_decode *)av_mallocz(sizeof(struct mp_decode));
    if (!d) return -1;

    // Initialize decoder context
    d->decoder = avcodec_alloc_context3(NULL);
    if (!d->decoder) {
        av_free(d);
        return -1;
    }

    // Initialize frames
    d->hw_frame = av_frame_alloc();
    d->sw_frame = av_frame_alloc();
    if (!d->hw_frame || !d->sw_frame) {
        avcodec_free_context(&d->decoder);
        av_free(d);
        return -1;
    }

    // Initialize hardware context
    d->hw_ctx = av_buffer_alloc(1024); // Example size
    if (!d->hw_ctx) {
        av_frame_free(&d->hw_frame);
        av_frame_free(&d->sw_frame);
        avcodec_free_context(&d->decoder);
        av_free(d);
        return -1;
    }

    // Clear packets
    mp_decode_clear_packets(d);

    // Free hardware frame if it exists
    if (d->hw_frame != NULL) {
        av_frame_unref(d->hw_frame);
        av_frame_free(&d->hw_frame);
    }

    // Free decoder context if it exists
    if (d->decoder != NULL) {
        avcodec_free_context(&d->decoder);
    }

    // Free software frame if it exists
    if (d->sw_frame != NULL) {
        av_frame_unref(d->sw_frame);
        av_frame_free(&d->sw_frame);
    }

    // Unreference hardware context if it exists
    if (d->hw_ctx != NULL) {
        av_buffer_unref(&d->hw_ctx);
    }

    // Free the mp_decode structure
    av_free(d);

    return 0;
}

int main() {
    return Test_mp_decode_free();
}
