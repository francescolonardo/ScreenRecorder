#include <signal.h>
#include "ScreenRecorder.h"
#include <thread>

using namespace std;

bool sig_ctrl_c = false;
void intSignalHandler(int signum)
{
	cout << "\t-> Interrupt signal (CTRL+C) received" << endl;
	sig_ctrl_c = true;
}

int main(int argc, char const *argv[])
{
	try
	{
		if (argc < 4)
		{
			cout << "Missing arguments! | e.g. ./main 320x240 100,50 video.mp4" << endl; // TODO: improve this!
			return -1;
		}
		// TODO: add arguments' checks
		string area_size = argv[1], area_offsets = argv[2];
		string out_filename = argv[3];

		// register signal SIGINT (CTRL+C) and signal handler
		signal(SIGINT, intSignalHandler);

		ScreenRecorder sr{area_size, area_offsets, out_filename}; // TODO: add all args!

		sr.openInputDeviceVideo();
		sr.openInputDeviceAudio();

		sr.prepareDecoderVideo();
		sr.prepareDecoderAudio();

		sr.prepareEncoderVideo();
		sr.prepareEncoderAudio();

		sr.prepareOutputFile();

		sr.prepareCaptureVideo();
		sr.prepareCaptureAudio();

		//sr.captureFramesVideo(sig_ctrl_c);
		//sr.captureFramesAudio(sig_ctrl_c);
		thread thrd_video(&ScreenRecorder::captureFramesVideo, &sr, ref(sig_ctrl_c));
		//thread thrd_audio(&ScreenRecorder::captureFramesAudio, &sr, ref(sig_ctrl_c));
		thrd_video.join();
		//thrd_audio.join();
	}
	catch (const std::exception &ex)
	{
		cerr << ex.what() << endl;
	}

	return 0;
}
