export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig/" && g++ -Wno-deprecated -Wno-format-zero-length -Wno-write-strings -g main.cpp ScreenRecorder.cpp ListAVDevices.cpp -I/usr/local/include/ $(pkg-config --libs libavformat libavcodec libavdevice libavfilter libavutil libswscale libswresample) -lz -lpthread -o main
