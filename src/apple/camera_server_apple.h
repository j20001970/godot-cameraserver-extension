#ifndef CAMERA_SERVER_APPLE_H
#define CAMERA_SERVER_APPLE_H

#include "camera_server.h"

#include <Foundation/Foundation.h>

using namespace godot;

class CameraServerApple;

@interface DeviceNotification : NSObject {
	CameraServerApple *camera_server;
}
@end

class CameraServerApple : public extension::CameraServer {
private:
	DeviceNotification *notification;

public:
	CameraServerApple(CameraServerExtension *p_server);
	~CameraServerApple();

    void update_feeds();

	bool permission_granted() override;
	bool request_permission() override;
};

#endif
