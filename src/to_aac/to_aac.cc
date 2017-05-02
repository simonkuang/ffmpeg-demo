#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_CONSTANT_MACROS

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#ifdef __cplusplus
}
#endif

#define MAX_AUDIO_FRAME_SIZE 192000  // 1 second 32bit 48kHz audio

#define LOGE(...) \
  printf(__VA_ARGS__);\

#define LOGI(...) \
  printf(__VA_ARGS__);\


int main(int argc, char* argv[]) {
  int   audio_stream, i, ret;
  AVFormatContext *avformat_ctx;

  // decode. audio source file to pcm audio
  AVCodecParameters *codec_par;
  AVCodec *codec;
  AVCodecContext *codec_ctx;

  // encode. from pcm audio to aac destination audio
  AVCodec *encoder;
  AVCodecContext *encoder_ctx;

  AVPacket *packet;
  AVFrame *frame;

  SwrContext *swr_ctx;

  if (argc <= 1) {
    LOGE("You must specify the input audio file.\n");
    return -1;
  }

  if (argc <= 2) {
    LOGE("You must specify the output audio file prefix.\n");
    return -1;
  }

  int tmp_argv2_len = sizeof(argv[2]) + 4;
  char *output_file = (char *)av_malloc(tmp_argv2_len);
  memset(output_file, '\0', tmp_argv2_len);
  strcpy(output_file, argv[2]);
  strcat(output_file, ".pcm");
  FILE *output_pcm_fp = fopen(output_file, "wb");
  memset(output_file, '\0', tmp_argv2_len);
  strcpy(output_file, argv[2]);
  strcat(output_file, ".aac");
  FILE *output_aac_fp = fopen(output_file, "wb");
  av_free(output_file);
  char *url = argv[1];

  av_register_all();
  avformat_network_init();

  // init AVFormat Context
  avformat_ctx = avformat_alloc_context();

  // open input file
  if (avformat_open_input(&avformat_ctx, argv[1], NULL, NULL) != 0) {
    LOGE("Error in open input file.\n");
    return -1;
  }

  // find the format info
  if (avformat_find_stream_info(avformat_ctx, NULL) < 0) {
    LOGE("Error in find the stream info.\n");
    return -1;
  }

  // dump the valid stream format info on stderr
  av_dump_format(avformat_ctx, 0, url, false);

  audio_stream = -1;
  // get the first audio stream
  for (i = 0; i < avformat_ctx->nb_streams; i++) {
    if (avformat_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_stream = i;
      break;
    }
  }

  if (audio_stream == -1) {
    LOGE("Cannot find an audio stream.\n");
    return -1;
  }


  // retrieve AVCodecParameters object
  codec_par = avformat_ctx->streams[audio_stream]->codecpar;

  // find the decoder for audio stream
  codec = avcodec_find_decoder(codec_par->codec_id);
  if (codec == NULL) {
    LOGE("Could not find the audio decoder.\n");
    return -1;
  }

  // alloc and init AVCodec Context object
  codec_ctx = avcodec_alloc_context3(codec);
  if (codec_ctx == NULL || avcodec_open2(codec_ctx, codec, NULL) < 0) {
    LOGE("alloc AVCodec Context failed.\n");
    return -1;
  }

  // init AVPacket
  packet = (AVPacket *)av_malloc(sizeof(AVPacket));
  av_init_packet(packet);

  // setup the output stream format
  uint64_t output_channel_layout = AV_CH_LAYOUT_STEREO;
  int output_nb_samples = codec_par->frame_size;
  AVSampleFormat output_sample_format = AV_SAMPLE_FMT_S16;
  int output_sample_rate = 24000;
  int output_channel_count =
      av_get_channel_layout_nb_channels(output_channel_layout);
  int output_buffer_size =
      av_samples_get_buffer_size(NULL,
                                 output_channel_count,
                                 output_nb_samples,
                                 output_sample_format,
                                 1);

  uint8_t *output_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
  frame = av_frame_alloc();

  // retrieve input stream format
  uint64_t input_channel_layout =
      av_get_default_channel_layout(codec_par->channels);

  // SWResample part
  if ((swr_ctx = swr_alloc()) == NULL) {
    LOGE("Error in alloc SWResample Context.\n");
    return -1;
  }

  // setup the resample source (format) and destination (format), then init
  swr_ctx = swr_alloc_set_opts(swr_ctx,
      output_channel_layout, output_sample_format, output_sample_rate,
      input_channel_layout, (AVSampleFormat)codec_par->format,
      codec_par->sample_rate, 0, NULL);
  if (swr_ctx == NULL || swr_init(swr_ctx) != 0) {
    LOGE("Error in setup swr context or init it.\n");
    return -1;
  }

  // generate the encoder
  encoder = avcodec_find_encoder_by_name("aac");
  if (encoder == NULL) {
    LOGE("Cannot find the encoder named \"aac\".\n");
    return -1;
  }

  // process resampling frame by frame
  i = 0;
  while (av_read_frame(avformat_ctx, packet) >= 0) {
    if (packet->stream_index != audio_stream) {
      // it's not the audio stream
      continue;
    }

    if ((avcodec_send_packet(codec_ctx, packet) != 0) ||
        (avcodec_receive_frame(codec_ctx, frame) != 0)) {
      LOGE("Error in reform data into frame from packet.\n");
      return -1;
    }

    ret = swr_convert(swr_ctx, &output_buffer, MAX_AUDIO_FRAME_SIZE,
                      (const uint8_t **)frame->data, frame->nb_samples);
    if (ret < 0) {
      LOGE("Error in swr_convert section.\n");
      return ret;
    }

    LOGI("index: %d\t pts: %lld\t packet size: %d\n",
         i++, packet->pts, packet->size);

    // write output pcm file
    fwrite(output_buffer, 1, output_buffer_size, output_pcm_fp);

    // encoding to aac
  }



  // finished, destroying the resources
  fclose(output_pcm_fp);
  fclose(output_aac_fp);
  if (swr_ctx != NULL) {
    swr_free(&swr_ctx);
  }
  av_free(output_buffer);
  av_free(packet);
  if (avformat_ctx != NULL) {
    avformat_free_context(avformat_ctx);
  }
  avformat_network_deinit();

  return 0;
}

int EncodingThread(const uint8_t **data) {
  return sizeof(data);
}
