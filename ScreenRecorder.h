#include "CommandLineInterface.h"

#if defined(__linux__)
#include <X11/Xlib.h>
#elif defined(_WIN32) || defined(__CYGWIN__)
#include <wtypes.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <CoreGraphics/CoreGraphics.h>
#endif

#include <string>
#include <queue>
#include <set>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>

#define STATELESS 0
#define RECORDING 1
#define PAUSED 2
#define STOPPED 3

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
	// *** CLI object
	CommandLineInterface cli{};

	// executable's parameters
	string area_size, area_offsets;
	string area_width, area_height, area_x_offset, area_y_offset;
	string video_fps;
	bool audio_flag;
	string out_filename;

	// rec_status change management
	uint8_t rec_status;
	mutex rec_status_mtx;

	// av queues management
	queue<AVPacket *> vin_packets_q;
	mutex vin_packets_q_mtx;
	condition_variable vin_packets_q_cv;
	queue<AVPacket *> ain_packets_q;
	mutex ain_packets_q_mtx;
	condition_variable ain_packets_q_cv;

	// av_frame(...) mutual exclusion
	mutex av_write_frame_mtx;

	// captured/elaborated packets mutual esclusion
	uint64_t v_packets_captured, v_packets_elaborated; // counters
	mutex v_packets_captured_mtx;
	mutex v_packets_elaborated_mtx;

	// errors' management
	ofstream log_file; // logger()
	char err_buf[32];  // debugger()

	// globals' definition
	char tmp_str[100];
	int value; // value returned by ffmpeg's functions

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

#if defined(__APPLE__) && defined(__MACH__)
	AVFrame *vout_frame_filtered;
	AVFilterContext *buffersink_ctx;
	AVFilterContext *buffersrc_ctx;
	AVFilterGraph *filter_graph;
	string filter_descr;
#endif

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

	string getCurrentTimestamp();
	void logFileError(string str);
	void debugThrowError(string str, int level, int errnum);
	string getCurrentTimeRecorded(unsigned int packets_counter, unsigned int video_fps);
	void changeRecordingStatus();

	void openInputDeviceVideo();
	void openInputDeviceAudio();
	void prepareDecoderVideo();
	void prepareDecoderAudio();
	void prepareEncoderVideo();
	void prepareEncoderAudio();
	void prepareCaptureVideo();
	void prepareCaptureAudio();
#if defined(__APPLE__) && defined(__MACH__)
	void prepareFilterVideo();
#endif
	void prepareOutputFile();

	void capturePacketsVideo();
	void elaboratePacketsVideo();
	void writePacketVideo(AVPacket *vin_packet, uint64_t ref_ts, int ref_response);
	void capturePacketsAudio();
	void elaboratePacketsAudio();

	void deallocateResourcesVideo();
	void deallocateResourcesAudio();

public:
	ScreenRecorder(string area_size, string area_offsets, string video_fps, bool audio_flag, string out_filename);
	~ScreenRecorder();
	void record();
};
