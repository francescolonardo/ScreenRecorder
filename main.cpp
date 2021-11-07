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
void signalHandler( int signum ) {
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

	// input filename
	string input_filename;

	// get current display information
	int screen_width = 0, screen_height = 0;
	if (PLATFORM_NAME == "linux")
	{
		Display *display = XOpenDisplay(":0");
		if (!display)
		{
			cout << "Cannot open display :0" << endl;
			display = XOpenDisplay(":1");
			input_filename = ":1.0+0,0"; // current screen (:1, x=0, y=0)
			// print display information (:1)
			printf("Display detected: :1, ");
		}
		else
		{
			input_filename = ":0.0+0,0"; // current screen (:0, x=0, y=0)
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

	// ---------------------- Input device part ---------------------- //

	// registering device
	avdevice_register_all(); // Must be executed, otherwise av_find_input_format() fails

	// specifying the input format as: x11grab (Linux), dshow (Windows) or avfoundation (Mac)
	// AVInputFormat holds the header information from the input format (container)
	string input_device;
	if (PLATFORM_NAME == "linux")
		input_device = "x11grab";
	else if (PLATFORM_NAME == "windows")
		input_device = "dshow";
	else if (PLATFORM_NAME == "mac")
		input_device = "avfoundation";
	AVInputFormat *pInputFormat = av_find_input_format(input_device.c_str());
	if (!pInputFormat)
	{
		av_log(NULL, AV_LOG_ERROR, "Unknow format\n");
		cout << "Unknow input format" << endl;
		exit(1);
	}

	// filling the AVFormatContext opening the input file and reading its header
	// (codecs are not opened, so we can't analyse them)
	// AVFormatContext will hold information about the new input format (old one with options added) ???
	AVFormatContext *pInFormatContext = NULL;
	pInFormatContext = avformat_alloc_context();
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
	value = avformat_open_input(&pInFormatContext, input_filename.c_str(), pInputFormat, &in_options);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		cout << "Cannot open input file" << endl;
		return value;
	}
	av_dict_free(&in_options);

	// stream (packets' flow) information analysis
	// reads packets to get stream information
	// this function populates pInFormatContext->streams (with pInFormatContext->nb_streams streams)
	value = avformat_find_stream_info(pInFormatContext, NULL);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		cout << "Cannot find stream information" << endl;
		return value;
	}

	// print input file information
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ input device ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(pInFormatContext, 0, input_filename.c_str(), 0);
	cout << "Input file format (container) name: " << pInFormatContext->iformat->name << " (" << pInFormatContext->iformat->long_name << ")" << endl;

	// ---------------------- Decoding part ---------------------- //

	// in our case we must have just a stream (video: AVMEDIA_TYPE_VIDEO)
	// cout << "Input file has " << pInFormatContext->nb_streams << "streams" << endl;
	value = av_find_best_stream(pInFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		cout << "Cannot find a video stream in the input file" << endl;
		avformat_close_input(&pInFormatContext);
		return value;
	}
	int video_stream_index = value;
	// cout << "Video stream index: " << video_stream_index  << endl;

	// this is the input video stream
	AVStream *input_stream = pInFormatContext->streams[video_stream_index];
	// guessing input stream framerate
	AVRational input_framerate = av_guess_frame_rate(pInFormatContext, input_stream, NULL);
	// AVRational input_framerate = av_make_q(15, 1); // 15 fps
	// the component that knows how to encode the stream it's the codec
	// we can get it from the parameters of the codec used by the video stream (we just need codec_id)
	AVCodec *pInCodec = avcodec_find_decoder(input_stream->codecpar->codec_id);
	if (!pInCodec)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find the decoder\n");
		cout << "Cannot find the decoder" << endl;
		return -1;
	}

	// allocate memory for the input codec context
	// AVCodecContext holds data about media configuration
	// such as bit rate, frame rate, sample rate, channels, height, and many others
	AVCodecContext *pInCodecContext = avcodec_alloc_context3(pInCodec);
	if (!pInCodecContext)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate memory for the decoding context\n");
		cout << "Cannot allocate memory for the decoding context" << endl;
		avformat_close_input(&pInFormatContext);
		return AVERROR(ENOMEM);
	}
	// fill the input codec context with the input stream parameters
	value = avcodec_parameters_to_context(pInCodecContext, input_stream->codecpar);
	if (value < 0)
	{
		cout << "Unable to copy input stream parameters to input codec context" << endl;
		avformat_close_input(&pInFormatContext);
		avcodec_free_context(&pInCodecContext);
		return value;
	}

	// turns on the decoder
	// so we can proceed to the decoding process
	value = avcodec_open2(pInCodecContext, pInCodec, NULL);
	if (value < 0)
	{
		cout << "Unable to turn on the decoder" << endl;
		avformat_close_input(&pInFormatContext);
		avcodec_free_context(&pInCodecContext);
		return value;
	}
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ codecs/streams ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	cout << "Input codec: " << avcodec_get_name(pInCodecContext->codec_id) << " (ID: " << pInCodecContext->codec_id << ")" << endl;

	// ---------------------- Init output file part ---------------------- //
	// initializes the video output file and its properties

	// (try to) guess output format from output filename
	AVOutputFormat *pOutputFormat = av_guess_format(NULL, output_filename, NULL);
	if (!pOutputFormat)
	{
		cout << "Failed to guess output format" << endl;
		exit(1);
	}

	// we need to prepare the output media file
	// allocate memory for the output format context
	AVFormatContext *pOutFormatContext = NULL;
	value = avformat_alloc_output_context2(&pOutFormatContext, NULL, NULL, output_filename);
	if (!pOutFormatContext)
	{
		cout << "Cannot allocate output AVFormatContext" << endl;
		exit(1);
	}

	// find and fill output codec
	AVCodec *pOutCodec = avcodec_find_encoder(pOutputFormat->video_codec);
	if (!pOutCodec)
	{
		cout << "Error finding among the existing codecs, try again with another codec" << endl;
		exit(1);
	}

	// create video stream in the output format context
	AVStream *output_stream = avformat_new_stream(pOutFormatContext, pOutCodec);
	if (!output_stream)
	{
		cout << "Cannot create an output AVStream" << endl;
		exit(1);
	}
	output_stream->id = pOutFormatContext->nb_streams - 1;

	// allocate memory for the output codec context
	AVCodecContext *pOutCodecContext = avcodec_alloc_context3(pOutCodec);
	if (!pOutCodecContext)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate memory for the encoding context\n");
		cout << "Cannot allocate memory for the encoding context" << endl;
		avformat_close_input(&pOutFormatContext);
		return AVERROR(ENOMEM);
	}
	cout << "Output codec: " << avcodec_get_name(pOutCodecContext->codec_id) << " (ID: " << pOutCodecContext->codec_id << ")" << endl;

	// setting up output codec context properties
	// useless: pOutCodecContext->codec_id = pOutFormatContext->video_codec; // AV_CODEC_ID_H264; AV_CODEC_ID_MPEG4; AV_CODEC_ID_MPEG1VIDEO
	// useless: pOutCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	pOutCodecContext->width = pInCodecContext->width;
	pOutCodecContext->height = pInCodecContext->height;
	pOutCodecContext->sample_aspect_ratio = pInCodecContext->sample_aspect_ratio;
	if (pOutCodec->pix_fmts)
		pOutCodecContext->pix_fmt = pOutCodec->pix_fmts[0];
	else
		pOutCodecContext->pix_fmt = pInCodecContext->pix_fmt;
	// print output codec context properties
	printf("Output codec context: dimension=%dx%d, sample_aspect_ratio=%d/%d, pix_fmt=%s\n",
		pOutCodecContext->width, pOutCodecContext->height,
		pOutCodecContext->sample_aspect_ratio.num, pOutCodecContext->sample_aspect_ratio.den,
		av_get_pix_fmt_name(pOutCodecContext->pix_fmt)
	);
	
	// other output codec context properties
	// pOutCodecContext->bit_rate = 400 * 1000 * 1000; // 400000 kbps
	// output_stream->codecpar->bit_rate = 400 * 1000;
	// pOutCodecContext->max_b_frames = 2; // I think we have just I frames (useless)
	// pOutCodecContext->gop_size = 12; // I think we have just I frames (useless)

	// setting up output codec context timebase/framerate
	pOutCodecContext->time_base = av_inv_q(input_framerate);
	pOutCodecContext->framerate = input_framerate;
	printf("Output codec context: time_base=%d/%d, framerate=%d/%d\n",
		pOutCodecContext->time_base.num, pOutCodecContext->time_base.den,
		pOutCodecContext->framerate.num, pOutCodecContext->framerate.den
	);

	// setting up output stream timebase/framerate
	// output_stream->time_base = pOutCodecContext->time_base;
	// output_stream->r_frame_rate = pOutCodecContext->framerate;

	// some container formats (like MP4) require global headers
	// mark the encoder so that it behaves accordingly ???
	if (pOutFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		pOutFormatContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	// setting up options for the demuxer
	AVDictionary *out_options = NULL;
	if (pOutCodecContext->codec_id == AV_CODEC_ID_H264) //H.264
	{
		av_dict_set(&out_options, "preset", "medium", 0); // or slow ???
		av_dict_set(&out_options, "tune", "zerolatency", 0);
	}
	if (pOutCodecContext->codec_id == AV_CODEC_ID_H265) //H.265
	{
		av_dict_set(&out_options, "preset", "ultrafast", 0);
		av_dict_set(&out_options, "tune", "zero-latency", 0);
	}
	// turns on the encoder
	// so we can proceed to the encoding process
	value = avcodec_open2(pOutCodecContext, pOutCodec, &out_options);
	if (value < 0)
	{
		cout << "Unable to turn on the encoder" << endl;
		avformat_close_input(&pOutFormatContext);
		avcodec_free_context(&pOutCodecContext);
		return value;
	}
	av_dict_free(&out_options);

	// get output stream parameters from output codec context
	value = avcodec_parameters_from_context(output_stream->codecpar, pOutCodecContext);
	if (value < 0)
	{
		cout << "Unable to copy output stream parameters from output codec context" << endl;
		avformat_close_input(&pInFormatContext);
		avcodec_free_context(&pInCodecContext);
		return value;
	}

	// print output stream information
	printf("Output stream: bit_rate=%ld, time_base=%d/%d, framerate=%d/%d\n",
		output_stream->codecpar->bit_rate,
		output_stream->time_base.num, output_stream->time_base.den,
		output_stream->r_frame_rate.num, output_stream->r_frame_rate.den
	);
	
	// unless it's a no file (we'll talk later about that) write to the disk (FLAG_WRITE)
	// but basically it's a way to save the file to a buffer so you can store it wherever you want
	if (!(pOutFormatContext->oformat->flags & AVFMT_NOFILE))
	{
		value = avio_open(&pOutFormatContext->pb, output_filename, AVIO_FLAG_WRITE);
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
	// av_opt_set(pOutCodecContext->priv_data, "movflags", "frag_keyframe+delay_moov", 0);
    // av_opt_set_int(pOutCodecContext->priv_data, "crf", 28, AV_OPT_SEARCH_CHILDREN); // change `cq` to `crf` if using libx264

	// mp4 container (or some advanced container file) requires header information
	value = avformat_write_header(pOutFormatContext, &hdr_options);
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
	av_dump_format(pOutFormatContext, 0, output_filename, 1);
	cout << "Output file format (container) name: " << pOutFormatContext->oformat->name << " (" << pOutFormatContext->oformat->long_name << ")" << endl;

	// ---------------------- Capture video frames part ---------------------- //

	// initialize sample scaler context (for converting from RGB to YUV)
	const int dst_width = pOutCodecContext->width;
    const int dst_height = pOutCodecContext->height;
    const AVPixelFormat dst_pix_fmt = pOutCodecContext->pix_fmt;
	SwsContext* swsContext = sws_getContext(
		pInCodecContext->width, pInCodecContext->height, pInCodecContext->pix_fmt,
		dst_width, dst_height, dst_pix_fmt,
		SWS_DIRECT_BGR, NULL, NULL, NULL); // or SWS_BILINEAR
    if (!swsContext)
	{
        cout << "Failed to allocate sws context" << endl;
        return -1;
    }

	// now we're going to read the packets from the stream and decode them into frames
	// but first, we need to allocate memory for both components
	AVPacket *pInPacket = av_packet_alloc();
	if (!pInPacket)
	{
		cout << "Failed to allocate memory for input AVPacket" << endl;
		return -1;
	}
	AVFrame *pInFrame = av_frame_alloc();
	if (!pInFrame)
	{
		cout << "Failed to allocate memory for the input AVFrame" << endl;
		return -1;
	}

	// allocate memory for output packets
	AVPacket *pOutPacket = av_packet_alloc();
	if (!pOutPacket)
	{
		cout << "Failed to allocate memory for output AVPacket" << endl;
		return -1;
	}

	// we also need an output frame (for converting from RGB to YUV)
	AVFrame *pOutFrame = av_frame_alloc();
	if (!pOutFrame)
	{
		cout << "Failed to allocate memory for the output AVFrame" << endl;
		return -1;
	}

	int response = 0;
	int ts = 0;
	// let's feed our packets from the input stream
	// until it has packets or until user hits CTRL+C
	while (av_read_frame(pInFormatContext, pInPacket) >= 0 && keepRunning)
	{
		// process only video stream (index 0)
		if (pInPacket->stream_index == video_stream_index)
		{
			// <Transcoding output video>

			// let's send the input (compressed) packet to the decoder
			// through the input codec context
			response = avcodec_send_packet(pInCodecContext, pInPacket);
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
				response = avcodec_receive_frame(pInCodecContext, pInFrame);
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
				pOutFrame->width = dst_width;
				pOutFrame->height = dst_height;
				pOutFrame->format = static_cast<int>(dst_pix_fmt);
				value = av_frame_get_buffer(pOutFrame, 32);
				if (value < 0)
				{
					cout << "Failed to  allocate picture" << endl;
					return value;
				}

				// copying input frame information to output frame
				av_frame_copy(pOutFrame, pInFrame);
				av_frame_copy_props(pOutFrame, pInFrame);

				// from RGB to YUV
				sws_scale(swsContext, pInFrame->data, pInFrame->linesize, 0, pInCodecContext->height, pOutFrame->data, pOutFrame->linesize);
									
				// useless (I think): pOutFrame->pict_type = AV_PICTURE_TYPE_NONE;

				// printing output frame info
				if (pInCodecContext->frame_number == 1)
					cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output frame ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
				printf("Frame #%d (format=%s, type=%c): pts=%ld [dts=%ld], pts_time=%ld\n",
					pInCodecContext->frame_number,
					av_get_pix_fmt_name(static_cast<AVPixelFormat>(pOutFrame->format)),
					av_get_picture_type_char(pOutFrame->pict_type),
					pOutFrame->pts,
					pOutFrame->pkt_dts,
					pOutFrame->pts * output_stream->time_base.num / output_stream->time_base.den
				);

				// let's send the uncompressed output frame to the encoder
				// through the output codec context
				response = avcodec_send_frame(pOutCodecContext, pOutFrame);	
				while (response >= 0)
				{

					// and let's (try to) receive the output packet (compressed) from the encoder
					// through the output codec context
					response = avcodec_receive_packet(pOutCodecContext, pOutPacket);
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

					pOutPacket->stream_index = video_stream_index; // useless (I think)

					// ---------------------- synchronize output packet --------------------- //

					// adjusting dts/pts/duration
					/*
					pOutPacket->pts = ts;
					pOutPacket->dts = ts;
					pOutPacket->duration = av_rescale_q(pOutPacket->duration, input_stream->time_base, output_stream->time_base);
					ts += pOutPacket->duration;
					pOutPacket->pos = -1;
					*/
					// pOutPacket->duration = output_stream->time_base.den / output_stream->time_base.num / input_stream->avg_frame_rate.num * input_stream->avg_frame_rate.den;
					
					// adjusting output packet timestamps
					av_packet_rescale_ts(pOutPacket, input_stream->time_base, output_stream->time_base);
					
					// print output packet information
					printf(" - Output packet: pts=%ld [dts=%ld], duration:%ld, size=%d\n",
						pOutPacket->pts, pOutPacket->pts, pOutPacket->duration, pOutPacket->size
					);

					// ------------------------------------------------------------------------ //

					// write frames in output packet
					response = av_interleaved_write_frame(pOutFormatContext, pOutPacket);
					if (response < 0)
					{
						cout << "Error while receiving packet from decoder" << endl;
						return -1;
					}

				}
				av_packet_unref(pOutPacket); // release output packet buffer data
				av_frame_unref(pOutFrame); // release output frame buffer data
				// </Encoding output video>

				av_frame_unref(pInFrame); // release input frame buffer data
			}
			av_frame_unref(pInFrame); // release input frame buffer data
			// </Transcoding output video>

			av_packet_unref(pInPacket); // release input packet buffer data
		}
	}

	// https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga7f14007e7dc8f481f054b21614dfec13
	av_write_trailer(pOutFormatContext);
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ end ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

end:

	// free input format context
	avformat_close_input(&pInFormatContext);
	if (!pInFormatContext)
	{
		cout << "Input AVFormatContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free input AVFormatContext" << endl;
		exit(1);
	}

	// free input codec context
	avcodec_free_context(&pInCodecContext);
	if (!pInCodecContext)
	{
		cout << "Input AVCodecContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free input AVCodecContext" << endl;
		exit(1);
	}

	// free output format context
	if (pOutFormatContext && !(pOutFormatContext->oformat->flags & AVFMT_NOFILE))
	{
		avio_closep(&pOutFormatContext->pb);
	}
	avformat_close_input(&pOutFormatContext);
	if (!pOutFormatContext)
	{
		cout << "Output AVFormatContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free output AVFormatContext" << endl;
		exit(1);
	}

	// free output codec context
	avcodec_free_context(&pOutCodecContext);
	if (!pOutCodecContext)
	{
		cout << "Output AVCodecContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free output AVCodecContext" << endl;
		exit(1);
	}

	// free sws context
	sws_freeContext(swsContext);
	if (!swsContext)
	{
		cout << "SwsContext freed successfully" << endl;
	}
	else
	{
		cout << "Unable to free SwsContext" << endl;
		exit(1);
	}


	// free packets/frames
	if (pInPacket)
	{
		av_packet_free(&pInPacket);
		pInPacket = NULL;
	}
	if (pInFrame)
	{
		av_frame_free(&pInFrame);
		pInFrame = NULL;
	}
	if (pOutPacket)
	{
		av_packet_free(&pOutPacket);
		pOutPacket = NULL;
	}
	if (pOutFrame)
	{
		av_frame_free(&pOutFrame);
		pOutFrame = NULL;
	}

	cout << "Program executed successfully" << endl;

	return 0;
}
