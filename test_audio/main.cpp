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
#include <ctime>
#include <assert.h>

#define __STDC_CONSTANT_MACROS

// FFMPEG LIBRARIES
#ifdef __cplusplus
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
#endif

using namespace std;

string getTimestamp()
{
	const auto now = time(NULL);
	char ts_str[16];
	return strftime(ts_str, sizeof(ts_str), "%Y%m%d%H%M%S", localtime(&now)) ? ts_str : "";
}

ofstream log_file;
void logger(string str)
{
	log_file.open("logs/log_" + getTimestamp() + ".txt", ofstream::app);
	log_file.write(str.c_str(), str.size());
	log_file.close();
}

char errbuf[32];
void debugger(string str, int level, int errnum)
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

bool keepRunning = true;
void signalHandler(int signum)
{
	cout << "Interrupt signal (CTRL+C) received" << endl;

	keepRunning = false;
	// cleanup and close up stuff here

	// terminate program
	// exit(signum);
}

int main(int argc, const char *argv[]) // argv[1]:
{
	/*
	// TODO: add an "help"
	// we expect at most 3 arguments (the last one is optional):
	// the audio flag (e.g. audioy or audion), the output filename (e.g. video.mp4) and the screen size (e.g. 320x240)
	if (argc < 2)
	{
		// TODO: write it up better
		cout << "Too few arguments, they must be at least 2" << endl;
		exit(1);
	}
	else if (argc > 3)
	{
		// TODO: write it up better
		cout << "Too many arguments, they can be at most 3" << endl;
		exit(1);
	}

	// checking audio flag
	if (argv[1] != "audioy" || argv[1] != "audion")
	{
		// TODO: write it up better
		cout << "Wrong (1st) argument, it can be: audioy or audion" << endl;
		exit(1);
	}
	string audio_flag(argv[1]);
	// checking output filename
	vector<regex> regex_extensions({".*\\.mp4$", ".*\\.avi$"}); // accepted output extensions
	for (int i=0; i<regex_extensions.size(); i++)
	{
		if (!regex_match(argv[2], regex_extensions[i]))
		{
			// TODO: write it up better
			cout << "Wrong (2st) argument, the output format can be: mp4 or avi" << endl;
			exit(1);
		}
	}
	string output_filename_(argv[2]);
	// checking screen size
	if (argc == 3)
	{
		regex regex_size("(\d+)x(\d+)"); // accepted screen size format
		if (!regex_match(argv[3], regex_size))
		{
			// TODO: write it up better
			cout << "Wrong (3st) argument, the screen size must be in this format: widthxheight (e.g. 320x240)" << endl;
			exit(1);
		}
	}
	string screen_size(argv[3]);
	*/

	int value = 0;
	char tmp_str[100];

	// register signal SIGINT (CTRL+C) and signal handler
	signal(SIGINT, signalHandler);

	// print detected OS
	debugger("OS detected: " + static_cast<string>(PLATFORM_NAME) + "\n", AV_LOG_INFO, 0);

	// output filename
	const char *output_filename = "test.mp4";

	// ------------------------------ open input devices ----------------------------- //

	// registering devices
	avdevice_register_all(); // Must be executed, otherwise av_find_input_format() fails

	// -------------- audio ------------- //

	// specifying the microphone device/url
	string mic_device, mic_url;
#if defined(_WIN32) || defined(__CYGWIN__)
	mic_url = DS_GetDefaultDevice("a");
	if (mic_url == "")
		debugger("Failed to get default microphone device\n", AV_LOG_ERROR, 0);
	mic_url = "audio=" + mic_url;
#elif defined(__linux__)
	mic_device = "pulse";
	mic_url = "default";
#elif defined(__APPLE__) && defined(__MACH__)
	mic_url = ":0";
#endif

	// AVInputFormat holds the header information from the input format (container)
	AVInputFormat *ain_format = av_find_input_format(mic_device.c_str());
	if (!ain_format)
		debugger("Unknow mic device\n", AV_LOG_ERROR, 0);

	// filling the AVFormatContext opening the input file (mic) and reading its header
	// (codecs are not opened, so we can't analyse them)
	AVFormatContext *ain_format_context = NULL;
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
	cout << "Input file format (container) name: " << ain_format_context->iformat->name << " (" << ain_format_context->iformat->long_name << ")" << endl;

	// ------------------------------- prepare decoders ------------------------------ //

	// -------------- audio ------------- //

	// we have to find a stream (stream type: AVMEDIA_TYPE_AUDIO)
	value = av_find_best_stream(ain_format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (value < 0)
		debugger("Cannot find an audio stream in the input file\n", AV_LOG_ERROR, value);
	int astream_idx = value;

	// this is the input audio stream
	AVStream *ain_stream = ain_format_context->streams[astream_idx];

	// the component that knows how to decode the stream it's the codec
	// we can get it from the parameters of the codec used by the audio stream (we just need codec_id)
	AVCodec *ain_codec = avcodec_find_decoder(ain_stream->codecpar->codec_id);
	if (!ain_codec)
		debugger("Cannot find the audio decoder\n", AV_LOG_ERROR, 0);

	// allocate memory for the (audio) input codec context
	// AVCodecContext holds data about media configuration
	// such as bit rate, frame rate, sample rate, channels, height, and many others
	AVCodecContext *ain_codec_context = avcodec_alloc_context3(ain_codec);
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

	// ------------------------------- prepare encoders ------------------------------ //

	// (try to) guess output format from output filename
	AVOutputFormat *out_format = av_guess_format(NULL, output_filename, NULL);
	if (!out_format)
		debugger("Failed to guess output format\n", AV_LOG_ERROR, 0);

	// we need to prepare the output media file
	// allocate memory for the output format context
	AVFormatContext *out_format_context = NULL;
	value = avformat_alloc_output_context2(&out_format_context, NULL, NULL, output_filename);
	if (!out_format_context)
		debugger("Failed to allocate memory for the output format context\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// -------------- audio ------------- //

	// find and fill (audio) output codec
	AVCodec *aout_codec = avcodec_find_encoder(out_format->audio_codec);
	if (!aout_codec)
		debugger("Error finding audio output codec among the existing ones\n", AV_LOG_ERROR, 0);

	// create audio stream in the output format context
	AVStream *aout_stream = avformat_new_stream(out_format_context, aout_codec);
	if (!aout_stream)
		debugger("Cannot create an output audio stream\n", AV_LOG_ERROR, 0);

	aout_stream->id = out_format_context->nb_streams - 1;

	// allocate memory for the (audio) output codec context
	AVCodecContext *aout_codec_context = avcodec_alloc_context3(aout_codec);
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
	aout_codec_context->bit_rate = 96 * 1000; // 96 kbps
	// aout_stream->codecpar->bit_rate = 96 * 1000; // TODO: check this!

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
	aout_codec_context->time_base = (AVRational){1, aout_codec_context->sample_rate};

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

	// -------------- extra ------------- //

	// some container formats (MP4 is one of them) require global headers
	// we need to mark the encoder
	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
		out_format_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	// ----------------------------- prepare output file ----------------------------- //

	// unless it's a no file (we'll talk later about that) write to the disk (FLAG_WRITE)
	// but basically it's a way to save the file to a buffer so you can store it wherever you want
	if (!(out_format_context->oformat->flags & AVFMT_NOFILE))
	{
		value = avio_open(&out_format_context->pb, output_filename, AVIO_FLAG_WRITE);
		if (value < 0)
		{
			snprintf(tmp_str, sizeof(tmp_str), "Failed opening output file %s\n", output_filename);
			debugger(tmp_str, AV_LOG_ERROR, value);
		}
		// cout << "-> Empty output video file (" << output_filename << ") created" << endl;
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

	// cout << "-> Output file header writed" << endl;
	av_dict_free(&hdr_options);

	// print output file information
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output file ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	snprintf(tmp_str, sizeof(tmp_str), "Output file format (container) name: %s (%s)\n", out_format_context->oformat->name, out_format_context->oformat->long_name);
	debugger(tmp_str, AV_LOG_INFO, 0);
	av_dump_format(out_format_context, 0, output_filename, 1);

	// --------------------------- prepare (frames) capture -------------------------- //

	// now we're going to read the packets from the stream and decode them into frames
	// but first, we need to allocate memory for both components
	AVPacket *ain_packet = av_packet_alloc();
	if (!ain_packet)
		debugger("Failed to allocate memory for the audio input packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	AVFrame *ain_frame = av_frame_alloc();
	if (!ain_frame)
		debugger("Failed to allocate memory for the audio input frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// -------------- audio ------------- //

	// initialize resampler context
	// if input and output sample formats differ, a conversion is required
	// (from S16 to FLTP)
	const int dst_channel_layout = aout_codec_context->channel_layout;
	const AVSampleFormat dst_sample_fmt = aout_codec_context->sample_fmt;
	const int dst_sample_rate = aout_codec_context->sample_rate;
	SwrContext *resampler_context = swr_alloc_set_opts(NULL,
													   dst_channel_layout, dst_sample_fmt, dst_sample_rate,
													   av_get_default_channel_layout(ain_codec_context->channels), ain_codec_context->sample_fmt, ain_codec_context->sample_rate,
													   0, NULL);
	if (!resampler_context)
		debugger("Failed to allocate memory for the audio resampler context\n", AV_LOG_ERROR, AVERROR(ENOMEM));
	value = swr_init(resampler_context);
	if (value < 0)
		debugger("Failed to initialize the audio resampler context\n", AV_LOG_ERROR, value);

	// allocate a FIFO buffer for the (converted) audio input samples (to be encoded)
	// based on the specified output sample format
	const int dst_channels = aout_codec_context->channels;
	AVAudioFifo *a_fifo = av_audio_fifo_alloc(dst_sample_fmt, dst_channels, 1);
	if (!a_fifo)
		debugger("Failed to allocate memory for the audio samples fifo buffer\n", AV_LOG_ERROR, AVERROR(ENOMEM));

	// -------------------------------- capture frames ------------------------------- //

	int response = 0;
	uint64_t ts = 0;
	bool last_frame = false;
	//uint64_t aout_frame_count = 0;
	// let's feed our input packet from the input stream
	// until it has packets or until user hits CTRL+C
	while (av_read_frame(ain_format_context, ain_packet) >= 0 && !last_frame)
	{

		// process audio stream

		// ------------------------------- transcode audio ------------------------------- //

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
				debugger("Error receiving audio input uncompressed frame from the audio decoder\n", AV_LOG_ERROR, response);

			// --------------------------------- encode audio -------------------------------- //

			// allocate an array of as many pointers as audio channels (in audio output codec context)
			// each of one will point to the (converted) audio input samples of the corresponding channel
			// (a temporary storage for the (converted) audio input samples)
			uint8_t **a_converted_samples = NULL;
			value = av_samples_alloc_array_and_samples(&a_converted_samples, NULL, aout_codec_context->channels, ain_frame->nb_samples, dst_sample_fmt, 0);
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

			// clean ???
			if (a_converted_samples)
				av_freep(&a_converted_samples[0]);

			// if we have enough samples for the encoder, we encode them
			// or
			// if we stop the recording, the remaining samples are sent to the encoder
			while (av_audio_fifo_size(a_fifo) >= aout_codec_context->frame_size || (!keepRunning && av_audio_fifo_size(a_fifo) > 0))
			{
				// depending on the (while) case
				const int aout_frame_size = FFMIN(av_audio_fifo_size(a_fifo), aout_codec_context->frame_size);

				// we need an (audio) output frame to store the audio samples
				// (for temporary storage)
				AVFrame *aout_frame = av_frame_alloc();
				if (!aout_frame)
					debugger("Failed to allocate memory for the audio output frame\n", AV_LOG_ERROR, AVERROR(ENOMEM));

				// av_frame_get_buffer(...) fill AVFrame.data and AVFrame.buf arrays and, if necessary, allocate and fill AVFrame.extended_data and AVFrame.extended_buf
				aout_frame->nb_samples = aout_frame_size;
				aout_frame->channel_layout = dst_channel_layout;
				aout_frame->format = static_cast<int>(dst_sample_fmt);
				aout_frame->sample_rate = dst_sample_rate;
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
				AVPacket *aout_packet = av_packet_alloc();
				if (!aout_packet)
					debugger("Failed to allocate memory for the audio output packet\n", AV_LOG_ERROR, AVERROR(ENOMEM));

				// adjusting audio output frame pts/dts
				// based on its sample rate
				if (aout_frame)
				{
					aout_frame->pts = ts;
					ts += aout_frame->nb_samples;
				}
				//aout_frame->pts = aout_frame_count * aout_stream->time_base.den * 1024 / aout_codec_context->sample_rate;

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
						debugger("Error receiving output (compressed) packet from the audio encoder\n", AV_LOG_ERROR, response);

					aout_packet->stream_index = astream_idx;

					// --------------------------- synchronize ouput packet -------------------------- //

					// adjusting output packet pts/dts/duration
					//aout_packet->pts = aout_packet->dts = aout_frame_count * aout_stream->time_base.den * 1024 / aout_codec_context->sample_rate;
					//aout_packet->duration = aout_stream->time_base.den * 1024 / aout_codec_context->sample_rate;

					// print output packet information (audio)
					// FIXME: fix this!
					printf(" - Audio output packet: pts=%ld [dts=%ld], duration:%ld, size=%d\n",
						   aout_packet->pts, aout_packet->dts, aout_packet->duration, aout_packet->size);

					// -------------------------- /synchronize ouput packet -------------------------- //

					// write frames in output packet (audio)
					response = av_write_frame(out_format_context, aout_packet);
					if (response < 0)
						debugger("Error while receiving packet from audio decoder\n", AV_LOG_ERROR, response);

					// if we are at the end ???
					if (!keepRunning)
					{
						// flush the encoder as it may have delayed frames ???
						while (!last_frame)
						{

							// we need an (audio) output packet
							// (for temporary storage)
							AVPacket *aout_packet = av_packet_alloc();
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
									debugger("Error receiving output (compressed) packet from the audio encoder\n", AV_LOG_ERROR, response);

								aout_packet->stream_index = astream_idx;

								// --------------------------- synchronize ouput packet -------------------------- //

								// adjusting output packet pts/dts/duration
								//aout_packet->pts = aout_packet->dts = aout_frame_count * aout_stream->time_base.den * 1024 / aout_codec_context->sample_rate;
								//aout_packet->duration = aout_stream->time_base.den * 1024 / aout_codec_context->sample_rate;

								// print output packet information (audio)
								// FIXME: fix this!
								printf(" - Audio output packet: pts=%ld [dts=%ld], duration:%ld, size=%d\n",
									   aout_packet->pts, aout_packet->dts, aout_packet->duration, aout_packet->size);

								// -------------------------- /synchronize ouput packet -------------------------- //

								// write frames in output packet (audio)
								response = av_write_frame(out_format_context, aout_packet);
								if (response < 0)
									debugger("Error while receiving packet from audio decoder\n", AV_LOG_ERROR, response);
							}
						}
					}
					//aout_frame_count++;
				}
				av_frame_unref(aout_frame);	  // wipe output frame (audio) buffer data // TODO: check this!
				av_packet_unref(aout_packet); // wipe output packet (audio) buffer data // TODO: check this!
			}
			// -------------------------------- /encode audio -------------------------------- //
		}
		// ------------------------------- /transcode audio ------------------------------ //
	}

	// https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga7f14007e7dc8f481f054b21614dfec13
	av_write_trailer(out_format_context);
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ end ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

	// ----------------------------- deallocate resources ---------------------------- //

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

	// close output format context
	if (out_format_context && !(out_format_context->oformat->flags & AVFMT_NOFILE))
		avio_closep(&out_format_context->pb);

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
	// free (video) output codec context
	if (vout_codec_context)
	{
		avcodec_free_context(&vout_codec_context);
		if (!vout_codec_context)
			debugger("Video output codec context freed successfully\n", AV_LOG_INFO, 0);
		else
			debugger("Unable to free video output context\n", AV_LOG_WARNING, 0);
	}
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

	debugger("Program executed successfully\n", AV_LOG_INFO, 0);

	return 0;
}
