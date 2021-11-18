#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#if defined(__linux__)
#define PLATFORM_NAME "linux" // Linux
#include <X11/Xlib.h>
#elif defined(_WIN32) || defined(__CYGWIN__)
#define PLATFORM_NAME "windows" // Windows (x86 or x64)
#include <windows.h>
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_NAME "mac" // Apple Mac OS
#endif

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <math.h>
#include <string>
#include <regex>
#include <ctime>
#include <assert.h>

#define __STDC_CONSTANT_MACROS

// FFMPEG LIBRARIES
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/opt.h"
#include "libavutil/common.h"
#include "libavutil/channel_layout.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/file.h"
#include "libavutil/log.h"
#include "libavutil/error.h"
#include "libavutil/audio_fifo.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

using namespace std;

class ScreenRecorder
{
private:
	const char *out_filename;

	ofstream log_file; // logger()
	char errbuf[32];   // debugger()

	char tmp_str[100];
	int value;
	int response;

	// TODO: check if I need all these global variables
	// video
	AVInputFormat *vin_format;
	AVFormatContext *vin_format_context;
	AVStream *vin_stream;
	AVRational vin_fps;
	int vstream_idx;
	AVCodecContext *vin_codec_context;
	AVOutputFormat *out_format;			 // extra
	AVFormatContext *out_format_context; // extra
	AVStream *vout_stream;
	AVCodecContext *vout_codec_context;
	AVPacket *vin_packet;
	AVFrame *vin_frame;
	SwsContext *rescaler_context;
	AVFrame *vout_frame;
	AVPacket *vout_packet;

	// audio
	AVInputFormat *ain_format;
	AVFormatContext *ain_format_context;
	AVStream *ain_stream;
	int astream_idx;
	AVCodecContext *ain_codec_context;
	AVStream *aout_stream;
	AVCodecContext *aout_codec_context;
	AVPacket *ain_packet;
	AVFrame *ain_frame;
	SwrContext *resampler_context;
	AVAudioFifo *a_fifo;
	AVFrame *aout_frame;
	AVPacket *aout_packet;

public:
	ScreenRecorder();
	~ScreenRecorder();

	string getTimestamp();
	void logger(string str);
	void debugger(string str, int level, int errnum);

	void openInputDeviceVideo();
	void openInputDeviceAudio();
	void prepareDecoderVideo();
	void prepareDecoderAudio();
	void prepareEncoderVideo();
	void prepareEncoderAudio();
	void prepareOutputFile();
	void prepareCaptureVideo();
	void prepareCaptureAudio();
	void captureFramesVideo(bool &sig_ctrl_c);
	void captureFramesAudio(bool &sig_ctrl_c);
	void deallocateResourcesVideo();
	void deallocateResourcesAudio();
};

#endif
