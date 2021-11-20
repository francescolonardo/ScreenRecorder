#include "ScreenRecorder.h"

bool sig_ctrl_c = false;
void sigIntHandler(int signum)
{
	cout << "\t-> Interrupt signal (CTRL+C) received" << endl;
	sig_ctrl_c = true;
}

int main(int argc, char const *argv[])
{
	try
	{
		if (argc < 6)
		{
			cout << "Missing arguments! | ./main widthxheight x_offset,y_offset video_fps audio_flag out_filename" << endl; // TODO: improve this!
			return -1;
		}
		// TODO: add arguments' checks
		string area_size = argv[1], area_offsets = argv[2];
		string video_fps = argv[3];
		bool audio_flag = atoi(argv[4]) == 1 ? true : false;
		string out_filename = argv[5];

		//printf("Args main: %s %s %s %d %s\n", area_size.c_str(), area_offsets.c_str(), video_fps.c_str(), audio_flag, out_filename.c_str());

		// register signal SIGINT (CTRL+C) and signal handler
		signal(SIGINT, sigIntHandler);

		ScreenRecorder sr{area_size, area_offsets, video_fps, audio_flag, out_filename}; // TODO: add all args!
		sr.record(sig_ctrl_c);
	}
	catch (const std::exception &ex)
	{
		cerr << ex.what() << endl;
	}

	return 0;
}
