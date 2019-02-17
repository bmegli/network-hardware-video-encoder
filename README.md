# NHVE Network Hardware Video Encoder C library

Library for hardware video encoding over custom network protocol.

The intent behind library:
- minimize video latency
- minimize CPU usage (hardware encoding and color conversions)
- simple user interface

## Using

See examples directory for a more complete and commented examples with error handling.

```C
	//prepare library data
	struct nhve_net_config net_config = {IP, PORT};
	struct nhve_hw_config hw_config = {WIDTH, HEIGHT, FRAMERATE,
					DEVICE, PIXEL_FORMAT, PROFILE, BFRAMES, BITRATE};
	//initialize
	struct nhve *streamer = nhve_init(&net_config, &hw_config);
	
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
		frame.framenumber=framenumber++; //dummy framenumber
		frame.data[0]=Y; //dummy luminance plane
		frame.data[1]=color; //dummy UV plane
		
		//encode and send this frame
		if( nhve_send_frame(streamer, &frame) != NHVE_OK)
			break; //break on error
	}

	//flush the streamer by sending NULL frame
	nhve_send_frame(streamer, NULL);
	
	nhve_close(streamer);
```

That's it! You have just seen all the functions and data types in the library.
