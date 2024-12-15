#ifndef CAMERA_SERVER_DUMMY_H
#define CAMERA_SERVER_DUMMY_H

#include "camera_server.h"

using namespace godot;

class CameraServerDummy : public extension::CameraServer {
public:
	CameraServerDummy(CameraServerExtension *server);
	~CameraServerDummy();
};

#endif
