#ifndef CAMERA_FEED_WINDOWS_H
#define CAMERA_FEED_WINDOWS_H

#include "camera_feed.h"

#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>

class CameraFeedWindows;

class SourceReaderCallback : public IMFSourceReaderCallback {
private:
	long ref_count = 1;
	CRITICAL_SECTION critsec = {};
	HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	bool eos = false;
	CameraFeedWindows *feed = nullptr;

public:
	SourceReaderCallback(CameraFeedWindows *p_feed) {
		feed = p_feed;
		InitializeCriticalSection(&critsec);
	}

	STDMETHODIMP QueryInterface(REFIID iid, void **ppv) {
		static const QITAB qit[] = {
			QITABENT(SourceReaderCallback, IMFSourceReaderCallback),
			{ 0 },
		};
		return QISearch(this, qit, iid, ppv);
	}

	STDMETHODIMP_(ULONG)
	AddRef() {
		return InterlockedIncrement(&ref_count);
	}

	STDMETHODIMP_(ULONG)
	Release() {
		ULONG count = InterlockedDecrement(&ref_count);
		if (count == 0) {
			delete this;
		}
		return count;
	}

	STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
			DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample);

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *) {
		return S_OK;
	}

	STDMETHODIMP OnFlush(DWORD) {
		return S_OK;
	}

	void Wait() {
		WaitForSingleObject(event, INFINITE);
	}

	void Stop() {
		EnterCriticalSection(&critsec);
		eos = true;
		LeaveCriticalSection(&critsec);
	}
};

class CameraFeedWindows : public extension::CameraFeed {
private:
	IMFActivate *device = nullptr;
	IMFMediaSource *source = nullptr;
	IMFPresentationDescriptor *presentation_descriptor = nullptr;
	IMFStreamDescriptor *stream_descriptor = nullptr;
	IMFMediaTypeHandler *media_type_handler = nullptr;
	IMFSourceReader *reader = nullptr;
	SourceReaderCallback *callback = nullptr;

public:
	CameraFeedWindows(CameraFeedExtension *feed);
	CameraFeedWindows(IMFActivate *p_device);
	~CameraFeedWindows();

	bool activate_feed() override;
	void deactivate_feed() override;

	TypedArray<Dictionary> get_formats() const override;
	bool set_format(int p_index, const Dictionary &p_parameters) override;

	void set_this(CameraFeedExtension *p_feed) override;

	friend class SourceReaderCallback;
};

#endif
