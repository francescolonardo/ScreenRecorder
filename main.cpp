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
#include "libswscale/swscale.h"
}
#endif

using namespace std;

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

	// register signal SIGINT (CTRL+C) and signal handler
	signal(SIGINT, signalHandler);

	// print detected OS
	cout << "OS detected: " << PLATFORM_NAME << endl;

	// output filename
	const char *output_filename = "video.mp4";

	// input screen url (stream)
	string screen_url;

	// get current display information
	int screen_width = 0, screen_height = 0;
	if (PLATFORM_NAME == "linux")
	{
		Display *display = XOpenDisplay(":0");
		if (!display)
		{
			cout << "Cannot open display :0" << endl;
			display = XOpenDisplay(":1");
			screen_url = ":1.0+0,0"; // current screen (:1, x=0, y=0)
			// print display information (:1)
			printf("Display detected: :1, ");
		}
		else
		{
			screen_url = ":0.0+0,0"; // current screen (:0, x=0, y=0)
			// print display information (:0)
			printf("Display detected: :0, ");
		}
		Screen *screen = DefaultScreenOfDisplay(display);
		screen_width = screen->width;
		screen_height = screen->height;
		printf("%dx%d\n", screen_width, screen_height);
	}
	/*
	else if (PLATFORM_NAME == "windows")
	{
		screen_width = (int) GetSystemMetrics(SM_CXSCREEN);
  		screen_height = (int) GetSystemMetrics(SM_CYSCREEN);
	}
	*/

	// ------------------------------ open input devices ----------------------------- //

	// registering devices
	avdevice_register_all(); // Must be executed, otherwise av_find_input_format() fails

	// specifying the screen device as: x11grab (Linux), dshow (Windows) or avfoundation (Mac)
	// AVInputFormat holds the header information from the input format (container)
	string screen_device;
	if (PLATFORM_NAME == "linux")
		screen_device = "x11grab";
	else if (PLATFORM_NAME == "windows")
		screen_device = "dshow";
	else if (PLATFORM_NAME == "mac")
		screen_device = "avfoundation";
	AVInputFormat *vin_format = av_find_input_format(screen_device.c_str());
	if (!vin_format)
	{
		av_log(NULL, AV_LOG_ERROR, "Unknow format\n");
		cout << "Unknow input format" << endl;
		exit(1);
	}

	// filling the AVFormatContext opening the input file and reading its header
	// (codecs are not opened, so we can't analyse them)
	// AVFormatContext will hold information about the new input format (old one with options added) ???
	AVFormatContext *in_format_context = NULL;
	in_format_context = avformat_alloc_context();
	// setting up options for the demuxer
	AVDictionary *in_options = NULL;
	string video_size = to_string(screen_width) + "x" + to_string(screen_height);
	value = av_dict_set(&in_options, "video_size", video_size.c_str(), 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (video_size)\n");
		cout << "Error setting dictionary (video_size)" << endl;
		return value;
	}
	// value = av_dict_set(&in_options, "framerate", "30", 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (framerate)\n");
		cout << "Error setting dictionary (framerate)" << endl;
		return value;
	}
	//value = av_dict_set(&in_options, "preset", "ultraslow", 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (framerate)\n");
		cout << "Error setting dictionary (preset)" << endl;
		return value;
	}
	value = avformat_open_input(&in_format_context, screen_url.c_str(), vin_format, &in_options);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		cout << "Cannot open input file" << endl;
		return value;
	}
	av_dict_free(&in_options);

	// stream (packets' flow) information analysis
	// reads packets to get stream information
	// this function populates in_format_context->streams (with in_format_context->nb_streams streams)
	value = avformat_find_stream_info(in_format_context, NULL);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		cout << "Cannot find stream information" << endl;
		return value;
	}

	// print input file information
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ input device ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(in_format_context, 0, screen_url.c_str(), 0);
	cout << "Input file format (container) name: " << in_format_context->iformat->name << " (" << in_format_context->iformat->long_name << ")" << endl;

	// ------------------------------- prepare decoders ------------------------------ //

	// in our case we must have just a stream (stream type: AVMEDIA_TYPE_VIDEO)
	// cout << "Input file has " << in_format_context->nb_streams << "streams" << endl;
	value = av_find_best_stream(in_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		cout << "Cannot find a video stream in the input file" << endl;
		avformat_close_input(&in_format_context);
		return value;
	}
	int vstream_idx = value;
	// cout << "Video stream index: " << vstream_idx  << endl;

	// this is the input video stream
	AVStream *vin_stream = in_format_context->streams[vstream_idx];
	// guessing input stream framerate
	AVRational vin_fps = av_guess_frame_rate(in_format_context, vin_stream, NULL);
	// AVRational vin_fps = av_make_q(15, 1); // 15 fps
	// the component that knows how to encode the stream it's the codec
	// we can get it from the parameters of the codec used by the video stream (we just need codec_id)
	AVCodec *vin_codec = avcodec_find_decoder(vin_stream->codecpar->codec_id);
	if (!vin_codec)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find the decoder\n");
		cout << "Cannot find the decoder" << endl;
		return -1;
	}

	// allocate memory for the input codec context
	// AVCodecContext holds data about media configuration
	// such as bit rate, frame rate, sample rate, channels, height, and many others
	AVCodecContext *vin_codec_context = avcodec_alloc_context3(vin_codec);
	if (!vin_codec_context)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate memory for the decoding context\n");
		cout << "Cannot allocate memory for the decoding context" << endl;
		avformat_close_input(&in_format_context);
		return AVERROR(ENOMEM);
	}
	// fill the input codec context with the input stream parameters
	value = avcodec_parameters_to_context(vin_codec_context, vin_stream->codecpar);
	if (value < 0)
	{
		cout << "Unable to copy input stream parameters to input codec context" << endl;
		avformat_close_input(&in_format_context);
		avcodec_free_context(&vin_codec_context);
		return value;
	}

	// turns on the decoder
	// so we can proceed to the decoding process
	value = avcodec_open2(vin_codec_context, vin_codec, NULL);
	if (value < 0)
	{
		cout << "Unable to turn on the decoder" << endl;
		avformat_close_input(&in_format_context);
		avcodec_free_context(&vin_codec_context);
		return value;
	}
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ codecs/streams ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	cout << "Input codec: " << avcodec_get_name(vin_codec_context->codec_id) << " (ID: " << vin_codec_context->codec_id << ")" << endl;

	// ------------------------------- prepare encoders ------------------------------ //

	// initializes the video output file and its properties

	// (try to) guess output format from output filename
	AVOutputFormat *vout_format = av_guess_format(NULL, output_filename, NULL);
	if (!vout_format)
	{
		cout << "Failed to guess output format" << endl;
		exit(1);
	}

	// we need to prepare the output media file
	// allocate memory for the output format context
	AVFormatContext *out_format_context = NULL;
	value = avformat_alloc_output_context2(&out_format_context, NULL, NULL, output_filename);
	if (!out_format_context)
	{
		cout << "Cannot allocate output AVFormatContext" << endl;
		exit(1);
	}

	// find and fill output codec
	AVCodec *vout_codec = avcodec_find_encoder(vout_format->video_codec);
	if (!vout_codec)
	{
		cout << "Error finding among the existing codecs, try again with another codec" << endl;
		exit(1);
	}

	// create video stream in the output format context
	AVStream *vout_stream = avformat_new_stream(out_format_context, vout_codec);
	if (!vout_stream)
	{
		cout << "Cannot create an output AVStream" << endl;
		exit(1);
	}
	vout_stream->id = out_format_context->nb_streams - 1;

	// allocate memory for the output codec context
	AVCodecContext *vout_codec_context = avcodec_alloc_context3(vout_codec);
	if (!vout_codec_context)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate memory for the encoding context\n");
		cout << "Cannot allocate memory for the encoding context" << endl;
		avformat_close_input(&out_format_context);
		return AVERROR(ENOMEM);
	}
	cout << "Output codec: " << avcodec_get_name(vout_codec_context->codec_id) << " (ID: " << vout_codec_context->codec_id << ")" << endl;

	// setting up output codec context properties
	// useless: vout_codec_context->codec_id = out_format_context->video_codec; // AV_CODEC_ID_H264; AV_CODEC_ID_MPEG4; AV_CODEC_ID_MPEG1VIDEO
	// useless: vout_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
	vout_codec_context->width = vin_codec_context->width;
	vout_codec_context->height = vin_codec_context->height;
	vout_codec_context->sample_aspect_ratio = vin_codec_context->sample_aspect_ratio;
	if (vout_codec->pix_fmts)
		vout_codec_context->pix_fmt = vout_codec->pix_fmts[0];
	else
		vout_codec_context->pix_fmt = vin_codec_context->pix_fmt;
	// print output codec context properties
	printf("Output codec context: dimension=%dx%d, sample_aspect_ratio=%d/%d, pix_fmt=%s\n",
		   vout_codec_context->width, vout_codec_context->height,
		   vout_codec_context->sample_aspect_ratio.num, vout_codec_context->sample_aspect_ratio.den,
		   av_get_pix_fmt_name(vout_codec_context->pix_fmt));

	// other output codec context properties
	// vout_codec_context->bit_rate = 400 * 1000 * 1000; // 400000 kbps
	// vout_stream->codecpar->bit_rate = 400 * 1000;
	// vout_codec_context->max_b_frames = 2; // I think we have just I frames (useless)
	// vout_codec_context->gop_size = 12; // I think we have just I frames (useless)

	// setting up output codec context timebase/framerate
	vout_codec_context->time_base = av_inv_q(vin_fps);
	vout_codec_context->framerate = vin_fps;
	printf("Output codec context: time_base=%d/%d, framerate=%d/%d\n",
		   vout_codec_context->time_base.num, vout_codec_context->time_base.den,
		   vout_codec_context->framerate.num, vout_codec_context->framerate.den);

	// setting up output stream timebase/framerate
	// vout_stream->time_base = vout_codec_context->time_base;
	// vout_stream->r_frame_rate = vout_codec_context->framerate;

	// some container formats (like MP4) require global headers
	// mark the encoder so that it behaves accordingly ???
	if (out_format_context->oformat->flags & AVFMT_GLOBALHEADER)
	{
		out_format_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	// setting up options for the demuxer
	AVDictionary *out_options = NULL;
	if (vout_codec_context->codec_id == AV_CODEC_ID_H264) //H.264
	{
		av_dict_set(&out_options, "preset", "medium", 0); // or slow ???
		av_dict_set(&out_options, "tune", "zerolatency", 0);
	}
	if (vout_codec_context->codec_id == AV_CODEC_ID_H265) //H.265
	{
		av_dict_set(&out_options, "preset", "ultrafast", 0);
		av_dict_set(&out_options, "tune", "zero-latency", 0);
	}
	// turns on the encoder
	// so we can proceed to the encoding process
	value = avcodec_open2(vout_codec_context, vout_codec, &out_options);
	if (value < 0)
	{
		cout << "Unable to turn on the encoder" << endl;
		avformat_close_input(&out_format_context);
		avcodec_free_context(&vout_codec_context);
		return value;
	}
	av_dict_free(&out_options);

	// get output stream parameters from output codec context
	value = avcodec_parameters_from_context(vout_stream->codecpar, vout_codec_context);
	if (value < 0)
	{
		cout << "Unable to copy output stream parameters from output codec context" << endl;
		avformat_close_input(&in_format_context);
		avcodec_free_context(&vin_codec_context);
		return value;
	}

	// print output stream information
	printf("Output stream: bit_rate=%ld, time_base=%d/%d, framerate=%d/%d\n",
		   vout_stream->codecpar->bit_rate,
		   vout_stream->time_base.num, vout_stream->time_base.den,
		   vout_stream->r_frame_rate.num, vout_stream->r_frame_rate.den);

	// ----------------------------- prepare output file ----------------------------- //

	// unless it's a no file (we'll talk later about that) write to the disk (FLAG_WRITE)
	// but basically it's a way to save the file to a buffer so you can store it wherever you want
	if (!(out_format_context->oformat->flags & AVFMT_NOFILE))
	{
		value = avio_open(&out_format_context->pb, output_filename, AVIO_FLAG_WRITE);
		if (value < 0)
		{
			cout << "Failed opening output file " << output_filename << endl;
			return value;
			// goto end;
		}
		// cout << "-> Empty output video file (" << output_filename << ") created" << endl;
	}

	// setting up options for the demuxer
	AVDictionary *hdr_options = NULL;
	// https://superuser.com/questions/980272/what-movflags-frag-keyframeempty-moov-flag-means
	av_dict_set(&hdr_options, "movflags", "frag_keyframe+empty_moov+delay_moov+default_base_moof", 0);
	// av_opt_set(vout_codec_context->priv_data, "movflags", "frag_keyframe+delay_moov", 0);
	// av_opt_set_int(vout_codec_context->priv_data, "crf", 28, AV_OPT_SEARCH_CHILDREN); // change `cq` to `crf` if using libx264

	// mp4 container (or some advanced container file) requires header information
	value = avformat_write_header(out_format_context, &hdr_options);
	if (value < 0)
	{
		cout << "Error writing the header context" << endl;
		return value;
		// goto end;
	}
	// cout << "-> Output video file's header (" << output_filename << ") writed" << endl;
	av_dict_free(&hdr_options);

	// print output file information
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output file ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(out_format_context, 0, output_filename, 1);
	cout << "Output file format (container) name: " << out_format_context->oformat->name << " (" << out_format_context->oformat->long_name << ")" << endl;

	// --------------------------- prepare (frames) capture -------------------------- //

	// initialize sample scaler context (for converting from RGB to YUV)
	const int dst_width = vout_codec_context->width;
	const int dst_height = vout_codec_context->height;
	const AVPixelFormat dst_pix_fmt = vout_codec_context->pix_fmt;
	SwsContext *sws_context = sws_getContext(
		vin_codec_context->width, vin_codec_context->height, vin_codec_context->pix_fmt,
		dst_width, dst_height, dst_pix_fmt,
		SWS_DIRECT_BGR, NULL, NULL, NULL); // or SWS_BILINEAR
	if (!sws_context)
	{
		cout << "Failed to allocate sws context" << endl;
		return -1;
	}

	// now we're going to read the packets from the stream and decode them into frames
	// but first, we need to allocate memory for both components
	AVPacket *in_packet = av_packet_alloc();
	if (!in_packet)
	{
		cout << "Failed to allocate memory for input AVPacket" << endl;
		return -1;
	}
	AVFrame *in_frame = av_frame_alloc();
	if (!in_frame)
	{
		cout << "Failed to allocate memory for the input AVFrame" << endl;
		return -1;
	}

	// allocate memory for output packets
	AVPacket *out_packet = av_packet_alloc();
	if (!out_packet)
	{
		cout << "Failed to allocate memory for output AVPacket" << endl;
		return -1;
	}

	// we also need an output frame (for converting from RGB to YUV)
	AVFrame *out_frame = av_frame_alloc();
	if (!out_frame)
	{
		cout << "Failed to allocate memory for the output AVFrame" << endl;
		return -1;
	}

	// -------------------------------- capture frames ------------------------------- //

	int response = 0;
	int ts = 0;
	// let's feed our packets from the input stream
	// until it has packets or until user hits CTRL+C
	while (av_read_frame(in_format_context, in_packet) >= 0 && keepRunning)
	{
		// process only video stream (index 0)
		if (in_packet->stream_index == vstream_idx)
		{
			// <Transcoding output video>

			// let's send the input (compressed) packet to the decoder
			// through the input codec context
			response = avcodec_send_packet(vin_codec_context, in_packet);
			if (response < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Error sending input (compressed) packet to the decoder\n");
				cout << "Error sending input (compressed) packet to the decoder" << endl;
				return response;
			}

			while (response >= 0)
			{
				// and let's (try to) receive the input uncompressed frame from the decoder
				// through same codec context
				response = avcodec_receive_frame(vin_codec_context, in_frame);
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					// cout << "-> BREAKED!" << endl;
					break;
				}
				else if (response < 0)
				{
					av_log(NULL, AV_LOG_ERROR, "Error receiving input uncompressed frame from the decoder\n");
					cout << "Error receiving input uncompressed frame from the decoder" << endl;
					return response;
					// goto end;
				}

				// <Encoding output video>

				//
				out_frame->width = dst_width;
				out_frame->height = dst_height;
				out_frame->format = static_cast<int>(dst_pix_fmt);
				value = av_frame_get_buffer(out_frame, 32);
				if (value < 0)
				{
					cout << "Failed to  allocate picture" << endl;
					return value;
				}

				// copying input frame information to output frame
				av_frame_copy(out_frame, in_frame);
				av_frame_copy_props(out_frame, in_frame);

				// from RGB to YUV
				sws_scale(sws_context, in_frame->data, in_frame->linesize, 0, vin_codec_context->height, out_frame->data, out_frame->linesize);

				// useless (I think): out_frame->pict_type = AV_PICTURE_TYPE_NONE;

				// printing output frame info
				if (vin_codec_context->frame_number == 1)
					cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output frame ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
				printf("Frame #%d (format=%s, type=%c): pts=%ld [dts=%ld], pts_time=%ld\n",
					   vin_codec_context->frame_number,
					   av_get_pix_fmt_name(static_cast<AVPixelFormat>(out_frame->format)),
					   av_get_picture_type_char(out_frame->pict_type),
					   out_frame->pts,
					   out_frame->pkt_dts,
					   out_frame->pts * vout_stream->time_base.num / vout_stream->time_base.den);

				// let's send the uncompressed output frame to the encoder
				// through the output codec context
				response = avcodec_send_frame(vout_codec_context, out_frame);
				while (response >= 0)
				{

					// and let's (try to) receive the output packet (compressed) from the encoder
					// through the output codec context
					response = avcodec_receive_packet(vout_codec_context, out_packet);
					if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
					{
						/*
						// all fine: https://stackoverflow.com/questions/55354120/ffmpeg-avcodec-receive-frame-returns-averroreagain
						if (response == AVERROR(EAGAIN))
							cout << "-> BREAKED! response=" << response << endl;
						*/
						break;
					}
					else if (response < 0)
					{
						av_log(NULL, AV_LOG_ERROR, "Error receiving output (compressed) packet from the encoder\n");
						cout << "Error receiving output (compressed) packet from the encoder" << endl;
						return response;
					}

					out_packet->stream_index = vstream_idx; // useless (I think)

					// ---------------------- synchronize output packet --------------------- //

					// adjusting dts/pts/duration
					/*
					out_packet->pts = ts;
					out_packet->dts = ts;
					out_packet->duration = av_rescale_q(out_packet->duration, vin_stream->time_base, vout_stream->time_base);
					ts += out_packet->duration;
					out_packet->pos = -1;
					*/
					// out_packet->duration = vout_stream->time_base.den / vout_stream->time_base.num / vin_stream->avg_frame_rate.num * vin_stream->avg_frame_rate.den;

					// adjusting output packet timestamps
					av_packet_rescale_ts(out_packet, vin_stream->time_base, vout_stream->time_base);

					// print output packet information
					printf(" - Output packet: pts=%ld [dts=%ld], duration:%ld, size=%d\n",
						   out_packet->pts, out_packet->pts, out_packet->duration, out_packet->size);

					// ------------------------------------------------------------------------ //

					// write frames in output packet
					response = av_interleaved_write_frame(out_format_context, out_packet);
					if (response < 0)
					{
						cout << "Error while receiving packet from decoder" << endl;
						return -1;
					}
				}
				av_packet_unref(out_packet); // release output packet buffer data
				av_frame_unref(out_frame);	 // release output frame buffer data
				// </Encoding output video>

				av_frame_unref(in_frame); // release input frame buffer data
			}
			av_frame_unref(in_frame); // release input frame buffer data
			// </Transcoding output video>

			av_packet_unref(in_packet); // release input packet buffer data
		}
	}

	// https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga7f14007e7dc8f481f054b21614dfec13
	av_write_trailer(out_format_context);
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ end ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

	// ----------------------------- deallocate resources ---------------------------- //

	// free input format context
	avformat_close_input(&in_format_context);
	if (!in_format_context)
	{
		cout << "Input AVFormatContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free input AVFormatContext" << endl;
		exit(1);
	}

	// free input codec context
	avcodec_free_context(&vin_codec_context);
	if (!vin_codec_context)
	{
		cout << "Input AVCodecContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free input AVCodecContext" << endl;
		exit(1);
	}

	// free output format context
	if (out_format_context && !(out_format_context->oformat->flags & AVFMT_NOFILE))
	{
		avio_closep(&out_format_context->pb);
	}
	avformat_close_input(&out_format_context);
	if (!out_format_context)
	{
		cout << "Output AVFormatContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free output AVFormatContext" << endl;
		exit(1);
	}

	// free output codec context
	avcodec_free_context(&vout_codec_context);
	if (!vout_codec_context)
	{
		cout << "Output AVCodecContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free output AVCodecContext" << endl;
		exit(1);
	}

	// free sws context
	if (sws_context)
	{
		sws_freeContext(sws_context);
		sws_context = NULL;
	}

	// free packets/frames
	if (in_packet)
	{
		av_packet_free(&in_packet);
		in_packet = NULL;
	}
	if (in_frame)
	{
		av_frame_free(&in_frame);
		in_frame = NULL;
	}
	if (out_packet)
	{
		av_packet_free(&out_packet);
		out_packet = NULL;
	}
	if (out_frame)
	{
		av_frame_free(&out_frame);
		out_frame = NULL;
	}

	cout << "Program executed successfully" << endl;

	return 0;
}
