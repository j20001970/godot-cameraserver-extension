#include "camera_server_linux.h"

#include <fcntl.h>
#include <memory>

#include <spa/utils/keys.h>

#include "camera_feed_linux.h"
#include "portal.h"

pw_thread_loop *CameraServerLinux::loop = nullptr;

static void on_permission_callback(GDBusConnection *connection, const char *sender_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, void *user_data) {
	CameraServerLinux *server = (CameraServerLinux *)(user_data);
	g_autoptr(GVariant) result = nullptr;
	uint32_t response;
	g_variant_get(parameters, "(u@a{sv})", &response, &result);
	bool granted = response == 0;
	if (granted) {
		server->pipewire_connect();
	}
	server->this_->emit_signal("permission_result", granted);
}

static void on_registry_event_global(void *user_data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props) {
	CameraServerLinux *server = (CameraServerLinux *)user_data;
	if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
		return;
	}
	const char *media_class = spa_dict_lookup(props, SPA_KEY_MEDIA_CLASS);
	const char *media_role = spa_dict_lookup(props, SPA_KEY_MEDIA_ROLE);
	if (media_class == nullptr || media_role == nullptr) {
		return;
	}
	if (strcmp(media_class, "Video/Source") && strcmp(media_role, "Camera")) {
		return;
	}
	TypedArray<CameraFeed> feeds = server->this_->get_server()->feeds();
	for (int i = 0; i < feeds.size(); i++) {
		Ref<CameraFeedExtension> feed = feeds[i];
		if (feed.is_null()) {
			continue;
		}
		CameraFeedLinux *impl = (CameraFeedLinux *)feed->get_impl();
		if (impl->get_object_id() == id) {
			return;
		}
	}
	struct pw_properties *feed_props = pw_properties_new_dict(props);
	struct pw_properties *stream_props = pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Video",
			PW_KEY_MEDIA_CATEGORY, "Capture",
			PW_KEY_MEDIA_ROLE, "Camera",
			PW_KEY_TARGET_OBJECT, pw_properties_get(feed_props, PW_KEY_NODE_NAME),
			NULL);
	pw_stream *stream = pw_stream_new(server->core, "", stream_props);
	struct pw_proxy *feed_proxy = (struct pw_proxy *)pw_registry_bind(server->registry, id, type, version, 0);
	const char *feed_name = pw_properties_get(feed_props, PW_KEY_NODE_DESCRIPTION);
	std::unique_ptr<CameraFeedLinux> feed_impl = std::make_unique<CameraFeedLinux>(id, feed_name, feed_proxy, stream);
	Ref<CameraFeedExtension> feed = memnew(CameraFeedExtension(std::move(feed_impl)));
	server->this_->get_server()->add_feed(feed);
	pw_properties_free(feed_props);
}

static void on_registry_event_global_remove(void *user_data, uint32_t id) {
	CameraServerLinux *server = (CameraServerLinux *)user_data;
	TypedArray<CameraFeed> feeds = server->this_->get_server()->feeds();
	for (int i = 0; i < feeds.size(); i++) {
		Ref<CameraFeedExtension> feed = feeds[i];
		if (feed.is_null()) {
			continue;
		}
		CameraFeedLinux *impl = (CameraFeedLinux *)feed->get_impl();
		if (impl->get_object_id() == id) {
			server->this_->get_server()->remove_feed(feed);
			break;
		}
	}
}

static const struct pw_registry_events registry_events = {
	.version = PW_VERSION_REGISTRY_EVENTS,
	.global = on_registry_event_global,
	.global_remove = on_registry_event_global_remove,
};

pw_thread_loop *CameraServerLinux::get_loop() {
	return loop;
}

CameraServerLinux::CameraServerLinux(CameraServerExtension *server) :
		extension::CameraServer(server) {
	g_autoptr(GError) err = nullptr;
	proxy = g_dbus_proxy_new_sync(
			DesktopPortal::get_dbus_session(),
			G_DBUS_PROXY_FLAGS_NONE, nullptr,
			"org.freedesktop.portal.Desktop",
			"/org/freedesktop/portal/desktop",
			"org.freedesktop.portal.Camera",
			nullptr, &err);
	pw_init(nullptr, nullptr);
	pipewire_connect();
}

CameraServerLinux::~CameraServerLinux() {
	if (loop) {
		pw_thread_loop_lock(loop);
	}
	if (registry) {
		pw_proxy_destroy((pw_proxy *)registry);
	}
	if (core) {
		pw_core_disconnect(core);
	}
	if (context) {
		pw_context_destroy(context);
	}
	if (loop) {
		pw_thread_loop_unlock(loop);
		pw_thread_loop_destroy(loop);
		loop = nullptr;
	}
	pw_deinit();
}

bool CameraServerLinux::is_camera_present() {
	g_autoptr(GVariant) result = nullptr;
	if (proxy == nullptr) {
		return false;
	}
	result = g_dbus_proxy_get_cached_property(proxy, "IsCameraPresent");
	return g_variant_get_boolean(result);
}

void CameraServerLinux::access_camera() {
	g_autoptr(GError) err = nullptr;
	GVariantBuilder builder;
	GVariant *parameters;
	if (proxy == nullptr) {
		return;
	}
	String token = DesktopPortal::get_request_token();
	DesktopPortal::portal_signal_subscribe(token, on_permission_callback, this);
	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string(token.utf8()));
	parameters = g_variant_new("(a{sv})", &builder);
	g_dbus_proxy_call_sync(proxy, "AccessCamera", parameters, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &err);
	if (err) {
		ERR_PRINT(err->message);
	}
}

int CameraServerLinux::open_pipewire_remote() {
	GVariantBuilder builder;
	GVariant *parameters;
	g_autoptr(GError) err = nullptr;
	g_autoptr(GVariant) result = nullptr;
	g_autoptr(GUnixFDList) fd_list = nullptr;
	int fd_index;
	int pw_fd;
	if (proxy == nullptr) {
		return -1;
	}
	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
	parameters = g_variant_new("(a{sv})", &builder);
	result = g_dbus_proxy_call_with_unix_fd_list_sync(
			proxy, "OpenPipeWireRemote", parameters, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &fd_list, nullptr, &err);
	if (err) {
		return -1;
	}
	g_variant_get(result, "(h)", &fd_index, &err);
	pw_fd = g_unix_fd_list_get(fd_list, fd_index, &err);
	return pw_fd;
}

void CameraServerLinux::pipewire_connect() {
	if (loop == nullptr) {
		loop = pw_thread_loop_new("", nullptr);
		ERR_FAIL_NULL(loop);
	}
	if (context == nullptr) {
		context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
		ERR_FAIL_NULL(context);
	}
	ERR_FAIL_COND(pw_thread_loop_start(loop) < 0);
	pw_thread_loop_lock(loop);
	if (core == nullptr) {
		core = pw_context_connect(context, nullptr, 0);
	}
	if (core == nullptr) {
		int fd = open_pipewire_remote();
		if (fd != -1) {
			core = pw_context_connect_fd(context, fcntl(fd, F_DUPFD_CLOEXEC, 5), nullptr, 0);
		}
	}
	if (core && registry == nullptr) {
		registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
		if (registry) {
			pw_registry_add_listener(registry, &registry_listener, &registry_events, this);
		}
	}
	pw_thread_loop_unlock(loop);
}

bool CameraServerLinux::request_permission() {
	if (open_pipewire_remote() != -1) {
		return true;
	}
	access_camera();
	return false;
}

bool CameraServerLinux::permission_granted() {
	if (core) {
		return true;
	}
	return open_pipewire_remote() != -1;
}

void CameraServerExtension::set_impl() {
	impl = std::make_unique<CameraServerLinux>(this);
}
