#if defined(__linux__)
#include <ncurses.h>
#elif defined(_WIN32) || defined(__CYGWIN__)
#include <curses.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <ncurses.h>
#endif
#include <iostream>
#include <math.h>

using namespace std;

class CommandLineInterface
{
private:
	// (n)curses
	WINDOW *win;
	int inner_box_height = 3, inner_box_width = 24;
	int rec_info_row = 0;

	void cliStartWindow(string area_size, string area_offsets, string video_fps, bool audio_flag, string out_filename);
	void cliVideoStreamInfo(const char *codec_id_name, const char *pix_fmt_name, bool audio_flag);
	void cliAudioStreamInfo(const char *a_coded_id_name, int a_sample_rate, int a_bit_rate);
	void cliFramesTimeCentered(uint64_t v_packets_elaborated, uint64_t v_packets_captured, string time_str);
	void cliKeyActionsInfo();
	void cliKeyDetectedPause();
	void cliKeyDetectedRecord();
	void cliKeyDetectedStop();
	void cliEndWindow(string out_filename);
	void debug(int p, string s);

public:
	CommandLineInterface();
	~CommandLineInterface();

	friend class ScreenRecorder;
};
