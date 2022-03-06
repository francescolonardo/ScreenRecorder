#include "ScreenRecorder.h"
#include <regex>

#define DEFAULT_VIDEO_FPS "15"

string area_size;
string area_offsets;
bool audio_flag;
string out_filename;

void getParametersFromMenu()
{
	cout << "*****************************************" << endl;
	cout << "*          ScreenRecorder menu          *" << endl;
	cout << "*****************************************" << endl;

	cout << "- Set the area size to be recorded: its format must be `widthxheight` (e.g. 1920x1200) -> ";
	cin >> area_size;

	cout << "- Set the area offsets: its format must be `x_offset,y_offset` (e.g. 0,0) -> ";
	cin >> area_offsets;

	cout << "- Do you want to record the microphone audio? (press `1` if yes, `0` if no) -> ";
	cin >> audio_flag;

	cout << "- Set the output filename, the extension must be `.mp4` (e.g. output_video.mp4) -> ";
	cin >> out_filename;
}

bool checkArgumentsNumber(int argc)
{
	if (argc == 1)
	{
		getParametersFromMenu();
		return true;
	}
	else
	{
		if (argc < 5)
		{
			cerr << "Missing arguments!" << endl;
			cout << "Usage: ./main widthxheight x_offset,y_offset audio_flag output_filename.mp4" << endl;
			cout << "Example: ./main 1920x1200 0,0 1 output_video.mp4" << endl;
			return false;
		}
		else if (argc > 5)
		{
			cerr << "Too much arguments!" << endl;
			cout << "Usage: ./main widthxheight x_offset,y_offset audio_flag output_filename.mp4" << endl;
			cout << "Example: ./main 1920x1200 0,0 1 output_video.mp4" << endl;
			return false;
		}
		else
			return true;
	}
}

bool checkArgumentsFormat(char const *argv[])
{
	string arguments_check_errors = "";

	regex area_size_rx("[0-9]+(x[0-9]+)+");
	if (regex_match(argv[1], area_size_rx))
		area_size = argv[1];
	else
		arguments_check_errors += "Check the area size's format: it must be `widthxheight` (e.g. 1920x1200)\n";

	regex area_offsets_rx("[0-9]+(,[0-9]+)+");
	if (regex_match(argv[2], area_offsets_rx))
		area_offsets = argv[2];
	else
		arguments_check_errors += "Check the area offsets' format: it must be `x_offset,y_offset` (e.g. 0,0)\n";

	if (atoi(argv[3]) == 0 || atoi(argv[3]) == 1)
		audio_flag = atoi(argv[3]) == 1 ? true : false;
	else
		arguments_check_errors += "Check the audio flag's format: it must be `0` or `1`\n";

	regex out_filename_rx(".*\\.mp4$");
	if (regex_match(argv[4], out_filename_rx))
		out_filename = argv[4];
	else
		arguments_check_errors += "Check the output filename's extension: it must be `.mp4` (e.g. output_video.mp4)\n";

	if (arguments_check_errors != "")
	{
		cerr << arguments_check_errors << endl;
		return false;
	}
	else
		return true;
}

int main(int argc, char const *argv[])
{
	try
	{
		// check arguments' number
		if (!checkArgumentsNumber(argc))
			exit(1);

		// check arguments' format
		if (!checkArgumentsFormat(argv))
			exit(1);

		ScreenRecorder sr{area_size, area_offsets, DEFAULT_VIDEO_FPS, audio_flag, out_filename};
		sr.record();
	}
	catch (const exception &ex)
	{
		cerr << endl
			 << ex.what() << endl;
	}

	return 0;
}
