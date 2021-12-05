#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#if defined(__linux__)
#define PLATFORM_NAME "linux" // Linux
#include <X11/Xlib.h>
#include <termios.h>
#elif defined(_WIN32) || defined(__CYGWIN__)
#define PLATFORM_NAME "windows" // Windows (x86 or x64)
#include <windows.h>
#include "ListAVDevices.h"
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_NAME "mac" // Apple Mac OS
#endif

#define STATELESS 0
#define RECORDING 1
#define PAUSED 2
#define STOPPED 3

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <math.h>
#include <string>
#include <regex>
#include <ctime>
#include <queue>
#include <set>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unistd.h>

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
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
	struct termios old_tio, new_tio;
#endif

	// rec_status change management
	uint8_t rec_status;
	mutex v_rec_status_mtx;
	mutex a_rec_status_mtx;
	condition_variable v_rec_status_cv;
	condition_variable a_rec_status_cv;

	// av queues management
	queue<AVPacket *> vin_packets_q;
	mutex vin_packets_q_mtx;
	condition_variable vin_packets_q_cv;
	queue<AVPacket *> ain_packets_q;
	mutex ain_packets_q_mtx;
	condition_variable ain_packets_q_cv;

	// av_frame(...) mutual exclusion
	mutex av_write_frame_mtx;

	// executable's parameters
	string area_size, area_offsets;
	string video_fps;
	bool audio_flag;
	string out_filename;

	ofstream log_file; // logger()
	char errbuf[32];   // debugger()

	char tmp_str[100];
	int value;
	int response;

	// threads' pointers
	unique_ptr<thread> capture_video_thrd;
	unique_ptr<thread> elaborate_video_thrd;
	unique_ptr<thread> capture_audio_thrd;
	unique_ptr<thread> elaborate_audio_thrd;
	unique_ptr<thread> change_rec_status_thrd;

	// TODO: check if I need all these global variables
	// video
	AVInputFormat *vin_format;
	AVFormatContext *vin_format_context;
	AVStream *vin_stream;
	AVRational vin_fps;
	int vin_stream_idx;
	int vout_stream_idx;
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
	int ain_stream_idx;
	int aout_stream_idx;
	AVCodecContext *ain_codec_context;
	AVStream *aout_stream;
	AVCodecContext *aout_codec_context;
	AVPacket *ain_packet;
	AVFrame *ain_frame;
	SwrContext *resampler_context;
	AVAudioFifo *a_fifo;
	AVFrame *aout_frame;
	AVPacket *aout_packet;

	string getTimestamp();
	void logger(string str);
	void debugger(string str, int level, int errnum);

	void openInputDeviceVideo();
	void openInputDeviceAudio();
	void prepareDecoderVideo();
	void prepareDecoderAudio();
	void prepareEncoderVideo();
	void prepareEncoderAudio();
	void prepareCaptureVideo();
	void prepareCaptureAudio();
	void prepareOutputFile();

	void capturePacketsVideo();
	void capturePacketsAudio();
	void elaboratePacketsVideo();
	void elaboratePacketsAudio();
	void changeRecStatus();

	void deallocateResourcesVideo();
	void deallocateResourcesAudio();

#if defined(_WIN32) || defined(__CYGWIN__)
	char getchar_win();
#endif

public:
	ScreenRecorder(string area_size, string area_offsets, string video_fps, bool audio_flag, string out_filename);
	~ScreenRecorder();
	void record();
};

#endif
