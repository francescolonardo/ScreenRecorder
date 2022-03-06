# ScreenRecorder

Project for the ***Programmazione di Sistema*** course at the ***Politecnico di Torino***

### Group

- [Francesco Lonardo] (https://github.com/francescolonardo)
- [Marco Barca] (https://github.com/marcobarca)

## About the Project

**ScreenRecorder** is a project based on two main components:

- the **ScreenRecorder library**, a multiplatform **C++** library, which allows to record the video from the screen and (optionally) the audio from the microphone.

- the **CommandLineInterface library**, a multiplatform **C++** cli frontend, used to interact with the **ScreenRecorder library**.

### ScreenRecorder Library

Composed by **ScreenRecorder.ccp** and **ScreenRecorder.h**, it uses [**ffmpeg**] (https://ffmpeg.org/) libraries to capture the frames (video/audio), to elaborate them, and to save the result in a **.mp4** file.
Those libraries are **avcodec**, **avdevice**, **avfilter**, **avformat**, **avutil**, **libswscale** and **libswresample**.


### CommandLineInterface Library

Composed by **CommandLineInterface.ccp** and **CommandLineInterface.h**, it uses [**ncurses**] () libraries to show in the command line an interface
