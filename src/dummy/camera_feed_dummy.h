#ifndef CAMERA_FEED_DUMMY_H
#define CAMERA_FEED_DUMMY_H

#include "camera_feed.h"

class CameraFeedDummy : public extension::CameraFeed {
public:
	CameraFeedDummy(CameraFeedExtension *feed);
	~CameraFeedDummy();
};

#endif
