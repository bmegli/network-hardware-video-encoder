# NHVE Network Hardware Video Encoder C library

Library for hardware video encoding and streaming over custom [MLSP](https://github.com/bmegli/minimal-latency-streaming-protocol) protocol.

See also twin [NHVD](https://github.com/bmegli/network-hardware-video-decoder) network decoder.

See [hardware-video-streaming](https://github.com/bmegli/hardware-video-streaming) for other related projects.

The intent behind library:
- minimize video latency
- minimize CPU usage (hardware encoding and color conversions)
- multi-frame streaming (e.g. depth + texture)
- auxiliary data channels (e.g. IMU, odometry, metadata)
- simple user interface

If you have no specific needs you should start with gstreamer or FFmpeg command line streaming instead.

## Platforms 

Unix-like operating systems (e.g. Linux).
The dependency is through [MLSP](https://github.com/bmegli/minimal-latency-streaming-protocol) socket use (easily portable).

Tested on Ubuntu 18.04.

## Hardware

Intel VAAPI compatible hardware encoders (Quick Sync Video).

ATI/AMD may also work through VAAPI (libva-mesa-driver, not tested however).

The dependency is through [HVE](https://github.com/bmegli/hardware-video-encoder) implementation (see [HVE issues](https://github.com/bmegli/hardware-video-encoder/issues/5)).

Tested on LattePanda Alpha and i7-7820HK laptop.

## Dependencies

Library depends on:
- [HVE Hardware Video Encoder](https://github.com/bmegli/hardware-video-encoder)
	- FFmpeg `avcodec`, `avutil`, `avfilter` (at least 3.4 version)
- [MLSP Minimal Latency Streaming Protocol](https://github.com/bmegli/minimal-latency-streaming-protocol)

HVE and MLSP are included as submodules so you only need to satifisy HVE dependencies.

Works with system FFmpeg on Ubuntu 18.04 and doesn't on 16.04 (outdated FFmpeg and VAAPI ecosystem).

## Building Instructions

Tested on Ubuntu 18.04.

``` bash
# update package repositories
sudo apt-get update 
# get avcodec and avutil
sudo apt-get install ffmpeg libavcodec-dev libavutil-dev libavfilter-dev
# get compilers and make 
sudo apt-get install build-essential
# get cmake - we need to specify libcurl4 for Ubuntu 18.04 dependencies problem
sudo apt-get install libcurl4 cmake
# get git
sudo apt-get install git
# clone the repository with *RECURSIVE* for submodules
git clone --recursive https://github.com/bmegli/network-hardware-video-encoder.git

# finally build the library and examples
cd network-hardware-video-encoder
mkdir build
cd build
cmake ..
make
```

## Running example

Stream procedurally generated H.264/HEVC video over UDP (moving through greyscale)

```bash
# Usage: ./nhve-stream-* <ip> <port> <seconds> [device]
./nhve-stream-h264 127.0.0.1 9766 10
./nhve-stream-hevc10 127.0.0.1 9766 10
./nhve-stream-multi 127.0.0.1 9766 10
./nhve-stream-h264-aux 127.0.0.1 9766 10
```

You may need to specify VAAPI device if you have more than one (e.g. NVIDIA GPU + Intel CPU).

```bash
# Usage: ./nhve-stream-* <ip> <port> <seconds> [device]
./nhve-stream-h264 127.0.0.1 9766 10 /dev/dri/renderD128 #or D129
./nhve-stream-hevc10 127.0.0.1 9766 10 /dev/dri/renderD128 #or D129
./nhve-stream-multi 127.0.0.1 9766 10 /dev/dri/renderD128 #or D129
./nhve-stream-h264-aux 127.0.0.1 9766 10 /dev/dri/renderD128 #or D129
```

If you don't have receiving end you will just see if hardware encoding worked.

If you get errors see also HVE [troubleshooting](https://github.com/bmegli/hardware-video-encoder/wiki/Troubleshooting).

## Using

See examples directory for more complete and commented examples with error handling.

See [HVE](https://github.com/bmegli/hardware-video-encoder) docs for details about hardware configuration.


```C
//prepare library data
struct nhve_net_config net_config = {IP, PORT};
struct nhve_hw_config hw_config = {WIDTH, HEIGHT, FRAMERATE, DEVICE, ENCODER,
                                   PIXEL_FORMAT, PROFILE, BFRAMES, BITRATE,
                                   QP, GOP_SIZE, COMPRESSION_LEVEL, LOW_POWER};

//initialize single hardware encoder
struct nhve *streamer = nhve_init(&net_config, &hw_config, 1, 0);

struct nhve_frame frame = { 0 };

//later assuming PIXEL_FORMAT is "nv12" (you can use something else)

//fill with your stride (width including padding if any)
frame.linesize[0] = frame.linesize[1] = WIDTH;

//...
//whatever logic you have to prepare data source
//..

while(keep_streaming)
{
	//...
	//update NV12 Y and color data (e.g. get them from camera)
	//...

	//fill nhve_frame with increasing framenumber and
	//pointers to your data in NV12 pixel format
	frame.data[0]=Y; //dummy luminance plane
	frame.data[1]=color; //dummy UV plane
	
	//encode and send this frame
	if( nhve_send(streamer, &frame, 0) != NHVE_OK)
		break; //break on error
}

//flush the streamer by sending NULL frame
nhve_send(streamer, NULL, 0);

nhve_close(streamer);
```

That's it! You have just seen all the functions and data types in the library.

The same interface works for multi-frame streaming with:
- array of hardware configurations in `nhve_init`
- `nhve_send(streamer, &frame0, 0)`, `nhve_send(streamer, &frame1, 1)`, ...

The same interface works for non-video (raw) data streaming with:
- number of auxiliary channels in `nhve_init`
- `nhve_send` with `frame.data[0]` of size `frame.linesize[0]` raw data

## Compiling your code

### IDE (recommended)

The simplest way is to copy headers and sources of HVE, MLSP and NHVE to your project and link with avcodec, avutil and avfilter.

### CMake

See [realsense-network-hardware-video-encoder](https://github.com/bmegli/realsense-network-hardware-video-encoder) as example.

## License

Library and my dependencies are licensed under Mozilla Public License, v. 2.0

This is similiar to LGPL but more permissive:
- you can use it as LGPL in prioprietrary software
- unlike LGPL you may compile it statically with your code

Like in LGPL, if you modify this library, you have to make your changes available.
Making a github fork of the library with your changes satisfies those requirements perfectly.

Since you are linking to FFmpeg libraries consider also `avcodec`, `avutil` and `avfilter` licensing.

## Additional information

### Library uses

Realsense D400 infrared/color H.264 and infrared/color/depth HEVC streaming - [realsense-network-hardware-video-encoder](https://github.com/bmegli/realsense-network-hardware-video-encoder)
