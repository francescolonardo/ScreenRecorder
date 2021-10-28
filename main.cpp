#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <math.h>
#include <string.h>

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

// Open input
const char *input_filename = ":0.0+0,0"; // screen (1): x=0, y=0

// show x11grab device
void show_x11grab_device()
{
	AVFormatContext *pInFormatContext = avformat_alloc_context();
	AVDictionary *options = NULL;
	av_dict_set(&options, "list_devices", "true", 0); // ???
	AVInputFormat *pInputFormat = av_find_input_format("x11grab");
	cout << "======== Device Info =============" << endl;
	avformat_open_input(&pInFormatContext, "video=dummy", pInputFormat, &options);
	cout << "==================================" << endl;

	avformat_close_input(&pInFormatContext);
}

// show x11grab device's option
void show_x11grab_device_option()
{
	AVFormatContext *pInFormatContext = avformat_alloc_context();
	AVDictionary *options = NULL;
	av_dict_set(&options, "list_options", "true", 0);
	AVInputFormat *pInputFormat = av_find_input_format("x11grab");
	cout << "======== Device Option Info ======" << endl;
	avformat_open_input(&pInFormatContext, input_filename, pInputFormat, &options);
	cout << "==================================" << endl;

	avformat_close_input(&pInFormatContext);
}

int main()
{
	/*
	avdevice_register_all();
	show_x11grab_device();
	show_x11grab_device_option();
	return 0;
	*/

	int value = 0;

	// ---------------------- Input device part ---------------------- //

	/*
	 * Originally, to open an input file, only one step and two parameters are needed:
	 * - avformat_open_input(&input_fmt_ctx, input_file, NULL, NULL)
	 * At present, there are more steps needed
	 */

	// 1. Register device
	avdevice_register_all(); // Must be executed, otherwise av_find_input_format() fails

	// 2. Specify the input format as: x11grab (Linux), dshow (Windows) or avfoundation (Mac)
	// current below is for screen recording; to connect with camera use v4l2 as input parameter for av_find_input_format()
	// AVInputFormat holds the header information from the input format (container)
	AVInputFormat *pInputFormat = av_find_input_format("x11grab");
	if (!pInputFormat)
	{
		av_log(NULL, AV_LOG_ERROR, "Unknow format\n");
		cout << "Unknow input format" << endl;
		exit(1);
	}

	// 3. Set options for the demuxer
	// that demultiplies the single input format (container) into different streams
	AVDictionary *options = NULL;
	value = av_dict_set(&options, "video_size", "320x240", 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (video_size)\n");
		cout << "Error setting dictionary (video_size)" << endl;
		return value;
	}
	value = av_dict_set(&options, "framerate", "30", 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (framerate)\n");
		cout << "Error setting dictionary (framerate)" << endl;
		return value;
	}

	// 4. Fill the AVFormatContext opening the input file and reading its header
	// (codecs are not opened, so we can't analyse them)
	// AVFormatContext will hold information about the new input format (old one with options added) ???
	AVFormatContext *pInFormatContext = NULL;
	value = avformat_open_input(&pInFormatContext, input_filename, pInputFormat, &options);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		cout << "Cannot open input file" << endl;
		return value;
	}

	// stream (packets' flow) information analysis
	// reads packets to get stream information
	// this function populates pInFormatContext->streams (of size equals to pInFormatContext->nb_streams)
	value = avformat_find_stream_info(pInFormatContext, NULL);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		cout << "Cannot find stream information" << endl;
		return value;
	}

	// print input file information
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ input file information ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(pInFormatContext, 0, input_filename, 0);
	cout << "Input file format (container) name: " << pInFormatContext->iformat->name << " (" << pInFormatContext->iformat->long_name << ")" << endl;
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

	// ---------------------- Decoding part ---------------------- //


	/* useless, see next block lines
	int video_stream_index = -1;
	// finds the first video stream index; there is also an API available to do the below operations
	for (int i = 0; i < pInFormatContext->nb_streams; i++)
	{
		if (pInFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = i;
			break;
		}
	}
	if (video_stream_index == -1)
	{
		cout << "Unable to find the video stream index" << endl;
		exit(1);
	}
	cout << "Video stream index: " << video_stream_index  << endl;
	*/

	// in our case we must have just a stream (video: AVMEDIA_TYPE_VIDEO)
	// cout << "Input file has " << pInFormatContext->nb_streams << "streams" << endl;
	value = av_find_best_stream(pInFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, -1);
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
	// AVCodecParameters describes the properties of the codec used by the video stream
	AVCodecParameters *pInCodecParameters = input_stream->codecpar;
	// the component that knows how to encode the stream it's the codec
	// we can get it from the parameters (we just need codec_id)
	AVCodec *pInCodec = avcodec_find_decoder(pInCodecParameters->codec_id);
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
	// fill the codec context with the information of the codec parameters
	value = avcodec_parameters_to_context(pInCodecContext, pInCodecParameters);
	if (value < 0)
	{
		cout << "Unable to copy codec params to codec context" << endl;
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
	cout << "Input codec: " << pInCodec->long_name << endl;
	cout << "Input codec ID (from context): " << pInCodecContext->codec_id << " (AV_CODEC_ID_RAWVIDEO)" << endl;


	// ---------------------- Init output file (video.mp4) part ---------------------- //
	// initializes the video output file and its properties

	value = 0;
	const char *output_filename = "video.mp4";

	// we need to prepare the output media file
	// allocate memory for the output format context
	AVFormatContext *pOutFormatContext = NULL;
	avformat_alloc_output_context2(&pOutFormatContext, NULL, NULL, output_filename);
	if (!pOutFormatContext)
	{
		cout << "Cannot allocate output AVFormatContext" << endl;
		exit(1);
	}

	// create video stream in the output format context
	AVStream *output_stream = avformat_new_stream(pOutFormatContext, NULL);
	if (!output_stream)
	{
		cout << "Cannot create an output AVStream" << endl;
		exit(1);
	}

	/*
	// TODO: why this?!
	// copying input codec parameters to output codec parameters
	value = avcodec_parameters_copy(output_stream->codecpar, pInCodecParameters);
	if (value < 0) {
      cout << "Failed to copy codec parameters" << endl;
      return value;
	  // goto end;
    }
	*/

	// TODO: checkpoint

	// find and fill output codec
	AVCodec *pOutCodec = avcodec_find_encoder_by_name("libx264");
	if (!pOutCodec)
	{
		cout << "Error finding among the existing codecs, try again with another codec" << endl;
		exit(1);
	}

	// allocate memory for the output codec context
	AVCodecContext *pOutCodecContext = avcodec_alloc_context3(pOutCodec);
	if (!pOutCodecContext)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate memory for the encoding context\n");
		cout << "Cannot allocate memory for the encoding context" << endl;
		avformat_close_input(&pOutFormatContext);
		return AVERROR(ENOMEM);
	}
	cout << "Output codec: " << pOutCodec->long_name << endl;
	cout << "Output codec ID (from context): " << pOutCodecContext->codec_id << " (AV_CODEC_ID_H264)" << endl;

	// set output codec context properties
	/*
	pOutCodecContext->codec_id = AV_CODEC_ID_H264; // AV_CODEC_ID_MPEG4; // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
	pOutCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	pOutCodecContext->pix_fmt = AV_PIX_FMT_YUV420P; // TODO: check this!
	*/

	if (pOutCodec->pix_fmts)
	{
		pOutCodecContext->pix_fmt = pOutCodec->pix_fmts[0];
	}
	else
	{
		pOutCodecContext->pix_fmt = pInCodecContext->pix_fmt;
	}
	
	pOutCodecContext->height = pInCodecContext->height; // 1200
	cout << "height: " << pInCodecContext->height << endl;
	pOutCodecContext->width = pInCodecContext->width; // 1920
	cout << "width: " << pInCodecContext->width << endl;

	// pOutCodecContext->sample_aspect_ratio = pInCodecContext->sample_aspect_ratio;
	// cout << "sample aspect ratio: " << pOutCodecContext->sample_aspect_ratio << endl;

	pOutCodecContext->bit_rate = 2 * 1000 * 1000;
	pOutCodecContext->rc_buffer_size = 4 * 1000 * 1000;
	pOutCodecContext->rc_max_rate = 2 * 1000 * 1000;
	pOutCodecContext->rc_min_rate = 2.5 * 1000 * 1000;

	// pOutCodecContext->gop_size = 3; // ???
	// pOutCodecContext->max_b_frames = 2; // ???

	// 
	AVRational input_framerate = av_guess_frame_rate(pInFormatContext, input_stream, NULL);
	pOutCodecContext->time_base = av_inv_q(input_framerate);
	output_stream->time_base = pInCodecContext->time_base; // TODO: check this!

	/*
	if (pOutCodecContext->codec_id == AV_CODEC_ID_H264)
	{
		av_opt_set(pOutCodecContext->priv_data, "preset", "slow", 0); // ???
	}
	*/

	// turns on the encoder
	// so we can proceed to the encoding process
	value = avcodec_open2(pOutCodecContext, pOutCodec, NULL);
	if (value < 0)
	{
		cout << "Unable to turn on the encoder" << endl;
		avformat_close_input(&pOutFormatContext);
		avcodec_free_context(&pOutCodecContext);
		return value;
	}

	avcodec_parameters_from_context(output_stream->codecpar, pOutCodecContext); // ???

	/*
	 * some container formats (like MP4) require global headers
	 * mark the encoder so that it behaves accordingly ???
	 */
	if (pOutFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		pOutCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	// unless it's a no file (we'll talk later about that) write to the disk (FLAG_WRITE)
	// but basically it's a way to save the file to a buffer so you can store it wherever you want
	if (!(pOutFormatContext->oformat->flags & AVFMT_NOFILE))
	{
		cout << "Output file (" << output_filename << ") created" << endl;
		value = avio_open(&pOutFormatContext->pb, output_filename, AVIO_FLAG_WRITE);
		if (value < 0)
		{
			cout << "Could not open output file " << output_filename << endl;
			return value;
			// goto end;
		}
	}

	// TODO: check if I have to add muxer_opts

	// mp4 container or some advanced container file required header information
	value = avformat_write_header(pOutFormatContext, NULL);
	if (value < 0)
	{
		cout << "Error writing the header context" << endl;
		return value;
		// goto end;
	}

	// print output file information
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output file information ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(pOutFormatContext, 0, output_filename, 1);
	cout << "Output file format (container) name: " << pOutFormatContext->oformat->name << " (" << pOutFormatContext->oformat->long_name << ")" << endl;
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

	
	// ---------------------- Capture video frame part ---------------------- //

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
		cout << "Failed to allocate memory for input AVFrame" << endl;
		return -1;
	}

	int response = 0;
	// let's feed our packets from the input stream (until it has packets)
	while (av_read_frame(pInFormatContext, pInPacket) >= 0)
	{
		// process only video stream (index 0)
		if (pInFormatContext->streams[pInPacket->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			// <Transcoding output video>
			// let's send the raw data packet (with compressed frame) to the decoder
			// through the codec context
			response = avcodec_send_packet(pInCodecContext, pInPacket);
			if (response < 0)
			{
				cout << "Error sending a raw data packet to the decoder" << endl;
				return response;
			}
			
			while (response >= 0)
			{
				// and let's receive the raw data uncompressed frame from the decoder
				// through the same codec context
				response = avcodec_receive_frame(pInCodecContext, pInFrame);
				if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
				{
					av_log(NULL, AV_LOG_ERROR, "Error (specific) receiving an uncompressed frame from the decoder\n");
					cout << "Error receiving an uncompressed frame from the decoder" << endl;
					break;
				}
				else if (response < 0)
				{
					av_log(NULL, AV_LOG_ERROR, "Error receiving an uncompressed frame from the decoder\n");
					cout << "Error receiving an uncompressed frame from the decoder" << endl;
					// goto end;
				}
				else
				{
					// <Encoding output video>
					pInFrame->pict_type = AV_PICTURE_TYPE_NONE; // ???	

					AVPacket *pOutPacket = av_packet_alloc();
					if (!pOutPacket)
					{
						cout << "Could not allocate memory for output packet" << endl;
						return -1;
					}

					// ERROR!
					response = avcodec_send_frame(pOutCodecContext, pInFrame);

					while (response >= 0) {

						response = avcodec_receive_packet(pOutCodecContext, pOutPacket);
						if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
						{
							break;
						}
						else if (response < 0)
						{
							cout << "Error while receiving packet from encoder" << endl;
							return -1;
						}

						pOutPacket->stream_index = video_stream_index;
						pOutPacket->duration = output_stream->time_base.den / output_stream->time_base.num / output_stream->avg_frame_rate.num * output_stream->avg_frame_rate.den;

						av_packet_rescale_ts(pOutPacket, output_stream->time_base, output_stream->time_base);
						response = av_interleaved_write_frame(pOutFormatContext, pOutPacket);
						if (response != 0)
						{
							cout << "Error while receiving packet from decoder" << endl;
							return -1;
						}

					}

					av_packet_unref(pOutPacket);
					av_packet_free(&pOutPacket); // release output packet buffer data
					// </Encoding output video>
				}

				av_frame_unref(pInFrame); // release input frame buffer data
			}
			// </Transcoding output video>

			av_packet_unref(pInPacket); // release input packet buffer data
		}
	}


	// <Encoding output video (NULL pInFrame)>
	AVPacket *pOutPacket = av_packet_alloc();
	if (!pOutPacket)
	{
		cout << "Could not allocate memory for output packet" << endl;
		return -1;
	}

	response = avcodec_send_frame(pOutCodecContext, NULL); // TODO: check NULL!

	while (response >= 0) {

		response = avcodec_receive_packet(pOutCodecContext, pOutPacket);
		if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
		{
			break;
		}
		else if (response < 0)
		{
			cout << "Error while receiving packet from encoder" << endl;
			return -1;
		}

		pOutPacket->stream_index = video_stream_index;
		pOutPacket->duration = output_stream->time_base.den / output_stream->time_base.num / output_stream->avg_frame_rate.num * output_stream->avg_frame_rate.den;

		av_packet_rescale_ts(pOutPacket, output_stream->time_base, output_stream->time_base);
		response = av_interleaved_write_frame(pOutFormatContext, pOutPacket);
		if (response != 0)
		{
			cout << "Error while receiving packet from decoder" << endl;
			return -1;
		}

	}

	av_packet_unref(pOutPacket);
	av_packet_free(&pOutPacket); // release output packet buffer data
	// </Encoding output video (NULL pInFrame)>

	// https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga7f14007e7dc8f481f054b21614dfec13
	av_write_trailer(pOutFormatContext);

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

	// free input options TODO: check this!
	av_dict_free(&options);

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

	if (!pInFrame)
	{
		av_frame_free(&pInFrame);
		pInFrame = NULL;
	}
	if (!pInPacket)
	{
		av_packet_free(&pInPacket);
		pInPacket = NULL;
	}


	cout << "Program executed successfully" << endl;

	return 0;
}
