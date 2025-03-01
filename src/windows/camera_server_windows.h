#ifndef CAMERA_SERVER_WINDOWS_H
#define CAMERA_SERVER_WINDOWS_H

#include "camera_server.h"

using namespace godot;

class CameraServerWindows : public extension::CameraServer {
public:
	CameraServerWindows(CameraServerExtension *server);
	~CameraServerWindows();

	bool request_permission() override;
	bool permission_granted() override;
};

#endif
