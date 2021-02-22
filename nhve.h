/*
 * NHVE Network Hardware Video Encoder C library header
 *
 * Copyright 2019-2020 (C) Bartosz Meglicki <meglickib@gmail.com>
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

/**
 * @struct nhve
 * @brief Internal library data passed around by the user.
 * @see nhve_init, nhve_close
 */
struct nhve;

/**
 * @struct nhve_hw_config
 * @brief Hardware encoder configuration.
 *
 * For more details see:
 * <a href="https://bmegli.github.io/hardware-video-encoder/structhve__config.html">HVE documentation</a>
 *
 * @see nhve_init
 */
struct nhve_hw_config
{
	int width; //!< width of the encoded frames
	int height; //!< height of the encoded frames
	int framerate; //!< framerate of the encoded video
	const char *device; //!< NULL / "" or device, e.g. "/dev/dri/renderD128"
	const char *encoder; //!< NULL / "" or encoder, e.g. "h264_vaapi"
	const char *pixel_format; //!< NULL / "" for NV12 or format, e.g. "rgb0", "bgr0", "nv12", "yuv420p"
	int profile; //!< 0 to guess from input or profile e.g. FF_PROFILE_H264_MAIN, FF_PROFILE_H264_HIGH
	int max_b_frames; //!< maximum number of B-frames between non-B-frames (disable if you need low latency)
	int bit_rate; //!< average bitrate in VBR mode (bit_rate != 0 and qp == 0)
	int qp; //!< quantization parameter in CQP mode (qp != 0 and bit_rate == 0)
	int gop_size; //!<  group of pictures size, 0 for default, -1 for intra only
	int compression_level; //!< speed-quality tradeoff, 0 for default, 1 for the highest quality, 7 for the fastest
	int low_power; //!< alternative limited low-power encoding if non-zero
};

/**
 * @struct nhve_net_config
 * @brief Network configuration.
 *
 * For more details see:
 * <a href="https://github.com/bmegli/minimal-latency-streaming-protocol">MLSP</a>
 *
 * @see nhve_init
 */
struct nhve_net_config
{
	const char *ip; //!< IP (send to)
	uint16_t port; //!< server port
};

/**
 * @struct nhve_frame
 * @brief Data to be encoded (single frame) or raw auxiliary data.
 *
 * Fill linsize array with stride (width and padding) of the data in bytes.
 * Fill data with pointers to the data (no copying is needed).
 *
 * For non planar formats or auxiliary data only data[0] and linesize[0] is used.
 *
 * @see nhve_send
 */
struct nhve_frame
{
	uint8_t *data[AV_NUM_DATA_POINTERS]; //!< array of pointers to frame planes (e.g. Y plane and UV plane)
	int linesize[AV_NUM_DATA_POINTERS]; //!< array of strides (width + padding) for planar frame formats
};

/**
  * @brief Constants returned by most of library functions
  */
enum nhve_retval_enum
{
	NHVE_ERROR=-1, //!< error occured
	NHVE_OK=0, //!< succesfull execution
};

/**
 * @brief Initialize internal library data.
 *
 * Initialize streaming and single or multiple (hw_size > 1) hardware decoders
 * and (aux_size > 1) auxiliary non-video raw data channels.
 *
 * @param net_config network configuration
 * @param hw_config hardware encoders configuration of hw_size size
 * @param hw_size number of supplied hardware encoder configurations
 * @param aux_size number of auxiliary non-video raw data channels
 * @return
 * - pointer to internal library data
 * - NULL on error, errors printed to stderr
 *
 * @see nhve_net_config, nhve_hw_config
 */
struct nhve *nhve_init(const struct nhve_net_config *net_config, const struct nhve_hw_config *hw_config, int hw_size, int aux_size);

/**
 * @brief Free library resources
 *
 * Cleans and frees library memory.
 *
 * @param n pointer to internal library data
 * @see nhve_init
 *
 */
void nhve_close(struct nhve *n);

/**
 * @brief Encode if necessary and send next frame
 *
 * Function blocks until frame is hardware encoded and passed to network stack or error occurs.
 *
 * In typical scenario (hw_size == 1, aux_size == 0) just call
 * nhve_send(n, frame, 0) every time you have new video data.
 *
 * For auxiliary (non-video) frames raw data (not encoded is sent).
 *
 * In general case the sending sequence should follow nhve_init hw_size and aux_size.
 * E.g. for hw_size == 2 and aux_size == 1
 * - nhve_send(n, f1, 0) with first video data
 * - nhve_send(n, f2, 1) with second video data
 * - nhve_send(n, f3, 2) with auxiliary data
 *
 * For video frames:
 * - NULL frame to flush encoder
 * - NULL frame->data[0] is legal, results in sending empty frame
 * - this is necessary to support e.g. different framerates or B frames in multi-frame scenario
 *
 * For auxiliary frames:
 * - only frame->data[0] of size frame->linesize[0] is sent
 * - NULL frame is legal, results in sending empty frame
 * - NULL frame->data is legal, results in sending empty frame
 *
 * @param n pointer to internal library data
 * @param frame pointer to video data or raw auxiliary data
 * @param subframe determines subframe (channel) as defined by nhve_init hw_size and aux_size
 * @return
 * - NHVE_OK on success
 * - NHVE_ERROR on error
 *
 * @see nhve_init
 *
 *
 */
int nhve_send(struct nhve *n, const struct nhve_frame *frame, uint8_t subframe);

#ifdef __cplusplus
}
#endif

#endif
