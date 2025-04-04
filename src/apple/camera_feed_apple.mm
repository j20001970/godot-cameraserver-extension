#include "camera_feed_apple.h"

@implementation OutputDelegate

- (instancetype)init:(godot::CameraFeed *)p_feed {
	self = [super init];
	self.feed = p_feed;
	return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
		didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
			   fromConnection:(AVCaptureConnection *)connection {
	PackedByteArray data;
	Ref<Image> image;
	CVPixelBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
	ERR_FAIL_COND(CVPixelBufferGetPixelFormatType(imageBuffer) != kCVPixelFormatType_24RGB);
	CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
	uint8_t *baseAddress = (uint8_t *)CVPixelBufferGetBaseAddress(imageBuffer);
	size_t width = CVPixelBufferGetWidth(imageBuffer);
	size_t height = CVPixelBufferGetHeight(imageBuffer);
	size_t size = CVPixelBufferGetDataSize(imageBuffer);
	data.resize(size);
	memcpy(data.ptrw(), baseAddress, size);
	CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
	image.instantiate();
	image->set_data(width, height, false, Image::FORMAT_RGB8, data);
	self.feed->set_rgb_image(image);
	self.feed->emit_signal("frame_changed");
}

@end

CameraFeedApple::CameraFeedApple(CameraFeedExtension *p_feed) :
		extension::CameraFeed(p_feed) {}

CameraFeedApple::CameraFeedApple(AVCaptureDevice *p_device) :
		device(p_device) {}

CameraFeedApple::~CameraFeedApple() {
	if (this_->is_active()) {
		deactivate_feed();
	}
	if (delegate) {
		[delegate dealloc];
		delegate = nil;
	}
}

AVCaptureDevice *CameraFeedApple::get_device() const {
	return device;
}

void CameraFeedApple::set_this(CameraFeedExtension *p_feed) {
	this_ = p_feed;
	delegate = [[OutputDelegate alloc] init:this_];
	this_->set_name(godot::String::utf8(device.localizedName.UTF8String));
	godot::CameraFeed::FeedPosition position = godot::CameraFeed::FeedPosition::FEED_UNSPECIFIED;
	if (device.position == AVCaptureDevicePositionFront) {
		position = godot::CameraFeed::FeedPosition::FEED_FRONT;
	} else if (device.position == AVCaptureDevicePositionBack) {
		position = godot::CameraFeed::FeedPosition::FEED_BACK;
	}
	this_->set_position(position);
}

bool CameraFeedApple::activate_feed() {
	NSError *err;
	if (session && [session isRunning]) {
		return true;
	}
	ERR_FAIL_COND_V(delegate == nil, false);
	if (@available(macOS 10.14, iOS 7.0, *)) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 70000
		AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
		if (status == AVAuthorizationStatusNotDetermined) {
			[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
									 completionHandler:^(BOOL granted) {
										 if (granted) {
											 activate_feed();
										 } else {
											 deactivate_feed();
										 }
									 }];
			return true;
		} else if (status != AVAuthorizationStatusAuthorized) {
			return false;
		}
#endif
	}
	if (selected_format != -1) {
		ERR_FAIL_INDEX_V(selected_format, device.formats.count, false);
		deviceLocked = [device lockForConfiguration:&err];
		ERR_FAIL_COND_V_MSG(!deviceLocked, false, err.localizedFailureReason.UTF8String);
		[device setActiveFormat:device.formats[selected_format]];
	}
	input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&err];
	ERR_FAIL_COND_V_MSG(!input, false, err.localizedFailureReason.UTF8String);
	output = [AVCaptureVideoDataOutput new];
	[output setAlwaysDiscardsLateVideoFrames:YES];
	// CameraFeed does not seem to display YUV image data correctly so we have to do conversion here.
	[output setVideoSettings:@{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_24RGB)}];
	[output setSampleBufferDelegate:delegate queue:dispatch_get_main_queue()];
	session = [[AVCaptureSession alloc] init];
	[session beginConfiguration];
	[session addInput:input];
	[session addOutput:output];
	[session commitConfiguration];
	[session startRunning];
	return true;
}

void CameraFeedApple::deactivate_feed() {
	if (session && [session isRunning]) {
		[session removeInput:input];
		input = nil;
		[session removeOutput:output];
		output = nil;
		[session stopRunning];
		session = nil;
	}
	if (deviceLocked) {
		[device unlockForConfiguration];
		deviceLocked = false;
	}
}

TypedArray<Dictionary> CameraFeedApple::get_formats() const {
	TypedArray<Dictionary> formats;
	for (AVCaptureDeviceFormat *format in device.formats) {
		Dictionary dictionary;
		CMFormatDescriptionRef formatDescription = format.formatDescription;
		FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(formatDescription);
		dictionary["subType"] =
				String::chr((char)(fourcc >> 24) & 0xFF) +
				String::chr((char)(fourcc >> 16) & 0xFF) +
				String::chr((char)(fourcc >> 8) & 0xFF) +
				String::chr((char)(fourcc >> 0) & 0xFF);
		CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(formatDescription);
		dictionary["width"] = dimension.width;
		dictionary["height"] = dimension.height;
		TypedArray<Dictionary> framerates;
		for (AVFrameRateRange *framerate in format.videoSupportedFrameRateRanges) {
			Dictionary dictionary;
			dictionary["minFrameRate"] = framerate.minFrameRate;
			dictionary["maxFrameRate"] = framerate.maxFrameRate;
			framerates.append(dictionary);
		}
		dictionary["framerates"] = framerates;
		formats.append(dictionary);
	}
	return formats;
}

bool CameraFeedApple::set_format(int p_index, const Dictionary &p_parameters) {
	if (p_index == -1) {
		selected_format = p_index;
		this_->emit_signal("format_changed");
		return true;
	}
	ERR_FAIL_INDEX_V(p_index, device.formats.count, false);
	ERR_FAIL_COND_V(this_->is_active(), false);
	selected_format = p_index;
	this_->emit_signal("format_changed");
	return true;
}

CameraFeedExtension::CameraFeedExtension() {
	impl = std::make_unique<CameraFeedApple>(this);
}
