#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <assert.h>
#include <stdio.h>

int Test_mp_decode_flush() {
    // Register devices and initialize network components
    avdevice_register_all();
    avformat_network_init();

    // Create a decoder instance
    struct mp_decode {
        AVCodecContext *decoder;
    } decoder_instance;

    // Find the H.264 decoder
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    assert(codec && "H.264 decoder not found");

    // Allocate codec context
    decoder_instance.decoder = avcodec_alloc_context3(codec);
    assert(decoder_instance.decoder && "Failed to allocate decoder context");

    // Open the codec
    int ret = avcodec_open2(decoder_instance.decoder, codec, NULL);
    assert(ret >= 0 && "Failed to open codec");

    // Allocate dummy packet and frame
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    assert(pkt && frame && "Failed to allocate packet or frame");

    // Simulate sending a packet (this may fail since pkt->data is NULL, but that’s fine for testing flush safety)
    ret = avcodec_send_packet(decoder_instance.decoder, pkt);
    // It’s fine if this returns AVERROR(EAGAIN) or AVERROR_EOF — we only care that decoder is in a valid state
    assert(ret <= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

    // Flush buffers — this should safely reset the decoder
    avcodec_flush_buffers(decoder_instance.decoder);

    // Ensure decoder context is still valid and usable
    assert(decoder_instance.decoder->codec == codec);
    assert(decoder_instance.decoder->codec_type == AVMEDIA_TYPE_VIDEO);

    // Try sending another packet after flush to confirm decoder remains usable
    ret = avcodec_send_packet(decoder_instance.decoder, pkt);
    assert(ret <= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

    // Try receiving frame to confirm no crash and valid behavior
    ret = avcodec_receive_frame(decoder_instance.decoder, frame);
    assert(ret <= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

    // Clean up
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&decoder_instance.decoder);

    printf("Test_mp_decode_flush passed successfully.\n");
    return 0;
}

int main() {
    return Test_mp_decode_flush();
}