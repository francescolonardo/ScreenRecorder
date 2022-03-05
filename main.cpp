#include "ScreenRecorder.h"

#define DEFAULT_VIDEO_FPS "15"

int main(int argc, char const* argv[]) {

	string area_size;
	string area_offset_x;
	string area_offset_y;
	string area_offsets;
	char audio_flag_c;
	bool audio_flag;
	string out_filename;

	cout << "*********** SCREEN CAPTURE *************"<<endl;
	cout << "-"<<endl;

	cout << "Set area size:" << endl;
	cin >> area_size;

	cout << "Set area offset x:" << endl;
	cin >> area_offset_x;

	cout << "Set area offset y:" << endl;
	cin >> area_offset_y;

	area_offsets = area_offset_x + "," + area_offset_y;

	cout << "Do you want to record audio?" << endl;
	cin >> audio_flag_c;
	audio_flag = atoi(audio_flag_c) == 1 ? true : false;

	cout << "Set output file name:" << endl;
	cin >> out_filename;

	/*
	try {
		if (argc < 5) {
			cerr << "Missing arguments!" << endl;
			cout << "Usage: ./main widthxheight x_offset,y_offset audio_flag out_filename.extension" << endl;
			cout << "Example: ./main 1920x1200 0,0 1 test_video.mp4" << endl;
			exit(1);
		}
		else if (argc > 5) {
			cerr << "Too much arguments!" << endl;
			cout << "Usage: ./main widthxheight x_offset,y_offset audio_flag out_filename.extension" << endl;
			cout << "Example: ./main 1920x1200 0,0 1 test_video.mp4" << endl;
			exit(1);
		}

		// TODO: add on arguments checks
		string area_size = argv[1], area_offsets = argv[2];
		bool audio_flag = atoi(argv[3]) == 1 ? true : false;
		string out_filename = argv[4];

		ScreenRecorder sr{ area_size, area_offsets, DEFAULT_VIDEO_FPS, audio_flag, out_filename };
		sr.record();
	}
	catch (const exception& ex) {
		cerr << endl
			<< ex.what() << endl;
	}

	*/

	ScreenRecorder sr{ area_size, area_offsets, DEFAULT_VIDEO_FPS, audio_flag, out_filename };
	sr.record();

	return 0;
}
