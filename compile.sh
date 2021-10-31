g++ -Wno-deprecated -g main.cpp $(pkg-config --libs libavformat libavcodec libavdevice libavfilter libavutil libswscale libswresample) -lz -lpthread -o main
