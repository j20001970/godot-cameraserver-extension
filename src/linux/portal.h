#ifndef LINUX_DESKTOP_PORTAL_H
#define LINUX_DESKTOP_PORTAL_H

#include <memory>
#include <thread>

#include <glib.h>
#include <gio/gio.h>

#include "godot_cpp/variant/string.hpp"

using namespace godot;

class DesktopPortal {
	private:
		static std::unique_ptr<std::thread> monitor_thread;
		static GMainLoop *loop;
		static GDBusConnection *session;

	public:
		static GDBusConnection *get_dbus_session();
		static String get_request_token();
		static void portal_signal_subscribe(const String &token, GDBusSignalCallback callback, gpointer user_data);
};

#endif
