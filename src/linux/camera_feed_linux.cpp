#include "camera_feed_linux.h"

#include <pipewire/pipewire.h>
#include <spa/debug/types.h>
#include <spa/param/video/format-utils.h>

#include "godot_cpp/classes/image.hpp"

#include "camera_server_linux.h"

static void on_node_info(void *data, const struct pw_node_info *info) {
	CameraFeedLinux *feed = (CameraFeedLinux *)data;

	if (info == nullptr || !(info->change_mask & PW_NODE_CHANGE_MASK_PARAMS)) {
		return;
	}

	for (int i = 0; i < info->n_params; i++) {
		spa_param_info *param = &info->params[i];
		if (!(param->flags & SPA_PARAM_INFO_READ)) {
			continue;
		}
		pw_node_enum_params(feed->node, ++param->seq, param->id, 0, -1, nullptr);
	}
}

static void on_node_param(void *data, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod *param) {
	uint32_t media_type, media_subtype, format;
	spa_rectangle resolution = {};
	const spa_pod_prop *framerate_prop;
	const spa_pod *framerate_pod;
	uint32_t n_framerates, framerate_choice;
	const spa_fraction *framerate_values;
	CameraFeedLinux *feed = (CameraFeedLinux *)data;

	if (id != SPA_PARAM_EnumFormat || param == nullptr) {
		return;
	}
	if (spa_format_parse(param, &media_type, &media_subtype) < 0) {
		return;
	}
	if (media_type != SPA_MEDIA_TYPE_video) {
		return;
	}

	if (media_subtype == SPA_MEDIA_SUBTYPE_raw) {
		spa_video_info_raw info = {};
		spa_format_video_raw_parse(param, &info);
		switch (info.format) {
			case SPA_VIDEO_FORMAT_YUY2:
			case SPA_VIDEO_FORMAT_YVYU:
			case SPA_VIDEO_FORMAT_UYVY:
			case SPA_VIDEO_FORMAT_VYUY:
				format = info.format;
				resolution = info.size;
				break;
			default:
				return;
		}
	} else {
		return;
	}

	framerate_prop = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_framerate);
	if (!framerate_prop) {
		return;
	}
	framerate_pod = spa_pod_get_values(&framerate_prop->value, &n_framerates, &framerate_choice);
	if (framerate_pod->type != SPA_TYPE_Fraction) {
		return;
	}
	framerate_values = (spa_fraction *)SPA_POD_BODY(framerate_pod);
	if (framerate_choice == SPA_CHOICE_Enum) {
		// Index 0 is the default.
		for (int i = 1; i < n_framerates; i++) {
			feed->add_format(media_subtype, format, resolution, framerate_values[i]);
		}
	}
}

static void on_proxy_destroy(void *data) {
	CameraFeedLinux *feed = (CameraFeedLinux *)data;
	feed->proxy = nullptr;
	feed->node = nullptr;
}

static void on_stream_destroy(void *data) {
	CameraFeedLinux *feed = (CameraFeedLinux *)data;
	feed->stream = nullptr;
}

static void on_stream_process(void *data) {
	pw_buffer *b = nullptr;
	spa_buffer *buf = nullptr;
	CameraFeedLinux *feed = (CameraFeedLinux *)data;
	pw_stream *stream = feed->stream;

	if (stream == nullptr) {
		return;
	}

	while (true) {
		pw_buffer *t;
		if ((t = pw_stream_dequeue_buffer(stream)) == nullptr) {
			break;
		}
		if (b) {
			pw_stream_queue_buffer(stream, b);
		}
		b = t;
	}
	if (b == nullptr) {
		WARN_PRINT("Out of buffer.");
		return;
	}

	buf = b->buffer;
	feed->buffer->start = buf->datas[0].data;
	feed->buffer->length = buf->datas[0].chunk->size;
	feed->decoder->decode(*feed->buffer);
	feed->this_->emit_signal("frame_changed");
}

static const struct pw_node_events node_events = {
	.version = PW_VERSION_NODE_EVENTS,
	.info = on_node_info,
	.param = on_node_param,
};

static const struct pw_proxy_events proxy_events = {
	.version = PW_VERSION_PROXY_EVENTS,
	.destroy = on_proxy_destroy,
};

static const struct pw_stream_events stream_events = {
	.version = PW_VERSION_STREAM_EVENTS,
	.destroy = on_stream_destroy,
	.process = on_stream_process,
};

CameraFeedLinux::CameraFeedLinux(CameraFeedExtension *feed) :
		extension::CameraFeed(feed) {}

CameraFeedLinux::CameraFeedLinux(uint32_t id, const char *name, pw_proxy *proxy, pw_stream *stream) :
		id(id), name(name), proxy(proxy), stream(stream) {
	buffer = new StreamingBuffer();
	node = (pw_node *)proxy;
	pw_node_add_listener(node, &node_listener, &node_events, this);
	pw_proxy_add_listener(proxy, &proxy_listener, &proxy_events, this);
	pw_stream_add_listener(stream, &stream_listener, &stream_events, this);
}

CameraFeedLinux::~CameraFeedLinux() {
	if (this_->is_active()) {
		deactivate_feed();
	}
	pw_thread_loop *loop = CameraServerLinux::get_loop();
	if (loop == nullptr) {
		return;
	}
	pw_thread_loop_lock(loop);
	if (stream) {
		pw_stream_destroy(stream);
	}
	if (proxy) {
		pw_proxy_destroy(proxy);
	}
	pw_thread_loop_unlock(loop);
	delete buffer;
}

void CameraFeedLinux::add_format(const uint32_t media_subtype, const uint32_t format, const spa_rectangle resolution, const spa_fraction framerate) {
	FeedFormat feed_format = {};
	feed_format.media_subtype = media_subtype;
	feed_format.format = format;
	feed_format.resolution = resolution;
	feed_format.framerate = framerate;
	formats.push_back(feed_format);
}

void CameraFeedLinux::set_this(CameraFeedExtension *feed) {
	this_ = feed;
	this_->set_name(name);
}

uint32_t CameraFeedLinux::get_object_id() { return id; }

bool CameraFeedLinux::activate_feed() {
	int result = 0;
	ERR_FAIL_COND_V_MSG(selected_format == -1, false, "CameraFeed format needs to be set before activating.");

	pw_thread_loop *loop = CameraServerLinux::get_loop();
	if (loop == nullptr) {
		return false;
	}
	if (stream == nullptr) {
		return false;
	}

	FeedFormat feed_format = formats[selected_format];
	uint32_t media_subtype = feed_format.media_subtype;
	uint32_t format = feed_format.format;
	spa_rectangle resolution = feed_format.resolution;
	spa_fraction framerate = feed_format.framerate;

	int *indexes;
	if (media_subtype == SPA_MEDIA_SUBTYPE_raw) {
		switch (format) {
			case SPA_VIDEO_FORMAT_YUY2:
				indexes = new int[4]{ 0, 2, 1, 3 };
				decoder = memnew(YuyvToRgbBufferDecoder(this_, resolution.width, resolution.height, indexes));
				break;
			case SPA_VIDEO_FORMAT_YVYU:
				indexes = new int[4]{ 0, 2, 3, 1 };
				decoder = memnew(YuyvToRgbBufferDecoder(this_, resolution.width, resolution.height, indexes));
				break;
			case SPA_VIDEO_FORMAT_UYVY:
				indexes = new int[4]{ 1, 3, 0, 2 };
				decoder = memnew(YuyvToRgbBufferDecoder(this_, resolution.width, resolution.height, indexes));
				break;
			case SPA_VIDEO_FORMAT_VYUY:
				indexes = new int[4]{ 1, 3, 2, 0 };
				decoder = memnew(YuyvToRgbBufferDecoder(this_, resolution.width, resolution.height, indexes));
				break;
			default:
				return false;
		}
	} else {
		return false;
	}

	pw_thread_loop_lock(loop);
	while (true) {
		pw_stream_flags stream_flags = pw_stream_flags(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
		result = pw_stream_connect(stream, PW_DIRECTION_INPUT, PW_ID_ANY, stream_flags, nullptr, 0);
		if (result < 0) {
			break;
		}
		const struct spa_pod *param[1];
		uint8_t buffer[1024];
		struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
		param[0] = (spa_pod *)spa_pod_builder_add_object(&builder,
				SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
				SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
				SPA_FORMAT_mediaSubtype, SPA_POD_Id(media_subtype),
				SPA_FORMAT_VIDEO_format, SPA_POD_Id(format),
				SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&resolution),
				SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&framerate));
		result = pw_stream_update_params(stream, param, 1);
		break;
	}
	pw_thread_loop_unlock(loop);
	return result == 0;
}

void CameraFeedLinux::deactivate_feed() {
	pw_thread_loop *loop = CameraServerLinux::get_loop();
	if (loop == nullptr) {
		return;
	}
	pw_thread_loop_lock(loop);
	if (stream) {
		pw_stream_disconnect(stream);
	}
	memdelete(decoder);
	pw_thread_loop_unlock(loop);
}

TypedArray<Dictionary> CameraFeedLinux::get_formats() const {
	TypedArray<Dictionary> result;
	for (const FeedFormat &format : formats) {
		Dictionary dictionary;
		if (format.media_subtype == SPA_MEDIA_SUBTYPE_raw) {
			dictionary["format"] = spa_debug_type_find_short_name(spa_type_video_format, format.format);
		} else {
			dictionary["format"] = spa_debug_type_find_short_name(spa_type_media_subtype, format.media_subtype);
		}
		dictionary["width"] = format.resolution.width;
		dictionary["height"] = format.resolution.height;
		dictionary["frame_numerator"] = format.framerate.denom;
		dictionary["frame_denominator"] = format.framerate.num;
		result.push_back(dictionary);
	}
	return result;
}

bool CameraFeedLinux::set_format(int p_index, const Dictionary &p_parameters) {
	ERR_FAIL_COND_V_MSG(this_->is_active(), false, "Feed is active.");
	ERR_FAIL_INDEX_V_MSG(p_index, formats.size(), false, "Invalid format index.");

	selected_format = p_index;
	this_->emit_signal("format_changed");
	return true;
}

CameraFeedExtension::CameraFeedExtension() {
	impl = std::make_unique<CameraFeedLinux>(this);
}
