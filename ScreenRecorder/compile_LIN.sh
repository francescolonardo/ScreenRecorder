g++ -Wno-deprecated -Wno-format-zero-length -Wno-write-strings -g main.cpp ScreenRecorder.cpp $(pkg-config --libs libavformat libavcodec libavdevice libavfilter libavutil libswscale libswresample x11) -lpthread -lncurses -o main
