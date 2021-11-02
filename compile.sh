g++ -Wno-deprecated -g main.cpp $(pkg-config --libs libavformat libavcodec libavdevice libavfilter libavutil libswscale libswresample x11) -lz -lpthread -o main
