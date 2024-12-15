#include "camera_server_dummy.h"

CameraServerDummy::CameraServerDummy(CameraServerExtension *server) :
		extension::CameraServer(server) {}

CameraServerDummy::~CameraServerDummy() = default;

void CameraServerExtension::set_impl() {
	impl = std::make_unique<CameraServerDummy>(this);
}
