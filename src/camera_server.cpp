#include "camera_server.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/core/class_db.hpp"

#include "camera_feed.h"

namespace extension {
CameraServer::CameraServer(CameraServerExtension *server) :
		this_(server) {}

CameraServer::~CameraServer() = default;

bool CameraServer::request_permission() { return true; }

bool CameraServer::permission_granted() { return true; }
} // namespace extension

CameraServerExtension *CameraServerExtension::singleton = nullptr;

void CameraServerExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("request_permission"), &CameraServerExtension::request_permission);
	ClassDB::bind_method(D_METHOD("permission_granted"), &CameraServerExtension::permission_granted);
	ADD_SIGNAL(MethodInfo("permission_result", PropertyInfo(Variant::BOOL, "granted")));
}

CameraServerExtension::CameraServerExtension() {
	if (singleton == nullptr) {
		singleton = this;
		server = CameraServer::get_singleton();
		ERR_FAIL_COND(server == nullptr);
		set_impl();
	}
}

CameraServerExtension::~CameraServerExtension() {
	if (singleton == this) {
		TypedArray<CameraFeed> feeds = server->feeds();
		for (int i = feeds.size() - 1; i >= 0; i--) {
			Ref<CameraFeedExtension> feed = feeds[i];
			if (feed.is_null()) {
				continue;
			}
			feed->set_active(false);
			server->remove_feed(feed);
		}
		impl = nullptr;
		singleton = nullptr;
	}
}

bool CameraServerExtension::request_permission() { return singleton->impl->request_permission(); }

bool CameraServerExtension::permission_granted() { return singleton->impl->permission_granted(); }

CameraServer *CameraServerExtension::get_server() const { return singleton->server; }
