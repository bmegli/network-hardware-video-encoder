/*
 * NHVE Network Hardware Video Encoder C library implementation
 *
 * Copyright 2019-2020 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "nhve.h"

// Minimal Latency Streaming Protocol library
#include "mlsp.h"
// Hardware Video Encoder library
#include "hve.h"

#include <stdio.h>

//benchmark related
#include <time.h> //clock_gettime


enum NHVE_COMPILE_TIME_CONSTANTS
{
	NHVE_MAX_ENCODERS=3, //!< max number of encoders in multi encoding
};

struct nhve
{
	struct mlsp *network_streamer;
	struct hve *hardware_encoder[NHVE_MAX_ENCODERS];
	int hardware_encoders_size;
};

static struct nhve *nhve_close_and_return_null(struct nhve *n, const char *msg);
static int NHVE_ERROR_MSG(const char *msg);

struct nhve *nhve_init(const struct nhve_net_config *net_config,const struct nhve_hw_config *hw_config, int hw_size)
{
	struct nhve *n, zero_nhve = {0};
	struct mlsp_config mlsp_cfg = {net_config->ip, net_config->port, 0};

	if(hw_size > NHVE_MAX_ENCODERS)
		return nhve_close_and_return_null(NULL, "the maximum number of encoders (compile time) exceeded");

	if( ( n = (struct nhve*)malloc(sizeof(struct nhve))) == NULL )
		return nhve_close_and_return_null(NULL, "not enough memory for nhve");

	*n = zero_nhve;

	if( (n->network_streamer = mlsp_init_client(&mlsp_cfg)) == NULL )
		return nhve_close_and_return_null(n, "failed to initialize network client");

	n->hardware_encoders_size = hw_size;

	for(int i=0;i<hw_size;++i)
	{
		struct hve_config hve_cfg = {hw_config[i].width, hw_config[i].height, hw_config[i].framerate, hw_config[i].device,
		hw_config[i].encoder, hw_config[i].pixel_format, hw_config[i].profile, hw_config[i].max_b_frames,
		hw_config[i].bit_rate, hw_config[i].qp, hw_config[i].gop_size, hw_config[i].compression_level};

		if( (n->hardware_encoder[i] = hve_init(&hve_cfg)) == NULL )
			return nhve_close_and_return_null(n, "failed to initalize hardware encoder");
	}

	return n;
}

static struct nhve *nhve_close_and_return_null(struct nhve *n, const char *msg)
{
	if(msg)
		fprintf(stderr, "nhve: %s\n", msg);

	nhve_close(n);

	return NULL;
}

void nhve_close(struct nhve *n)
{
	if(n == NULL)
		return;

	mlsp_close(n->network_streamer);
	for(int i=0;i<n->hardware_encoders_size;++i)
		hve_close(n->hardware_encoder[i]);
	free(n);
}

//NULL frames to flush all encoders
//NULL frames[i].data[0] is legal and silently skipped
//this is necessary to support multiple encoders with e.g. different B frames
int nhve_send(struct nhve *n, uint16_t framenumber, struct nhve_frame *frames)
{
	struct hve_frame video_frames[NHVE_MAX_ENCODERS] = {0};
	const int encoders = n->hardware_encoders_size;

	struct timespec start, encode, encode_send;

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);

	if(frames) //NULL frames is valid input - flush the encoders
		for(int i=0;i<encoders;++i)
		{	//copy pointers to data and linesizes (just a few bytes)
			memcpy(video_frames[i].data, frames[i].data, sizeof(frames[i].data));
			memcpy(video_frames[i].linesize, frames[i].linesize, sizeof(frames[i].linesize));
		}

	for(int i=0;i<encoders;++i)
	{
		if(!frames) //flush all encoders
			if( hve_send_frame(n->hardware_encoder[i], NULL) != HVE_OK)
				return NHVE_ERROR_MSG("failed to send flush frame to hardware");

		//sending data only to selected encoders is legal and will not result in flushing
		if( video_frames[i].data[0] && (hve_send_frame(n->hardware_encoder[i], &video_frames[i]) != HVE_OK) )
			return NHVE_ERROR_MSG("failed to send frame to hardware");
	}

	AVPacket *encoded_frames[NHVE_MAX_ENCODERS];
	int failed[NHVE_MAX_ENCODERS] = {0};
	int keep_working;

	//while any of encoders still produces data we should send frames
	do
	{
		struct mlsp_frame network_frame = {0};

		keep_working = 0;

		for(int i=0;i<encoders;++i)
			if( (encoded_frames[i] = hve_receive_packet(n->hardware_encoder[i], &failed[i])) )
			{
				network_frame.data[i] = encoded_frames[i]->data;
				network_frame.size[i] = encoded_frames[i]->size;
				//this should be increased with every frame, otherwise the world will explode
				//this means that it not good for flushing now
				network_frame.framenumber = framenumber;
				keep_working = 1; //some encoder still produces output
			}

		clock_gettime(CLOCK_MONOTONIC_RAW, &encode);

		if(keep_working && (mlsp_send(n->network_streamer, &network_frame) != MLSP_OK) )
			return NHVE_ERROR_MSG("failed to send frame");

	} while(keep_working);

	clock_gettime(CLOCK_MONOTONIC_RAW, &encode_send);

	//NULL packet and non-zero failed indicates failure during encoding
	for(int i=0;i<encoders;++i)
		if(failed[i] != HVE_OK)
			return NHVE_ERROR_MSG("failed to encode frame");

	double encode_ms = (encode.tv_nsec - start.tv_nsec) / 1000000.0;
	double encode_send_ms = (encode_send.tv_nsec - start.tv_nsec) / 1000000.0;

	printf("encoded in %f ms, encoded/sent in %f ms\n", encode_ms, encode_send_ms);

	return NHVE_OK;
}

static int NHVE_ERROR_MSG(const char *msg)
{
	fprintf(stderr, "nhve: %s\n", msg);
	return NHVE_ERROR;
}
