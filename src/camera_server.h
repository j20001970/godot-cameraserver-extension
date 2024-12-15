#ifndef CAMERA_SERVER_H
#define CAMERA_SERVER_H

#include <memory>

#include "godot_cpp/classes/camera_server.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"

using namespace godot;

class CameraServerExtension;

namespace extension {
class CameraServer {
protected:
	CameraServerExtension *this_;

public:
	CameraServer(CameraServerExtension *server);
	virtual ~CameraServer();

	virtual bool request_permission();
	virtual bool permission_granted();
};
} // namespace extension

class CameraServerExtension : public RefCounted {
	GDCLASS(CameraServerExtension, RefCounted)

private:
	static CameraServerExtension *singleton;

	CameraServer *server;
	std::unique_ptr<extension::CameraServer> impl;

	void set_impl();

protected:
	static void _bind_methods();

public:
	CameraServerExtension();
	~CameraServerExtension();

	bool request_permission();
	bool permission_granted();

	CameraServer *get_server() const;
};

#endif
