/*
 * Copyright (c) 2015 Ludmila Glinskih
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * av_image_alloc test.
 */

#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to create a minimal valid video file
static int create_test_video_file(const char *filename)
{
    printf("[DEBUG] Creating test video file: %s\n", filename);
    
    AVFormatContext *fmt_ctx = NULL;
    AVStream *stream = NULL;
    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    int ret = 0;
    int i;
    
    // Find encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
    if (!codec) {
        printf("[DEBUG] MPEG1VIDEO encoder not found, trying MPEG2VIDEO\n");
        codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    }
    if (!codec) {
        printf("[DEBUG] No suitable encoder found, trying rawvideo\n");
        codec = avcodec_find_encoder(AV_CODEC_ID_RAWVIDEO);
    }
    if (!codec) {
        printf("[DEBUG] ERROR: No encoder available\n");
        return -1;
    }
    printf("[DEBUG] Using encoder: %s\n", codec->name);
    
    // Allocate output format context
    ret = avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not allocate output context: %d\n", ret);
        return ret;
    }
    
    // Create stream
    stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        printf("[DEBUG] ERROR: Could not create stream\n");
        avformat_free_context(fmt_ctx);
        return -1;
    }
    
    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        printf("[DEBUG] ERROR: Could not allocate codec context\n");
        avformat_free_context(fmt_ctx);
        return -1;
    }
    
    // Set codec parameters
    codec_ctx->codec_id = codec->id;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = 1920;
    codec_ctx->height = 1080;
    codec_ctx->time_base = (AVRational){1, 25};
    codec_ctx->framerate = (AVRational){25, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = 400000;
    codec_ctx->gop_size = 10;
    codec_ctx->max_b_frames = 0;
    
    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    
    // Open codec
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not open codec: %d\n", ret);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not copy codec parameters: %d\n", ret);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    stream->time_base = codec_ctx->time_base;
    
    // Open output file
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf("[DEBUG] ERROR: Could not open output file: %d\n", ret);
            avcodec_free_context(&codec_ctx);
            avformat_free_context(fmt_ctx);
            return ret;
        }
    }
    
    // Write header
    ret = avformat_write_header(fmt_ctx, NULL);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not write header: %d\n", ret);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    // Allocate frame
    frame = av_frame_alloc();
    if (!frame) {
        printf("[DEBUG] ERROR: Could not allocate frame\n");
        av_write_trailer(fmt_ctx);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return -1;
    }
    
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not allocate frame buffer: %d\n", ret);
        av_frame_free(&frame);
        av_write_trailer(fmt_ctx);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    // Allocate packet
    pkt = av_packet_alloc();
    if (!pkt) {
        printf("[DEBUG] ERROR: Could not allocate packet\n");
        av_frame_free(&frame);
        av_write_trailer(fmt_ctx);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return -1;
    }
    
    // Encode a few frames
    for (i = 0; i < 5; i++) {
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            break;
        
        // Fill frame with dummy data
        memset(frame->data[0], i * 10, frame->linesize[0] * codec_ctx->height);
        memset(frame->data[1], 128, frame->linesize[1] * codec_ctx->height / 2);
        memset(frame->data[2], 128, frame->linesize[2] * codec_ctx->height / 2);
        
        frame->pts = i;
        
        // Send frame
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0)
            break;
        
        // Receive packets
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                ret = 0;
                break;
            } else if (ret < 0) {
                break;
            }
            
            av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            
            ret = av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
            if (ret < 0)
                break;
        }
        
        if (ret < 0)
            break;
    }
    
    // Flush encoder
    avcodec_send_frame(codec_ctx, NULL);
    while (1) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            break;
        if (ret >= 0) {
            av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
        }
    }
    
    // Write trailer
    av_write_trailer(fmt_ctx);
    
    // Cleanup
    av_packet_free(&pkt);
    av_frame_free(&frame);
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmt_ctx->pb);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    
    printf("[DEBUG] Test video file created successfully\n");
    return 0;
}

static int image_alloc_example(const char *input_filename)
{
    printf("[DEBUG] Starting image_alloc_example function\n");
    printf("[DEBUG] Input filename: %s\n", input_filename);
    
    const AVCodec *codec = NULL;
    AVCodecContext *ctx= NULL;
    AVCodecParameters *origin_par = NULL;
    AVFormatContext *fmt_ctx = NULL;
    int video_stream;
    int result;
    
    // Initialize parameters for sws_getContext
    int srcW = 1920;
    int srcH = 1080;
    enum AVPixelFormat srcFormat = AV_PIX_FMT_YUV420P;
    int dstW = 1280;
    int dstH = 720;
    enum AVPixelFormat dstFormat = AV_PIX_FMT_RGB24;
    int flags = SWS_BILINEAR;
    SwsFilter *srcFilter = NULL;
    SwsFilter *dstFilter = NULL;
    const double *param = NULL;
    struct SwsContext *sws_ctx = NULL;
    
    printf("[DEBUG] Initialized sws parameters: srcW=%d, srcH=%d, srcFormat=%d, dstW=%d, dstH=%d, dstFormat=%d\n",
           srcW, srcH, srcFormat, dstW, dstH, dstFormat);
    
    // Initialize parameters for av_image_alloc for destination
    uint8_t *dst_pointers[4] = {NULL};
    int dst_linesizes[4] = {0};
    int align = 32;
    int buffer_size;
    
    // Initialize parameters for av_image_alloc for source
    uint8_t *src_pointers[4] = {NULL};
    int src_linesizes[4] = {0};
    int src_buffer_size;
    int i;

    printf("[DEBUG] Calling avformat_open_input with filename: %s\n", input_filename);
    result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    printf("[DEBUG] avformat_open_input returned: %d\n", result);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        printf("[DEBUG] ERROR: Failed to open input file, returning %d\n", result);
        return result;
    }
    printf("[DEBUG] Successfully opened input file\n");

    printf("[DEBUG] Calling avformat_find_stream_info\n");
    result = avformat_find_stream_info(fmt_ctx, NULL);
    printf("[DEBUG] avformat_find_stream_info returned: %d\n", result);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        printf("[DEBUG] ERROR: Failed to get stream info, returning %d\n", result);
        return result;
    }
    printf("[DEBUG] Successfully retrieved stream info\n");

    printf("[DEBUG] Calling av_find_best_stream for video\n");
    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    printf("[DEBUG] av_find_best_stream returned: %d\n", video_stream);
    if (video_stream < 0) {
      av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      printf("[DEBUG] ERROR: No video stream found, returning -1\n");
      return -1;
    }
    printf("[DEBUG] Found video stream at index: %d\n", video_stream);

    origin_par = fmt_ctx->streams[video_stream]->codecpar;
    printf("[DEBUG] Retrieved codec parameters from stream %d\n", video_stream);
    printf("[DEBUG] Codec ID: %d, Width: %d, Height: %d, Format: %d\n", 
           origin_par->codec_id, origin_par->width, origin_par->height, origin_par->format);

    printf("[DEBUG] Calling avcodec_find_decoder with codec_id: %d\n", origin_par->codec_id);
    codec = avcodec_find_decoder(origin_par->codec_id);
    printf("[DEBUG] avcodec_find_decoder returned: %p\n", (void*)codec);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        printf("[DEBUG] ERROR: Decoder not found, returning -1\n");
        return -1;
    }
    printf("[DEBUG] Successfully found decoder: %s\n", codec->name);

    printf("[DEBUG] Calling avcodec_alloc_context3\n");
    ctx = avcodec_alloc_context3(codec);
    printf("[DEBUG] avcodec_alloc_context3 returned: %p\n", (void*)ctx);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
        printf("[DEBUG] ERROR: Failed to allocate decoder context, returning ENOMEM\n");
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] Successfully allocated decoder context\n");

    printf("[DEBUG] Calling avcodec_parameters_to_context\n");
    result = avcodec_parameters_to_context(ctx, origin_par);
    printf("[DEBUG] avcodec_parameters_to_context returned: %d\n", result);
    if (result) {
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        printf("[DEBUG] ERROR: Failed to copy decoder context, returning %d\n", result);
        return result;
    }
    printf("[DEBUG] Successfully copied parameters to context\n");

    printf("[DEBUG] Calling avcodec_open2\n");
    result = avcodec_open2(ctx, codec, NULL);
    printf("[DEBUG] avcodec_open2 returned: %d\n", result);
    if (result < 0) {
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        printf("[DEBUG] ERROR: Failed to open decoder, returning %d\n", result);
        return result;
    }
    printf("[DEBUG] Successfully opened decoder\n");
    
    // Call sws_getContext
    printf("[DEBUG] Calling sws_getContext with srcW=%d, srcH=%d, srcFormat=%d, dstW=%d, dstH=%d, dstFormat=%d, flags=%d\n",
           srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags);
    sws_ctx = sws_getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags, srcFilter, dstFilter, param);
    printf("[DEBUG] sws_getContext returned: %p\n", (void*)sws_ctx);
    
    if (!sws_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to create sws_ctx\n");
        printf("[DEBUG] ERROR: sws_getContext failed, cleaning up and returning -1\n");
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Successfully created sws context\n");
    
    // Call av_image_alloc for destination
    printf("[DEBUG] Calling av_image_alloc for destination: dstW=%d, dstH=%d, dstFormat=%d, align=%d\n",
           dstW, dstH, dstFormat, align);
    buffer_size = av_image_alloc(dst_pointers, dst_linesizes, dstW, dstH, dstFormat, align);
    printf("[DEBUG] av_image_alloc (destination) returned buffer_size: %d\n", buffer_size);
    
    // Check return value is non-negative (size in bytes required for the image buffer)
    if (buffer_size < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate destination image buffer: negative error code %d\n", buffer_size);
        printf("[DEBUG] ERROR: Destination buffer allocation failed with error code %d\n", buffer_size);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Destination buffer allocation succeeded\n");
    
    // Verify pointers array is filled with pointer for each image plane
    printf("[DEBUG] Checking destination pointers: [0]=%p, [1]=%p, [2]=%p, [3]=%p\n",
           (void*)dst_pointers[0], (void*)dst_pointers[1], (void*)dst_pointers[2], (void*)dst_pointers[3]);
    if (dst_pointers[0] == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Destination buffer pointer[0] is NULL - pointers array not filled correctly\n");
        printf("[DEBUG] ERROR: dst_pointers[0] is NULL\n");
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Destination pointer[0] is valid\n");
    
    // Verify linesizes array is filled
    printf("[DEBUG] Destination linesizes: [0]=%d, [1]=%d, [2]=%d, [3]=%d\n",
           dst_linesizes[0], dst_linesizes[1], dst_linesizes[2], dst_linesizes[3]);
    if (dst_linesizes[0] <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Destination linesizes[0] is invalid: %d - linesizes array not filled correctly\n", dst_linesizes[0]);
        printf("[DEBUG] ERROR: dst_linesizes[0] is invalid: %d\n", dst_linesizes[0]);
        av_freep(&dst_pointers[0]);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Destination linesize[0] is valid\n");
    
    // Verify buffer_size is positive and reasonable
    if (buffer_size == 0) {
        av_log(NULL, AV_LOG_ERROR, "Buffer size is zero - unexpected for valid image dimensions\n");
        printf("[DEBUG] ERROR: Destination buffer_size is zero\n");
        av_freep(&dst_pointers[0]);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Destination buffer_size is valid (non-zero)\n");
    
    printf("Destination buffer allocated successfully: pointer=%p, linesize=%d, size=%d\n", 
           (void*)dst_pointers[0], dst_linesizes[0], buffer_size);
    
    // Call av_image_alloc for source
    printf("[DEBUG] Calling av_image_alloc for source: srcW=%d, srcH=%d, srcFormat=%d, align=%d\n",
           srcW, srcH, srcFormat, align);
    src_buffer_size = av_image_alloc(src_pointers, src_linesizes, srcW, srcH, srcFormat, align);
    printf("[DEBUG] av_image_alloc (source) returned buffer_size: %d\n", src_buffer_size);
    
    // Check return value is non-negative (size in bytes required for the image buffer)
    if (src_buffer_size < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate source image buffer: negative error code %d\n", src_buffer_size);
        printf("[DEBUG] ERROR: Source buffer allocation failed with error code %d\n", src_buffer_size);
        av_freep(&dst_pointers[0]);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Source buffer allocation succeeded\n");
    
    // Verify pointers array is filled with pointer for each image plane
    printf("[DEBUG] Checking source pointers: [0]=%p, [1]=%p, [2]=%p, [3]=%p\n",
           (void*)src_pointers[0], (void*)src_pointers[1], (void*)src_pointers[2], (void*)src_pointers[3]);
    if (src_pointers[0] == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Source buffer pointer[0] is NULL - pointers array not filled correctly\n");
        printf("[DEBUG] ERROR: src_pointers[0] is NULL\n");
        av_freep(&dst_pointers[0]);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Source pointer[0] is valid\n");
    
    // Verify linesizes array is filled
    printf("[DEBUG] Source linesizes: [0]=%d, [1]=%d, [2]=%d, [3]=%d\n",
           src_linesizes[0], src_linesizes[1], src_linesizes[2], src_linesizes[3]);
    if (src_linesizes[0] <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Source linesizes[0] is invalid: %d - linesizes array not filled correctly\n", src_linesizes[0]);
        printf("[DEBUG] ERROR: src_linesizes[0] is invalid: %d\n", src_linesizes[0]);
        av_freep(&src_pointers[0]);
        av_freep(&dst_pointers[0]);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Source linesize[0] is valid\n");
    
    // Verify buffer_size is positive and reasonable
    if (src_buffer_size == 0) {
        av_log(NULL, AV_LOG_ERROR, "Source buffer size is zero - unexpected for valid image dimensions\n");
        printf("[DEBUG] ERROR: Source buffer_size is zero\n");
        av_freep(&src_pointers[0]);
        av_freep(&dst_pointers[0]);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    printf("[DEBUG] Source buffer_size is valid (non-zero)\n");
    
    printf("Source buffer allocated successfully: pointer=%p, linesize=%d, size=%d\n", 
           (void*)src_pointers[0], src_linesizes[0], src_buffer_size);
    
    // Initialize source buffer with test data
    printf("[DEBUG] Initializing source buffer with test data (size=%d bytes)\n", src_buffer_size);
    for (i = 0; i < src_buffer_size; i++) {
        src_pointers[0][i] = (uint8_t)(i % 256);
    }
    printf("Source buffer initialized with test data\n");
    printf("[DEBUG] Source buffer initialization complete\n");
    
    // Call sws_scale
    printf("[DEBUG] Checking conditions for sws_scale: sws_ctx=%p, buffer_size=%d, src_buffer_size=%d\n",
           (void*)sws_ctx, buffer_size, src_buffer_size);
    if (sws_ctx && buffer_size >= 0 && src_buffer_size >= 0) {
        printf("[DEBUG] Conditions met, calling sws_scale\n");
        int srcSliceY = 0;
        int srcSliceH = srcH;
        printf("[DEBUG] sws_scale parameters: srcSliceY=%d, srcSliceH=%d, expected dstH=%d\n",
               srcSliceY, srcSliceH, dstH);
        int output_height = sws_scale(sws_ctx, (const uint8_t *const *)src_pointers, src_linesizes, srcSliceY, srcSliceH, dst_pointers, dst_linesizes);
        printf("[DEBUG] sws_scale returned output_height: %d\n", output_height);
        
        if (output_height != dstH) {
            av_log(NULL, AV_LOG_ERROR, "sws_scale output height mismatch: expected %d, got %d\n", dstH, output_height);
            printf("[DEBUG] ERROR: sws_scale output height mismatch: expected %d, got %d\n", dstH, output_height);
            av_freep(&src_pointers[0]);
            av_freep(&dst_pointers[0]);
            sws_freeContext(sws_ctx);
            avcodec_free_context(&ctx);
            avformat_close_input(&fmt_ctx);
            return -1;
        }
        printf("[DEBUG] sws_scale completed successfully with correct output height\n");
    } else {
        printf("[DEBUG] WARNING: Skipping sws_scale due to failed conditions\n");
    }
    
    // Free buffers as documented: using av_freep(&pointers[0])
    printf("[DEBUG] Starting cleanup: freeing destination buffer\n");
    av_freep(&dst_pointers[0]);
    printf("[DEBUG] Freed destination buffer\n");
    
    printf("[DEBUG] Freeing source buffer\n");
    av_freep(&src_pointers[0]);
    printf("[DEBUG] Freed source buffer\n");
    
    printf("[DEBUG] Freeing sws context\n");
    sws_freeContext(sws_ctx);
    printf("[DEBUG] Freed sws context\n");
    
    printf("[DEBUG] Closing format context\n");
    avformat_close_input(&fmt_ctx);
    printf("[DEBUG] Closed format context\n");
    
    printf("[DEBUG] Freeing codec context\n");
    avcodec_free_context(&ctx);
    printf("[DEBUG] Freed codec context\n");
    
    printf("[DEBUG] image_alloc_example function completed successfully, returning 0\n");
    return 0;
}

int main(int argc, char **argv)
{
    printf("[DEBUG] ========== PROGRAM START ==========\n");
    printf("[DEBUG] main() called with argc=%d\n", argc);
    
    for (int i = 0; i < argc; i++) {
        printf("[DEBUG] argv[%d]: %s\n", i, argv[i]);
    }
    
    const char *test_filename = "/tmp/test_video.mpg";
    int result;
    
    // Create test video file
    printf("[DEBUG] Creating test video file: %s\n", test_filename);
    result = create_test_video_file(test_filename);
    if (result < 0) {
        printf("[DEBUG] ERROR: Failed to create test video file, result=%d\n", result);
        printf("[DEBUG] ========== PROGRAM END (ERROR) ==========\n");
        return 1;
    }
    printf("[DEBUG] Test video file created successfully\n");

    printf("[DEBUG] Calling image_alloc_example with input file: %s\n", test_filename);
    result = image_alloc_example(test_filename);
    printf("[DEBUG] image_alloc_example returned: %d\n", result);
    
    // Clean up test file
    printf("[DEBUG] Removing test file: %s\n", test_filename);
    remove(test_filename);
    
    if (result != 0) {
        printf("[DEBUG] ========== PROGRAM END (FAILURE - return code %d) ==========\n", result);
        return 1;
    }

    printf("[DEBUG] ========== PROGRAM END (SUCCESS) ==========\n");
    return 0;
}
