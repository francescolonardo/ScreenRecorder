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
	// register signal SIGINT (CTRL+C) and signal handler
	signal(SIGINT, intSignalHandler);

	ScreenRecorder sr;

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
