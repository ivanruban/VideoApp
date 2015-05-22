#ifndef H264_DECODER_H
#define H264_DECODER_H

#include <stdlib.h>
#include <stdint.h>


extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

typedef struct  __attribute__((packed))
{

   uint8_t *data;//AV_PIX_FMT_BGR24
   /**
    * width and height of the video frame
    */
   int width, height;
   /**
    * format of the frame, -1 if unknown or unset
    * Values correspond to enum AVPixelFormat for video frames,
    * enum AVSampleFormat for audio)
    */
   int format;

   /**
    * DTS copied from the AVPacket that triggered returning this frame. (if frame threading isn't used)
    * This is also the Presentation time of this AVFrame calculated from
    * only AVPacket.dts values without pts values.
    */
   int64_t pkt_dts;
} Frame;

/**
 * Implements ffmpeg based simple stream decoder which outputs converted to BGR24(with the sws_scale())
 * buffer to the provided(with the setOutputBuffer()) memory area.
 */
class H264_Decoder
{
 public:
   int  load(const char *filepath, int &w, int &h, double &framerate);
   int  setOutputBuffer(const uint8_t *buffer, const int size);

   /**
    * Decodes a nest frame from the stream and fills the frame with output buffer.
    * @return 0 if output buffer is filled or POSIX error code otherwise.
    */
   int  getFrameRGB(Frame *frame);
   void free();
   void seekToStart();

 private:
   AVFormatContext *pFormatCtx;
   AVCodecContext  *pCodecCtx;
   AVCodec * pCodec;
   AVFrame *pFrame, *pFrameRGB;
   int videoStream;
   AVPixelFormat  pFormat;
};

#endif
