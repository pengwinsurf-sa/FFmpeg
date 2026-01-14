#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

int Test_ff_codec_desc_free() {
    // Initialize FFmpeg libraries
    av_register_all();
    avformat_network_init();

    // Begin function parameters
    const AVCodecDescriptor *codec_desc = avcodec_descriptor_next(NULL);
    // End function parameters
    const AVCodecDescriptor *current_desc = codec_desc;
    while (current_desc != NULL) {
        const AVCodecDescriptor *next_desc = avcodec_descriptor_next(current_desc);
        // No need to free codec descriptors, they are statically allocated
        current_desc = next_desc;
    }

    // Clean up FFmpeg libraries
    avformat_network_deinit();

    return 0;
}

int main() {
    return Test_ff_codec_desc_free();
}
