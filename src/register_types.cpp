#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "camera_feed.h"
#include "camera_server.h"

using namespace godot;

void initialize_camera_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<CameraFeedExtension>();
		ClassDB::register_class<CameraServerExtension>();
	}
}

void uninitialize_camera_module(ModuleInitializationLevel p_level) {
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT camera_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_camera_module);
	init_obj.register_terminator(uninitialize_camera_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
