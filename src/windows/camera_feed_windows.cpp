#include "camera_feed_windows.h"

#include <mfapi.h>

#include "godot_cpp/classes/image.hpp"

HRESULT SourceReaderCallback::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
		DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample) {
	DWORD size = 0;
	UINT32 width, height;
	IMFMediaType *media_type = nullptr;
	IMFMediaBuffer *output_buffer = nullptr;
	BYTE *buffer = nullptr;
	PackedByteArray data;
	Ref<Image> image;
	EnterCriticalSection(&critsec);
	if (FAILED(hrStatus) || eos) {
		eos = false;
		SetEvent(event);
		goto done;
	}
	if (!feed || !feed->reader) {
		goto done;
	}
	feed->reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr);
	if (dwStreamFlags != 0) {
		goto done;
	}
	if (FAILED(feed->reader->GetCurrentMediaType(0, &media_type))) {
		goto done;
	}
	if (FAILED(MFGetAttributeSize(media_type, MF_MT_FRAME_SIZE, &width, &height))) {
		goto done;
	}
	if (FAILED(pSample->ConvertToContiguousBuffer(&output_buffer))) {
		goto done;
	}
	if (FAILED(output_buffer->Lock(&buffer, nullptr, &size))) {
		goto done;
	}
	data.resize(size);
	memcpy(data.ptrw(), buffer, size);
	if (FAILED(output_buffer->Unlock())) {
		goto done;
	}
	// MFVideoFormat_RGB32 is in fact BGRA for memory layout.
	for (uint8_t *b = data.ptrw(); b < data.ptrw() + data.size(); b += 4) {
		uint8_t *r = b + 2;
		std::swap(*r, *b);
	}
	image.instantiate();
	image->set_data(width, height, false, Image::FORMAT_RGBA8, data);
	image->convert(Image::FORMAT_RGB8);
	feed->this_->set_rgb_image(image);
	feed->this_->call_deferred("emit_signal", "frame_changed");
done:
	if (output_buffer) {
		output_buffer->Release();
	}
	if (media_type) {
		media_type->Release();
	}
	LeaveCriticalSection(&critsec);
	return S_OK;
}

CameraFeedWindows::CameraFeedWindows(CameraFeedExtension *feed) :
		extension::CameraFeed(feed) {}

CameraFeedWindows::CameraFeedWindows(IMFActivate *p_device) :
		device(p_device) {
	BOOL selected;
	callback = new SourceReaderCallback(this);
	if (FAILED(device->ActivateObject(IID_PPV_ARGS(&source)))) {
		return;
	}
	ERR_FAIL_COND(FAILED(source->CreatePresentationDescriptor(&presentation_descriptor)));
	// Assuming only one stream descriptor for webcam device.
	ERR_FAIL_COND(FAILED(presentation_descriptor->GetStreamDescriptorByIndex(0, &selected, &stream_descriptor)));
	ERR_FAIL_COND(FAILED(stream_descriptor->GetMediaTypeHandler(&media_type_handler)));
}

CameraFeedWindows::~CameraFeedWindows() {
	if (this_->is_active()) {
		deactivate_feed();
	}
	if (callback) {
		callback->Release();
	}
	if (reader) {
		reader->Release();
	}
	if (media_type_handler) {
		media_type_handler->Release();
	}
	if (stream_descriptor) {
		stream_descriptor->Release();
	}
	if (presentation_descriptor) {
		presentation_descriptor->Release();
	}
	if (source) {
		source->Stop();
		source->Shutdown();
		source->Release();
	}
	if (device) {
		device->ShutdownObject();
		device->Release();
	}
}

bool CameraFeedWindows::activate_feed() {
	bool result = false;
	IMFAttributes *attributes = nullptr;
	IMFMediaType *media_type = nullptr;
	IMFMediaType *output_type = nullptr;
	if (reader) {
		return true;
	}
	ERR_FAIL_NULL_V(source, false);
	ERR_FAIL_NULL_V(media_type_handler, false);
	ERR_FAIL_NULL_V(callback, false);
	if (selected_format != -1) {
		ERR_FAIL_COND_V(FAILED(media_type_handler->GetMediaTypeByIndex(selected_format, &media_type)), false);
		if (FAILED(media_type_handler->SetCurrentMediaType(media_type))) {
			ERR_PRINT("Failed to set current media type.");
			goto done;
		}
	} else {
		ERR_FAIL_COND_V(FAILED(media_type_handler->GetCurrentMediaType(&media_type)), false);
	}
	if (FAILED(MFCreateMediaType(&output_type))) {
		ERR_PRINT("Failed to create output media type.");
		goto done;
	}
	if (FAILED(media_type->CopyAllItems(output_type))) {
		ERR_PRINT("Failed to copy media type.");
		goto done;
	}
	if (FAILED(output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32))) {
		ERR_PRINT("Failed to set video format.");
		goto done;
	}
	if (FAILED(MFCreateAttributes(&attributes, 3))) {
		ERR_PRINT("Failed to create attributes.");
		goto done;
	}
	if (FAILED(attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback))) {
		ERR_PRINT("Failed to set callback.");
		goto done;
	}
	if (FAILED(attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE))) {
		ERR_PRINT("Failed to enable video processing.");
		goto done;
	}
	if (FAILED(attributes->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, TRUE))) {
		ERR_PRINT("Failed to set manual release media source.");
		goto done;
	}
	if (FAILED(MFCreateSourceReaderFromMediaSource(source, attributes, &reader))) {
		ERR_PRINT("Failed to create source reader.");
		goto done;
	}
	if (FAILED(reader->SetCurrentMediaType(0, nullptr, output_type))) {
		ERR_PRINT("Failed to set current media type for source reader.");
		goto done;
	}
	if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr))) {
		ERR_PRINT("Failed to read sample.");
		goto done;
	}
	result = true;
done:
	if (!result) {
		reader->Release();
		reader = nullptr;
	}
	if (output_type) {
		output_type->Release();
	}
	if (media_type) {
		media_type->Release();
	}
	if (attributes) {
		attributes->Release();
	}
	return result;
}

void CameraFeedWindows::deactivate_feed() {
	if (reader == nullptr) {
		return;
	}
	if (callback) {
		callback->Stop();
		callback->Wait();
	}
	if (source) {
		source->Pause();
	}
	reader->Release();
	reader = nullptr;
}

TypedArray<Dictionary> CameraFeedWindows::get_formats() const {
	TypedArray<Dictionary> formats;
	IMFMediaType *type = nullptr;
	DWORD count = 0;
	if (!media_type_handler) {
		return formats;
	}
	ERR_FAIL_COND_V(FAILED(media_type_handler->GetMediaTypeCount(&count)), formats);
	for (int i = 0; i < count; i++) {
		Dictionary format;
		uint32_t width = 0;
		uint32_t height = 0;
		GUID subtype;
		UINT32 numerator;
		UINT32 denominator;
		ERR_CONTINUE(FAILED(media_type_handler->GetMediaTypeByIndex(i, &type)));
		if (SUCCEEDED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height))) {
			format["width"] = width;
			format["height"] = height;
		}
		if (SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE, &subtype))) {
			format["format"] =
					String::chr((subtype.Data1 >> 0) & 0xFF) +
					String::chr((subtype.Data1 >> 8) & 0xFF) +
					String::chr((subtype.Data1 >> 16) & 0xFF) +
					String::chr((subtype.Data1 >> 24) & 0xFF);
		}
		if (SUCCEEDED(MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator, &denominator))) {
			format["framerate_numerator"] = numerator;
			format["framerate_denominator"] = denominator;
		}
		formats.append(format);
		type->Release();
		type = nullptr;
	}
	return formats;
}

bool CameraFeedWindows::set_format(int p_index, const Dictionary &p_parameters) {
	if (p_index == -1) {
		selected_format = p_index;
		this_->emit_signal("format_changed");
		return true;
	}
	TypedArray<Dictionary> formats = get_formats();
	ERR_FAIL_INDEX_V(p_index, formats.size(), false);
	Dictionary format = formats[p_index];
	selected_format = p_index;
	this_->emit_signal("format_changed");
	return true;
}

void CameraFeedWindows::set_this(CameraFeedExtension *p_feed) {
	this_ = p_feed;
	WCHAR *name;
	UINT32 length = 0;
	ERR_FAIL_NULL(device);
	ERR_FAIL_COND(FAILED(
			device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &length)));
	String str = String(name);
	this_->set_name(name);
	CoTaskMemFree(name);
}

CameraFeedExtension::CameraFeedExtension() {
	impl = std::make_unique<CameraFeedWindows>(this);
}
