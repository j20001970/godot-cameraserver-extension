#ifndef CAMERA_FEED_APPLE_H
#define CAMERA_FEED_APPLE_H

#include "camera_feed.h"

#include <AVFoundation/AVFoundation.h>
#include <Foundation/Foundation.h>
#include <objc/NSObject.h>

#include "godot_cpp/classes/image.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/typed_array.hpp"

using namespace godot;

@interface OutputDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic) CameraFeed *feed;
@end

class CameraFeedApple : public extension::CameraFeed {
private:
	bool deviceLocked = false;
	AVCaptureDevice *device = nil;
	AVCaptureDeviceInput *input = nil;
	AVCaptureVideoDataOutput *output = nil;
	AVCaptureSession *session = nil;
	OutputDelegate *delegate = nil;

public:
	CameraFeedApple(CameraFeedExtension *feed);
	CameraFeedApple(AVCaptureDevice *p_device);
	~CameraFeedApple();

	AVCaptureDevice *get_device() const;

	bool activate_feed() override;
	void deactivate_feed() override;

	TypedArray<Dictionary> get_formats() const override;
	bool set_format(int p_index, const Dictionary &p_parameters) override;

	//// Debugging method - comment out when not needed
	//Dictionary get_current_format() const override;

	void set_this(CameraFeedExtension *p_feed) override;
};

#endif
