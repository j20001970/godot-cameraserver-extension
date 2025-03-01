#include "camera_server_windows.h"

#include <mfapi.h>
#include <mfidl.h>

#include "camera_feed_windows.h"

CameraServerWindows::CameraServerWindows(CameraServerExtension *server) :
		extension::CameraServer(server) {
	IMFAttributes *attributes = nullptr;
	UINT32 count = 0;
	IMFActivate **devices = nullptr;
	ERR_FAIL_COND(FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
	ERR_FAIL_COND(FAILED(MFStartup(MF_VERSION)));
	if (FAILED(MFCreateAttributes(&attributes, 1))) {
		goto done;
	}
	if (FAILED(attributes->SetGUID(
				MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
				MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
		goto done;
	}
	if (FAILED(MFEnumDeviceSources(attributes, &devices, &count))) {
		goto done;
	}
	for (int i = 0; i < count; i++) {
		IMFActivate *device = devices[i];
		std::unique_ptr<CameraFeedWindows> feed_impl = std::make_unique<CameraFeedWindows>(device);
		Ref<CameraFeedExtension> feed = memnew(CameraFeedExtension(std::move(feed_impl)));
		this_->get_server()->add_feed(feed);
	}
done:
	attributes->Release();
}

CameraServerWindows::~CameraServerWindows() {}

bool CameraServerWindows::permission_granted() {
	LSTATUS result = ERROR_SUCCESS;
	bool granted = true;
	HKEY hkey = nullptr;
	WCHAR buffer[MAX_PATH];
	DWORD size = MAX_PATH;
	LPCWSTR subkey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\webcam";
	// Check "Allow access to the camera on this device"
	result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &hkey);
	if (result != ERROR_SUCCESS) {
		// Nothing stops us from accessing cameras if the registry does not exist.
		goto done;
	}
	result = RegGetValueW(hkey, nullptr, L"Value", RRF_RT_REG_SZ, nullptr, buffer, &size);
	if (result != ERROR_SUCCESS) {
		goto done;
	}
	if (String::utf16((const char16_t *)buffer, size) == "Deny") {
		granted = false;
		goto done;
	}
	RegCloseKey(hkey);
	hkey = nullptr;
	// Check "Allow apps to access your camera"
	result = RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_READ, &hkey);
	if (result != ERROR_SUCCESS) {
		goto done;
	}
	result = RegGetValueW(hkey, nullptr, L"Value", RRF_RT_REG_SZ, nullptr, buffer, &size);
	if (result != ERROR_SUCCESS) {
		goto done;
	}
	if (String::utf16((const char16_t *)buffer, size) == "Deny") {
		granted = false;
		goto done;
	}
	// Check "Allow desktop apps to access your camera"
	result = RegGetValueW(hkey, L"NonPackaged", L"Value", RRF_RT_REG_SZ, nullptr, buffer, &size);
	if (result != ERROR_SUCCESS) {
		goto done;
	}
	if (String::utf16((const char16_t *)buffer, size) == "Deny") {
		granted = false;
		goto done;
	}
done:
	if (hkey) {
		RegCloseKey(hkey);
	}
	return granted;
}

bool CameraServerWindows::request_permission() {
	if (permission_granted()) {
		return true;
	}
	return false;
}

void CameraServerExtension::set_impl() {
	impl = std::make_unique<CameraServerWindows>(this);
}
