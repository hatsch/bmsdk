/* -LICENSE-START-
** Copyright (c) 2013 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
** 
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#include <atomic>
#include <functional>
#include <vector>
#include "DeckLinkAPI_h.h"

#pragma once

enum class DeviceError
{
	NoError = 0,
	EnableVideoInputFailed,
	StartStreamsFailed,
	ReenableVideoInputFailed,
};

class DeckLinkDevice : public IDeckLinkInputCallback
{
	using QueryDisplayModeFunc = std::function<void(CComPtr<IDeckLinkDisplayMode>&)>;
	using DeviceErrorOccuredFunc = std::function<void(DeviceError)>;
	using VideoFormatChangedCallback = std::function<void(BMDDisplayMode)>;
	using VideoFrameArrivedCallback = std::function<void(CComPtr<IDeckLinkVideoInputFrame>&)>;

public:
	DeckLinkDevice(CComPtr<IDeckLink>& device);
	virtual ~DeckLinkDevice() = default;

	bool								init();
	const CString&						getDeviceName() { return m_deviceName; };
	bool								isCapturing() { return m_currentlyCapturing; };
	bool								doesSupportFormatDetection() { return (m_supportsFormatDetection == TRUE); };
	bool								startCapture(BMDDisplayMode displayMode, IDeckLinkScreenPreviewCallback* screenPreviewCallback, bool applyDetectedInputMode);
	void								stopCapture();
	CComPtr<IDeckLink>					getDeckLinkInstance() { return m_deckLink; }
	CComPtr<IDeckLinkInput>				getDeckLinkInput() { return m_deckLinkInput; };
	CComPtr<IDeckLinkConfiguration>		getDeckLinkConfiguration() { return m_deckLinkConfig; };

	void								queryDisplayModes(QueryDisplayModeFunc func);
	void								setErrorListener(const DeviceErrorOccuredFunc& func) { m_errorListener = func; }
	void								onVideoFormatChange(const VideoFormatChangedCallback& callback) { m_videoFormatChangedCallback = callback; }
	void								onVideoFrameArrival(const VideoFrameArrivedCallback& callback) { m_videoFrameArrivedCallback = callback; }

	// IUnknown interface
	HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv) override;
	ULONG	STDMETHODCALLTYPE	AddRef() override;
	ULONG	STDMETHODCALLTYPE	Release() override;

	// IDeckLinkInputCallback interface
	HRESULT STDMETHODCALLTYPE	VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode* newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags) override;
	HRESULT STDMETHODCALLTYPE	VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override;

private:
	std::atomic<ULONG>					m_refCount;
	//
	CString								m_deviceName;
	CComPtr<IDeckLink>					m_deckLink;
	CComQIPtr<IDeckLinkInput>			m_deckLinkInput;
	CComQIPtr<IDeckLinkConfiguration>	m_deckLinkConfig;
	CComQIPtr<IDeckLinkHDMIInputEDID>	m_deckLinkHDMIInputEDID;

	BOOL								m_supportsFormatDetection;
	bool								m_currentlyCapturing;
	bool								m_applyDetectedInputMode;

	DeviceErrorOccuredFunc				m_errorListener;
	VideoFormatChangedCallback			m_videoFormatChangedCallback;
	VideoFrameArrivedCallback			m_videoFrameArrivedCallback;
};
