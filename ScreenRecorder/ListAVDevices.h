#ifndef AUDIORECORDER_LISTAVDEVICES_H
#define AUDIORECORDER_LISTAVDEVICES_H

#ifdef WINDOWS
// ref:  https://blog.csdn.net/jhqin/article/details/5929796
#include <windows.h>
#include <vector>
#include <dshow.h>

#ifndef MACRO_GROUP_DEVICENAME
#define MACRO_GROUP_DEVICENAME

#define MAX_FRIENDLY_NAME_LENGTH 128
#define MAX_MONIKER_NAME_LENGTH 256

#endif

HRESULT DS_GetAudioVideoInputDevices(vector<string> &vectorDevices, string deviceType);
string GbkToUtf8(const char *src_str);
string Utf8ToGbk(const char *src_str);

string DS_GetDefaultDevice(string type);

#endif
#endif // AUDIORECORDER_LISTAVDEVICES_H
