/*
 * NHVE Network Hardware Video Encoder C library header
 *
 * Copyright 2019 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef NHVE_H
#define NHVE_H

#include <libavcodec/avcodec.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//internal library data
struct nhve;

struct nhve_hw_config
{
	int width; //!< width of the encoded frames
	int height; //!< height of the encoded frames
	int framerate; //!< framerate of the encoded video
	const char *device; //!< NULL / "" or device, e.g. "/dev/dri/renderD128"
	const char *pixel_format; //!< NULL / "" for NV12 or format, e.g. "rgb0", "bgr0", "nv12", "yuv420p"
	int profile; //!< 0 to guess from input or profile e.g. FF_PROFILE_H264_MAIN, FF_PROFILE_H264_HIGH
	int max_b_frames; //!< maximum number of B-frames between non-B-frames (disable if you need low latency)
	int bit_rate; //!< the average bitrate in VBR mode
};

struct nhve_net_config
{
	const char *ip; //!< IP (send to)
	uint16_t port; //!< server port
};

struct nhve_frame
{
	uint16_t framenumber;
	uint8_t *data[AV_NUM_DATA_POINTERS]; //!< array of pointers to frame planes (e.g. Y plane and UV plane)
	int linesize[AV_NUM_DATA_POINTERS]; //!< array of strides (width + padding) for planar frame formats
};

enum nhve_retval_enum
{
	NHVE_ERROR=-1, //!< error occured
	NHVE_OK=0, //!< succesfull execution
};

//NULL on error, non NULL on success
struct nhve *nhve_init(const struct nhve_net_config *net_config, const struct nhve_hw_config *hw_config);
void nhve_close(struct nhve *n);

//NHVE_OK on success, NHVE_ERROR on error
//pass NULL frame to flush encoder
int nhve_send_frame(struct nhve *n,struct nhve_frame *frame);

#ifdef __cplusplus
}
#endif

#endif
