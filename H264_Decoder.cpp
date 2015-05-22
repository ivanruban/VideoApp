#include "string.h"

#include "errno.h"
#include "H264_Decoder.hpp"

/**
 * Try to force YUV420P as decoder output format.
 */
static enum AVPixelFormat chosePixelFormat(struct AVCodecContext *s, const enum AVPixelFormat * fmt)
{
//   int i =0;
//   while(fmt[i] != -1)
//   {
//      i++;
//   }
   return AV_PIX_FMT_YUV420P;
}

/**
 * Decodes a nest frame from the stream and fills the frame with output buffer.
 * @return 0 if output buffer is filled or POSIX error code otherwise.
 */
int  H264_Decoder::getFrameRGB(Frame *frame)
{
   int frameFinished;
   AVPacket packet;
   while(av_read_frame(pFormatCtx, &packet)>=0)
   {
       if(packet.stream_index == videoStream)
       {
           avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
           if(frameFinished)
           {
               struct SwsContext * img_convert_ctx;
               img_convert_ctx = sws_getCachedContext(NULL, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt
                                                      ,pCodecCtx->width, pCodecCtx->height, pFormat, SWS_BICUBIC, NULL, NULL,NULL);
               sws_scale(img_convert_ctx, ((AVPicture*)pFrame)->data, ((AVPicture*)pFrame)->linesize, 0, pCodecCtx->height
                          ,((AVPicture *)pFrameRGB)->data, ((AVPicture *)pFrameRGB)->linesize);

               frame->data = pFrameRGB->data[0];
               frame->width = pCodecCtx->width;
               frame->height = pCodecCtx->height;
               frame->format = pFrame->format;
               frame->pkt_dts = pFrame->pkt_dts;

               av_free_packet(&packet);
               sws_freeContext(img_convert_ctx);

               return 0;
           }
       }
   }

   return EIO;
}

void H264_Decoder::seekToStart()
{
   avformat_seek_file(pFormatCtx, videoStream, 0, 0, 0, AVSEEK_FLAG_BYTE);
   avcodec_flush_buffers(pCodecCtx);
}


void H264_Decoder::free()
{
   avcodec_close(pCodecCtx);
   av_frame_free(&pFrame);
   av_frame_free(&pFrameRGB);
   avformat_close_input(&pFormatCtx);
}


int H264_Decoder::load(const char *filepath, int &w, int &h, double &framerate)
{
   pFormat = AV_PIX_FMT_BGR24;

   avcodec_register_all();
   av_register_all();

   pFormatCtx = avformat_alloc_context();
   if(avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0)
   {
      fprintf(stderr, "avformat_open_input(%s) failed\n", filepath);
      return EIO;
   }

   if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
   {
      fprintf(stderr, "avformat_find_stream_info failed");
      return EIO;
   }

   av_dump_format(pFormatCtx, 0, filepath, 0);
   videoStream = -1;
   for(int i=0; i < (int)pFormatCtx->nb_streams; i++)
   {
       if(pFormatCtx->streams[i]->codec->coder_type==AVMEDIA_TYPE_VIDEO)
       {
           videoStream = i;
           break;
       }
   }
   if(videoStream == -1) return EIO;

   pCodecCtx = pFormatCtx->streams[videoStream]->codec;
   /*specify the format clarification function explicitly to force AV_PIX_FMT_YUV420P*/
   pCodecCtx->get_format = chosePixelFormat;

   pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
   if(pCodec==NULL)
   {
      fprintf(stderr, "avcodec_find_decoder() failed");
      return EIO;
   }

   if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
   {
      fprintf(stderr, "avcodec_open2 failed");
      return EIO;
   }

   pFrame    = av_frame_alloc();
   pFrameRGB = av_frame_alloc();

   w = pCodecCtx->width;
   h = pCodecCtx->height;
   framerate = av_q2d(pCodecCtx->framerate);
   return 0;
}

int H264_Decoder::setOutputBuffer(const uint8_t *buffer, const int size)
{
   int numBytes;
   numBytes = avpicture_get_size(pFormat, pCodecCtx->width, pCodecCtx->height);
   if(numBytes > size)
   {
      return ENOMEM;
   }
   avpicture_fill((AVPicture *) pFrameRGB, buffer, pFormat, pCodecCtx->width, pCodecCtx->height);

   return 0;
}

