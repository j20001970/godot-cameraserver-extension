#include "portal.h"

#include "godot_cpp/classes/crypto.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/variant.hpp"

GDBusConnection *DesktopPortal::get_dbus_session() {
	g_autoptr(GError) err = nullptr;
	if (session == nullptr) {
		session = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
		if (err) {
			ERR_PRINT(err->message);
		}
	}
	return session;
}

String DesktopPortal::get_request_token() {
	Ref<Crypto> rng = memnew(Crypto);
	PackedByteArray uuid = rng->generate_random_bytes(64);
	String token = uuid.hex_encode();
	return token;
}

void DesktopPortal::portal_signal_subscribe(const String &token, GDBusSignalCallback callback, gpointer user_data) {
	GMainContext *ctx;
	if (loop == nullptr) {
		ctx = g_main_context_new();
		loop = g_main_loop_new(ctx, false);
		monitor_thread = std::make_unique<std::thread>(g_main_loop_run, loop);
	}
	ctx = g_main_loop_get_context(loop);
	g_main_context_push_thread_default(ctx);
	GDBusConnection *session = get_dbus_session();
	String sender = String(g_dbus_connection_get_unique_name(session)).replace(".", "_").replace(":", "");
	String object_path = vformat("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
	g_dbus_connection_signal_subscribe(
			DesktopPortal::get_dbus_session(),
			"org.freedesktop.portal.Desktop",
			"org.freedesktop.portal.Request",
			"Response", object_path.utf8(), nullptr,
			G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
			callback, user_data, nullptr);
	g_main_context_pop_thread_default(ctx);
}

std::unique_ptr<std::thread> DesktopPortal::monitor_thread = nullptr;
GMainLoop *DesktopPortal::loop = nullptr;
GDBusConnection *DesktopPortal::session = nullptr;
