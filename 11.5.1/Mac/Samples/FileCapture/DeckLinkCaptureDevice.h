/* -LICENSE-START-
 ** Copyright (c) 2018 Blackmagic Design
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
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include "DeckLinkDeviceState.h"
#include "DeckLinkMediaWriter.h"
#include "com_ptr.h"

static const intptr_t kInvalidDevice = 0;

class DeckLinkCaptureDevice;

typedef std::function<void(com_ptr<DeckLinkCaptureDevice>, DeviceError)> DeviceErrorOccurredFunc;
typedef std::function<void(com_ptr<DeckLinkCaptureDevice>, DeviceStatus)> DeviceStatusUpdateFunc;
typedef std::function<void(com_ptr<DeckLinkCaptureDevice>, DeviceManagerStatus)> DeviceManagerStatusUpdateFunc;
typedef std::function<void(com_ptr<IDeckLinkDisplayMode>&)> DeckLinkDisplayModeQueryFunc;

class DeckLinkCaptureDevice : public IDeckLinkInputCallback, public IDeckLinkNotificationCallback, public IDeckLinkProfileCallback
{
public:
	explicit DeckLinkCaptureDevice(com_ptr<IDeckLink>& deckLink);
	virtual ~DeckLinkCaptureDevice();

	// IUnknown
	HRESULT	QueryInterface(REFIID iid, LPVOID *ppv) override;
	ULONG	AddRef() override;
	ULONG	Release() override;

	// IDeckLinkInputCallback
	HRESULT	VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode* newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags) override;
	HRESULT	VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override;
	
	// IDeckLinkNotificationCallback
	virtual HRESULT Notify(BMDNotifications topic, uint64_t param1, uint64_t param2) override;

	// IDeckLinkProfileCallback
	HRESULT ProfileChanging(IDeckLinkProfile *profileToBeActivated, bool streamsWillBeForcedToStop) override;
	HRESULT ProfileActivated(IDeckLinkProfile *activatedProfile) override;
	
	// Initialization
	bool	init(std::shared_ptr<DeckLinkMediaWriter> mediaWriter);
	bool	update();

	// Device status
	CFStringRef		displayName() const;
	intptr_t		getDeviceID() const;
	DeviceIOState	getDeviceIOState() const;
	void			setErrorListener(const DeviceErrorOccurredFunc& func);
	void			setStatusListener(const DeviceStatusUpdateFunc& func);
	void			notifyStatus(DeviceStatus status);
	void			notifyError(DeviceError error);

	// Capture control
	void			capture(com_ptr<IDeckLinkScreenPreviewCallback> previewCallback, BMDDisplayMode displayMode);
	void			disable();
	
	// Input control
	void			setVideoInputConnection(int64_t connection);

	// Media file writer
	void			record(const std::string& filePath);
	void			stopRecording(bool save = true);

	// Device status
	BMDDisplayMode	activeDisplayMode() const;
	bool			formatAutoDetect() const;
	bool			supportsFormatAutoDetect() const;
	int64_t			videoInputConnections() const;
	int64_t			activeVideoInputConnection() const;
	void			queryDisplayModes(DeckLinkDisplayModeQueryFunc func);
	std::string		filePath() const;

	void			setState(DeviceIOState deviceState);
	void			setFormatAutoDetect(bool enabled);
	bool			isAvailable() const;

private:
	// Device control
	void enableVideoInput(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat, BMDVideoInputFlags flags);
	void disableVideoInput();
	void enableAudioInput(BMDAudioSampleRate sampleRate, BMDAudioSampleType sampleType, uint32_t channelCount);
	void disableAudioInput();

	// File IO
	bool convertVideoFrame(com_ptr<IDeckLinkVideoInputFrame> sourceFrame, com_ptr<IDeckLinkVideoFrame>& targetFrame);
	bool writeVideoFrame(com_ptr<IDeckLinkVideoInputFrame> deckLinkFrame, com_ptr<IDeckLinkAudioInputPacket> audioPacket);
	void checkWriteResult(WriteResult writeResult);

	std::atomic<ULONG>						m_refCount;
	bool									m_init;
	std::mutex								m_mutex;
	dispatch_queue_t						m_fileWriterQueue;
	DeviceStatusUpdateFunc					m_statusListener;
	DeviceErrorOccurredFunc					m_errorListener;
	CFStringRef								m_displayName;
	std::atomic<DeviceIOState>				m_deviceState;
	com_ptr<IDeckLink>						m_deckLink;
	com_ptr<IDeckLinkInput>					m_deckLinkInput;
	com_ptr<IDeckLinkConfiguration>			m_deckLinkConfiguration;
	com_ptr<IDeckLinkVideoConversion>		m_deckLinkVideoConversion;
	BMDDisplayMode							m_displayMode;
	BMDTimeValue							m_frameDuration;
	BMDTimeScale							m_timeScale;
	std::shared_ptr<DeckLinkMediaWriter>	m_mediaWriter;
	int64_t									m_videoInputConnections;
	int64_t									m_activeVideoInputConnection;
	bool									m_detectFormat;
	bool									m_supportsFormatDetection;
	bool									m_videoInputEnabled;
	bool									m_audioInputEnabled;
	std::atomic<bool>						m_isAvailable;
	DeviceStatus							m_lastValidFrameStatus;
};
