#undef _GLIBCXX_DEPRECATED
#define _GLIBCXX_DEPRECATED

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <math.h>
#include <string.h>
#include <vector>
#include <csignal>

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

bool keepRunning = true;
void signalHandler( int signum ) {
	cout << "Interrupt signal (CTRL+C) received." << endl;

	keepRunning = false;
	// cleanup and close up stuff here  
	// terminate program  

	// exit(signum);  
}

int main()
{

	// register signal SIGINT (CTRL+C) and signal handler  
   	signal(SIGINT, signalHandler);  


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
	value = av_dict_set(&options, "video_size", "1920x1200", 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (video_size)\n");
		cout << "Error setting dictionary (video_size)" << endl;
		return value;
	}
	value = av_dict_set(&options, "framerate", "60", 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (framerate)\n");
		cout << "Error setting dictionary (framerate)" << endl;
		return value;
	}
	/*
	value = av_dict_set(&options, "preset", "medium", 0);
	if (value < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Error setting dictionary (preset)\n");
		cout << "Error setting dictionary (preset)" << endl;
		return value;
	}
	*/


	// 4. Fill the AVFormatContext opening the input file and reading its header
	// (codecs are not opened, so we can't analyse them)
	// AVFormatContext will hold information about the new input format (old one with options added) ???
	AVFormatContext *pInFormatContext = NULL;
	pInFormatContext = avformat_alloc_context(); // TODO: check this!
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
	cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ input device ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(pInFormatContext, 0, input_filename, 0);
	cout << "Input file format (container) name: " << pInFormatContext->iformat->name << " (" << pInFormatContext->iformat->long_name << ")" << endl;

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
	// fill the input codec context with the input stream parameters
	value = avcodec_parameters_to_context(pInCodecContext, pInCodecParameters);
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


	// ---------------------- Init output file (video.mp4) part ---------------------- //
	// initializes the video output file and its properties

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

	// TODO: why this?!
	// copying input codec parameters to output codec parameters
	// value = avcodec_parameters_copy(output_stream->codecpar, pInCodecParameters);
	// if (value < 0) {
    //  cout << "Failed to copy codec parameters" << endl;
    //  return value;
	//  // goto end;
    // }


	// find and fill output codec
	AVCodec *pOutCodec = avcodec_find_encoder_by_name("libx264");
	if (!pOutCodec)
	{
		cout << "Error finding among the existing codecs, try again with another codec" << endl;
		exit(1);
	}
	// or
	/*
	AVCodec *pOutCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!pOutCodec)
	{
		cout << "Error finding among the existing codecs, try again with another codec" << endl;
		exit(1);
	}
	*/


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

	// TODO: check this!
	// av_opt_set(pOutCodecContext->priv_data, "preset", "fast", 0);
	/*
	if (pOutCodecContext->codec_id == AV_CODEC_ID_H264)
	{
		av_opt_set(pOutCodecContext->priv_data, "preset", "slow", 0); // ???
	}
	*/

	// set output codec context properties
	// useless: pOutCodecContext->codec_id = AV_CODEC_ID_H264; // AV_CODEC_ID_MPEG4; // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
	// useless: pOutCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	pOutCodecContext->width = pInCodecContext->width; // 1920
	pOutCodecContext->height = pInCodecContext->height; // 1200
	pOutCodecContext->sample_aspect_ratio = pInCodecContext->sample_aspect_ratio; // 0/1
	if (pOutCodec->pix_fmts)
	{
		pOutCodecContext->pix_fmt = pOutCodec->pix_fmts[0];
	}
	else
	{
		pOutCodecContext->pix_fmt = pInCodecContext->pix_fmt;
	}
	// print output codec context properties
	printf("Output codec context: dimension=%dx%d - sample aspect ratio=%d/%d - pixel format=%s\n",
		pOutCodecContext->width, pOutCodecContext->height,
		pOutCodecContext->sample_aspect_ratio.num, pOutCodecContext->sample_aspect_ratio.den,
		av_get_pix_fmt_name(pOutCodecContext->pix_fmt)
	);
	

	
	pOutCodecContext->bit_rate = 400000;
	pOutCodecContext->gop_size = 3;
	pOutCodecContext->max_b_frames = 2;



	// setting up timebase(s)
	AVRational input_framerate = av_guess_frame_rate(pInFormatContext, input_stream, NULL);
	pOutCodecContext->time_base = av_inv_q(input_framerate);
	//pOutCodecContext->framerate = input_framerate;
	printf("Output codec context: timebase=%d/%d, framerate=%d/%d\n",
		pOutCodecContext->time_base.num, pOutCodecContext->time_base.den,
		pOutCodecContext->framerate.num, pOutCodecContext->framerate.den
	);

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


	// get output stream parameters from output codec context
	value = avcodec_parameters_from_context(output_stream->codecpar, pOutCodecContext);
	if (value < 0)
	{
		cout << "Unable to copy output stream parameters from output codec context" << endl;
		avformat_close_input(&pInFormatContext);
		avcodec_free_context(&pInCodecContext);
		return value;
	}
	/*
	output_stream->time_base = pOutCodecContext->time_base;
	output_stream->r_frame_rate = pOutCodecContext->framerate;
	printf("Output stream: timebase=%d/%d, framerate=%d/%d\n",
		output_stream->time_base.num, output_stream->time_base.den,
		output_stream->r_frame_rate.num, output_stream->r_frame_rate.den
	);
	*/
	

	// some container formats (like MP4) require global headers
	// mark the encoder so that it behaves accordingly ???
	if (pOutFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		pOutFormatContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

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

	// mp4 container (or some advanced container file) requires header information
	AVDictionary *muxer_options = NULL;
	value = avformat_write_header(pOutFormatContext, &muxer_options); // can I use options ???
	if (value < 0)
	{
		cout << "Error writing the header context" << endl;
		return value;
		// goto end;
	}
	// cout << "-> Output video file's header (" << output_filename << ") writed" << endl;


	// print output file information
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output file ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
	av_dump_format(pOutFormatContext, 0, output_filename, 1);
	cout << "Output file format (container) name: " << pOutFormatContext->oformat->name << " (" << pOutFormatContext->oformat->long_name << ")" << endl;

	
	// ---------------------- Capture video frames part ---------------------- //

	// initialize sample scaler
	const int dst_width = pOutCodecContext->width;
    const int dst_height = pOutCodecContext->height;
    const AVPixelFormat dst_pix_fmt = pOutCodecContext->pix_fmt;
	SwsContext* swsContext = sws_getContext(
		pInCodecContext->width, pInCodecContext->height, pInCodecContext->pix_fmt,
		dst_width, dst_height, dst_pix_fmt,
		SWS_BILINEAR, NULL, NULL, NULL); // SWS_BILINEAR or SWS_BICUBIC ???
    if (!swsContext)
	{
        cout << "Fail to sws_getContext" << endl;
        return 2;
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

	// 
	AVFrame *pOutFrame = av_frame_alloc();
	if (!pOutFrame)
	{
		cout << "Failed to allocate memory for the output AVFrame" << endl;
		return -1;
	}
	pOutFrame->width = dst_width;
    pOutFrame->height = dst_height;
   	pOutFrame->format = static_cast<int>(dst_pix_fmt);
	value = av_frame_get_buffer(pOutFrame, 32);
    if (value < 0)
	{
        std::cerr << "Failed to av_frame_get_buffer()";
        return 2;
    }


	int response = 0;
	// let's feed our packets from the input stream
	// until it has packets or until user hits CTRL+C
	while (av_read_frame(pInFormatContext, pInPacket) >= 0 && keepRunning)
	{
		// process only video stream (index 0)
		// if (pInPacket->stream_index == video_stream_index)
		// or
		if (pInFormatContext->streams[pInPacket->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
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

			while (response >= 0 && keepRunning)
			{
				// and let's (try to) receive the input uncompressed frame from the decoder
				// through the input codec context
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

				if (response >= 0) // = 0 (success)
				{

					// <Encoding output video>

					// copying input frame information to output frame
					av_frame_copy(pOutFrame, pInFrame);
					av_frame_copy_props(pOutFrame, pInFrame);

					// 
					sws_scale(swsContext, pInFrame->data, pInFrame->linesize, 0, pInCodecContext->height, pOutFrame->data, pOutFrame->linesize);

					// pOutFrame->pict_type = AV_PICTURE_TYPE_NONE; // ???

					// printing output frame info
					if (pInCodecContext->frame_number == 1)
					{
						cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ output frame ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
					}
					printf(
						"Frame #%d (format=%s, type=%c): pts=%ld [dts=%ld], pts_time=%ld\n",
						pInCodecContext->frame_number,
						av_get_pix_fmt_name(static_cast<AVPixelFormat>(pOutFrame->format)),
						av_get_picture_type_char(pOutFrame->pict_type),
						pOutFrame->pts,
						pOutFrame->pkt_dts,
						pOutFrame->pts * output_stream->time_base.num / output_stream->time_base.den
					);

					/*
					// ---------------------------- save frame (PGM) ---------------------------- //

					// preparing PGM file
					char frame_filename[32];
					sprintf(frame_filename, "frame-%d.pgm", pInCodecContext->frame_number);
					
					// opening file
					FILE *fp;
					fp = fopen(frame_filename, "wb");
					
					// writing header
					fprintf(fp, "P5\n%d %d\n255\n", pOutFrame->width, pOutFrame->height);

					// writing pixel data (line by line)
					for (int y = 0; y < pOutFrame->height; y++)
					{
						fwrite(pOutFrame->data[0] + y * pOutFrame->linesize[0], 1, pOutFrame->width, fp);
					}

					// closing file
					fclose(fp);
					
					// ------------------------------------------------------------------------ //
					*/

					// 
					AVPacket *pOutPacket = av_packet_alloc();
					if (!pOutPacket)
					{
						cout << "Failed to allocate memory for output AVPacket" << endl;
						return -1;
					}

					// ERROR! (response < 0)!
					// let's send the uncompressed output frame to the encoder
					// through the output codec context
					response = avcodec_send_frame(pOutCodecContext, pOutFrame);
					while (response >= 0 && keepRunning)
					{
						// and let's (try to) receive the output packet (compressed) from the encoder
						// through the output codec context
						response = avcodec_receive_packet(pOutCodecContext, pOutPacket);
						if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
						{
							/*
							// ALL OK: https://stackoverflow.com/questions/55354120/ffmpeg-avcodec-receive-frame-returns-averroreagain
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

						// useless: pOutPacket->stream_index = video_stream_index;


						// ---------------------- synchronize output packet --------------------- //
						
						// adjusting output packet timestamps
						av_packet_rescale_ts(pOutPacket, input_stream->time_base, output_stream->time_base);
						
						// adjusting output packet pts/dts
						/*
						cout << "Output packet duration: " << pOutPacket->duration << endl;
						pOutPacket->pts = av_rescale_q(pOutPacket->pts, pOutCodecContext->time_base, output_stream->time_base);
						pOutPacket->dts = av_rescale_q(pOutPacket->dts, pOutCodecContext->time_base, output_stream->time_base);
						cout << pOutPacket->pts << endl;
						cout << pOutPacket->dts << endl;
						*/
						
						// adjusting output packet duration
						// ERROR! (/0)
						// pOutPacket->duration = (output_stream->time_base.den / output_stream->time_base.num) / (output_stream->avg_frame_rate.num * output_stream->avg_frame_rate.den);
						// pOutPacket->duration = av_rescale_q(pOutPacket->duration, input_stream->time_base, output_stream->time_base);
						/*
						printf("Output stream: timebase=%d/%d, framerate=%d/%d\n", 
							output_stream->time_base.num, output_stream->time_base.den,
							output_stream->avg_frame_rate.num, output_stream->avg_frame_rate.den
						);			
						cout << "Duration: " << pOutPacket->duration << endl;
						*/

						// ------------------------------------------------------------------------ //

						// // 
						response = av_interleaved_write_frame(pOutFormatContext, pOutPacket);
						if (response < 0)
						{
							cout << "Error while receiving packet from decoder" << endl;
							return -1;
						}

					}
					av_packet_unref(pOutPacket); // release output packet buffer data
					av_packet_free(&pOutPacket); // dealloc output packet buffer data
					// </Encoding output video>

				}
				av_frame_unref(pInFrame); // release input frame buffer data
			}
			av_frame_unref(pInFrame); // release input frame buffer data
			// </Transcoding output video>

			av_packet_unref(pInPacket); // release input packet buffer data
		}
	}


	// https://ffmpeg.org/doxygen/trunk/group__lavf__encoding.html#ga7f14007e7dc8f481f054b21614dfec13
	av_write_trailer(pOutFormatContext);
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ END ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

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

	if (!pOutFrame)
	{
		av_frame_free(&pOutFrame);
		pOutFrame = NULL;
	}


	cout << "Program executed successfully" << endl;

	return 0;
}
