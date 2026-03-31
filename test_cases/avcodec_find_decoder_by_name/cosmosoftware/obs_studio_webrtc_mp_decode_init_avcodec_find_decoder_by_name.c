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
 * avcodec_find_decoder_by_name test.
 */

#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/dict.h"
#include "libavutil/hwcontext.h"
#include "libavutil/buffer.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Helper function to create a minimal test video file
static int create_test_video_file(const char *filename)
{
    printf("[DEBUG] === Creating test video file: %s ===\n", filename);
    
    AVFormatContext *fmt_ctx = NULL;
    AVStream *stream = NULL;
    AVCodecContext *codec_ctx = NULL;
    const AVCodec *codec = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    int ret = 0;
    
    // Try to find a software encoder that's more likely to be available
    const char *encoder_names[] = {
        "libx264",
        "mpeg4",
        "libxvid",
        "mpeg2video",
        "h263",
        "msmpeg4v2",
        NULL
    };
    
    // Try each encoder in order
    for (int i = 0; encoder_names[i] != NULL; i++) {
        printf("[DEBUG] Trying encoder: %s\n", encoder_names[i]);
        codec = avcodec_find_encoder_by_name(encoder_names[i]);
        if (codec) {
            printf("[DEBUG] Found encoder: %s\n", codec->name);
            break;
        }
    }
    
    // If no named encoder found, try by codec ID
    if (!codec) {
        printf("[DEBUG] No named encoder found, trying by codec ID\n");
        const enum AVCodecID codec_ids[] = {
            AV_CODEC_ID_MPEG4,
            AV_CODEC_ID_MPEG2VIDEO,
            AV_CODEC_ID_H263,
            AV_CODEC_ID_MSMPEG4V2,
            AV_CODEC_ID_NONE
        };
        
        for (int i = 0; codec_ids[i] != AV_CODEC_ID_NONE; i++) {
            codec = avcodec_find_encoder(codec_ids[i]);
            if (codec) {
                printf("[DEBUG] Found encoder by ID: %s\n", codec->name);
                break;
            }
        }
    }
    
    if (!codec) {
        printf("[DEBUG] No suitable encoder found\n");
        return -1;
    }
    printf("[DEBUG] Using encoder: %s\n", codec->name);
    
    // Allocate output format context
    ret = avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename);
    if (ret < 0) {
        printf("[DEBUG] Failed to allocate output context\n");
        return ret;
    }
    
    // Create new stream
    stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        printf("[DEBUG] Failed to create stream\n");
        avformat_free_context(fmt_ctx);
        return -1;
    }
    
    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        printf("[DEBUG] Failed to allocate codec context\n");
        avformat_free_context(fmt_ctx);
        return -1;
    }
    
    // Set codec parameters
    codec_ctx->width = 320;
    codec_ctx->height = 240;
    codec_ctx->time_base = (AVRational){1, 25};
    codec_ctx->framerate = (AVRational){25, 1};
    codec_ctx->gop_size = 10;
    codec_ctx->max_b_frames = 0;
    
    // Set pixel format based on what the codec supports
    if (codec->pix_fmts) {
        codec_ctx->pix_fmt = codec->pix_fmts[0];
        printf("[DEBUG] Using pixel format: %d\n", codec_ctx->pix_fmt);
    } else {
        codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        printf("[DEBUG] Using default pixel format: YUV420P\n");
    }
    
    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    
    // Set additional options for specific encoders
    AVDictionary *opts = NULL;
    if (strcmp(codec->name, "libx264") == 0) {
        av_dict_set(&opts, "preset", "ultrafast", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);
    }
    
    // Open codec
    ret = avcodec_open2(codec_ctx, codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        printf("[DEBUG] Failed to open codec, error: %d\n", ret);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0) {
        printf("[DEBUG] Failed to copy codec parameters\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    stream->time_base = codec_ctx->time_base;
    
    // Open output file
    ret = avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
        printf("[DEBUG] Failed to open output file\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    // Write header
    ret = avformat_write_header(fmt_ctx, NULL);
    if (ret < 0) {
        printf("[DEBUG] Failed to write header\n");
        avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    // Create a simple frame
    frame = av_frame_alloc();
    if (!frame) {
        printf("[DEBUG] Failed to allocate frame\n");
        av_write_trailer(fmt_ctx);
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
        printf("[DEBUG] Failed to allocate frame buffer\n");
        av_frame_free(&frame);
        av_write_trailer(fmt_ctx);
        avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    
    pkt = av_packet_alloc();
    if (!pkt) {
        printf("[DEBUG] Failed to allocate packet\n");
        av_frame_free(&frame);
        av_write_trailer(fmt_ctx);
        avio_closep(&fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return -1;
    }
    
    // Encode a few frames
    for (int i = 0; i < 5; i++) {
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            break;
        
        // Fill with simple pattern based on pixel format
        if (codec_ctx->pix_fmt == AV_PIX_FMT_YUV420P) {
            for (int y = 0; y < codec_ctx->height; y++) {
                for (int x = 0; x < codec_ctx->width; x++) {
                    frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
                }
            }
            
            for (int y = 0; y < codec_ctx->height/2; y++) {
                for (int x = 0; x < codec_ctx->width/2; x++) {
                    frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                    frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
                }
            }
        } else {
            // For other formats, just fill with a simple pattern
            memset(frame->data[0], i * 50, frame->linesize[0] * codec_ctx->height);
        }
        
        frame->pts = i;
        
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0)
            break;
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0)
                goto cleanup;
            
            av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            
            ret = av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
            if (ret < 0)
                goto cleanup;
        }
    }
    
    // Flush encoder
    avcodec_send_frame(codec_ctx, NULL);
    while (1) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            break;
        if (ret < 0)
            goto cleanup;
        
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }
    
    ret = 0;
    
cleanup:
    av_write_trailer(fmt_ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avio_closep(&fmt_ctx->pb);
    avcodec_free_context(&codec_ctx);
    avformat_free_context(fmt_ctx);
    
    printf("[DEBUG] Test video file created successfully\n");
    return ret;
}

static int test_find_decoder_by_name(const char *input_filename)
{
    printf("[DEBUG] === Starting test_find_decoder_by_name function ===\n");
    printf("[DEBUG] Input filename: %s\n", input_filename);
    
    const AVCodec *codec = NULL;
    AVCodecContext *ctx = NULL;
    AVCodecParameters *origin_par = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVBufferRef *hw_device_ctx = NULL;
    int video_stream;
    int result;
    enum AVMediaType type = AVMEDIA_TYPE_VIDEO;
    enum AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
    enum AVCodecID id;

    printf("[DEBUG] Initialized variables\n");
    
    // Test avcodec_find_decoder_by_name with various scenarios
    
    // Test 1: Valid decoder name - should return non-NULL
    const char *valid_codec_name = "h264";
    printf("[DEBUG] Test 1: Calling avcodec_find_decoder_by_name with valid codec name: '%s'\n", valid_codec_name);
    const AVCodec *test_codec1 = avcodec_find_decoder_by_name(valid_codec_name);
    printf("[DEBUG] Test 1: avcodec_find_decoder_by_name returned: %p\n", (void*)test_codec1);
    
    if (test_codec1 == NULL) {
        printf("[DEBUG] Test 1: FAILED - test_codec1 is NULL\n");
        av_log(NULL, AV_LOG_ERROR, "avcodec_find_decoder_by_name failed to find valid decoder '%s'\n", valid_codec_name);
        return -1;
    }
    printf("[DEBUG] Test 1: test_codec1 is not NULL, checking name\n");
    printf("[DEBUG] Test 1: test_codec1->name = '%s'\n", test_codec1->name);
    
    if (strcmp(test_codec1->name, valid_codec_name) != 0) {
        printf("[DEBUG] Test 1: FAILED - name mismatch\n");
        av_log(NULL, AV_LOG_ERROR, "Returned decoder name '%s' does not match requested name '%s'\n", test_codec1->name, valid_codec_name);
        return -1;
    }
    printf("[DEBUG] Test 1: PASSED - name matches\n");
    av_log(NULL, AV_LOG_INFO, "SUCCESS: Found decoder '%s' as expected\n", test_codec1->name);
    
    // Test 2: Invalid decoder name - should return NULL
    const char *invalid_codec_name = "nonexistent_codec_xyz123";
    printf("[DEBUG] Test 2: Calling avcodec_find_decoder_by_name with invalid codec name: '%s'\n", invalid_codec_name);
    const AVCodec *test_codec2 = avcodec_find_decoder_by_name(invalid_codec_name);
    printf("[DEBUG] Test 2: avcodec_find_decoder_by_name returned: %p\n", (void*)test_codec2);
    
    if (test_codec2 != NULL) {
        printf("[DEBUG] Test 2: FAILED - test_codec2 is not NULL\n");
        av_log(NULL, AV_LOG_ERROR, "avcodec_find_decoder_by_name should return NULL for invalid decoder name '%s'\n", invalid_codec_name);
        return -1;
    }
    printf("[DEBUG] Test 2: PASSED - returned NULL as expected\n");
    av_log(NULL, AV_LOG_INFO, "SUCCESS: Returned NULL for invalid decoder name as expected\n");
    
    // Test 3: Another valid decoder name
    const char *valid_codec_name2 = "vp8";
    printf("[DEBUG] Test 3: Calling avcodec_find_decoder_by_name with codec name: '%s'\n", valid_codec_name2);
    const AVCodec *test_codec3 = avcodec_find_decoder_by_name(valid_codec_name2);
    printf("[DEBUG] Test 3: avcodec_find_decoder_by_name returned: %p\n", (void*)test_codec3);
    
    if (test_codec3 == NULL) {
        printf("[DEBUG] Test 3: test_codec3 is NULL (codec may not be compiled in)\n");
        av_log(NULL, AV_LOG_WARNING, "avcodec_find_decoder_by_name failed to find decoder '%s' (may not be compiled in)\n", valid_codec_name2);
    } else {
        printf("[DEBUG] Test 3: test_codec3 is not NULL, checking name\n");
        printf("[DEBUG] Test 3: test_codec3->name = '%s'\n", test_codec3->name);
        
        if (strcmp(test_codec3->name, valid_codec_name2) != 0) {
            printf("[DEBUG] Test 3: FAILED - name mismatch\n");
            av_log(NULL, AV_LOG_ERROR, "Returned decoder name '%s' does not match requested name '%s'\n", test_codec3->name, valid_codec_name2);
            return -1;
        }
        printf("[DEBUG] Test 3: PASSED - name matches\n");
        av_log(NULL, AV_LOG_INFO, "SUCCESS: Found decoder '%s' as expected\n", test_codec3->name);
    }

    printf("[DEBUG] Calling avformat_open_input with filename: %s\n", input_filename);
    result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    printf("[DEBUG] avformat_open_input returned: %d\n", result);
    
    if (result < 0) {
        printf("[DEBUG] FAILED - avformat_open_input returned error code: %d\n", result);
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        return result;
    }
    printf("[DEBUG] Successfully opened input file, fmt_ctx = %p\n", (void*)fmt_ctx);

    printf("[DEBUG] Calling avformat_find_stream_info\n");
    result = avformat_find_stream_info(fmt_ctx, NULL);
    printf("[DEBUG] avformat_find_stream_info returned: %d\n", result);
    
    if (result < 0) {
        printf("[DEBUG] FAILED - avformat_find_stream_info returned error code: %d\n", result);
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }
    printf("[DEBUG] Successfully found stream info\n");

    printf("[DEBUG] Calling av_find_best_stream for video stream\n");
    video_stream = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    printf("[DEBUG] av_find_best_stream returned: %d\n", video_stream);
    
    if (video_stream < 0) {
        printf("[DEBUG] FAILED - no video stream found\n");
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
        return -1;
    }
    printf("[DEBUG] Found video stream at index: %d\n", video_stream);

    origin_par = fmt_ctx->streams[video_stream]->codecpar;
    printf("[DEBUG] Got codec parameters, origin_par = %p\n", (void*)origin_par);
    
    id = origin_par->codec_id;
    printf("[DEBUG] Codec ID: %d\n", id);

    // av_dict_get
    AVDictionaryEntry *tag = NULL;
    printf("[DEBUG] Calling av_dict_get for 'alpha_mode' metadata\n");
    tag = av_dict_get(fmt_ctx->streams[video_stream]->metadata, "alpha_mode", tag, AV_DICT_IGNORE_SUFFIX);
    printf("[DEBUG] av_dict_get returned: %p\n", (void*)tag);
    
    if (tag && strcmp(tag->value, "1") == 0) {
        printf("[DEBUG] Alpha mode detected, tag->value = '%s'\n", tag->value);
        const char *codec_name = (id == AV_CODEC_ID_VP8) ? "libvpx" : "libvpx-vp9";
        printf("[DEBUG] Determined codec_name based on codec_id: '%s'\n", codec_name);
        av_log(NULL, AV_LOG_INFO, "Alpha mode detected, searching for codec: %s\n", codec_name);
        
        // avcodec_find_decoder_by_name
        printf("[DEBUG] Calling avcodec_find_decoder_by_name for alternative codec: '%s'\n", codec_name);
        const AVCodec *alt_codec = avcodec_find_decoder_by_name(codec_name);
        printf("[DEBUG] avcodec_find_decoder_by_name returned: %p\n", (void*)alt_codec);
        
        if (alt_codec) {
            printf("[DEBUG] Alternative codec found, checking name\n");
            printf("[DEBUG] alt_codec->name = '%s'\n", alt_codec->name);
            
            // Verify the returned codec has the correct name
            if (strcmp(alt_codec->name, codec_name) != 0) {
                printf("[DEBUG] FAILED - alternative codec name mismatch\n");
                av_log(NULL, AV_LOG_ERROR, "Returned decoder name '%s' does not match requested name '%s'\n", alt_codec->name, codec_name);
                avformat_close_input(&fmt_ctx);
                return -1;
            }
            printf("[DEBUG] Alternative codec name matches, using it\n");
            codec = alt_codec;
            av_log(NULL, AV_LOG_INFO, "Using alternative codec: %s\n", codec->name);
        } else {
            printf("[DEBUG] Alternative codec not found\n");
            av_log(NULL, AV_LOG_INFO, "Alternative codec %s not found, will use default\n", codec_name);
        }
    } else {
        printf("[DEBUG] Alpha mode not detected or tag is NULL\n");
    }

    if (!codec) {
        printf("[DEBUG] codec is NULL, calling avcodec_find_decoder with id: %d\n", id);
        codec = avcodec_find_decoder(id);
        printf("[DEBUG] avcodec_find_decoder returned: %p\n", (void*)codec);
        
        if (!codec) {
            printf("[DEBUG] FAILED - avcodec_find_decoder returned NULL\n");
            av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
            return -1;
        }
        printf("[DEBUG] Found decoder: %s\n", codec->name);
    } else {
        printf("[DEBUG] codec already set: %s\n", codec->name);
    }

    printf("[DEBUG] Calling avcodec_alloc_context3\n");
    ctx = avcodec_alloc_context3(codec);
    printf("[DEBUG] avcodec_alloc_context3 returned: %p\n", (void*)ctx);
    
    if (!ctx) {
        printf("[DEBUG] FAILED - avcodec_alloc_context3 returned NULL\n");
        av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] Successfully allocated decoder context\n");

    printf("[DEBUG] Calling avcodec_parameters_to_context\n");
    result = avcodec_parameters_to_context(ctx, origin_par);
    printf("[DEBUG] avcodec_parameters_to_context returned: %d\n", result);
    
    if (result) {
        printf("[DEBUG] FAILED - avcodec_parameters_to_context returned error: %d\n", result);
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        return result;
    }
    printf("[DEBUG] Successfully copied parameters to context\n");

    // avcodec_get_hw_config
    const AVCodecHWConfig *hw_config = NULL;
    printf("[DEBUG] Enumerating hardware configurations\n");
    for (int i = 0;; i++) {
        printf("[DEBUG] Calling avcodec_get_hw_config with index: %d\n", i);
        hw_config = avcodec_get_hw_config(codec, i);
        printf("[DEBUG] avcodec_get_hw_config returned: %p\n", (void*)hw_config);
        
        if (!hw_config) {
            printf("[DEBUG] No more hardware configurations (hw_config is NULL)\n");
            break;
        }
        printf("[DEBUG] Found hardware configuration at index %d\n", i);
    }

    // av_hwdevice_ctx_create
    printf("[DEBUG] Checking hw_type: %d (AV_HWDEVICE_TYPE_NONE = %d)\n", hw_type, AV_HWDEVICE_TYPE_NONE);
    if (hw_type != AV_HWDEVICE_TYPE_NONE) {
        printf("[DEBUG] hw_type is not NONE, calling av_hwdevice_ctx_create\n");
        result = av_hwdevice_ctx_create(&hw_device_ctx, hw_type, NULL, NULL, 0);
        printf("[DEBUG] av_hwdevice_ctx_create returned: %d, hw_device_ctx = %p\n", result, (void*)hw_device_ctx);
        
        if (result >= 0 && hw_device_ctx) {
            printf("[DEBUG] Hardware device context created successfully, calling av_buffer_ref\n");
            // av_buffer_ref
            ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            printf("[DEBUG] av_buffer_ref returned: %p\n", (void*)ctx->hw_device_ctx);
        } else {
            printf("[DEBUG] Hardware device context creation failed or returned NULL\n");
        }
    } else {
        printf("[DEBUG] hw_type is NONE, skipping hardware device context creation\n");
    }

    printf("[DEBUG] Calling avcodec_open2\n");
    result = avcodec_open2(ctx, codec, NULL);
    printf("[DEBUG] avcodec_open2 returned: %d\n", result);
    
    if (result < 0) {
        printf("[DEBUG] FAILED - avcodec_open2 returned error: %d\n", result);
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        
        if (hw_device_ctx) {
            printf("[DEBUG] Cleaning up hw_device_ctx\n");
            av_buffer_unref(&hw_device_ctx);
        }
        printf("[DEBUG] Freeing codec context\n");
        avcodec_free_context(&ctx);
        printf("[DEBUG] Closing format context\n");
        avformat_close_input(&fmt_ctx);
        return result;
    }
    printf("[DEBUG] Successfully opened decoder\n");

    if (hw_device_ctx) {
        printf("[DEBUG] Unreferencing hw_device_ctx\n");
        av_buffer_unref(&hw_device_ctx);
        printf("[DEBUG] hw_device_ctx unreferenced\n");
    } else {
        printf("[DEBUG] hw_device_ctx is NULL, no need to unref\n");
    }
    
    printf("[DEBUG] Freeing codec context\n");
    avcodec_free_context(&ctx);
    printf("[DEBUG] Codec context freed\n");
    
    printf("[DEBUG] Closing format context\n");
    avformat_close_input(&fmt_ctx);
    printf("[DEBUG] Format context closed\n");
    
    av_log(NULL, AV_LOG_INFO, "Test completed successfully\n");
    printf("[DEBUG] === test_find_decoder_by_name function completed successfully ===\n");
    return 0;
}

int main(int argc, char **argv)
{
    printf("[DEBUG] ========== PROGRAM START ==========\n");
    printf("[DEBUG] argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("[DEBUG] argv[%d] = %s\n", i, argv[i]);
    }
    
    const char *test_filename = "/tmp/test_video_file.mp4";
    
    // Create test video file if no argument provided
    if (argc < 2) {
        printf("[DEBUG] No input file provided, creating test file: %s\n", test_filename);
        int create_result = create_test_video_file(test_filename);
        if (create_result < 0) {
            printf("[DEBUG] FAILED - could not create test video file\n");
            av_log(NULL, AV_LOG_ERROR, "Failed to create test video file\n");
            return 1;
        }
        printf("[DEBUG] Test file created successfully\n");
    } else {
        test_filename = argv[1];
        printf("[DEBUG] Using provided input file: %s\n", test_filename);
    }
    
    printf("[DEBUG] Calling test_find_decoder_by_name\n");
    int test_result = test_find_decoder_by_name(test_filename);
    printf("[DEBUG] test_find_decoder_by_name returned: %d\n", test_result);
    
    if (test_result != 0) {
        printf("[DEBUG] FAILED - test returned non-zero: %d\n", test_result);
        printf("[DEBUG] ========== PROGRAM END (FAILURE) ==========\n");
        return 1;
    }

    printf("[DEBUG] Test passed successfully\n");
    printf("[DEBUG] ========== PROGRAM END (SUCCESS) ==========\n");
    return 0;
}
