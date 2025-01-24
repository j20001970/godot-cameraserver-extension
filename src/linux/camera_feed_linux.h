#ifndef CAMERA_FEED_LINUX_H
#define CAMERA_FEED_LINUX_H

#include "camera_feed.h"

#include <pipewire/pipewire.h>

#include "buffer_decoder.h"

static void on_node_info(void *data, const struct pw_node_info *info);
static void on_node_param(void *data, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod *param);
static void on_proxy_destroy(void *data);
static void on_stream_destroy(void *data);
static void on_stream_process(void *data);

class CameraFeedLinux : public extension::CameraFeed {
private:
	struct FeedFormat {
		uint32_t media_subtype;
		uint32_t format;
		spa_rectangle resolution;
		spa_fraction framerate;
	};

	uint32_t id = -1;
	const char *name = "";
	pw_proxy *proxy = nullptr;
	pw_node *node = nullptr;
	pw_stream *stream = nullptr;
	spa_hook node_listener = {};
	spa_hook proxy_listener = {};
	spa_hook stream_listener = {};
	Vector<FeedFormat> formats;
	BufferDecoder *decoder = nullptr;
	StreamingBuffer *buffer = nullptr;


	void add_format(const uint32_t media_subtype, const uint32_t format, const spa_rectangle resolution, const spa_fraction framerate);

	void set_this(CameraFeedExtension *feed) override;

public:
	CameraFeedLinux(CameraFeedExtension *feed);
	CameraFeedLinux(uint32_t id, const char *name, pw_proxy *proxy, pw_stream *stream);
	~CameraFeedLinux();

	uint32_t get_object_id();

	bool activate_feed() override;
	void deactivate_feed() override;

	TypedArray<Dictionary> get_formats() const override;
	bool set_format(int p_index, const Dictionary &p_parameters) override;

	friend void on_node_info(void *data, const struct pw_node_info *info);
	friend void on_node_param(void *data, int seq, uint32_t id, uint32_t index, uint32_t next, const struct spa_pod *param);
	friend void on_proxy_destroy(void *data);
	friend void on_stream_destroy(void *data);
	friend void on_stream_process(void *data);
};

#endif
