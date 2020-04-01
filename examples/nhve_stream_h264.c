/*
 * NHVE Network Hardware Video Encoder library example of streaming H.264
 *
 * Copyright 2019 (C) Bartosz Meglicki <meglickib@gmail.com>
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
const int BITRATE=0; //average bitrate in VBR mode (bit_rate != 0 and qp == 0)
const int QP=0; //quantization parameter in CQP mode (qp != 0 and bit_rate == 0)
const int GOP_SIZE=0; //group of pictures size, 0 for default (determines keyframe period)

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
	struct nhve_hw_config hw_config = {WIDTH, HEIGHT, FRAMERATE, DEVICE, ENCODER,
					PIXEL_FORMAT, PROFILE, BFRAMES, BITRATE, QP, GOP_SIZE};
	struct nhve *streamer;

	//initialize library with nhve_init
	if( (streamer = nhve_init(&net_config, &hw_config)) == NULL )
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
	struct nhve_frame frame = { 0 };
	int frames=SECONDS*FRAMERATE, f;
	const useconds_t useconds_per_frame = 1000000/FRAMERATE;

	//we are working with NV12 because we specified nv12 pixel format
	//when calling nhve_init, in principle we could use other format
	//if hardware supported it (e.g. RGB0 is supported on my Intel)
	uint8_t Y[WIDTH*HEIGHT]; //dummy NV12 luminance data
	uint8_t color[WIDTH*HEIGHT/2]; //dummy NV12 color data

	//fill with your stride (width including padding if any)
	frame.linesize[0] = frame.linesize[1] = WIDTH;

	for(f=0;f<frames;++f)
	{
		//prepare dummy image date, normally you would take it from camera or other source
		memset(Y, f % 255, WIDTH*HEIGHT); //NV12 luminance (ride through greyscale)
		memset(color, 128, WIDTH*HEIGHT/2); //NV12 UV (no color really)

		//fill hve_frame with pointers to your data in NV12 pixel format
		frame.data[0]=Y;
		frame.data[1]=color;
		
		//increase the framenumber, this is mandatory for the network protocol
		frame.framenumber = f;

		//encode and send this frame
		if( nhve_send_frame(streamer, &frame) != NHVE_OK)
			break; //break on error

		//simulate real time source (sleep according to framerate)
		usleep(useconds_per_frame);
	}

	//flush the encoder by sending NULL frame, encode some last frames returned from hardware
	nhve_send_frame(streamer, NULL);

	//did we encode everything we wanted?
	//convention 0 on success, negative on failure
	return f == frames ? 0 : -1;
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
