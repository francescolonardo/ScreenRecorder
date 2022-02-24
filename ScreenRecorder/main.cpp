#include "ScreenRecorder.h"

int main(int argc, char const *argv[])
{
	try
	{
		if (argc < 6)
		{
			cout << "Missing arguments! | ./main widthxheight x_offset,y_offset video_fps audio_flag out_filename" << endl; // TODO: improve this!
			return -1;																										// ??? or exit(1)
		}
		// TODO: add on arguments checks
		string area_size = argv[1], area_offsets = argv[2];
		string video_fps = argv[3];
		bool audio_flag = atoi(argv[4]) == 1 ? true : false;
		string out_filename = argv[5];

		bool test_flag = false;
		if (argc == 7)
			test_flag = atoi(argv[6]) == 1 ? true : false;

		ScreenRecorder sr{area_size, area_offsets, video_fps, audio_flag, out_filename, test_flag}; // TODO: add all args!
		sr.record();
	}
	catch (const exception &ex)
	{
		cerr << ex.what() << endl;
	}

	return 0;
}
