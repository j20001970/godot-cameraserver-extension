/**************************************************************************/
/*  buffer_decoder.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef BUFFER_DECODER_H
#define BUFFER_DECODER_H

#include "godot_cpp/classes/camera_feed.hpp"
#include "godot_cpp/classes/image.hpp"
#include "godot_cpp/classes/ref.hpp"

using namespace godot;

struct StreamingBuffer {
	void *start = nullptr;
	size_t length = 0;
};

class BufferDecoder {
protected:
	CameraFeed *camera_feed = nullptr;
	Ref<Image> image;
	int width = 0;
	int height = 0;

public:
	virtual void decode(StreamingBuffer p_buffer, int p_rotation = 0) = 0;

	BufferDecoder(CameraFeed *p_camera_feed);
	virtual ~BufferDecoder() {}

	void rotate_image(int p_rotation);
};

class AbstractYuyvBufferDecoder : public BufferDecoder {
protected:
	int *component_indexes = nullptr;

public:
	AbstractYuyvBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, int *p_component_indexes);
	~AbstractYuyvBufferDecoder();
};

class YuyvToGrayscaleBufferDecoder : public AbstractYuyvBufferDecoder {
private:
	PackedByteArray image_data;

public:
	YuyvToGrayscaleBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, int *p_component_indexes);
	virtual void decode(StreamingBuffer p_buffer, int p_rotation = 0) override;
};

class YuyvToRgbBufferDecoder : public AbstractYuyvBufferDecoder {
private:
	PackedByteArray image_data;

public:
	YuyvToRgbBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, int *p_component_indexes);
	virtual void decode(StreamingBuffer p_buffer, int p_rotation = 0) override;
};

class CopyBufferDecoder : public BufferDecoder {
private:
	PackedByteArray image_data;
	bool rgba = false;

public:
	CopyBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, bool p_rgba);
	virtual void decode(StreamingBuffer p_buffer, int p_rotation = 0) override;
};

class JpegBufferDecoder : public BufferDecoder {
private:
	PackedByteArray image_data;

public:
	JpegBufferDecoder(CameraFeed *p_camera_feed);
	virtual void decode(StreamingBuffer p_buffer, int p_rotation = 0) override;
};

#endif // BUFFER_DECODER_H
