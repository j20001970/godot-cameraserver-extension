#include "camera_server_apple.h"

#include <AVFoundation/AVFoundation.h>
#include <memory>

#include "apple/camera_feed_apple.h"
#include "camera_feed.h"

@implementation DeviceNotification

- (instancetype)init:(CameraServerApple *)p_server {
	self = [super init];
	camera_server = p_server;
	[[NSNotificationCenter defaultCenter]
			addObserver:self
			   selector:@selector(devices_changed:)
				   name:AVCaptureDeviceWasConnectedNotification
				 object:nil];
	[[NSNotificationCenter defaultCenter]
			addObserver:self
			   selector:@selector(devices_changed:)
				   name:AVCaptureDeviceWasDisconnectedNotification
				 object:nil];
	return self;
}

- (void)dealloc {
	[[NSNotificationCenter defaultCenter]
			removeObserver:self
					  name:AVCaptureDeviceWasConnectedNotification
					object:nil];
	[[NSNotificationCenter defaultCenter]
			removeObserver:self
					  name:AVCaptureDeviceWasDisconnectedNotification
					object:nil];
	[super dealloc];
}

- (void)devices_changed:(NSNotification *)notification {
	camera_server->update_feeds();
}

@end

CameraServerApple::CameraServerApple(CameraServerExtension *p_server) :
		extension::CameraServer(p_server) {
	notification = [[DeviceNotification alloc] init:this];
	update_feeds();
}

CameraServerApple::~CameraServerApple() = default;

void CameraServerApple::update_feeds() {
	NSArray<AVCaptureDevice *> *devices;
	if (@available(macOS 10.15, iOS 10.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 100000
		NSArray<AVCaptureDeviceType> *deviceTypes;
		if (@available(macOS 14.0, iOS 17.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 170000
			deviceTypes = @[ AVCaptureDeviceTypeContinuityCamera, AVCaptureDeviceTypeBuiltInWideAngleCamera ];
#endif
		} else {
#if TARGET_OS_OSX
			deviceTypes = @[ AVCaptureDeviceTypeExternalUnknown, AVCaptureDeviceTypeBuiltInWideAngleCamera ];
#else
			deviceTypes = @[ AVCaptureDeviceTypeBuiltInWideAngleCamera ];
#endif
		}
		AVCaptureDeviceDiscoverySession *session =
				[AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:deviceTypes
																	   mediaType:AVMediaTypeVideo
																		position:AVCaptureDevicePositionUnspecified];
		devices = session.devices;
#else
	} else {
		devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#endif
	}

	TypedArray<CameraFeed> feeds = this_->get_server()->feeds();
	for (int i = feeds.size() - 1; i >= 0; i--) {
		Ref<CameraFeedExtension> feed = feeds[i];
		if (feed.is_null()) {
			continue;
		}
		CameraFeedApple *impl = (CameraFeedApple *)feed->get_impl();
		if (![devices containsObject:impl->get_device()]) {
			this_->get_server()->remove_feed(feed);
		};
	}
	for (AVCaptureDevice *device in devices) {
		bool new_device = true;
		for (int i = 0; i < feeds.size(); i++) {
			Ref<CameraFeedExtension> feed = feeds[i];
			if (feed.is_null()) {
				continue;
			}
			CameraFeedApple *impl = (CameraFeedApple *)feed->get_impl();
			if (device == impl->get_device()) {
				new_device = false;
				break;
			}
		}
		if (new_device) {
			std::unique_ptr<CameraFeedApple> feed_impl = std::make_unique<CameraFeedApple>(device);
			Ref<CameraFeedExtension> feed = memnew(CameraFeedExtension(std::move(feed_impl)));
			this_->get_server()->add_feed(feed);
		}
	}
}

bool CameraServerApple::permission_granted() {
	if (@available(macOS 10.14, iOS 7.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 70000
		return [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo] == AVAuthorizationStatusAuthorized;
#endif
	}
	return true;
}

bool CameraServerApple::request_permission() {
	if (@available(macOS 10.14, iOS 7.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 70000
		if (permission_granted()) {
			return true;
		}
		[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
								 completionHandler:^(BOOL granted) {
									 this_->emit_signal("permission_result", granted);
								 }];
		return false;
#endif
	}
	return true;
}

void CameraServerExtension::set_impl() {
	impl = std::make_unique<CameraServerApple>(this);
}
