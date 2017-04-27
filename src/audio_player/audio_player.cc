#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif
#endif

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

int main(int argc, char* argv[])
{
  AVFormatContext  *pFormatCtx;
  int        i, audioStream;
  AVCodecContext  *pCodecCtx;
  AVCodecParameters  *pCodecPar;
  AVCodec      *pCodec;
  AVPacket    *packet;
  uint8_t      *out_buffer;
  AVFrame      *pFrame;
  int ret;
  //uint32_t len = 0;
  //int got_picture;
  int index = 0;
  int64_t in_channel_layout;
  struct SwrContext *au_convert_ctx;

  FILE *pFile=fopen("output.pcm", "wb");
  char url[]="skycity1.mp3";

  av_register_all();
  avformat_network_init();
  pFormatCtx = avformat_alloc_context();
  //Open
  if(avformat_open_input(&pFormatCtx,url,NULL,NULL)!=0){
    printf("Couldn't open input stream.\n");
    return -1;
  }
  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx,NULL)<0){
    printf("Couldn't find stream information.\n");
    return -1;
  }
  // Dump valid information onto standard error
  av_dump_format(pFormatCtx, 0, url, false);

  // Find the first audio stream
  audioStream=-1;
  for(i=0; i < pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
      audioStream=i;
      break;
    }

  if(audioStream==-1){
    printf("Didn't find a audio stream.\n");
    return -1;
  }

  // Get a pointer to the codec context for the audio stream
  pCodecPar = pFormatCtx->streams[audioStream]->codecpar;

  // Find the decoder for the audio stream
  pCodec=avcodec_find_decoder(pCodecPar->codec_id);
  printf("AVCodec Paramters object carrys on the codec id: %d\n", pCodecPar->codec_id);
  if(pCodec==NULL){
    printf("Codec not found.\n");
    return -1;
  }
  printf("Codec been found has name: %s\n", pCodec->name);
  printf("Codec has codec id: %d\n", pCodec->id);

  // alloc AVCodecContext
  pCodecCtx = avcodec_alloc_context3(pCodec);

  // Open codec
  ret = avcodec_open2(pCodecCtx, pCodec, NULL);
  printf("AVCodec Context initialized return value: %d\n", ret);
  if (ret < 0) {
    printf("Could not open codec.\n");
    return -1;
  }

  packet = (AVPacket *)av_malloc(sizeof(AVPacket));
  av_init_packet(packet);

  //Out Audio Param
  uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
  pCodecCtx->frame_size || (pCodecCtx->frame_size = pCodecPar->frame_size);
  //nb_samples: AAC-1024 MP3-1152
  int out_nb_samples = pCodecCtx->frame_size;
  printf("Output sample size is: %d\n", pCodecPar->frame_size);
  AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
  int out_sample_rate = 44100;
  int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
  //Out Buffer Size
  int out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);

  out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
  pFrame=av_frame_alloc();

  //FIX:Some Codec's Context Information is missing
  pCodecCtx->channels || (pCodecCtx->channels = pCodecPar->channels);
  pCodecCtx->sample_rate || (pCodecCtx->sample_rate = pCodecPar->sample_rate);
  printf("channels count from AVCodec Context vars: %d\n", pCodecCtx->channels);
  printf("sampling rate from AVCodec Context vars: %d\n", pCodecCtx->sample_rate);
  printf("Channels from AVCodec Parameters is: %d\n", pCodecPar->channels);
  printf("Channels from AVCodec Parameters is: %d\n", pCodecPar->sample_rate);
  in_channel_layout = av_get_default_channel_layout(
    pCodecCtx->channels ? pCodecCtx->channels : pCodecPar->channels);
  printf("SWResample channel layout count: %lld\n", in_channel_layout);
  printf("Reached here ~~~\n");
  //Swr
  au_convert_ctx = swr_alloc();
  if (au_convert_ctx == NULL) {
    printf("Error in alloc SWResampling Context object.\n");
    return -1;
  }
  au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
                                      //in_channel_layout,pCodecCtx->sample_fmt , pCodecCtx->sample_rate,0, NULL);
                                      in_channel_layout, (AVSampleFormat)pCodecPar->format , pCodecPar->sample_rate, 0, NULL);
  printf("Output sample size is: %d\n", pCodecPar->frame_size);
  if (au_convert_ctx == NULL) {
    printf("Error in setting SWResampling Context alloc options.\n");
    return -1;
  }
  ret = swr_init(au_convert_ctx);
  if (ret != 0) {
    printf("Error in initializing SWResampleContext with ret: %d\n", ret);
    return -1;
  }

  printf("packet size is: %d\n", packet->size);
  while(av_read_frame(pFormatCtx, packet)>=0){
    if(packet->stream_index==audioStream){

      //ret = avcodec_decode_audio4( pCodecCtx, pFrame,&got_picture, packet);
      ret = avcodec_send_packet(pCodecCtx, packet);
      if (ret != 0) {
        printf("Error in decoding process while sending frame.\n");
        return -1;
      }
      ret = avcodec_receive_frame(pCodecCtx, pFrame);
      if (ret < 0) {
        printf("Error in decoding audio frame.\n");
        return -1;
      }
      //if ( got_picture > 0 ){
      if (ret == 0) {
        swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame->data , pFrame->nb_samples);

        printf("index:%5d\t pts:%lld\t packet size:%d\n",index,packet->pts,packet->size);
        //Write PCM
        fwrite(out_buffer, 1, out_buffer_size, pFile);
        index++;
      }
    }
    av_packet_unref(packet);
  }

  swr_free(&au_convert_ctx);

  fclose(pFile);

  av_free(out_buffer);
  // Close the codec
  avcodec_close(pCodecCtx);
  // Close the video file
  avformat_close_input(&pFormatCtx);

  return 0;
}

