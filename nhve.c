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

enum NHVE_COMPILE_TIME_CONSTANTS
{
	NHVE_MAX_ENCODERS=3, //!< max number of encoders in multi encoding
};

struct nhve
{
	struct mlsp *network_streamer;
	struct hve *hardware_encoder[NHVE_MAX_ENCODERS];
	int hardware_encoders_size;
	int auxiliary_channels_size;
};

static int nhve_send_video(struct nhve *n, const struct nhve_frame *frame, uint8_t subframe);
static int nhve_send_auxiliary(struct nhve *n, const struct nhve_frame *frame, uint8_t subframe);

static struct nhve *nhve_close_and_return_null(struct nhve *n, const char *msg);
static int NHVE_ERROR_MSG(const char *msg);

struct nhve *nhve_init(const struct nhve_net_config *net_config,const struct nhve_hw_config *hw_config, int hw_size, int aux_size)
{
	struct nhve *n, zero_nhve = {0};
	struct mlsp_config mlsp_cfg = {net_config->ip, net_config->port, 0, hw_size + aux_size};

	if(hw_size > NHVE_MAX_ENCODERS)
		return nhve_close_and_return_null(NULL, "the maximum number of encoders (compile time) exceeded");

	if( ( n = (struct nhve*)malloc(sizeof(struct nhve))) == NULL )
		return nhve_close_and_return_null(NULL, "not enough memory for nhve");

	*n = zero_nhve;

	if( (n->network_streamer = mlsp_init_client(&mlsp_cfg)) == NULL )
		return nhve_close_and_return_null(n, "failed to initialize network client");

	n->hardware_encoders_size = hw_size;
	n->auxiliary_channels_size = aux_size;

	for(int i=0;i<hw_size;++i)
	{
		struct hve_config hve_cfg = {hw_config[i].width, hw_config[i].height, hw_config[i].width, hw_config[i].height,
		hw_config[i].framerate, hw_config[i].device, hw_config[i].encoder, hw_config[i].pixel_format,
		hw_config[i].profile, hw_config[i].max_b_frames, hw_config[i].bit_rate, hw_config[i].qp, hw_config[i].gop_size,
		hw_config[i].compression_level};

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

int nhve_send(struct nhve *n, const struct nhve_frame *frame, uint8_t subframe)
{
	if(subframe >= n->hardware_encoders_size + n->auxiliary_channels_size)
		return NHVE_ERROR_MSG("subframe exceeds configured video/aux channels");

	if(subframe < n->hardware_encoders_size)
		return nhve_send_video(n, frame, subframe);

	return nhve_send_auxiliary(n, frame, subframe);
}

//3 scenarios:
//NULL frame - flush encoder
//non NULL frame and non NULL frame->data[0] - encode and send (typical)
//non NULL frame and NULL frame->data[0] - send empty frame
//this is necessary to support e.g. different framerates or B frames in multi-frame scenario
static int nhve_send_video(struct nhve *n, const struct nhve_frame *frame, uint8_t subframe)
{
	struct hve_frame video_frame = {0};
	struct mlsp_frame network_frame = {0};

	if(!frame) //NULL frame is valid input - flush the encoder
		if( hve_send_frame(n->hardware_encoder[subframe], NULL) != HVE_OK)
			return NHVE_ERROR_MSG("failed to send flush frame to hardware");

	if(frame)
	{
		if( !frame->data[0] ) //empty data, send empty MLSP frame
		{
			if( mlsp_send(n->network_streamer, &network_frame, subframe) != MLSP_OK)
				return NHVE_ERROR_MSG("failed to send frame");

			return NHVE_OK;
		}
		//copy pointers to data planes and linesizes (just a few bytes)
		memcpy(video_frame.data, frame->data, sizeof(frame->data));
		memcpy(video_frame.linesize, frame->linesize, sizeof(frame->linesize));

		if( hve_send_frame(n->hardware_encoder[subframe], &video_frame) != HVE_OK )
			return NHVE_ERROR_MSG("failed to send frame to hardware");
	}

	AVPacket *encoded_frame;
	int failed;

	//the only scenario when we get more than 1 frame is flushing
	//in such case we send only first encoded frame and drain the rest
	//otherwise the receiving side will not collect packet in multi-frame scenario
	while( (encoded_frame = hve_receive_packet(n->hardware_encoder[subframe], &failed)) )
	{
		if(network_frame.data)
			continue; //if we already sent something (flushing), ignore the rest of data

		network_frame.data = encoded_frame->data;
		network_frame.size = encoded_frame->size;

		if( mlsp_send(n->network_streamer, &network_frame, subframe) != MLSP_OK)
			return NHVE_ERROR_MSG("failed to send frame");
	}

	//NULL packet and non-zero failed indicates failure during encoding
	if(failed != HVE_OK)
		return NHVE_ERROR_MSG("failed to encode frame");

	return NHVE_OK;
}

static int nhve_send_auxiliary(struct nhve *n, const struct nhve_frame *frame, uint8_t subframe)
{
	struct mlsp_frame network_frame = {0};
	//empty frames are legal and result in sending 0 size frames
	if(frame && frame->data[0])
	{
		network_frame.data = frame->data[0];
		network_frame.size = frame->linesize[0];
	}

	if( mlsp_send(n->network_streamer, &network_frame, subframe) != MLSP_OK)
		return NHVE_ERROR_MSG("failed to send aux frame");

	return NHVE_OK;
}

static int NHVE_ERROR_MSG(const char *msg)
{
	fprintf(stderr, "nhve: %s\n", msg);
	return NHVE_ERROR;
}
