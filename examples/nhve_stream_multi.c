/*
 * NHVE Network Hardware Video Encoder library example of
 * streaming with multiple simultanous hardware encoders
 *
 * Copyright 2019-2020 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <stdio.h> //printf, fprintf
#include <inttypes.h> //uint8_t
#include <unistd.h> //usleep

#include "../nhve.h"

const char *IP; //e.g "127.0.0.1"
unsigned short PORT; //e.g. 9667 

const int WIDTH=640;
const int HEIGHT=360;
const int FRAMERATE=30;
int SECONDS=10;
const char *DEVICE; //NULL for default or device e.g. "/dev/dri/renderD128"
const char *ENCODER=NULL;//NULL for default (h264_vaapi) or FFmpeg encoder e.g. "hevc_vaapi", ...
const char *PIXEL_FORMAT="nv12"; //NULL / "" for default (NV12) or pixel format e.g. "rgb0"
const int PROFILE=FF_PROFILE_H264_HIGH; //or FF_PROFILE_H264_MAIN, FF_PROFILE_H264_CONSTRAINED_BASELINE, ...
const int BFRAMES=0; //max_b_frames, set to 0 to minimize latency, non-zero to minimize size
const int BITRATE1=500000; //average bitrate in VBR mode (bit_rate != 0 and qp == 0)
const int BITRATE2=2000000; //average bitrate in VBR mode (bit_rate != 0 and qp == 0)
const int QP=0; //quantization parameter in CQP mode (qp != 0 and bit_rate == 0)
const int GOP_SIZE=0; //group of pictures size, 0 for default (determines keyframe period)
const int COMPRESSION_LEVEL=0; //speed-quality tradeoff, 0 for default, 1 for the highest quality, 7 for the fastest

//IP, PORT, SECONDS and DEVICE are read from user input

int streaming_loop(struct nhve *streamer);
int process_user_input(int argc, char* argv[]);
int hint_user_on_failure(char *argv[]);
void hint_user_on_success();

int main(int argc, char* argv[])
{
	//get SECONDS and DEVICE from the command line
	if( process_user_input(argc, argv) < 0 )
		return -1;

	//prepare library data
	struct nhve_net_config net_config = {IP, PORT};
	
	struct nhve_hw_config hw_config[2] = 
	{  //those could be completely different encoders using different hardware, here just different bitrate
		{WIDTH, HEIGHT, FRAMERATE, DEVICE, ENCODER, PIXEL_FORMAT, PROFILE, BFRAMES, BITRATE1, QP, GOP_SIZE, COMPRESSION_LEVEL},
		{WIDTH, HEIGHT, FRAMERATE, DEVICE, ENCODER, PIXEL_FORMAT, PROFILE, BFRAMES, BITRATE2, QP, GOP_SIZE, COMPRESSION_LEVEL}
	};

	struct nhve *streamer;

	//initialize library with nhve_multi_init
	if( (streamer = nhve_init(&net_config, hw_config, 2)) == NULL )
		return hint_user_on_failure(argv);

	//do the actual encoding
	int status = streaming_loop(streamer);

	nhve_close(streamer);

	if(status == 0)
		hint_user_on_success();

	return status;
}

int streaming_loop(struct nhve *streamer)
{
	const int TOTAL_FRAMES = SECONDS*FRAMERATE;
	const useconds_t useconds_per_frame = 1000000/FRAMERATE;
	int f;
	struct nhve_frame frames[2] = { 0 };
	

	//we are working with NV12 because we specified nv12 pixel formats
	//when calling nhve_multi_init, in principle we could use other format
	//if hardware supported it (e.g. RGB0 is supported on my Intel)
	uint8_t Y1[WIDTH*HEIGHT]; //dummy NV12 luminance data for encoder 1
	uint8_t Y2[WIDTH*HEIGHT]; //                          for encoder 2

	uint8_t color[WIDTH*HEIGHT/2]; //same dummy NV12 color data for both encoders

	//fill with your stride (width including padding if any)
	frames[0].linesize[0] = frames[1].linesize[0] = WIDTH; //Y planes stride
	frames[0].linesize[1] = frames[1].linesize[1] = WIDTH; //UV planes stride 

	for(f=0;f<TOTAL_FRAMES;++f)
	{
		//prepare dummy images data, normally you would take it from cameras or other source
		memset(Y1, f % 255, WIDTH*HEIGHT); //NV12 luminance (ride through greyscale)
		memset(Y2, 255 - (f % 255), WIDTH*HEIGHT); //NV12 luminance (reverse ride through greyscale)
		memset(color, 128, WIDTH*HEIGHT/2); //NV12 UV (no color really)

		//fill nhve_frame with pointers to your data in NV12 pixel format
		frames[0].data[0]=Y1;
		frames[0].data[1]=color;

		frames[1].data[0]=Y2;
		frames[1].data[1]=color;
		
		//encode and send this frame, the framenumber f has to increase
		if(nhve_send(streamer, f, frames) != NHVE_OK)
			break; //break on error

		//simulate real time source (sleep according to framerate)
		usleep(useconds_per_frame);
	}

	//flush the encoder by sending NULL frame, encode some last frames returned from hardware
	nhve_send(streamer, f, NULL);

	//did we encode everything we wanted?
	//convention 0 on success, negative on failure
	return f == TOTAL_FRAMES ? 0 : -1;
}

int process_user_input(int argc, char* argv[])
{
	if(argc < 4)
	{
		fprintf(stderr, "Usage: %s <ip> <port> <seconds> [device]\n", argv[0]);
		fprintf(stderr, "\nexamples:\n");
		fprintf(stderr, "%s 127.0.0.1 9766 10\n", argv[0]);
		fprintf(stderr, "%s 127.0.0.1 9766 10 /dev/dri/renderD128\n", argv[0]);
		return -1;
	}
	
	IP = argv[1];
	PORT = atoi(argv[2]);
	SECONDS = atoi(argv[3]);
	DEVICE=argv[4]; //NULL as last argv argument, or device path

	return 0;
}

int hint_user_on_failure(char *argv[])
{
	fprintf(stderr, "unable to initalize, try to specify device e.g:\n\n");
	fprintf(stderr, "%s 127.0.0.1 9766 10 /dev/dri/renderD128\n", argv[0]);
	return -1;
}
void hint_user_on_success()
{
	printf("finished successfully\n");
}
