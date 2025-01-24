#ifndef CAMERA_SERVER_LINUX_H
#define CAMERA_SERVER_LINUX_H

#include "camera_server.h"

#include <gio/gio.h>
#include <pipewire/pipewire.h>

static void on_permission_callback(GDBusConnection *connection, const char *sender_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, void *user_data);
static void on_registry_event_global(void *user_data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props);
static void on_registry_event_global_remove(void *user_data, uint32_t id);

class CameraServerLinux : public extension::CameraServer {
private:
	static pw_thread_loop *loop;

	GDBusProxy *proxy = nullptr;
	pw_core *core = nullptr;
	pw_context *context = nullptr;
	pw_registry *registry = nullptr;
	spa_hook registry_listener = {};

	bool is_camera_present();
	void access_camera();
	int open_pipewire_remote();

	void pipewire_connect(int pw_fd);

public:
	static pw_thread_loop *get_loop();

	CameraServerLinux(CameraServerExtension *server);
	~CameraServerLinux();

	bool request_permission() override;
	bool permission_granted() override;

	friend void on_permission_callback(GDBusConnection *connection, const char *sender_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, void *user_data);
	friend void on_registry_event_global(void *user_data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props);
	friend void on_registry_event_global_remove(void *user_data, uint32_t id);
};

#endif
