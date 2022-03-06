#include "ScreenRecorder.h"

ScreenRecorder::ScreenRecorder(string area_size, string area_offsets, string video_fps, bool audio_flag, string out_filename) : area_size(area_size), area_offsets(area_offsets), video_fps(video_fps), audio_flag(audio_flag), out_filename(out_filename), rec_status(STATELESS) {
	// print error messages only
	av_log_set_level(AV_LOG_ERROR);

	// *** CLI - start window
	cli.cliStartWindow(area_size, area_offsets, video_fps, audio_flag, out_filename);

	// globals' initialization
	value = 0;
	v_packets_captured = 0;	  // # of captured packets
	v_packets_elaborated = 0; // # of captured packets

	// video initialization
	vin_format = NULL;
	vin_format_context = NULL;
	vin_stream = NULL;
	vin_stream_idx = 0;
	vout_stream_idx = 0;
	vin_codec_context = NULL;
	vout_stream = NULL;
	vout_codec_context = NULL;
	vin_packet = NULL;
	vin_frame = NULL;
	rescaler_context = NULL;
	vout_frame = NULL;
	vout_packet = NULL;
	// audio/video output
	// check extra part in prepareEncoderVideo()
	out_format = NULL;
	out_format_context = NULL;

	// audio initialization
	if (audio_flag) {
		ain_format = NULL;
		ain_format_context = NULL;
		ain_stream = NULL;
		ain_stream_idx = 0;
		aout_stream_idx = 0;
		ain_codec_context = NULL;
		aout_stream = NULL;
		aout_codec_context = NULL;
		ain_packet = NULL;
		ain_frame = NULL;
		resampler_context = NULL;
		a_fifo = NULL;
		aout_frame = NULL;
		aout_packet = NULL;
	}

	// video init
	openInputDeviceVideo();
	prepareDecoderVideo();
	prepareEncoderVideo();
	prepareCaptureVideo();

	// audio init
	if (audio_flag) {
		openInputDeviceAudio();
		prepareDecoderAudio();
		prepareEncoderAudio();
		prepareCaptureAudio();
	}

#if defined(__APPLE__) && defined(__MACH__)
	// filter init
	prepareFilterVideo();
#endif

	// output init
	prepareOutputFile();

	// *** CLI - key actions (pause, stop, record) info
	cli.cliKeyActionsInfo();
}

ScreenRecorder::~ScreenRecorder() {
	capture_video_thrd.get()->join();
	elaborate_video_thrd.get()->join();
	if (audio_flag) {
		capture_audio_thrd.get()->join();
		elaborate_audio_thrd.get()->join();
	}
	change_rec_status_thrd.get()->join();
	av_write_trailer(out_format_context);
	
	// deallocate everything
	deallocateResourcesVideo();
	if (audio_flag)
		deallocateResourcesAudio();

	// TODO: remember to clean everything (e.g. tmp_str)
	// *** CLI - end window
	cli.cliEndWindow(out_filename);
}

// record function starts five threads:
// - video packets capture
// - audio packets capture
// - video packets elaborate
// - audio packets elaborate
// - manage recording status
void ScreenRecorder::record() {
	rec_status = RECORDING;

	// capture video packets
	capture_video_thrd = make_unique<thread>([this]() { capturePacketsVideo(); });
	// capture audio packets
	if (audio_flag)
		capture_audio_thrd = make_unique<thread>([this]() { capturePacketsAudio(); });
	// elaborate video packets
	elaborate_video_thrd = make_unique<thread>([this]() { elaboratePacketsVideo(); });
	// elaborate audio packets
	if (audio_flag)
		elaborate_audio_thrd = make_unique<thread>([this]() { elaboratePacketsAudio(); });

	// change recording status
	change_rec_status_thrd = make_unique<thread>([this]() { changeRecordingStatus(); });
}

/*
 ***********************
 *        UTILS        *
 ***********************
 */

// getCurrentTimeStamp function returns the current local timestamp
string ScreenRecorder::getCurrentTimestamp() {
	const auto now = time(NULL);
	char ts_str[16];
	return strftime(ts_str, sizeof(ts_str), "%Y%m%d%H%M%S", localtime(&now)) ? ts_str : "";
}

// logfileError function writes the log file in case of errors.
void ScreenRecorder::logFileError(string str) {
	log_file.open("logs/log_" + getCurrentTimestamp() + ".txt", ofstream::app);
	log_file.write(str.c_str(), str.size());
	log_file.close();
}

// debugThrowError function handles throwing of runtime exceptions giving 
// information about the error.
void ScreenRecorder::debugThrowError(string error_str, int level, int err_num) {
	if (err_num < 0) {
		av_strerror(err_num, err_buf, sizeof(err_buf));
		error_str += err_buf;
	}
	if (level == AV_LOG_ERROR) {
		logFileError(error_str);
		throw runtime_error{ error_str };
	}
}

// getCurrentTimeRecorded function takes packets_counter and video_fps parameters as input
// returns the current time of recording.
string ScreenRecorder::getCurrentTimeRecorded(unsigned int packets_counter, unsigned int video_fps) {
	int time_recorded_msec = 1000 * packets_counter / video_fps;

	int hours = time_recorded_msec / (3600 * 1000);
	int rem1 = time_recorded_msec % (3600 * 1000); // hours remainder
	int minutes = rem1 / (60 * 1000);
	int rem2 = rem1 % (60 * 1000); // minutes remainder
	int seconds = rem2 / 1000;
	int milliseconds = rem2 % 1000; // seconds remainder

	char time_buf[13]; // hh:mm:ss.SSS (13 = 12 chars + '\0')
	snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
	string time_str(time_buf, time_buf + 12);

	return time_str;
}

// changeRecordingStatus function handles the commands of the user (RECORDING, STOPPED, PAUSED).
// This function access just one shared resource:
// - rec_status -> status of the program (RECORDING, PAUSE, ...)
void ScreenRecorder::changeRecordingStatus() {
	unique_lock<mutex> rec_status_ul(rec_status_mtx, defer_lock);

	char pressed_char;
	set<char> accepted_chars = { 'p', 'P', 'r', 'R', 's', 'S' };
	set<char>::iterator iter;

	printf("hello");
	rec_status_ul.lock();
	while (rec_status != STOPPED) {
		rec_status_ul.unlock();

		do {
			pressed_char = getch(); // waiting for a key // (n)curses
			iter = accepted_chars.find(pressed_char);
		} while (iter == accepted_chars.end());

		rec_status_ul.lock();
		if (rec_status == RECORDING && (pressed_char == 'p' || pressed_char == 'P')) {
			rec_status_ul.unlock();

			// *** CLI key detected [pause]
			cli.cliKeyDetectedPause();

			rec_status_ul.lock();
			rec_status = PAUSED;
			rec_status_ul.unlock();
		}
		else if (rec_status == PAUSED && (pressed_char == 'r' || pressed_char == 'R')) {
			rec_status_ul.unlock();

			// *** CLI key detected [record]
			cli.cliKeyDetectedRecord();

			rec_status_ul.lock();
			rec_status = RECORDING;
			rec_status_ul.unlock();
		}
		else if (pressed_char == 's' || pressed_char == 'S') {
			rec_status_ul.unlock();

			// *** CLI key detected [stop]
			cli.cliKeyDetectedStop();

			rec_status_ul.lock();
			rec_status = STOPPED;
			rec_status_ul.unlock();
			return;
		}
		else
			rec_status_ul.unlock();

		rec_status_ul.lock();
	}

	rec_status_ul.unlock();
}

/*
 ********************************
 *        INITIALIZATION        *
 ********************************
 */

// openInputDeviceVideo function sets the screen_device based on which is the 
// OS then creates the AVFormatContext and fills it with the header of the 
// screen_device and an AVDictionary that contains all the options needed to the demuxer.
void ScreenRecorder::openInputDeviceVideo() {
	// registering devices
	// must be executed, otherwise av_find_input_format() will fail
	avdevice_register_all();

	// specifying the screen device as: x11grab (Linux), gdigrab (Windows) or avfoundation (Mac)
	string screen_device;
#if defined(__linux__)
	screen_device = "x11grab";
#elif defined(_WIN32) || defined(__CYGWIN__)
	screen_device = "gdigrab";
#elif defined(__APPLE__) && defined(__MACH__)
	screen_device = "avfoundation";
#endif

	// AVInputFormat holds the header information from the input format (container)
	vin_format = av_find_input_format(screen_device.c_str());
	if (!vin_format)
		debugThrowError("Unknow screen device\n", AV_LOG_ERROR, 0);

	// extracting width and height from area_size
	int delimiter1_pos = area_size.find("x");
	area_width = area_size.substr(0, delimiter1_pos);
	area_height = area_size.substr(delimiter1_pos + 1, area_size.length());

	// extracting x_offset and y_offset from area_offsets
	int delimiter2_pos = area_offsets.find(",");
	area_x_offset = area_offsets.substr(0, delimiter2_pos);
	area_y_offset = area_offsets.substr(delimiter2_pos + 1, area_offsets.length());

	// getting current display information
	string screen_number, screen_width, screen_height, screen_url;

#if defined(__linux__)
	// get current display number
	screen_number = getenv("DISPLAY");
	Display* display = XOpenDisplay(screen_number.c_str());
	if (!display) {
		snprintf(tmp_str, sizeof(tmp_str), "Cannot open current display (%s)\n",
			screen_number.c_str());
		debugThrowError(tmp_str, AV_LOG_WARNING, 0);
	}
	// get current screen's size
	Screen* screen = DefaultScreenOfDisplay(display);
	screen_width = to_string(screen->width);
	screen_height = to_string(screen->height);
	XCloseDisplay(display);

	// setting screen_url basing on screen_number and area_offests
	screen_url = screen_number + ".0+" + area_offsets;

#elif defined(_WIN32) || defined(__CYGWIN__)
	// get current screen's size
	DEVMODE devmode;
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
	screen_width = to_string(devmode.dmPelsWidth);
	screen_height = to_string(devmode.dmPelsHeight);

	// setting default screen_url for Windows
	screen_url = "desktop";

#elif defined(__APPLE__) && defined(__MACH__)
	// get current screen's size
	/*
	#include <CoreGraphics/CoreGraphics.h>
	auto mainDisplayId = CGMainDisplayID();
	screen_width = to_string(CGDisplayPixelsWide(mainDisplayId));
	screen_height = to_string(CGDisplayPixelsHigh(mainDisplayId));
	*/

	// setting default screen_url for Mac OS
	screen_url = "Capture screen 0:none"; // screen_url = "Capture screen 0:none"; or screen_url = "0:none";
#endif

#if defined(__linux__) || defined(_WIN32) || defined(__CYGWIN__)
	// area_size and area_offsets check (towards current screen's size)
	if ((stoi(area_x_offset) + stoi(area_width)) > stoi(screen_width))
		debugThrowError("Check the input parameters: (area_x_offset + area_width) exceeds the current screen width\n", AV_LOG_ERROR, 0);
	if ((stoi(area_y_offset) + stoi(area_height)) > stoi(screen_height))
		debugThrowError("Check the input parameters: (area_y_offset + area_height) exceeds the current screen height\n", AV_LOG_ERROR, 0);
#endif

	// allocate memory to the component AVFormatContext that will hold information about the format (container),
	// opening the input file (screen) and reading its header
	vin_format_context = avformat_alloc_context();

	// setting up (video) input options for the demuxer
	AVDictionary* vin_options = NULL;

	value = av_dict_set(&vin_options, "pixel_format", "bgr0", 0); // bgr0 or yuyv422 or uyvy422
	if (value < 0)
		debugThrowError("Error setting video input options (pixel_format)\n", AV_LOG_ERROR, value);

#if defined(__linux__) || defined(_WIN32) || defined(__CYGWIN__)
	value = av_dict_set(&vin_options, "video_size", area_size.c_str(), 0);
	if (value < 0)
		debugThrowError("Error setting video input options (video_size)\n", AV_LOG_ERROR, value);
#elif defined(__APPLE__) && defined(__MACH__)
	// cropping basing on the input values
	filter_descr = "crop=" + area_width + ":" + area_height + ":" + area_x_offset + ":" + area_y_offset;
#endif

	value = av_dict_set(&vin_options, "framerate", video_fps.c_str(), 0);
	if (value < 0)
		debugThrowError("Error setting video input options (framerate)\n", AV_LOG_ERROR, value);

	value = av_dict_set(&vin_options, "preset", "fast", 0);
	if (value < 0)
		debugThrowError("Error setting input options (preset)\n", AV_LOG_ERROR, value);

#if defined(__linux__)
	value = av_dict_set(&vin_options, "show_region", "1", 0); // https://stackoverflow.com/questions/52863787/record-region-of-screen-using-ffmpeg
	if (value < 0)
		debugThrowError("Error setting video input options (show_region)\n", AV_LOG_ERROR, value);
#endif

	value = av_dict_set(&vin_options, "probesize", "20M", 0); // https://stackoverflow.com/questions/57903639/why-getting-and-how-to-fix-the-warning-error-on-ffmpeg-not-enough-frames-to-es
	if (value < 0)
		debugThrowError("Error setting video input options (probesize)\n", AV_LOG_ERROR, value);

	// open the screen url, read it's header and fill the AVFormatContext with minimal information about the format
	// It expects an AVFormatContext, a filename and two optional arguments: the AVInputFormat (if you pass NULL,
	// FFmpeg will guess the format but we've just got it) and the AVDictionary (which are the options to the demuxer).
	value = avformat_open_input(&vin_format_context, screen_url.c_str(), vin_format, &vin_options);
	if (value < 0)
		debugThrowError("Cannot open screen url\n", AV_LOG_ERROR, value);

	// to access the streams, we need to read data from the media using this function, the vin_format_context->nb_streams
	// will hold the amount of streams and the vin_format_context->streams[i] will give us the i stream (an AVStream).
	value = avformat_find_stream_info(vin_format_context, &vin_options);
	if (value < 0)
		debugThrowError("Cannot find stream (video) information\n", AV_LOG_ERROR, value);

	av_dict_free(&vin_options);
}

void ScreenRecorder::openInputDeviceAudio() {
	// specifying the microphone device/url
	string mic_device, mic_url;
#if defined(__linux__)
	mic_device = "pulse";
	mic_url = "default";

#elif defined(_WIN32) || defined(__CYGWIN__)
	mic_device = "dshow";
	/*
	mic_url = getAudioDevices()[0];
	if (mic_url == "")
		debugThrowError("Failed to get default microphone device\n", AV_LOG_ERROR, 0);
	mic_url = "audio=" + mic_url;
	*/
	mic_url = "audio=Microphone (Realtek(R) Audio)"; // ??? hardcoded!

#elif defined(__APPLE__) && defined(__MACH__)
	mic_device = "avfoundation";
	mic_url = "none:Built-in Microphone"; // mic_url = "none:Built-in Microphone"; or mic_url = "none:0";
#endif

	// AVInputFormat holds the header information from the input format (container)
	ain_format = av_find_input_format(mic_device.c_str());
	if (!ain_format)
		debugThrowError("Unknow mic device\n", AV_LOG_ERROR, 0);

	// filling the AVFormatContext opening the input file (mic) and reading its header
	// (codecs are not opened, so we can't analyse them)
	ain_format_context = avformat_alloc_context();

	// setting up (audio) input options for the demuxer
	AVDictionary* ain_options = NULL;
	value = av_dict_set(&ain_options, "sample_rate", "44100", 0);
	if (value < 0)
		debugThrowError("Error setting audio input options (sample_rate)\n", AV_LOG_ERROR, value);
	/*
	value = av_dict_set(&ain_options, "async", "25", 0);
	if (value < 0)
		debugThrowError("Error setting audio input options (async)\n", AV_LOG_ERROR, value);
	*/

	// opening mic url
	value = avformat_open_input(&ain_format_context, mic_url.c_str(), ain_format, &ain_options);
	if (value < 0)
		debugThrowError("Cannot open mic url\n", AV_LOG_ERROR, value);

	// stream (packets' flow) information analysis
	// reads packets to get stream information
	// this function populates ain_format_context->streams (with ain_format_context->nb_streams streams)
	value = avformat_find_stream_info(ain_format_context, &ain_options);
	if (value < 0)
		debugThrowError("Cannot find stream (audio) information\n", AV_LOG_ERROR, value);

	av_dict_free(&ain_options);
}

// prepareDecoderVideo function extracts the video stream from the AVFormatContext
// in order to get the the codec_id from it; this info is useful to initialize the  
// decoder and then proceed to the decoding process.
void ScreenRecorder::prepareDecoderVideo() {
	// we have to find a stream (stream type: AVMEDIA_TYPE_VIDEO)
	// value will be the index of the found stream
	value = av_find_best_stream(vin_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (value < 0)
		debugThrowError("Cannot find a video stream in the input file\n", AV_LOG_ERROR, value);

	int vin_stream_idx = value; // vin_stream_idx = 0

	// this is the input video stream
	vin_stream = vin_format_context->streams[vin_stream_idx];

	vin_fps = AVRational{ stoi(video_fps), 1 };

	// each stream contains codecpar that are parameters that tell us how to decode/encode the stream
	// passing its codec_id to the function avcodec_find_decoder we can find the
	// proper codec
	AVCodec* vin_codec = avcodec_find_decoder(vin_stream->codecpar->codec_id);
	if (!vin_codec)
		debugThrowError("Cannot find the video decoder\n", AV_LOG_ERROR, 0);

	// with the codec, we can allocate memory for the AVCodecContext, which will hold the context for
	// our decode/encode process.
	vin_codec_context = avcodec_alloc_context3(vin_codec);
	if (!vin_codec_context)
		debugThrowError("Failed to allocate memory for the video decoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// once we've allocated the memory for the codec context we need to fill it with CODEC parameters
	// such as bit rate, frame rate, sample rate, channels, height, and many others taking them
	// from the stream: vin_stream->codecpar
	value = avcodec_parameters_to_context(vin_codec_context, vin_stream->codecpar);
	if (value < 0)
		debugThrowError("Unable to copy input stream parameters to video input codec context\n", AV_LOG_ERROR, value);

	// turns on the (video) decoder
	// so we can proceed to the decoding process
	value = avcodec_open2(vin_codec_context, vin_codec, NULL);
	if (value < 0)
		debugThrowError("Unable to turn on the video decoder\n", AV_LOG_ERROR, value);
}

void ScreenRecorder::prepareDecoderAudio() {
	// we have to find a stream (stream type: AVMEDIA_TYPE_AUDIO)
	value = av_find_best_stream(ain_format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (value < 0)
		debugThrowError("Cannot find an audio stream in the input file\n", AV_LOG_ERROR, value);
	ain_stream_idx = value; // ain_stream_idx = 0

	// this is the input audio stream
	ain_stream = ain_format_context->streams[ain_stream_idx];

	// the component that knows how to decode the stream it's the codec
	// we can get it from the parameters of the codec used by the audio stream (we just need codec_id)
	AVCodec* ain_codec = avcodec_find_decoder(ain_stream->codecpar->codec_id);
	if (!ain_codec)
		debugThrowError("Cannot find the audio decoder\n", AV_LOG_ERROR, 0);

	// allocate memory for the (audio) input codec context
	// AVCodecContext holds data about media configuration
	// such as bit rate, frame rate, sample rate, channels, height, and many others
	ain_codec_context = avcodec_alloc_context3(ain_codec);
	if (!ain_codec_context)
		debugThrowError("Failed to allocate memory for the audio decoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// fill the (audio) input codec context with the input stream parameters
	value = avcodec_parameters_to_context(ain_codec_context, ain_stream->codecpar);
	if (value < 0)
		debugThrowError("Unable to copy input stream parameters to audio input codec context\n", AV_LOG_ERROR, value);

	// turns on the (audio) decoder
	// so we can proceed to the decoding process
	value = avcodec_open2(ain_codec_context, ain_codec, NULL);
	if (value < 0)
		debugThrowError("Unable to turn on the audio decoder\n", AV_LOG_ERROR, value);
}

// prepareEncoderVideo function allocates memory for the output format context 
// then prepare the output codec useful to encode. Once the output codec is obtained
// it preceeds creating a video stream in the output format context using the codec.
// Furthermore it sets up the output codec context parameters taking them from the 
// input codec context.
void ScreenRecorder::prepareEncoderVideo() {
	// -------------- extra ------------- //
	// (try to) guess output format from output filename
	out_format = av_guess_format(NULL, out_filename.c_str(), NULL);
	if (!out_format)
		debugThrowError("Failed to guess output format\n", AV_LOG_ERROR, 0);

	// we need to prepare the output media file
	// allocate memory for the output format context
	value = avformat_alloc_output_context2(&out_format_context, out_format, out_format->name, out_filename.c_str());
	if (!out_format_context)
		debugThrowError("Failed to allocate memory for the output format context\n", AV_LOG_ERROR, AVERROR(ENOMEM));
	// ------------- /extra ------------- //

	// find and fill (video) output codec useful to encode
	AVCodec* vout_codec = avcodec_find_encoder(out_format->video_codec);
	if (!vout_codec)
		debugThrowError("Error finding video output codec among the existing ones\n", AV_LOG_ERROR, 0);

	// create video stream in the output format context
	vout_stream = avformat_new_stream(out_format_context, vout_codec);
	if (!vout_stream)
		debugThrowError("Cannot create an output video stream\n", AV_LOG_ERROR, 0);

	vout_stream_idx = out_format_context->nb_streams - 1; // vout_stream_idx = 0

	// allocate memory for the (video) output codec context
	vout_codec_context = avcodec_alloc_context3(vout_codec);
	if (!vout_codec_context)
		debugThrowError("Failed to allocate memory for the video encoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// setting up output codec context parameters taking them from the input codec contex
#if defined(__linux__) || defined(_WIN32) || defined(__CYGWIN__)
	vout_codec_context->width = vin_codec_context->width;
	vout_codec_context->height = vin_codec_context->height;
#elif defined(__APPLE__) && defined(__MACH__)
	vout_codec_context->width = stoi(area_width);
	vout_codec_context->height = stoi(area_height);
#endif

	vout_codec_context->pix_fmt = vout_codec->pix_fmts ? vout_codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;

	// other (video) output codec context properties
	// total_bitrate = file_size / duration
	// total_bitrate - audio_bitrate = video_bitrate
	if (vout_codec_context->codec_id == AV_CODEC_ID_MPEG4)
		vout_codec_context->bit_rate = 500 * 1000; // 500 kbps | can't set directly with x264
	// https://stackoverflow.com/questions/18563764/why-low-qmax-value-improve-video-quality // higher the values, lower the quality
	/*
	vout_codec_context->qmin = 20;
	vout_codec_context->qmax = 25;
	*/
	/*
		vout_codec_context->gop_size = 50;	  // I think we have just I frames (useless)
		vout_codec_context->max_b_frames = 2; // I think we have just I frames (useless)
	*/

	// setting up (video) output codec context timebase/framerate
	vout_codec_context->time_base.num = 1;
	vout_codec_context->time_base.den = vin_fps.num; // av_inv_q(vin_fps);
	// vout_codec_context->framerate = vin_fps;		 // useless

	// setting up (video) ouptut options for the demuxer
	// https://trac.ffmpeg.org/wiki/Encode/H.264
	AVDictionary* vout_options = NULL;
	if (vout_codec_context->codec_id == AV_CODEC_ID_H264) {
		av_dict_set(&vout_options, "preset", "fast", 0);
		av_dict_set(&vout_options, "tune", "zerolatency", 0);

		/*
		av_opt_set(vout_codec_context, "preset", "ultrafast", 0);
		av_opt_set(vout_codec_context, "tune", "zerolatency", 0);
		av_opt_set(vout_codec_context, "cabac", "1", 0);
		av_opt_set(vout_codec_context, "ref", "3", 0);
		av_opt_set(vout_codec_context, "deblock", "1:0:0", 0);
		av_opt_set(vout_codec_context, "analyse", "0x3:0x113", 0);
		av_opt_set(vout_codec_context, "subme", "7", 0);
		av_opt_set(vout_codec_context, "chroma_qp_offset", "4", 0);
		av_opt_set(vout_codec_context, "rc", "crf", 0);
		av_opt_set(vout_codec_context, "rc_lookahead", "40", 0);
		av_opt_set(vout_codec_context, "crf", "10.0", 0);
		*/
	}

	// turns on the (video) encoder
	// so we can proceed to the encoding process
	value = avcodec_open2(vout_codec_context, vout_codec, &vout_options);
	if (value < 0)
		debugThrowError("Unable to turn on the video encoder\n", AV_LOG_ERROR, value);

	av_dict_free(&vout_options);

	// get (video) output stream parameters from output codec context
	value = avcodec_parameters_from_context(vout_stream->codecpar, vout_codec_context);
	if (value < 0)
		debugThrowError("Unable to copy video output stream parameters from video output codec context\n", AV_LOG_ERROR, value);

	// *** CLI - video stream info
	cli.cliVideoStreamInfo(avcodec_get_name(vout_codec_context->codec_id), av_get_pix_fmt_name(vout_codec_context->pix_fmt), audio_flag);
}

void ScreenRecorder::prepareEncoderAudio() {
	// find and fill (audio) output codec
	AVCodec* aout_codec = avcodec_find_encoder(out_format->audio_codec);
	if (!aout_codec)
		debugThrowError("Error finding audio output codec among the existing ones\n", AV_LOG_ERROR, 0);

	// create audio stream in the output format context
	aout_stream = avformat_new_stream(out_format_context, aout_codec);
	if (!aout_stream)
		debugThrowError("Cannot create an output audio stream\n", AV_LOG_ERROR, 0);

	aout_stream_idx = out_format_context->nb_streams - 1; // aout_stream_idx = 1

	// allocate memory for the (audio) output codec context
	aout_codec_context = avcodec_alloc_context3(aout_codec);
	if (!aout_codec_context)
		debugThrowError("Failed to allocate memory for the audio encoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// setting up (audio) output codec context properties
	// aout_codec_context->codec_id = out_format_context->audio_codec; // AV_CODEC_ID_AAC; // useless
	// aout_codec_context->codec_type = AVMEDIA_TYPE_AUDIO; // useless
	aout_codec_context->channels = ain_codec_context->channels;
	aout_codec_context->channel_layout = av_get_default_channel_layout(ain_codec_context->channels);
	aout_codec_context->sample_rate = ain_codec_context->sample_rate;
	aout_codec_context->sample_fmt = aout_codec->sample_fmts ? aout_codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;

	// FIXME: fix this!
	aout_codec_context->bit_rate = 96 * 1000; // 96 kbps

	// allow the use of the experimental AAC encoder
	aout_codec_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	// setting up (audio) output codec context timebase
	aout_codec_context->time_base = (AVRational){ 1, ain_codec_context->sample_rate }; // = ain_stream->time_base;

	// turns on the (audio) encoder
	// so we can proceed to the encoding process
	value = avcodec_open2(aout_codec_context, aout_codec, NULL);
	if (value < 0)
		debugThrowError("Unable to turn on the audio encoder\n", AV_LOG_ERROR, value);

	// get (audio) output stream parameters from output codec context
	value = avcodec_parameters_from_context(aout_stream->codecpar, aout_codec_context);
	if (value < 0)
		debugThrowError("Unable to copy audio output stream parameters from audio output codec context\n", AV_LOG_ERROR, value);

	// setting up (audio) output stream timebase
	// aout_stream->time_base = aout_codec_context->time_base; // TODO: check this!

	// *** CLI - audio stream info
	cli.cliAudioStreamInfo(avcodec_get_name(aout_codec_context->codec_id), aout_codec_context->sample_rate, aout_codec_context->sample_rate);
}

// prepareCaptureVideo function prepares the process of capturing the video. 
// The capture process consists in read packets and decode them into frames so it's
// mandatory to allocate memory for both the components (vin_packet and vin_frame).
// Furthermore if the pixel formats (input and required for output) are different
// it means that a Scale operation is necessary so a rescaler context is required.
void ScreenRecorder::prepareCaptureVideo() {
	// now we're going to read the packets from the stream and decode them into frames
	// but first, we need to allocate memory for both components
	vin_packet = av_packet_alloc();
	if (!vin_packet)
		debugThrowError("Failed to allocate memory for the video input packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	vin_frame = av_frame_alloc();
	if (!vin_frame)
		debugThrowError("Failed to allocate memory for the video input frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// initialize rescaler context
	// if input and output pixel formats differ, a conversion is required
	// (from BGR to YUV)
	rescaler_context = sws_getContext(
		vin_codec_context->width, vin_codec_context->height, vin_codec_context->pix_fmt,
		vin_codec_context->width, vin_codec_context->height, vout_codec_context->pix_fmt,
		SWS_BILINEAR, NULL, NULL, NULL); // SWS_DIRECT_BGR or SWS_BILINEAR ???

	if (!rescaler_context)
		debugThrowError("Failed to allocate memory for the video rescaler context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// we need a (video) output frame because of the rescaling (converting from bgr to yuv)
	// (for temporary storage)
	vout_frame = av_frame_alloc();
	if (!vout_frame)
		debugThrowError("Failed to allocate memory for the video output frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// av_frame_get_buffer(...) fills AVFrame.data and AVFrame.buf arrays and, if necessary, allocates and fills AVFrame.extended_data and AVFrame.extended_buf
	vout_frame->format = static_cast<int>(vout_codec_context->pix_fmt);
	vout_frame->width = vin_codec_context->width;
	vout_frame->height = vin_codec_context->height;
	value = av_frame_get_buffer(vout_frame, 0);
	if (value < 0)
		debugThrowError("Failed to allocate a buffer for the video output frame\n", AV_LOG_ERROR, value);

	// we need a (video) output packet
	// (for temporary storage)
	vout_packet = av_packet_alloc();
	if (!vout_packet)
		debugThrowError("Failed to allocate memory for the video output packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));
}

void ScreenRecorder::prepareCaptureAudio() {
	// now we're going to read the packets from the stream and decode them into frames
	// but first, we need to allocate memory for both components
	ain_packet = av_packet_alloc();
	if (!ain_packet)
		debugThrowError("Failed to allocate memory for the audio input packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	ain_frame = av_frame_alloc();
	if (!ain_frame)
		debugThrowError("Failed to allocate memory for the audio input frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// initialize resampler context
	// if input and output sample formats differ, a conversion is required
	// (from S16 to FLTP)
	resampler_context = swr_alloc_set_opts(NULL,
		aout_codec_context->channel_layout, aout_codec_context->sample_fmt, aout_codec_context->sample_rate,
		av_get_default_channel_layout(ain_codec_context->channels), ain_codec_context->sample_fmt, ain_codec_context->sample_rate,
		0, NULL);
	if (!resampler_context)
		debugThrowError("Failed to allocate memory for the audio resampler context\n", AV_LOG_ERROR, AVERROR(ENOMEM));
	value = swr_init(resampler_context);
	if (value < 0)
		debugThrowError("Failed to initialize the audio resampler context\n", AV_LOG_ERROR, value);

	// allocate a FIFO buffer for the (converted) audio input samples (to be encoded)
	// based on the specified output sample format
	a_fifo = av_audio_fifo_alloc(aout_codec_context->sample_fmt, aout_codec_context->channels, 1);
	if (!a_fifo)
		debugThrowError("Failed to allocate memory for the audio samples fifo buffer\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// we need an (audio) output frame to store the audio samples (for temporary storage)
	aout_frame = av_frame_alloc();
	if (!aout_frame)
		debugThrowError("Failed to allocate memory for the audio output frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// av_frame_get_buffer(...) fill AVFrame.data and AVFrame.buf arrays and, if necessary, allocate and fill AVFrame.extended_data and AVFrame.extended_buf
	aout_frame->nb_samples = aout_codec_context->frame_size;
	aout_frame->channel_layout = aout_codec_context->channel_layout;
	aout_frame->format = aout_codec_context->sample_fmt;
	aout_frame->sample_rate = aout_codec_context->sample_rate;
	value = av_frame_get_buffer(aout_frame, 0);
	if (value < 0)
		debugThrowError("Failed to allocate a buffer for the audio output frame\n", AV_LOG_ERROR, value);

	// we need a (audio) output packet (for temporary storage)
	aout_packet = av_packet_alloc();
	if (!aout_packet)
		debugThrowError("Failed to allocate memory for the audio output packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));
}

// prepareOutputFile function prepares the output file writing the header and setting
// up the options for the demuxer.
void ScreenRecorder::prepareOutputFile() {
	// some container formats (MP4 is one of them) require global headers
	// we need to mark the encoder
	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
		out_format_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	// unless it's a no file write to the disk (FLAG_WRITE) but basically it's a way to save
	// the file to a buffer so you can store it wherever you want
	if (!(out_format_context->oformat->flags & AVFMT_NOFILE)) {
		value = avio_open(&out_format_context->pb, out_filename.c_str(), AVIO_FLAG_WRITE);
		if (value < 0) {
			snprintf(tmp_str, sizeof(tmp_str), "Failed opening output file %s\n", out_filename.c_str());
			debugThrowError(tmp_str, AV_LOG_ERROR, value);
		}
	}

	// setting up header options for the demuxer
	AVDictionary* hdr_options = NULL;
	// https://superuser.com/questions/980272/what-movflags-frag-keyframeempty-moov-flag-means
	// av_dict_set(&hdr_options, "movflags", "frag_keyframe+empty_moov+delay_moov+default_base_moof", 0);
	// av_opt_set(vout_codec_context->priv_data, "movflags", "frag_keyframe+delay_moov", 0);
	// av_opt_set_int(vout_codec_context->priv_data, "crf", 28, AV_OPT_SEARCH_CHILDREN); // change `cq` to `crf` if using libx264

	// an advanced container file (e.g. mp4) requires header information
	value = avformat_write_header(out_format_context, &hdr_options);
	if (value < 0)
		debugThrowError("Error writing the output file header\n", AV_LOG_ERROR, value);

	av_dict_free(&hdr_options);
}

#if defined(__APPLE__) && defined(__MACH__)
void ScreenRecorder::prepareFilterVideo() {
	// we need a (video) input frame to store ???
	vout_frame_filtered = av_frame_alloc();
	if (!vout_frame_filtered)
		debugThrowError("Failed to allocate memory for the video input frame filtered\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	const AVFilter* buffersrc = avfilter_get_by_name("buffer");
	const AVFilter* buffersink = avfilter_get_by_name("buffersink");

	AVFilterInOut* inputs = avfilter_inout_alloc();
	AVFilterInOut* outputs = avfilter_inout_alloc();
	filter_graph = avfilter_graph_alloc();
	if (!inputs || !outputs || !filter_graph)
		debugThrowError("Failed to allocate memory for the filter graph\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	AVRational time_base = vout_codec_context->time_base;
	char args[512];
	// buffer video source: the decoded frames from the decoder will be inserted here
	snprintf(args, sizeof(args),
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		vout_codec_context->width, vout_codec_context->height, vout_codec_context->pix_fmt,
		time_base.num, time_base.den,
		vout_codec_context->sample_aspect_ratio.num, vout_codec_context->sample_aspect_ratio.den);

	value = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
	if (value < 0)
		debugThrowError("Cannot create buffer source\n", AV_LOG_ERROR, value);

	// buffer video sink: to terminate the filter chain
	value = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
	if (value < 0)
		debugThrowError("Cannot create buffer sink\n", AV_LOG_ERROR, value);

	enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
	value = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	if (value < 0)
		debugThrowError("Cannot set output pixel format\n", AV_LOG_ERROR, value);

	/**
	 * Set the endpoints for the filter graph. The filter_graph will
	 * be linked to the graph described by filter_descr.
	 */

	/**
	 * The buffer source output must be connected to the input pad of
	 * the first filter described by filter_descr; since the first
	 * filter input label is not specified, it is set to "in" by
	 * default.
	 */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	/**
	 * The buffer sink input must be connected to the output pad of
	 * the last filter described by filter_descr; since the last
	 * filter output label is not specified, it is set to "out" by
	 * default.
	 */
	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	value = avfilter_graph_parse_ptr(filter_graph, filter_descr.c_str(), &inputs, &outputs, NULL);
	if (value < 0)
		goto end;

	value = avfilter_graph_config(filter_graph, NULL);
	if (value < 0)
		goto end;

end:
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);
}
#endif

/*
 *************************************
 *        CAPTURE/ELABORATION        *
 *************************************
 */

// capturePacketsVideo function feeds packets from the video stream with the function 
// av_read_frame. All the read packets are stored into a queue from which the 
// elaboration function takes them in order to decode them into frames.
// This function access various shared resources:
// - rec_status -> status of the program (RECORDING, PAUSE, ...)
// - vin_packets_q -> queue of video packets
// - v_packets_captured -> variable that represents the number of captured video packets

void ScreenRecorder::capturePacketsVideo() {
	unique_lock<mutex> rec_status_ul(rec_status_mtx, defer_lock);
	unique_lock<mutex> vin_packets_q_ul{ vin_packets_q_mtx, defer_lock };
	unique_lock<mutex> v_packets_captured_ul{ v_packets_captured_mtx, defer_lock };

	AVPacket* tmp_vin_packet;

	rec_status_ul.lock();

	// the capture process stops only when the rec_status is STOPPED, it goes
	// on also if the status is PAUSE, but in this case it will discards every
	// packet.
	while (rec_status != STOPPED) {
		rec_status_ul.unlock();

		tmp_vin_packet = av_packet_alloc();
		if (av_read_frame(vin_format_context, tmp_vin_packet) == 0) {
			rec_status_ul.lock();

			if (rec_status == PAUSED) {
				rec_status_ul.unlock();

				av_packet_unref(tmp_vin_packet); // wipe input packet (video) buffer data (queue)
				av_packet_free(&tmp_vin_packet); // free input packet (video) buffer data (queue)
			}
			else {
				rec_status_ul.unlock();

				vin_packets_q_ul.lock();
				vin_packets_q.push(tmp_vin_packet);
				vin_packets_q_ul.unlock();
				vin_packets_q_cv.notify_one(); 

				v_packets_captured_ul.lock();
				v_packets_captured++;
				v_packets_captured_ul.unlock();
			}
		}
		else {
			av_packet_unref(tmp_vin_packet); // wipe input packet (video) buffer data (queue)
			av_packet_free(&tmp_vin_packet); // free input packet (video) buffer data (queue)
		}

		rec_status_ul.lock();
	}

	rec_status_ul.unlock();
}

// elaboratePacketsVideo function performs the transcoding process using the decoder
// in order to obtain a frame from a packet. Then the encode process rescales the frame
// if necessary and only for MacOs it executes a crop over it (filter) in order to get the requested
// section of the screen. The encode process takes the frame as input and gives back a packet
// where the codec is the requested one and sets all its parameters. Moreover it performs 
// a syncronization of the ouput packet setting the timestamp.
// This function access various shared resources:
// - rec_status -> status of the program (RECORDING, PAUSE, ...)
// - vin_packets_q -> queue of packets
// - v_packets_captured -> variable that represents the number of captured packets
// - v_packets_elaborated -> variable that represents the number of elaborated packets
// - out_format_context (av_write_frame) -> all the packets will be written in the 
// 	 output_format_context (both audio and video).
void ScreenRecorder::elaboratePacketsVideo() {
	unique_lock<mutex> vin_packets_q_ul{ vin_packets_q_mtx, defer_lock };
	unique_lock<mutex> rec_status_ul(rec_status_mtx, defer_lock);
	unique_lock<mutex> v_packets_captured_ul{ v_packets_captured_mtx, defer_lock };
	unique_lock<mutex> v_packets_elaborated_ul{ v_packets_elaborated_mtx, defer_lock };
	unique_lock<mutex> av_write_frame_ul{ av_write_frame_mtx, defer_lock };

	int response = 0;
	uint64_t ts = 0;

	vin_packets_q_ul.lock();
	rec_status_ul.lock();
	while (rec_status != STOPPED || !vin_packets_q.empty()) {
		rec_status_ul.unlock();
		vin_packets_q_cv.wait(vin_packets_q_ul, [this]() { return !vin_packets_q.empty(); });

		vin_packet = vin_packets_q.front();
		vin_packets_q.pop();
		vin_packets_q_ul.unlock();

		/*
		FIXME:
		uint64_t &ref_ts = ts;
		int &ref_response = response;
		writePacketVideo(vin_packet, ref_ts, ref_response);
		*/

		// -------------------------------- transcode video ------------------------------ //

		// the function avcodec_send_packet sends the raw data packet (COMPRESSED frame) to the decoder,
		// through the codec context
		 response = avcodec_send_packet(vin_codec_context, vin_packet);
		if (response < 0)
			debugThrowError("Error sending input (compressed) packet to the video decoder\n", AV_LOG_ERROR, response);

		av_packet_unref(vin_packet); // wipe input packet (video) buffer data
		av_packet_free(&vin_packet); // free input packet (video) buffer data

		while (response == 0) {
			// receive the raw data frame (UNCOMPRESSED frame) from the decoder, through the same codec context
			response = avcodec_receive_frame(vin_codec_context, vin_frame);
			if (response == AVERROR(EAGAIN)) // try again
				break;
			else if (response < 0)
				debugThrowError("Error receiving video input frame from the video decoder\n", AV_LOG_ERROR, response);

			// --------------------------------- encode video -------------------------------- //

			// if the pixel formats (input and required for output) are different
			// it means that a Scale operation is necessary. The rescaler_context has been initialized int 
			// prepareDecoderVideo function. (convert (scale) from BGR to YUV)
			sws_scale(rescaler_context, vin_frame->data, vin_frame->linesize, 0, vin_codec_context->height, vout_frame->data, vout_frame->linesize);

			// av_frame_unref(vin_frame); // wipe input frame (video) buffer data // TODO: change this!

#if defined(__APPLE__) && defined(__MACH__)
			// --------------------------------- filter video -------------------------------- //

			// push (video) output frame into the filtergraph
			response = av_buffersrc_add_frame_flags(buffersrc_ctx, vout_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
			if (response < 0)
				debugThrowError("Error while feeding the filtergraph\n", AV_LOG_ERROR, response);

			// av_frame_unref(vout_frame); // wipe output frame (video) buffer data // TODO: change this!

			// pull (video) output filtered frame from the filtergraph
			response = av_buffersink_get_frame(buffersink_ctx, vout_frame_filtered);
			if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				break;
			else if (response < 0)
				debugThrowError("Error filtering (cropping) video input frame\n", AV_LOG_ERROR, response);

			// --------------------------------- /filter video ------------------------------- //

			// setting (video) output frame pts
			vout_frame_filtered->pts = ts; // = av_rescale_q(vin_frame->pts, vout_stream->time_base, vin_stream->time_base);

			// let's send the uncompressed output frame to the video encoder
			// through the video output codec context
			response = avcodec_send_frame(vout_codec_context, vout_frame_filtered);

#elif defined(__linux__) || defined(_WIN32) || defined(__CYGWIN__)

			// setting (video) output frame pts
			vout_frame->pts = ts; // = av_rescale_q(vin_frame->pts, vout_stream->time_base, vin_stream->time_base);

			// let's send the uncompressed output frame to the video encoder
			// through the video output codec context
			response = avcodec_send_frame(vout_codec_context, vout_frame);

#endif
			while (response == 0) {
				// and let's (try to) receive the output packet (compressed) from the video encoder
				// through the same codec context
				response = avcodec_receive_packet(vout_codec_context, vout_packet);
				if (response == AVERROR(EAGAIN)) // try again
					break;
				else if (response < 0)
					debugThrowError("Error receiving video output packet from the video encoder\n", AV_LOG_ERROR, response);

				vout_packet->stream_index = vout_stream_idx; // vout_stream_idx = 0

				// ----------------------- synchronize (video) ouput packet ----------------------- //

				// adjusting output packet timestamps (video)
				// av_packet_rescale_ts(vout_packet, vin_stream->time_base, vout_stream->time_base);
				vout_packet->pts = vout_packet->dts = ts; // av_rescale_q(vout_packet->pts, vout_stream->time_base,

				// ----------------------- /synchronize (video) ouput packet ---------------------- //

				// write frames in output packet (video)
				av_write_frame_ul.lock();
				response = av_write_frame(out_format_context, vout_packet);
				av_write_frame_ul.unlock();
				if (response < 0)
					debugThrowError("Error writing video output frame\n", AV_LOG_ERROR, response);

				v_packets_elaborated_ul.lock();
				v_packets_elaborated++;
				v_packets_elaborated_ul.unlock();

				ts += av_rescale_q(1, vout_codec_context->time_base, vout_stream->time_base);
			}
			av_packet_unref(vout_packet); // wipe output packet (video) buffer data
										  // av_frame_unref(vout_frame);	  // wipe output frame (video) buffer data

			// -------------------------------- /encode video -------------------------------- //
		}
		av_frame_unref(vin_frame); // wipe input frame (video) buffer data

		// ------------------------------- /transcode video ------------------------------ //

		// *** CLI - show frames and time
		v_packets_captured_ul.lock();
		string time_str = getCurrentTimeRecorded(v_packets_captured, vin_fps.num);
		v_packets_elaborated_ul.lock();
		cli.cliFramesTimeCentered(v_packets_elaborated, v_packets_captured, time_str);
		v_packets_elaborated_ul.unlock();
		v_packets_captured_ul.unlock();

		vin_packets_q_ul.lock();
		rec_status_ul.lock();
	}

	rec_status_ul.unlock();
	vin_packets_q_ul.unlock();
}

void ScreenRecorder::writePacketVideo(AVPacket* vin_packet, uint64_t ref_ts, int ref_response) {
	unique_lock<mutex> v_packets_elaborated_ul{ v_packets_elaborated_mtx, defer_lock };
	unique_lock<mutex> av_write_frame_ul{ av_write_frame_mtx, defer_lock };

	// -------------------------------- transcode video ------------------------------ //

	// let's send the input (compressed) packet to the video decoder
	// through the video input codec context
	ref_response = avcodec_send_packet(vin_codec_context, vin_packet);
	if (ref_response < 0)
		debugThrowError("Error sending input (compressed) packet to the video decoder\n", AV_LOG_ERROR, ref_response);

	av_packet_unref(vin_packet); // wipe input packet (video) buffer data
	av_packet_free(&vin_packet); // free input packet (video) buffer data

	while (ref_response == 0) {
		// and let's (try to) receive the input uncompressed frame from the video decoder
		// through same codec context
		ref_response = avcodec_receive_frame(vin_codec_context, vin_frame);
		if (ref_response == AVERROR(EAGAIN)) // try again
			break;
		else if (ref_response < 0)
			debugThrowError("Error receiving video input frame from the video decoder\n", AV_LOG_ERROR, ref_response);

		// --------------------------------- encode video -------------------------------- //

		// convert (scale) from BGR to YUV
		sws_scale(rescaler_context, vin_frame->data, vin_frame->linesize, 0, vin_codec_context->height, vout_frame->data, vout_frame->linesize);

		// av_frame_unref(vin_frame); // wipe input frame (video) buffer data // TODO: change this!

#if defined(__APPLE__) && defined(__MACH__)
		// --------------------------------- filter video -------------------------------- //

		// push (video) output frame into the filtergraph
		ref_response = av_buffersrc_add_frame_flags(buffersrc_ctx, vout_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
		if (ref_response < 0)
			debugThrowError("Error while feeding the filtergraph\n", AV_LOG_ERROR, ref_response);

		// av_frame_unref(vout_frame); // wipe output frame (video) buffer data // TODO: change this!

		// pull (video) output filtered frame from the filtergraph
		ref_response = av_buffersink_get_frame(buffersink_ctx, vout_frame_filtered);
		if (ref_response == AVERROR(EAGAIN) || ref_response == AVERROR_EOF)
			break;
		else if (ref_response < 0)
			debugThrowError("Error filtering (cropping) video input frame\n", AV_LOG_ERROR, ref_response);

		// --------------------------------- /filter video ------------------------------- //

		// setting (video) output frame pts
		vout_frame_filtered->pts = ref_ts; // = av_rescale_q(vin_frame->pts, vout_stream->time_base, vin_stream->time_base);

		// let's send the uncompressed output frame to the video encoder
		// through the video output codec context
		ref_response = avcodec_send_frame(vout_codec_context, vout_frame_filtered);

#elif defined(__linux__) || defined(_WIN32) || defined(__CYGWIN__)

		// setting (video) output frame pts
		vout_frame->pts = ref_ts; // = av_rescale_q(vin_frame->pts, vout_stream->time_base, vin_stream->time_base);

		// let's send the uncompressed output frame to the video encoder
		// through the video output codec context
		ref_response = avcodec_send_frame(vout_codec_context, vout_frame);

#endif
		while (ref_response == 0) {
			// and let's (try to) receive the output packet (compressed) from the video encoder
			// through the same codec context
			ref_response = avcodec_receive_packet(vout_codec_context, vout_packet);
			if (ref_response == AVERROR(EAGAIN)) // try again
				break;
			else if (ref_response < 0)
				debugThrowError("Error receiving video output packet from the video encoder\n", AV_LOG_ERROR, ref_response);

			vout_packet->stream_index = vout_stream_idx; // vout_stream_idx = 0

			// ----------------------- synchronize (video) ouput packet ----------------------- //

			// adjusting output packet timestamps (video)
			// av_packet_rescale_ts(vout_packet, vin_stream->time_base, vout_stream->time_base);
			vout_packet->pts = vout_packet->dts = ref_ts; // av_rescale_q(vout_packet->pts, vout_stream->time_base,

			// ----------------------- /synchronize (video) ouput packet ---------------------- //

			// write frames in output packet (video)
			av_write_frame_ul.lock();
			ref_response = av_write_frame(out_format_context, vout_packet);
			av_write_frame_ul.unlock();
			if (ref_response < 0)
				debugThrowError("Error writing video output frame\n", AV_LOG_ERROR, ref_response);

			v_packets_elaborated_ul.lock();
			v_packets_elaborated++;
			v_packets_elaborated_ul.unlock();

			ref_ts += av_rescale_q(1, vout_codec_context->time_base, vout_stream->time_base);
		}
		av_packet_unref(vout_packet); // wipe output packet (video) buffer data
									  // av_frame_unref(vout_frame);	  // wipe output frame (video) buffer data

		// -------------------------------- /encode video -------------------------------- //
	}
	av_frame_unref(vin_frame); // wipe input frame (video) buffer data

	// ------------------------------- /transcode video ------------------------------ //
}

// This function access various shared resources:
// - rec_status -> status of the program (RECORDING, PAUSE, ...)
// - ain_packets_q -> queue of audio packets
void ScreenRecorder::capturePacketsAudio() {
	unique_lock<mutex> rec_status_ul(rec_status_mtx, defer_lock);
	unique_lock<mutex> ain_packets_q_ul{ ain_packets_q_mtx, defer_lock };

	AVPacket* tmp_ain_packet;

	rec_status_ul.lock();
	while (rec_status != STOPPED) {
		rec_status_ul.unlock();

		tmp_ain_packet = av_packet_alloc();
		if (av_read_frame(ain_format_context, tmp_ain_packet) == 0) {
			rec_status_ul.lock();
			if (rec_status == PAUSED) {
				rec_status_ul.unlock();

				av_packet_unref(tmp_ain_packet); // wipe input packet (audio) buffer data (queue)
				av_packet_free(&tmp_ain_packet); // free input packet (audio) buffer data (queue)
			}
			else {
				rec_status_ul.unlock();

				ain_packets_q_ul.lock();
				ain_packets_q.push(tmp_ain_packet);
				ain_packets_q_ul.unlock();
				ain_packets_q_cv.notify_one(); // notify elaboratePacketsAudio()
			}
		}
		else {
			av_packet_unref(tmp_ain_packet); // wipe input packet (audio) buffer data (queue)
			av_packet_free(&tmp_ain_packet); // free input packet (audio) buffer data (queue)
		}
		rec_status_ul.lock();
	}

	rec_status_ul.unlock();
}

// This function access various shared resources:
// - rec_status -> status of the program (RECORDING, PAUSE, ...)
// - ain_packets_q -> queue of audio packets
// - out_format_context (av_write_frame) -> all the packets will be written in the 
// 	 output_format_context (both audio and video).
void ScreenRecorder::elaboratePacketsAudio() {
	unique_lock<mutex> ain_packets_q_ul{ ain_packets_q_mtx, defer_lock };
	unique_lock<mutex> rec_status_ul(rec_status_mtx, defer_lock);
	unique_lock<mutex> av_write_frame_ul{ av_write_frame_mtx, defer_lock };

	int response = 0;
	uint64_t ts = 1024;
	// bool last_frame = false;

	ain_packets_q_ul.lock();
	rec_status_ul.lock();
	while (rec_status != STOPPED || !vin_packets_q.empty()) {
		rec_status_ul.unlock();
		ain_packets_q_cv.wait(ain_packets_q_ul, [this]() { return !ain_packets_q.empty(); });

		ain_packet = ain_packets_q.front();
		ain_packets_q.pop();
		ain_packets_q_ul.unlock();

		// -------------------------------- transcode audio ------------------------------- //

		// let's send the (audio) input (compressed) packet to the audio decoder
		// through the audio input codec context
		response = avcodec_send_packet(ain_codec_context, ain_packet);
		if (response < 0)
			debugThrowError("Error sending audio input (compressed) packet to the audio decoder\n", AV_LOG_ERROR, response);

		av_packet_unref(ain_packet); // wipe input packet (audio) buffer data // TODO: check this!
		av_packet_free(&ain_packet); // free input packet (audio) buffer data

		while (response == 0) {
			// and let's (try to) receive the (audio) input uncompressed frame from the audio decoder
			// through same codec context
			response = avcodec_receive_frame(ain_codec_context, ain_frame);
			if (response == AVERROR(EAGAIN)) // try again
				break;
			else if (response < 0)
				debugThrowError("Error receiving audio input frame from the audio decoder\n", AV_LOG_ERROR, response);

			// cout << "Input frame (nb_samples): " << ain_frame->nb_samples << endl;
			// cout << "Output codec context (frame size): " << aout_codec_context->frame_size << endl;

			// --------------------------------- encode audio --------------------------------- //

			// allocate an array of as many pointers as audio channels (in audio output codec context)
			// each of one will point to the (converted) audio input samples of the corresponding channel
			// (a temporary storage for the (converted) audio input samples)
			uint8_t** a_converted_samples = NULL;
			value = av_samples_alloc_array_and_samples(&a_converted_samples, NULL, aout_codec_context->channels, ain_frame->nb_samples, aout_codec_context->sample_fmt, 0);
			if (value < 0)
				debugThrowError("Failed to allocate (converted) audio input samples\n", AV_LOG_ERROR, value);

			// convert from S16 to FLTP
			value = swr_convert(resampler_context, a_converted_samples, ain_frame->nb_samples, (const uint8_t**)ain_frame->extended_data, ain_frame->nb_samples);
			if (value < 0)
				debugThrowError("Failed to convert the audio input samples\n", AV_LOG_ERROR, value);

			// make the FIFO buffer as large as it needs to be
			// to hold both, the old and the new (converted) audio input samples
			value = av_audio_fifo_realloc(a_fifo, av_audio_fifo_size(a_fifo) + ain_frame->nb_samples);
			if (value < 0)
				debugThrowError("Failed to reallocate memory for the (converted) audio input samples fifo buffer\n", AV_LOG_ERROR, AVERROR(ENOMEM));

			// add the (converted) audio input samples to the FIFO buffer
			value = av_audio_fifo_write(a_fifo, (void**)a_converted_samples, ain_frame->nb_samples);
			if (value < 0)
				debugThrowError("Failed to write data to (converted) audio input samples fifo buffer\n", AV_LOG_ERROR, 0);

			av_frame_unref(ain_frame); // wipe input frame (audio) buffer data // TODO: check this!

			// free the (converted) audio input samples array
			if (a_converted_samples)
				av_freep(&a_converted_samples[0]);

			// if we have enough samples for the encoder, we encode them
			// or
			// if we stop the recording, the remaining samples are sent to the encoder
			// while (av_audio_fifo_size(a_fifo) >= aout_codec_context->frame_size || (rec_status == PAUSED && av_audio_fifo_size(a_fifo) > 0))
			while (av_audio_fifo_size(a_fifo) >= aout_codec_context->frame_size) {
				// depending on the (while) case
				// const int aout_frame_size = FFMIN(aout_codec_context->frame_size, av_audio_fifo_size(a_fifo));
				int aout_frame_size = aout_codec_context->frame_size;

				// read from the (converted) audio input samples fifo buffer
				// as many samples as required to fill the audio output frame
				value = av_audio_fifo_read(a_fifo, (void**)aout_frame->data, aout_frame_size);
				if (value < 0)
					debugThrowError("Failed to read data from the audio samples fifo buffer\n", AV_LOG_ERROR, value);

				// adjusting audio output frame pts/dts
				// based on its samples number
				aout_frame->pts = ts;
				ts += aout_frame->nb_samples;

				// let's send the uncompressed (audio) output frame to the audio encoder
				// through the audio output codec context
				response = avcodec_send_frame(aout_codec_context, aout_frame);
				while (response == 0) {

					// and let's (try to) receive the output packet (compressed) from the audio encoder
					// through the same codec context
					response = avcodec_receive_packet(aout_codec_context, aout_packet);
					if (response == AVERROR(EAGAIN)) // try again
						break;
					else if (response < 0)
						debugThrowError("Error receiving audio output packet from the audio encoder\n", AV_LOG_ERROR, response);

					aout_packet->stream_index = aout_stream_idx; // aout_stream_idx = 1

					// ----------------------- synchronize (audio) output packet ---------------------- //

					// adjusting output packet pts/dts/duration
					av_packet_rescale_ts(aout_packet, aout_codec_context->time_base, aout_stream->time_base); // ???

					// TODO: check this!
					/*
					if (aout_packet->pts < 0)
						aout_packet->pts = aout_packet->dts = 0;
					*/

					// ---------------------- /synchronize (audio) output packet ---------------------- //

					// write frames in output packet (audio)
					av_write_frame_ul.lock();
					response = av_interleaved_write_frame(out_format_context, aout_packet);
					av_write_frame_ul.unlock();
					if (response < 0)
						debugThrowError("Error writing audio output frame\n", AV_LOG_ERROR, response);

					av_packet_unref(aout_packet); // wipe output packet (audio) buffer data

					/*
					// if we are at the end ???
					if (sig_ctrl_c)
					{
						// flush the encoder as it may have delayed frames ???
						while (!last_frame)
						{

							// let's send the uncompressed (audio) output frame to the audio encoder
							// through the audio output codec context
							response = avcodec_send_frame(aout_codec_context, NULL);
							while (!last_frame)
							{
								// and let's (try to) receive the output packet (compressed) from the audio encoder
								// through the same codec context
								response = avcodec_receive_packet(aout_codec_context, aout_packet);
								if (response == AVERROR(EAGAIN)) // try again
									break;
								else if (response == AVERROR_EOF)
								{
									last_frame = true;
									break;
								}
								else if (response < 0)
									debugThrowError("Error receiving audio output packet from the audio encoder\n", AV_LOG_ERROR, response);

								aout_packet->stream_index = astream_idx;

								// ----------------------- synchronize (audio) output packet ---------------------- //

								// adjusting output packet pts/dts/duration

								// print output packet information (audio)
								// FIXME: fix this!
								printf(" - Audio output packet: pts=%ld [dts=%ld], duration:%ld, size=%d\n",
									aout_packet->pts, aout_packet->dts, aout_packet->duration, aout_packet->size);

								// ---------------------- /synchronize (audio) output packet ---------------------- //

								// write frames in output packet (audio)
								response = av_interleaved_write_frame(out_format_context, aout_packet);
								if (response < 0)
									debugThrowError("Error writing audio output frame\n", AV_LOG_ERROR, response);

								av_packet_unref(aout_packet); // wipe output packet (audio) buffer data
							}
						}
					}
					*/
				}
				av_packet_unref(aout_packet); // wipe output packet (audio) buffer data
											  // av_frame_unref(aout_frame);	  // wipe output frame (audio) buffer data
			}
			// --------------------------------- /encode audio -------------------------------- //
		}
		av_frame_unref(ain_frame); // wipe input frame (audio) buffer data

		// ------------------------------- /transcode audio ------------------------------- //

		ain_packets_q_ul.lock();
		rec_status_ul.lock();
	}

	rec_status_ul.unlock();
	ain_packets_q_ul.unlock();
}

/*
 *************************************
 *       	  DEALLOCATION        	 *
 *************************************
 */
void ScreenRecorder::deallocateResourcesVideo() {
	// close (video) input format context
	if (vin_format_context) {
		avformat_close_input(&vin_format_context);
		if (!vin_format_context)
			debugThrowError("Video input format context closed successfully\n", AV_LOG_INFO, 0);
		else if (vin_format_context)
			debugThrowError("Unable to close video input format context\n", AV_LOG_WARNING, 0);
	}

	// free (video) input format context
	if (vin_format_context) {
		avformat_free_context(vin_format_context);
		if (!vin_format_context)
			debugThrowError("Video input format context freed successfully\n", AV_LOG_INFO, 0);
		else if (vin_format_context)
			debugThrowError("Unable to free video input format context\n", AV_LOG_WARNING, 0);
	}

	// close output format context
	if (out_format_context && !(out_format_context->oformat->flags & AVFMT_NOFILE))
		avio_closep(&out_format_context->pb);

	// free (video) input codec context
	if (vin_codec_context) {
		avcodec_free_context(&vin_codec_context);
		if (!vin_codec_context)
			debugThrowError("Video input codec context freed successfully\n", AV_LOG_INFO, 0);
		else if (vin_format_context)
			debugThrowError("Unable to free video input codec context\n", AV_LOG_WARNING, 0);
	}

	// free (video) output codec context
	if (vout_codec_context) {
		avcodec_free_context(&vout_codec_context);
		if (!vout_codec_context)
			debugThrowError("Video output codec context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugThrowError("Unable to free video output context\n", AV_LOG_WARNING, 0);
	}

#if defined(__APPLE__) && defined(__MACH__)
	// avfilter_graph_free(&filter_graph);
#endif

	// free rescaler context
	if (rescaler_context) {
		sws_freeContext(rescaler_context);
		rescaler_context = NULL;
	}

	// free packets/frames
	if (vin_packet) {
		av_packet_free(&vin_packet);
		vin_packet = NULL;
	}
	if (vin_frame) {
		av_frame_free(&vin_frame);
		vin_frame = NULL;
	}
	if (vout_frame) {
		av_frame_free(&vout_frame);
		vout_frame = NULL;
	}
	if (vout_packet) {
		av_packet_free(&vout_packet);
		vout_packet = NULL;
	}
}

void ScreenRecorder::deallocateResourcesAudio() {
	// close (audio) input format context
	if (ain_format_context) {
		avformat_close_input(&ain_format_context);
		if (!ain_format_context)
			debugThrowError("Audio input format context closed successfully\n", AV_LOG_INFO, 0);
		else if (vin_format_context)
			debugThrowError("Unable to close audio input format context\n", AV_LOG_WARNING, 0);
	}

	// free (audio) input format context
	if (ain_format_context) {
		avformat_free_context(ain_format_context);
		if (!ain_format_context)
			debugThrowError("Audio input format context freed successfully\n", AV_LOG_INFO, 0);
		else if (vin_format_context)
			debugThrowError("Unable to free audio input format context\n", AV_LOG_WARNING, 0);
	}

	// free (audio) input codec context
	if (ain_codec_context) {
		avcodec_free_context(&ain_codec_context);
		if (!ain_codec_context)
			debugThrowError("Audio input codec context freed successfully\n", AV_LOG_INFO, 0);
		else if (vin_format_context)
			debugThrowError("Unable to free audio input codec context\n", AV_LOG_WARNING, 0);
	}

	// free (audio) output codec context
	if (aout_codec_context) {
		avcodec_free_context(&aout_codec_context);
		if (!aout_codec_context)
			debugThrowError("Audio output codec context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugThrowError("Unable to free audio output context\n", AV_LOG_WARNING, 0);
	}

	// free resampler context
	if (resampler_context) {
		swr_free(&resampler_context);
		resampler_context = NULL;
	}

	// free packets/frames
	if (ain_packet) {
		av_packet_free(&ain_packet);
		ain_packet = NULL;
	}
	if (ain_frame) {
		av_frame_free(&ain_frame);
		ain_frame = NULL;
	}
	if (aout_frame) {
		av_frame_free(&aout_frame);
		aout_frame = NULL;
	}
	if (aout_packet) {
		av_packet_free(&aout_packet);
		aout_packet = NULL;
	}
}
