#include "camera_feed_dummy.h"

CameraFeedDummy::CameraFeedDummy(CameraFeedExtension *feed) :
		extension::CameraFeed(feed) {}

CameraFeedDummy::~CameraFeedDummy() = default;

CameraFeedExtension::CameraFeedExtension() {
	impl = std::make_unique<CameraFeedDummy>(this);
}
