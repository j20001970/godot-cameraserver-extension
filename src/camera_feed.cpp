#include "camera_feed.h"

#include "godot_cpp/core/class_db.hpp"

namespace extension {
CameraFeed::CameraFeed() :
		this_(nullptr) {}

CameraFeed::CameraFeed(CameraFeedExtension *feed) :
		this_(feed) {}

CameraFeed::~CameraFeed() = default;

void CameraFeed::set_this(CameraFeedExtension *feed) { this_ = feed; }

bool CameraFeed::set_format(int p_index, const Dictionary &p_parameters) { return false; }

TypedArray<Dictionary> CameraFeed::get_formats() const { return TypedArray<Dictionary>(); }

bool CameraFeed::activate_feed() { return true; }

void CameraFeed::deactivate_feed() {}
} // namespace extension

void CameraFeedExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_format", "index", "parameters"), &CameraFeedExtension::set_format);
	ClassDB::bind_method(D_METHOD("get_formats"), &CameraFeedExtension::get_formats);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "formats"), "", "get_formats");
}

CameraFeedExtension::CameraFeedExtension(std::unique_ptr<extension::CameraFeed> impl) {
	impl->set_this(this);
	this->impl = std::move(impl);
}

CameraFeedExtension::~CameraFeedExtension() = default;

bool CameraFeedExtension::set_format(int p_index, const Dictionary &p_parameters) { return impl->set_format(p_index, p_parameters); }

TypedArray<Dictionary> CameraFeedExtension::get_formats() const { return impl->get_formats(); }

bool CameraFeedExtension::_activate_feed() { return impl->activate_feed(); }

void CameraFeedExtension::_deactivate_feed() { impl->deactivate_feed(); }

extension::CameraFeed *CameraFeedExtension::get_impl() { return impl.get(); }
