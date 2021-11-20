#include "ScreenRecorder.h"

ScreenRecorder::ScreenRecorder(string area_size, string area_offsets, string video_fps, bool audio_flag, string out_filename) : area_size(area_size), area_offsets(area_offsets), video_fps(video_fps), audio_flag(audio_flag), out_filename(out_filename), sig_ctrl_c(false)
{
// OS detection
#if defined(__linux__)
#define PLATFORM_NAME "linux" // Linux
#elif defined(_WIN32) || defined(__CYGWIN__)
#define PLATFORM_NAME "windows" // Windows (x86 or x64)
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_NAME "mac" // Apple Mac OS
#endif
	// print detected OS
	debugger("OS detected: " + static_cast<string>(PLATFORM_NAME) + "\n", AV_LOG_INFO, 0);
	
	// global initialization
	value = 0;
	response = 0;

	// TODO: check if I need all these global variables
	// video initialization
	vin_format = NULL;
	vin_format_context = NULL;
	vin_stream = NULL;
	vstream_idx = 0;
	vin_codec_context = NULL;
	out_format = NULL;		   // extra
	out_format_context = NULL; // extra
	vout_stream = NULL;
	vout_codec_context = NULL;
	vin_packet = NULL;
	vin_frame = NULL;
	rescaler_context = NULL;
	vout_frame = NULL;
	vout_packet = NULL;

	// audio initialization
	ain_format = NULL;
	ain_format_context = NULL;
	ain_stream = NULL;
	astream_idx = 0;
	ain_codec_context = NULL;
	aout_stream = NULL;
	aout_codec_context = NULL;
	ain_packet = NULL;
	ain_frame = NULL;
	resampler_context = NULL;
	a_fifo = NULL;
	aout_frame = NULL;
	aout_packet = NULL;

	// video init
	openInputDeviceVideo();
	prepareDecoderVideo();
	prepareEncoderVideo();
	prepareCaptureVideo();

	// audio init
	if (audio_flag)
	{
		openInputDeviceAudio();
		prepareDecoderAudio();
		prepareEncoderAudio();
		prepareCaptureAudio();
	}
	// output init
	prepareOutputFile();
}

ScreenRecorder::~ScreenRecorder()
{
	capture_video_thrd.get()->join();
	if (audio_flag)
		capture_audio_thrd.get()->join();

	av_write_trailer(out_format_context);

	// deallocate everything
	deallocateResourcesVideo();
	if (audio_flag)
		deallocateResourcesAudio();

	// TODO: remember to clean everything (e.g. tmp_str)
}

void ScreenRecorder::record(bool &sig_ctrl_c)
{
	capture_video_thrd = make_unique<thread>([this, &sig_ctrl_c]()
											 { captureFramesVideo(sig_ctrl_c); });
	if (audio_flag)
		capture_audio_thrd = make_unique<thread>([this, &sig_ctrl_c]()
												 { captureFramesAudio(sig_ctrl_c); });
}

string ScreenRecorder::getTimestamp()
{
	const auto now = time(NULL);
	char ts_str[16];
	return strftime(ts_str, sizeof(ts_str), "%Y%m%d%H%M%S", localtime(&now)) ? ts_str : "";
}

void ScreenRecorder::logger(string str)
{
	log_file.open("logs/log_" + this->getTimestamp() + ".txt", ofstream::app);
	log_file.write(str.c_str(), str.size());
	log_file.close();
}

void ScreenRecorder::debugger(string str, int level, int errnum)
{
	if (errnum < 0)
	{
		av_strerror(errnum, errbuf, sizeof(errbuf));
		str += errbuf;
	}
	logger(str);
	av_log(NULL, level, str.c_str());
	if (level == AV_LOG_ERROR)
		if (errnum < 0)
			exit(errnum);
		else
			exit(-1);
}

void ScreenRecorder::openInputDeviceVideo()
{
	// registering devices
	avdevice_register_all(); // Must be executed, otherwise av_find_input_format() fails

	// specifying the screen device as: x11grab (Linux), dshow (Windows) or avfoundation (Mac)
	string screen_device;
	if (PLATFORM_NAME == "linux")
		screen_device = "x11grab";
	else if (PLATFORM_NAME == "windows")
		screen_device = "dshow";
	else if (PLATFORM_NAME == "mac")
		screen_device = "avfoundation";

	// AVInputFormat holds the header information from the input format (container)
	vin_format = av_find_input_format(screen_device.c_str());
	if (!vin_format)
		debugger("Unknow screen device\n", AV_LOG_ERROR, 0);

	// getting current display information
	string screen_number, screen_width, screen_height;
	string screen_url;
#if defined(__linux__)
	// get current display number
	screen_number = getenv("DISPLAY");
	Display *display = XOpenDisplay(screen_number.c_str());
	if (!display)
	{
		snprintf(tmp_str, sizeof(tmp_str), "Cannot open current display (%s)\n",
				 screen_number.c_str());
		debugger(tmp_str, AV_LOG_WARNING, 0);
	}
	// get current screen's size
	Screen *screen = DefaultScreenOfDisplay(display);
	screen_width = to_string(screen->width);
	screen_height = to_string(screen->height);
	XCloseDisplay(display);

	// print current display information
	snprintf(tmp_str, sizeof(tmp_str), "Current display: %s, %sx%s\n",
			 screen_number.c_str(), screen_width.c_str(), screen_height.c_str());
	debugger(tmp_str, AV_LOG_INFO, 0);

	// setting screen_url basing on screen_number and area_offests
	// TODO: add area_size and area_offsets check (towards current screen's size)
	screen_url = screen_number + ".0+" + area_offsets;

#elif defined(_WIN32) || defined(__CYGWIN__)  // TODO: implement it
	screen_width = (int)GetSystemMetrics(SM_CXSCREEN);
	screen_height = (int)GetSystemMetrics(SM_CYSCREEN);
#elif defined(__APPLE__) && defined(__MACH__) // TODO: implement it
#endif

	// filling the AVFormatContext opening the input file (screen) and reading its header
	// (codecs are not opened, so we can't analyse them)
	vin_format_context = avformat_alloc_context();

	// setting up (video) input options for the demuxer
	AVDictionary *vin_options = NULL;
	value = av_dict_set(&vin_options, "video_size", area_size.c_str(), 0);
	if (value < 0)
		debugger("Error setting input options (video_size)\n", AV_LOG_ERROR, value);
	value = av_dict_set(&vin_options, "framerate", video_fps.c_str(), 0);
	if (value < 0)
		debugger("Error setting input options (framerate)\n", AV_LOG_ERROR, value);
	/*
	value = av_dict_set(&vin_options, "preset", "ultrafast", 0);
	if (value < 0)
		debugger("Error setting input options (preset)\n", AV_LOG_ERROR, value);
	*/
	av_dict_set(&vin_options, "show_region", "1", 0); //https://stackoverflow.com/questions/52863787/record-region-of-screen-using-ffmpeg
	if (value < 0)
		debugger("Error setting input options (show_region)\n", AV_LOG_ERROR, value);
	av_dict_set(&vin_options, "probesize", "20M", 0); // https://stackoverflow.com/questions/57903639/why-getting-and-how-to-fix-the-warning-error-on-ffmpeg-not-enough-frames-to-es
	if (value < 0)
		debugger("Error setting input options (probesize)\n", AV_LOG_ERROR, value);

	// opening screen url
	value = avformat_open_input(&vin_format_context, screen_url.c_str(), vin_format, &vin_options);
	if (value < 0)
		debugger("Cannot open screen url\n", AV_LOG_ERROR, value);

	// stream (packets' flow) information analysis
	// reads packets to get stream information
	// this function populates vin_format_context->streams (with vin_format_context->nb_streams streams)
	value = avformat_find_stream_info(vin_format_context, &vin_options);
	if (value < 0)
		debugger("Cannot find stream (video) information\n", AV_LOG_ERROR, value);

	av_dict_free(&vin_options);

	// print input file (video) information
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ input device (video) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(vin_format_context, 0, screen_url.c_str(), 0);
	cout << "Video input file format name: " << vin_format_context->iformat->name << " (" << vin_format_context->iformat->long_name << ")" << endl;
}

void ScreenRecorder::openInputDeviceAudio()
{
	// specifying the microphone device/url
	string mic_device, mic_url;
#if defined(__linux__)
	mic_device = "pulse";
	mic_url = "default";
#elif defined(_WIN32) || defined(__CYGWIN__)
	// TODO: implement this
	mic_url = DS_GetDefaultDevice("a");
	if (mic_url == "")
		debugger("Failed to get default microphone device\n", AV_LOG_ERROR, 0);
	mic_url = "audio=" + mic_url;
#elif defined(__APPLE__) && defined(__MACH__)
	// TODO: implement this
	mic_url = ":0";
#endif

	// AVInputFormat holds the header information from the input format (container)
	ain_format = av_find_input_format(mic_device.c_str());
	if (!ain_format)
		debugger("Unknow mic device\n", AV_LOG_ERROR, 0);

	// filling the AVFormatContext opening the input file (mic) and reading its header
	// (codecs are not opened, so we can't analyse them)
	ain_format_context = avformat_alloc_context();

	// setting up (audio) input options for the demuxer
	AVDictionary *ain_options = NULL; // TODO: check if I need some (audio) options

	// opening mic url
	value = avformat_open_input(&ain_format_context, mic_url.c_str(), ain_format, &ain_options);
	if (value < 0)
		debugger("Cannot open screen url\n", AV_LOG_ERROR, value);

	av_dict_free(&ain_options);

	// stream (packets' flow) information analysis
	// reads packets to get stream information
	// this function populates ain_format_context->streams (with ain_format_context->nb_streams streams)
	value = avformat_find_stream_info(ain_format_context, NULL);
	if (value < 0)
		debugger("Cannot find stream (audio) information\n", AV_LOG_ERROR, value);

	// print input file (video) information
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ input device (audio) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(ain_format_context, 0, mic_url.c_str(), 0);
	cout << "Audio input file format name: " << ain_format_context->iformat->name << " (" << ain_format_context->iformat->long_name << ")" << endl;
}

void ScreenRecorder::prepareDecoderVideo()
{
	// we have to find a stream (stream type: AVMEDIA_TYPE_VIDEO)
	value = av_find_best_stream(vin_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (value < 0)
		debugger("Cannot find a video stream in the input file\n", AV_LOG_ERROR, value);
	int vstream_idx = value; // vstream_idx = 0

	// this is the input video stream
	vin_stream = vin_format_context->streams[vstream_idx];

	// FIXME: fix this!
	// guessing input stream (video) framerate
	vin_fps = av_guess_frame_rate(vin_format_context, vin_stream, NULL);
	//vin_fps = av_make_q(15, 1); // 15 fps

	// the component that knows how to decode the stream it's the codec
	// we can get it from the parameters of the codec used by the video stream (we just need codec_id)
	AVCodec *vin_codec = avcodec_find_decoder(vin_stream->codecpar->codec_id);
	if (!vin_codec)
		debugger("Cannot find the video decoder\n", AV_LOG_ERROR, 0);

	// allocate memory for the (video) input codec context
	// AVCodecContext holds data about media configuration
	// such as bit rate, frame rate, sample rate, channels, height, and many others
	vin_codec_context = avcodec_alloc_context3(vin_codec);
	if (!vin_codec_context)
		debugger("Failed to allocate memory for the video decoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// fill the (video) input codec context with the input stream parameters
	value = avcodec_parameters_to_context(vin_codec_context, vin_stream->codecpar);
	if (value < 0)
		debugger("Unable to copy input stream parameters to video input codec context\n", AV_LOG_ERROR, value);

	// turns on the (video) decoder
	// so we can proceed to the decoding process
	value = avcodec_open2(vin_codec_context, vin_codec, NULL);
	if (value < 0)
		debugger("Unable to turn on the video decoder\n", AV_LOG_ERROR, value);

	// print (video) input codec context name
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ codecs/streams ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	snprintf(tmp_str, sizeof(tmp_str), "Video input codec: %s (ID: %d)\n", avcodec_get_name(vin_codec_context->codec_id), vin_codec_context->codec_id);
	debugger(tmp_str, AV_LOG_INFO, 0);
	// print (video) input codec context pixel format
	snprintf(tmp_str, sizeof(tmp_str), "Video input pix_fmt: %s\n", av_get_pix_fmt_name(vin_codec_context->pix_fmt));
	debugger(tmp_str, AV_LOG_INFO, 0);
}

void ScreenRecorder::prepareDecoderAudio()
{
	// we have to find a stream (stream type: AVMEDIA_TYPE_AUDIO)
	value = av_find_best_stream(ain_format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (value < 0)
		debugger("Cannot find an audio stream in the input file\n", AV_LOG_ERROR, value);
	astream_idx = value; // astream_idx = 0

	// this is the input audio stream
	ain_stream = ain_format_context->streams[astream_idx];

	// the component that knows how to decode the stream it's the codec
	// we can get it from the parameters of the codec used by the audio stream (we just need codec_id)
	AVCodec *ain_codec = avcodec_find_decoder(ain_stream->codecpar->codec_id);
	if (!ain_codec)
		debugger("Cannot find the audio decoder\n", AV_LOG_ERROR, 0);

	// allocate memory for the (audio) input codec context
	// AVCodecContext holds data about media configuration
	// such as bit rate, frame rate, sample rate, channels, height, and many others
	ain_codec_context = avcodec_alloc_context3(ain_codec);
	if (!ain_codec_context)
		debugger("Failed to allocate memory for the audio decoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// fill the (audio) input codec context with the input stream parameters
	value = avcodec_parameters_to_context(ain_codec_context, ain_stream->codecpar);
	if (value < 0)
		debugger("Unable to copy input stream parameters to audio input codec context\n", AV_LOG_ERROR, value);

	// turns on the (audio) decoder
	// so we can proceed to the decoding process
	value = avcodec_open2(ain_codec_context, ain_codec, NULL);
	if (value < 0)
		debugger("Unable to turn on the audio decoder\n", AV_LOG_ERROR, value);

	// print (audio) input codec context name
	snprintf(tmp_str, sizeof(tmp_str), "Audio input codec: %s (ID: %d)\n", avcodec_get_name(ain_codec_context->codec_id), ain_codec_context->codec_id);
	debugger(tmp_str, AV_LOG_INFO, 0);
	// print (audio) input codec context sample format
	snprintf(tmp_str, sizeof(tmp_str), "Audio input sample_fmt: %s\n", av_get_sample_fmt_name(ain_codec_context->sample_fmt));
	debugger(tmp_str, AV_LOG_INFO, 0);
}

void ScreenRecorder::prepareEncoderVideo()
{
	// TODO: remove this
	// -------------- extra ------------- //
	if (!out_format)
	{
		// (try to) guess output format from output filename
		out_format = av_guess_format(NULL, out_filename.c_str(), NULL);
		if (!out_format)
			debugger("Failed to guess output format\n", AV_LOG_ERROR, 0);
	}
	if (!out_format_context)
	{
		// we need to prepare the output media file
		// allocate memory for the output format context
		value = avformat_alloc_output_context2(&out_format_context, out_format, NULL, out_filename.c_str());
		if (!out_format_context)
			debugger("Failed to allocate memory for the output format context\n", AV_LOG_ERROR, AVERROR(ENOMEM));
	}
	// ------------- /extra ------------- //

	// find and fill (video) output codec
	AVCodec *vout_codec = avcodec_find_encoder(out_format->video_codec);
	if (!vout_codec)
		debugger("Error finding video output codec among the existing ones\n", AV_LOG_ERROR, 0);

	// create video stream in the output format context
	vout_stream = avformat_new_stream(out_format_context, vout_codec);
	if (!vout_stream)
		debugger("Cannot create an output video stream\n", AV_LOG_ERROR, 0);

	vstream_idx = vout_stream->id = out_format_context->nb_streams - 1;

	// allocate memory for the (video) output codec context
	vout_codec_context = avcodec_alloc_context3(vout_codec);
	if (!vout_codec_context)
		debugger("Failed to allocate memory for the video encoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// print (video) output codec context name
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	snprintf(tmp_str, sizeof(tmp_str), "Video output codec: %s (ID: %d)\n", avcodec_get_name(vout_codec_context->codec_id), vout_codec_context->codec_id);
	debugger(tmp_str, AV_LOG_INFO, 0);

	// setting up (video) output codec context properties
	// useless: vout_codec_context->codec_id = out_format_context->video_codec; // AV_CODEC_ID_H264; AV_CODEC_ID_MPEG4; AV_CODEC_ID_MPEG1VIDEO
	// useless: vout_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
	vout_codec_context->width = vin_codec_context->width;
	vout_codec_context->height = vin_codec_context->height;
	//vout_codec_context->sample_aspect_ratio = vin_codec_context->sample_aspect_ratio; // useless (I think)
	if (vout_codec->pix_fmts)
		vout_codec_context->pix_fmt = vout_codec->pix_fmts[0];
	else
		vout_codec_context->pix_fmt = vin_codec_context->pix_fmt;
	//vout_codec_context->bit_rate = 400 * 1000; // ??? kbps
	// vout_stream->codecpar->bit_rate = 400 * 1000; // TODO: check this!

	// print (video) output codec context properties
	// FIXME: fix this!
	printf("Video output codec context: dimension=%dx%d, pix_fmt=%s\n",
		   vout_codec_context->width, vout_codec_context->height,
		   av_get_pix_fmt_name(vout_codec_context->pix_fmt));

	// other (video) output codec context properties
	// vout_codec_context->max_b_frames = 2; // I think we have just I frames (useless)
	// vout_codec_context->gop_size = 12; // I think we have just I frames (useless)

	// setting up (video) output codec context timebase/framerate
	vout_codec_context->time_base = vin_stream->time_base;
	//vout_codec_context->framerate = vin_stream->avg_frame_rate; // useless (I think)

	// print other (video) output codec context properties
	// FIXME: fix this!
	printf("Video output codec context: time_base=%d/%d, framerate=%d/%d\n",
		   vout_codec_context->time_base.num, vout_codec_context->time_base.den,
		   vout_codec_context->framerate.num, vout_codec_context->framerate.den);

	// setting up (video) ouptut options for the demuxer
	AVDictionary *vout_options = NULL;
	if (vout_codec_context->codec_id == AV_CODEC_ID_H264) //H.264
	{
		av_dict_set(&vout_options, "preset", "medium", 0); // or slow ???
		av_dict_set(&vout_options, "tune", "zerolatency", 0);
	}
	if (vout_codec_context->codec_id == AV_CODEC_ID_H265) //H.265
	{
		av_dict_set(&vout_options, "preset", "ultrafast", 0);
		av_dict_set(&vout_options, "tune", "zero-latency", 0);
	}

	// turns on the (video) encoder
	// so we can proceed to the encoding process
	value = avcodec_open2(vout_codec_context, vout_codec, &vout_options);
	if (value < 0)
		debugger("Unable to turn on the video encoder\n", AV_LOG_ERROR, value);

	av_dict_free(&vout_options);

	// get (video) output stream parameters from output codec context
	value = avcodec_parameters_from_context(vout_stream->codecpar, vout_codec_context);
	if (value < 0)
		debugger("Unable to copy video output stream parameters from video output codec context\n", AV_LOG_ERROR, value);

	// setting up (video) output stream timebase/framerate
	vout_stream->time_base = vout_codec_context->time_base;	   // TODO: check this!
	vout_stream->avg_frame_rate = vin_fps;					   // TODO: check this!
	vout_stream->r_frame_rate = vout_codec_context->framerate; // TODO: check this!

	// print (video) output stream information
	// FIXME: fix this!
	printf("Video output stream: bit_rate=%ld, time_base=%d/%d, framerate=%d/%d\n",
		   vout_stream->codecpar->bit_rate,
		   vout_stream->time_base.num, vout_stream->time_base.den,
		   vout_stream->r_frame_rate.num, vout_stream->r_frame_rate.den);
}

void ScreenRecorder::prepareEncoderAudio()
{
	// TODO: remove this
	// -------------- extra ------------- //
	if (!out_format)
	{
		// (try to) guess output format from output filename
		out_format = av_guess_format(NULL, out_filename.c_str(), NULL);
		if (!out_format)
			debugger("Failed to guess output format\n", AV_LOG_ERROR, 0);
	}

	if (!out_format_context)
	{
		// we need to prepare the output media file
		// allocate memory for the output format context
		value = avformat_alloc_output_context2(&out_format_context, out_format, NULL, out_filename.c_str());
		if (!out_format_context)
			debugger("Failed to allocate memory for the output format context\n", AV_LOG_ERROR, AVERROR(ENOMEM));
	}
	// ------------- /extra ------------- //

	// find and fill (audio) output codec
	AVCodec *aout_codec = avcodec_find_encoder(out_format->audio_codec);
	if (!aout_codec)
		debugger("Error finding audio output codec among the existing ones\n", AV_LOG_ERROR, 0);

	// create audio stream in the output format context
	aout_stream = avformat_new_stream(out_format_context, aout_codec);
	if (!aout_stream)
		debugger("Cannot create an output audio stream\n", AV_LOG_ERROR, 0);

	astream_idx = aout_stream->id = out_format_context->nb_streams - 1;

	// allocate memory for the (audio) output codec context
	aout_codec_context = avcodec_alloc_context3(aout_codec);
	if (!aout_codec_context)
		debugger("Failed to allocate memory for the audio encoding context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// print (audio) output codec context name
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	snprintf(tmp_str, sizeof(tmp_str), "Audio output codec: %s (ID: %d)\n", avcodec_get_name(aout_codec_context->codec_id), aout_codec_context->codec_id);
	debugger(tmp_str, AV_LOG_INFO, 0);

	// setting up (audio) output codec context properties
	// useless: aout_codec_context->codec_id = out_format_context->audio_codec; // AV_CODEC_ID_AAC;
	// useless: aout_codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
	aout_codec_context->channels = ain_codec_context->channels;
	aout_codec_context->channel_layout = av_get_default_channel_layout(ain_codec_context->channels);
	aout_codec_context->sample_rate = ain_codec_context->sample_rate;
	if (aout_codec->sample_fmts)
		aout_codec_context->sample_fmt = aout_codec->sample_fmts[0];
	else
		aout_codec_context->sample_fmt = ain_codec_context->sample_fmt;
	// FIXME: fix this!
	aout_codec_context->bit_rate = 96 * 1000; // 96 kbps

	// print (audio) output codec context properties // FIXME: fix this!
	printf("Audio output codec context: channels=%d, channel_layout=%ld, sample_rate=%d, sample_fmt=%s, bit_rate=%ld\n",
		   aout_codec_context->channels,
		   aout_codec_context->channel_layout,
		   aout_codec_context->sample_rate,
		   av_get_sample_fmt_name(aout_codec_context->sample_fmt),
		   aout_codec_context->bit_rate);

	// allow the use of the experimental AAC encoder
	//aout_codec_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	// setting up (audio) output codec context timebase
	aout_codec_context->time_base = ain_stream->time_base; // = (AVRational){1, aout_codec_context->sample_rate};

	// print other (audio) output codec context properties
	// FIXME: fix this!
	printf("Audio output codec context: time_base=%d/%d, sample_rate=%d\n",
		   aout_codec_context->time_base.num, aout_codec_context->time_base.den,
		   aout_codec_context->sample_rate);

	// turns on the (audio) encoder
	// so we can proceed to the encoding process
	value = avcodec_open2(aout_codec_context, aout_codec, NULL);
	if (value < 0)
		debugger("Unable to turn on the audio encoder\n", AV_LOG_ERROR, value);

	// get (audio) output stream parameters from output codec context
	value = avcodec_parameters_from_context(aout_stream->codecpar, aout_codec_context);
	if (value < 0)
		debugger("Unable to copy audio output stream parameters from audio output codec context\n", AV_LOG_ERROR, value);

	// setting up (audio) output stream timebase
	aout_stream->time_base = aout_codec_context->time_base; // TODO: check this!

	// print (audio) output stream information
	// FIXME: fix this!
	printf("Audio output stream: bit_rate=%ld, time_base=%d/%d\n",
		   aout_stream->codecpar->bit_rate,
		   aout_stream->time_base.num, aout_stream->time_base.den);
}

void ScreenRecorder::prepareOutputFile()
{
	// some container formats (MP4 is one of them) require global headers
	// we need to mark the encoder
	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
		out_format_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	// unless it's a no file (we'll talk later about that) write to the disk (FLAG_WRITE)
	// but basically it's a way to save the file to a buffer so you can store it wherever you want
	if (!(out_format_context->oformat->flags & AVFMT_NOFILE))
	{
		value = avio_open(&out_format_context->pb, out_filename.c_str(), AVIO_FLAG_WRITE);
		if (value < 0)
		{
			snprintf(tmp_str, sizeof(tmp_str), "Failed opening output file %s\n", out_filename.c_str());
			debugger(tmp_str, AV_LOG_ERROR, value);
		}
		// cout << "Empty output video file (" << out_filename << ") created" << endl;
	}

	// setting up header options for the demuxer
	AVDictionary *hdr_options = NULL;
	// https://superuser.com/questions/980272/what-movflags-frag-keyframeempty-moov-flag-means
	av_dict_set(&hdr_options, "movflags", "frag_keyframe+empty_moov+delay_moov+default_base_moof", 0);
	// av_opt_set(vout_codec_context->priv_data, "movflags", "frag_keyframe+delay_moov", 0);
	// av_opt_set_int(vout_codec_context->priv_data, "crf", 28, AV_OPT_SEARCH_CHILDREN); // change `cq` to `crf` if using libx264

	// some advanced container file (e.g. mp4) requires header information
	value = avformat_write_header(out_format_context, &hdr_options);
	if (value < 0)
		debugger("Error writing the output file header\n", AV_LOG_ERROR, value);

	// cout << "Output file header writed" << endl;
	av_dict_free(&hdr_options);

	// print output file information
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output file ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	snprintf(tmp_str, sizeof(tmp_str), "Output file format (container) name: %s (%s)\n", out_format_context->oformat->name, out_format_context->oformat->long_name);
	debugger(tmp_str, AV_LOG_INFO, 0);
	av_dump_format(out_format_context, 0, out_filename.c_str(), 1);
}

void ScreenRecorder::prepareCaptureVideo()
{
	// now we're going to read the packets from the stream and decode them into frames
	// but first, we need to allocate memory for both components
	vin_packet = av_packet_alloc();
	if (!vin_packet)
		debugger("Failed to allocate memory for the video input packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));
	vin_frame = av_frame_alloc();
	if (!vin_frame)
		debugger("Failed to allocate memory for the video input frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// initialize rescaler context
	// if input and output pixel formats differ, a conversion is required
	// (from BGR to YUV)
	rescaler_context = sws_getContext(
		vin_codec_context->width, vin_codec_context->height, vin_codec_context->pix_fmt,
		vout_codec_context->width, vout_codec_context->height, vout_codec_context->pix_fmt,
		SWS_DIRECT_BGR, NULL, NULL, NULL); // or SWS_BILINEAR
	if (!rescaler_context)
		debugger("Failed to allocate memory for the video rescaler context\n", AV_LOG_ERROR, AVERROR(ENOMEM));
}

void ScreenRecorder::prepareCaptureAudio()
{
	// now we're going to read the packets from the stream and decode them into frames
	// but first, we need to allocate memory for both components
	ain_packet = av_packet_alloc();
	if (!ain_packet)
		debugger("Failed to allocate memory for the audio input packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	ain_frame = av_frame_alloc();
	if (!ain_frame)
		debugger("Failed to allocate memory for the audio input frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// initialize resampler context
	// if input and output sample formats differ, a conversion is required
	// (from S16 to FLTP)
	resampler_context = swr_alloc_set_opts(NULL,
										   aout_codec_context->channel_layout, aout_codec_context->sample_fmt, aout_codec_context->sample_rate,
										   av_get_default_channel_layout(ain_codec_context->channels), ain_codec_context->sample_fmt, ain_codec_context->sample_rate,
										   0, NULL);
	if (!resampler_context)
		debugger("Failed to allocate memory for the audio resampler context\n", AV_LOG_ERROR, AVERROR(ENOMEM));
	value = swr_init(resampler_context);
	if (value < 0)
		debugger("Failed to initialize the audio resampler context\n", AV_LOG_ERROR, value);

	// allocate a FIFO buffer for the (converted) audio input samples (to be encoded)
	// based on the specified output sample format
	a_fifo = av_audio_fifo_alloc(aout_codec_context->sample_fmt, aout_codec_context->channels, 1);
	if (!a_fifo)
		debugger("Failed to allocate memory for the audio samples fifo buffer\n", AV_LOG_ERROR, AVERROR(ENOMEM));
}

void ScreenRecorder::captureFramesVideo(bool &sig_ctrl_c)
{
	// let's feed our input packet from the input stream
	// until it has packets or until user hits CTRL+C
	while (av_read_frame(vin_format_context, vin_packet) >= 0 && !sig_ctrl_c)
	{

		// -------------------------------- transcode video ------------------------------ //

		// let's send the input (compressed) packet to the video decoder
		// through the video input codec context
		response = avcodec_send_packet(vin_codec_context, vin_packet);
		if (response < 0)
			debugger("Error sending input (compressed) packet to the video decoder\n", AV_LOG_ERROR, response);

		av_packet_unref(vin_packet); // wipe input packet (video) buffer data

		while (response >= 0)
		{
			// and let's (try to) receive the input uncompressed frame from the video decoder
			// through same codec context
			response = avcodec_receive_frame(vin_codec_context, vin_frame);
			if (response == AVERROR(EAGAIN)) // try again
				break;
			else if (response < 0)
				debugger("Error receiving video input frame from the video decoder\n", AV_LOG_ERROR, response);

			// --------------------------------- encode video -------------------------------- //

			// we need an (video) output frame to store the audio samples
			// (for temporary storage)
			vout_frame = av_frame_alloc();
			if (!vout_frame)
				debugger("Failed to allocate memory for the video output frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

			// av_frame_get_buffer(...) fill AVFrame.data and AVFrame.buf arrays and, if necessary, allocate and fill AVFrame.extended_data and AVFrame.extended_buf
			vout_frame->format = static_cast<int>(vout_codec_context->pix_fmt);
			vout_frame->width = vout_codec_context->width;
			vout_frame->height = vout_codec_context->height;
			value = av_frame_get_buffer(vout_frame, 0);
			if (value < 0)
				debugger("Failed to allocate a buffer for the video output frame\n", AV_LOG_ERROR, value);

			// copying (video) input frame information to (video) output frame
			//av_frame_copy(vout_frame, vin_frame);
			av_frame_copy_props(vout_frame, vin_frame);

			// convert (scale) from BGR to YUV
			sws_scale(rescaler_context, vin_frame->data, vin_frame->linesize, 0, vin_codec_context->height, vout_frame->data, vout_frame->linesize);

			// useless (I think): vout_frame->pict_type = AV_PICTURE_TYPE_NONE;

			av_frame_unref(vin_frame); // wipe input frame (video) buffer data

			/*
			// printing output frame info
			if (vin_codec_context->frame_number == 1)
				cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output frame ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
			// FIXME: fix this!
			printf("Output frame (video) info: #%d (format=%s, type=%c): pts=%ld [dts=%ld], pts_time=%ld\n",
				   vin_codec_context->frame_number,
				   av_get_pix_fmt_name(static_cast<AVPixelFormat>(vout_frame->format)),
				   av_get_picture_type_char(vout_frame->pict_type),
				   vout_frame->pts,
				   vout_frame->pkt_dts,
				   vout_frame->pts * vout_stream->time_base.num / vout_stream->time_base.den);
			*/

			// we need an (video) output packet
			// (for temporary storage)
			vout_packet = av_packet_alloc();
			if (!vout_packet)
				debugger("Failed to allocate memory for the video output packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

			// let's send the uncompressed output frame to the video encoder
			// through the video output codec context
			response = avcodec_send_frame(vout_codec_context, vout_frame);
			while (response >= 0)
			{
				// and let's (try to) receive the output packet (compressed) from the video encoder
				// through the same codec context
				response = avcodec_receive_packet(vout_codec_context, vout_packet);
				if (response == AVERROR(EAGAIN)) // try again
					break;
				else if (response < 0)
					debugger("Error receiving video output packet from the video encoder\n", AV_LOG_ERROR, response);

				vout_packet->stream_index = vstream_idx;

				// ----------------------- synchronize (video) ouput packet ----------------------- //

				// adjusting output packet timestamps (video)
				//av_packet_rescale_ts(vout_packet, vin_stream->time_base, vout_stream->time_base);

				// print output packet information (video)
				// FIXME: fix this!
				printf(" - Video output packet: pts=%ld [dts=%ld], duration:%ld, size=%d\n",
					   vout_packet->pts, vout_packet->dts, vout_packet->duration, vout_packet->size);

				// ----------------------- /synchronize (video) ouput packet ---------------------- //

				// write frames in output packet (video)
				response = av_interleaved_write_frame(out_format_context, vout_packet);
				if (response < 0)
					debugger("Error writing video output frame\n", AV_LOG_ERROR, response);
			}
			av_packet_unref(vout_packet); // wipe output packet (video) buffer data
			av_frame_unref(vout_frame);	  // wipe output frame (video) buffer data

			// -------------------------------- /encode video -------------------------------- //
		}
		av_frame_unref(vin_frame); // wipe input frame (video) buffer data

		// ------------------------------- /transcode video ------------------------------ //
	}
}

void ScreenRecorder::captureFramesAudio(bool &sig_ctrl_c)
{
	uint64_t ts = 0;
	bool last_frame = false;
	//uint64_t aout_frame_count = 0;
	// let's feed our input packet from the input stream
	// until it has packets or until user hits CTRL+C
	while (av_read_frame(ain_format_context, ain_packet) >= 0 && !last_frame)
	{
		// -------------------------------- transcode audio ------------------------------- //

		// let's send the (audio) input (compressed) packet to the audio decoder
		// through the audio input codec context
		response = avcodec_send_packet(ain_codec_context, ain_packet);
		if (response < 0)
			debugger("Error sending audio input (compressed) packet to the audio decoder\n", AV_LOG_ERROR, response);

		av_packet_unref(ain_packet); // wipe input packet (audio) buffer data

		while (response >= 0)
		{
			// and let's (try to) receive the (audio) input uncompressed frame from the audio decoder
			// through same codec context
			response = avcodec_receive_frame(ain_codec_context, ain_frame);
			if (response == AVERROR(EAGAIN)) // try again
				break;
			else if (response < 0)
				debugger("Error receiving audio input frame from the audio decoder\n", AV_LOG_ERROR, response);

			//cout << "Input frame (nb_samples): " << ain_frame->nb_samples << endl;
			//cout << "Output codec context (frame size): " << aout_codec_context->frame_size << endl;

			// --------------------------------- encode audio --------------------------------- //

			// allocate an array of as many pointers as audio channels (in audio output codec context)
			// each of one will point to the (converted) audio input samples of the corresponding channel
			// (a temporary storage for the (converted) audio input samples)
			uint8_t **a_converted_samples = NULL;
			value = av_samples_alloc_array_and_samples(&a_converted_samples, NULL, aout_codec_context->channels, ain_frame->nb_samples, aout_codec_context->sample_fmt, 0);
			if (value < 0)
				debugger("Failed to allocate (converted) audio input samples\n", AV_LOG_ERROR, value);

			// convert from S16 to FLTP
			value = swr_convert(resampler_context, a_converted_samples, ain_frame->nb_samples, (const uint8_t **)ain_frame->extended_data, ain_frame->nb_samples);
			if (value < 0)
				debugger("Failed to convert the audio input samples\n", AV_LOG_ERROR, value);

			// make the FIFO buffer as large as it needs to be
			// to hold both, the old and the new (converted) audio input samples
			value = av_audio_fifo_realloc(a_fifo, av_audio_fifo_size(a_fifo) + ain_frame->nb_samples);
			if (value < 0)
				debugger("Failed to reallocate memory for the (converted) audio input samples fifo buffer\n", AV_LOG_ERROR, AVERROR(ENOMEM));

			// add the (converted) audio input samples to the FIFO buffer
			value = av_audio_fifo_write(a_fifo, (void **)a_converted_samples, ain_frame->nb_samples);
			if (value < 0)
				debugger("Failed to write data to (converted) audio input samples fifo buffer\n", AV_LOG_ERROR, 0);

			av_frame_unref(ain_frame); // wipe input frame (audio) buffer data

			// free the (converted) audio input samples array
			if (a_converted_samples)
				av_freep(&a_converted_samples[0]);

			// if we have enough samples for the encoder, we encode them
			// or
			// if we stop the recording, the remaining samples are sent to the encoder
			while (av_audio_fifo_size(a_fifo) >= aout_codec_context->frame_size || (sig_ctrl_c && av_audio_fifo_size(a_fifo) > 0))
			{
				// depending on the (while) case
				const int aout_frame_size = FFMIN(aout_codec_context->frame_size, av_audio_fifo_size(a_fifo));

				// we need an (audio) output frame to store the audio samples
				// (for temporary storage)
				aout_frame = av_frame_alloc();
				if (!aout_frame)
					debugger("Failed to allocate memory for the audio output frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

				// av_frame_get_buffer(...) fill AVFrame.data and AVFrame.buf arrays and, if necessary, allocate and fill AVFrame.extended_data and AVFrame.extended_buf
				aout_frame->nb_samples = aout_frame_size;
				aout_frame->channel_layout = aout_codec_context->channel_layout;
				aout_frame->format = static_cast<int>(aout_codec_context->sample_fmt);
				aout_frame->sample_rate = aout_codec_context->sample_rate;
				//aout_frame->channels = aout_codec_context->channels; // TODO: check this!
				value = av_frame_get_buffer(aout_frame, 0);
				if (value < 0)
					debugger("Failed to allocate a buffer for the audio output frame\n", AV_LOG_ERROR, value);

				// read from the (converted) audio input samples fifo buffer
				// as many samples as required to fill the audio output frame
				value = av_audio_fifo_read(a_fifo, (void **)aout_frame->data, aout_frame_size);
				if (value < 0)
					debugger("Failed to read data from the audio samples fifo buffer\n", AV_LOG_ERROR, value);

				// we need an (audio) output packet
				// (for temporary storage)
				aout_packet = av_packet_alloc();
				if (!aout_packet)
					debugger("Failed to allocate memory for the audio output packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

				// adjusting audio output frame pts/dts
				// based on its samples number
				aout_frame->pts = ts;
				ts += aout_frame->nb_samples;

				// let's send the uncompressed (audio) output frame to the audio encoder
				// through the audio output codec context
				response = avcodec_send_frame(aout_codec_context, aout_frame);
				while (response >= 0)
				{
					// and let's (try to) receive the output packet (compressed) from the audio encoder
					// through the same codec context
					response = avcodec_receive_packet(aout_codec_context, aout_packet);
					if (response == AVERROR(EAGAIN)) // try again
						break;
					else if (response < 0)
						debugger("Error receiving audio output packet from the audio encoder\n", AV_LOG_ERROR, response);

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
						debugger("Error writing audio output frame\n", AV_LOG_ERROR, response);

					av_packet_unref(aout_packet); // wipe output packet (audio) buffer data

					// if we are at the end ???
					if (sig_ctrl_c)
					{
						// flush the encoder as it may have delayed frames ???
						while (!last_frame)
						{

							// we need an (audio) output packet
							// (for temporary storage)
							aout_packet = av_packet_alloc();
							if (!aout_packet)
								debugger("Failed to allocate memory for the audio output packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

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
									debugger("Error receiving audio output packet from the audio encoder\n", AV_LOG_ERROR, response);

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
									debugger("Error writing audio output frame\n", AV_LOG_ERROR, response);

								av_packet_unref(aout_packet); // wipe output packet (audio) buffer data
							}
						}
					}
				}
				av_packet_unref(aout_packet); // wipe output packet (audio) buffer data
				av_frame_unref(aout_frame);	  // wipe output frame (audio) buffer data
			}
			// --------------------------------- /encode audio -------------------------------- //
		}
		av_frame_unref(ain_frame); // wipe input frame (audio) buffer data

		// ------------------------------- /transcode audio ------------------------------- //
	}
}

void ScreenRecorder::deallocateResourcesVideo()
{
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ deallocation ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

	// close (video) input format context
	if (vin_format_context)
	{
		avformat_close_input(&vin_format_context);
		if (!vin_format_context)
			debugger("Video input format context closed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to close video input format context\n", AV_LOG_WARNING, 0);
	}

	// free (video) input format context
	if (vin_format_context)
	{
		avformat_free_context(vin_format_context);
		if (!vin_format_context)
			debugger("Video input format context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to free video input format context\n", AV_LOG_WARNING, 0);
	}

	// close output format context
	if (out_format_context && !(out_format_context->oformat->flags & AVFMT_NOFILE))
		avio_closep(&out_format_context->pb);

	// free (video) input codec context
	if (vin_codec_context)
	{
		avcodec_free_context(&vin_codec_context);
		if (!vin_codec_context)
			debugger("Video input codec context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to free video input codec context\n", AV_LOG_WARNING, 0);
	}

	/*
	// free (video) output codec context
	if (vout_codec_context)
	{
		avcodec_free_context(&vout_codec_context);
		if (!vout_codec_context)
			debugger("Video output codec context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to free video output context\n", AV_LOG_WARNING, 0);
	}
	*/

	// TODO: add other freeing stuff

	// FIXME: fix this! (if necessary)
	// free rescaler context
	if (rescaler_context)
	{
		sws_freeContext(rescaler_context);
		rescaler_context = NULL;
	}

	// free packets/frames
	if (vin_packet)
	{
		av_packet_free(&vin_packet);
		vin_packet = NULL;
	}
	if (vin_frame)
	{
		av_frame_free(&vin_frame);
		vin_frame = NULL;
	}
	if (vout_frame)
	{
		av_frame_free(&vout_frame);
		vout_frame = NULL;
	}
	if (vout_packet)
	{
		av_packet_free(&vout_packet);
		vout_packet = NULL;
	}
}

void ScreenRecorder::deallocateResourcesAudio()
{
	// close (audio) input format context
	if (ain_format_context)
	{
		avformat_close_input(&ain_format_context);
		if (!ain_format_context)
			debugger("Audio input format context closed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to close audio input format context\n", AV_LOG_WARNING, 0);
	}

	// free (audio) input format context
	if (ain_format_context)
	{
		avformat_free_context(ain_format_context);
		if (!ain_format_context)
			debugger("Audio input format context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to free audio input format context\n", AV_LOG_WARNING, 0);
	}

	// free (audio) input codec context
	if (ain_codec_context)
	{
		avcodec_free_context(&ain_codec_context);
		if (!ain_codec_context)
			debugger("Audio input codec context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to free audio input codec context\n", AV_LOG_WARNING, 0);
	}

	/*
	// free (audio) output codec context
	if (aout_codec_context)
	{
		avcodec_free_context(&aout_codec_context);
		if (!aout_codec_context)
			debugger("Audio output codec context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to free audio output context\n", AV_LOG_WARNING, 0);
	}
	*/

	// FIXME: fix this! (if necessary)
	// free resampler context
	if (resampler_context)
	{
		swr_free(&resampler_context);
		resampler_context = NULL;
	}

	// free packets/frames
	if (ain_packet)
	{
		av_packet_free(&ain_packet);
		ain_packet = NULL;
	}
	if (ain_frame)
	{
		av_frame_free(&ain_frame);
		ain_frame = NULL;
	}
	if (aout_frame)
	{
		av_frame_free(&aout_frame);
		aout_frame = NULL;
	}
	if (aout_packet)
	{
		av_packet_free(&aout_packet);
		aout_packet = NULL;
	}
}
