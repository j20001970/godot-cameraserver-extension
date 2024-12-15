/**************************************************************************/
/*  buffer_decoder.cpp                                                    */
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

#include "buffer_decoder.h"

BufferDecoder::BufferDecoder(CameraFeed *p_camera_feed) {
	camera_feed = p_camera_feed;
	image.instantiate();
}

void BufferDecoder::rotate_image(int p_rotation) {
	if (p_rotation == 90) {
		image->rotate_90(ClockDirection::CLOCKWISE);
	} else if (p_rotation == 270) {
		image->rotate_90(ClockDirection::COUNTERCLOCKWISE);
	} else if (p_rotation == 180) {
		image->rotate_180();
	}
}

AbstractYuyvBufferDecoder::AbstractYuyvBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, int *p_component_indexes) :
		BufferDecoder(p_camera_feed) {
	width = p_width;
	height = p_height;
	component_indexes = p_component_indexes;
}

AbstractYuyvBufferDecoder::~AbstractYuyvBufferDecoder() {
	delete[] component_indexes;
}

YuyvToGrayscaleBufferDecoder::YuyvToGrayscaleBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, int *p_component_indexes) :
		AbstractYuyvBufferDecoder(p_camera_feed, p_width, p_height, p_component_indexes) {
	image_data.resize(width * height);
}

void YuyvToGrayscaleBufferDecoder::decode(StreamingBuffer p_buffer, int p_rotation) {
	uint8_t *dst = (uint8_t *)image_data.ptrw();
	uint8_t *src = (uint8_t *)p_buffer.start;
	uint8_t *y0_src = src + component_indexes[0];
	uint8_t *y1_src = src + component_indexes[1];

	for (int i = 0; i < width * height; i += 2) {
		*dst++ = *y0_src;
		*dst++ = *y1_src;

		y0_src += 4;
		y1_src += 4;
	}

	if (image.is_valid()) {
		image->set_data(width, height, false, Image::FORMAT_L8, image_data);
	} else {
		image.instantiate();
		image->set_data(width, height, false, Image::FORMAT_RGB8, image_data);
	}

	rotate_image(p_rotation);

	camera_feed->set_rgb_image(image);
}

YuyvToRgbBufferDecoder::YuyvToRgbBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, int *p_component_indexes) :
		AbstractYuyvBufferDecoder(p_camera_feed, p_width, p_height, p_component_indexes) {
	image_data.resize(width * height * 3);
}

void YuyvToRgbBufferDecoder::decode(StreamingBuffer p_buffer, int p_rotation) {
	uint8_t *src = (uint8_t *)p_buffer.start;
	uint8_t *y0_src = src + component_indexes[0];
	uint8_t *y1_src = src + component_indexes[1];
	uint8_t *u_src = src + component_indexes[2];
	uint8_t *v_src = src + component_indexes[3];
	uint8_t *dst = (uint8_t *)image_data.ptrw();

	for (int i = 0; i < width * height; i += 2) {
		int u = *u_src;
		int v = *v_src;
		int u1 = (((u - 128) << 7) + (u - 128)) >> 6;
		int rg = (((u - 128) << 1) + (u - 128) + ((v - 128) << 2) + ((v - 128) << 1)) >> 3;
		int v1 = (((v - 128) << 1) + (v - 128)) >> 1;

		*dst++ = CLAMP(*y0_src + v1, 0, 255);
		*dst++ = CLAMP(*y0_src - rg, 0, 255);
		*dst++ = CLAMP(*y0_src + u1, 0, 255);

		*dst++ = CLAMP(*y1_src + v1, 0, 255);
		*dst++ = CLAMP(*y1_src - rg, 0, 255);
		*dst++ = CLAMP(*y1_src + u1, 0, 255);

		y0_src += 4;
		y1_src += 4;
		u_src += 4;
		v_src += 4;
	}

	if (image.is_valid()) {
		image->set_data(width, height, false, Image::FORMAT_RGB8, image_data);
	} else {
		image.instantiate();
		image->set_data(width, height, false, Image::FORMAT_RGB8, image_data);
	}

	rotate_image(p_rotation);

	camera_feed->set_rgb_image(image);
}

CopyBufferDecoder::CopyBufferDecoder(CameraFeed *p_camera_feed, int p_width, int p_height, bool p_rgba) :
		BufferDecoder(p_camera_feed) {
	width = p_width;
	height = p_height;
	rgba = p_rgba;
	image_data.resize(width * height * (rgba ? 4 : 2));
}

void CopyBufferDecoder::decode(StreamingBuffer p_buffer, int p_rotation) {
	uint8_t *dst = (uint8_t *)image_data.ptrw();
	memcpy(dst, p_buffer.start, p_buffer.length);

	if (image.is_valid()) {
		image->set_data(width, height, false, rgba ? Image::FORMAT_RGBA8 : Image::FORMAT_LA8, image_data);
	} else {
		image.instantiate();
		image->set_data(width, height, false, rgba ? Image::FORMAT_RGBA8 : Image::FORMAT_LA8, image_data);
	}

	rotate_image(p_rotation);

	camera_feed->set_rgb_image(image);
}

JpegBufferDecoder::JpegBufferDecoder(CameraFeed *p_camera_feed) :
		BufferDecoder(p_camera_feed) {
}

void JpegBufferDecoder::decode(StreamingBuffer p_buffer, int p_rotation) {
	image_data.resize(p_buffer.length);
	uint8_t *dst = (uint8_t *)image_data.ptrw();
	memcpy(dst, p_buffer.start, p_buffer.length);
	if (image->load_jpg_from_buffer(image_data) == OK) {
		rotate_image(p_rotation);
		camera_feed->set_rgb_image(image);
	}
}
