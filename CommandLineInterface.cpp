#include "CommandLineInterface.h"

CommandLineInterface::CommandLineInterface()
{

	int i = 0;
	// initializes the screen
	initscr();
	noecho();
	// raw();
	curs_set(0); // hide cursor

	win = newwin(LINES, COLS, 0, 0);
	refresh(); //  needed to draw the root window
			   //  without this, apparently the child (win) never draw
}

CommandLineInterface::~CommandLineInterface()
{
	// deallocates memory and ends curses
	delwin(win);
	endwin();
}

void CommandLineInterface::debug(){
	mvwprintw(win, i, 50, "hello");
	i++;
	wrefresh(win);
}

void CommandLineInterface::cliStartWindow(string area_size, string area_offsets, string video_fps, bool audio_flag, string out_filename)
{
	// mvwprintw(win, rec_info_row++, 0, "Welcome to ScreenRecorder!");
	// Start writing from position 0,0
	mvwprintw(win, rec_info_row++, 0, "Recording area: %s from (%s)", area_size.c_str(), area_offsets.c_str());
	mvwprintw(win, rec_info_row++, 0, "Selected video fps: %s", video_fps.c_str());
	mvwprintw(win, rec_info_row++, 0, "Recording audio: %s", audio_flag ? "yes" : "no");
	mvwprintw(win, rec_info_row++, 0, "Output file: %s", out_filename.c_str());

	string press_any_str = "Press any key to start recording...";
	mvwprintw(win, LINES - 1, COLS - press_any_str.length(), press_any_str.c_str());

	wrefresh(win);

	char ch = getch(); // waiting for a key

	// move cursor in position (rec_info_row, 0) in order to erase
	wmove(win, rec_info_row, 0);
	wclrtobot(win); // erases the window's rows from the cursor's current location, downwards

	wrefresh(win);
}

void CommandLineInterface::cliVideoStreamInfo(const char *v_codec_id_name, const char *v_pix_fmt_name, bool audio_flag)
{
	mvwprintw(win, rec_info_row++, 0, "- stream #0 (video) %s, %s", v_codec_id_name, v_pix_fmt_name);
	if (!audio_flag)
		wrefresh(win);
}

void CommandLineInterface::cliAudioStreamInfo(const char *a_codec_id_name, int a_sample_rate, int a_bit_rate)
{
	mvwprintw(win, rec_info_row++, 0, "- stream #1 (audio) %s, %d Hz, %d kbps", a_codec_id_name, a_sample_rate, (a_bit_rate / 1000));
	wrefresh(win);
}

void CommandLineInterface::cliFramesTimeCentered(uint64_t v_packets_elaborated, uint64_t v_packets_captured, string time_str)
{
	wmove(win, 1 + (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2);
	wclrtoeol(win); // erase from the cursor's current location to the end of the row
	mvwprintw(win, 1 + (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2, "frames=[%d/%d]", v_packets_elaborated, v_packets_captured);

	// fix an uncommon problem on Windows // TODO: go deep!
	wmove(win, 1 + (LINES - inner_box_height) / 2, 10 + int(log10(v_packets_elaborated) + 1) + int(log10(v_packets_captured) + 1) + (COLS - inner_box_width) / 2);
	wclrtoeol(win); // erase from the cursor's current location to the end of the row

	wmove(win, 2 + (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2);
	wclrtoeol(win); // erase from the cursor's current location to the end of the row
	mvwprintw(win, 2 + (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2, "time=%s", time_str.c_str());

	// fix an uncommon problem on Windows // TODO: go deep!
	wmove(win, 2 + (LINES - inner_box_height) / 2, 17 + (COLS - inner_box_width) / 2);
	wclrtoeol(win); // erase from the cursor's current location to the end of the row

	wrefresh(win);
}

void CommandLineInterface::cliKeyActionsInfo()
{
	string help_str = "Press: [p] to pause, [r] to record, [s] to stop";
	mvwprintw(win, LINES - 1, COLS - help_str.length(), help_str.c_str());
	mvwprintw(win, (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2, "RECORDING...");

	wrefresh(win);
}

void CommandLineInterface::cliKeyDetectedPause()
{
	// flash();
	wmove(win, 0, COLS - 12); // 12 is "Detected [x]".length()
	wclrtoeol(win);
	string pressed_char_str = "Detected [p]";
	mvwprintw(win, 0, COLS - pressed_char_str.length(), pressed_char_str.c_str());

	wrefresh(win);
	napms(500);

	wmove(win, 0, COLS - pressed_char_str.length());
	wclrtoeol(win);

	wmove(win, (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2);
	wclrtoeol(win);
	mvwprintw(win, (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2, "PAUSED.");

	wrefresh(win);
}

void CommandLineInterface::cliKeyDetectedRecord()
{
	// flash();
	wmove(win, 0, COLS - 12);
	wclrtoeol(win);
	string pressed_char_str = "Detected [r]";
	mvwprintw(win, 0, COLS - pressed_char_str.length(), pressed_char_str.c_str());

	wrefresh(win);
	napms(500);

	wmove(win, 0, COLS - pressed_char_str.length());
	wclrtoeol(win);

	wmove(win, (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2);
	wclrtoeol(win);
	mvwprintw(win, (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2, "RECORDING...");

	wrefresh(win);
}

void CommandLineInterface::cliKeyDetectedStop()
{
	flash();
	wmove(win, 0, COLS - 12);
	wclrtoeol(win);
	string pressed_char_str = "Detected [s]";
	mvwprintw(win, 0, COLS - pressed_char_str.length(), pressed_char_str.c_str());

	wrefresh(win);
	napms(500);

	wmove(win, 0, COLS - pressed_char_str.length());
	wclrtoeol(win);

	wmove(win, (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2);
	wclrtoeol(win);
	mvwprintw(win, (LINES - inner_box_height) / 2, (COLS - inner_box_width) / 2, "STOPPED!");

	wrefresh(win);
}

void CommandLineInterface::cliEndWindow(string out_filename)
{
	napms(1000);
	werase(win);

	mvwprintw(win, 0, 0, "Output file `%s` successfully saved.", out_filename.c_str());
	string press_exit_str = "Press any key to exit...";
	mvwprintw(win, LINES - 1, COLS - press_exit_str.length(), press_exit_str.c_str());
	wrefresh(win);

	char ch = getch(); // wait for a key

	werase(win);
	wrefresh(win);
}
