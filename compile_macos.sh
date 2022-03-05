export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/local/ffmpeg/lib/pkgconfig" && g++ -Wno-deprecated -Wno-format-zero-length -Wno-write-strings -std=c++14 -g main.cpp ScreenRecorder.cpp CommandLineInterface.cpp -I/usr/local/ffmpeg/include/ $(pkg-config --libs libavformat libavcodec libavdevice libavfilter libavutil libswscale libswresample) -lncurses -o main
