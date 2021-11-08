#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#if defined(_WIN32) || defined(__CYGWIN__)
#define PLATFORM_NAME "windows" // Windows (x86 or x64)
#include <windows.h>
#elif defined(__linux__)
#define PLATFORM_NAME "linux" // Linux
#include <X11/Xlib.h>
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
#include <vector>
#include <csignal>
#include <regex>

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
#include "libswscale/swscale.h"
}

using namespace std;

class ScreenRecorder
{
private:
	string screen_device;
	string mic_device;
	string screen_url;
	string mic_url;
	string out_filename;

	AVInputFormat *ain_format;
	AVOutputFormat *aout_format;
	AVInputFormat *vin_format;
	AVOutputFormat *vout_format;

	AVFormatContext *in_format_context;
	AVFormatContext *out_format_context;

	AVCodec *ain_codec;
	AVCodec *aout_codec;
	AVCodec *vin_codec;
	AVCodec *vout_codec;

	AVCodecContext *ain_codec_context;
	AVCodecContext *aout_codec_context;
	AVCodecContext *vin_codec_context;
	AVCodecContext *vout_codec_context;

	AVDictionary *in_options;
	AVDictionary *hdr_options;
	AVDictionary *out_options;

	AVStream *ain_stream;
	AVStream *aout_stream;
	AVStream *vin_stream;
	AVStream *vout_stream;

	SwsContext *sws_context;

	AVPacket *in_packet;
	AVPacket *out_packet;
	AVFrame *in_frame;
	AVFrame *out_frame;

	int astream_idx;
	int vstream_idx;
	AVRational vin_fps;
	int ret;

public:
	ScreenRecorder();
	~ScreenRecorder();
	void openInputDevices();
	void prepareDecoders();
	void prepareEncoders();
	void prepareOutputFile();
	void prepareCapture();
	void captureFrames();
	void deallocateResources();
};

#endif
