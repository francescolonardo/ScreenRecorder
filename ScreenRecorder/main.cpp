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
	if (argc < 2)
	{
		cout << "Missing arguments! | e.g. ./main video.mp4" << endl; // TODO: fix this!
		return -1;
	}
	string out_filename = argv[1];

	// register signal SIGINT (CTRL+C) and signal handler
	signal(SIGINT, intSignalHandler);

	ScreenRecorder sr{out_filename}; // TODO: add all args!

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

	return 0;
}
