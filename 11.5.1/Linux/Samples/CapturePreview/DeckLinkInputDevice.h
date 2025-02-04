/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
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
#include <functional>
#include <QString>

#include "DeckLinkAPI.h"
#include "com_ptr.h"
#include "CapturePreviewEvents.h"
#include "AncillaryDataTable.h"

class DeckLinkInputDevice : public IDeckLinkInputCallback
{
	using DisplayModeQueryFunc = std::function<void(com_ptr<IDeckLinkDisplayMode>&)>;

public:
	DeckLinkInputDevice(QObject* owner, com_ptr<IDeckLink>& deckLink);
	virtual ~DeckLinkInputDevice() = default;

	bool						Init();

	const QString&				getDeviceName() const { return m_deviceName; }
	bool						isCapturing() const { return m_currentlyCapturing; }
	bool						supportsFormatDetection() const { return m_supportsFormatDetection; }
	BMDVideoConnection			getVideoConnections() const { return (BMDVideoConnection) m_supportedInputConnections; }
	void						queryDisplayModes(DisplayModeQueryFunc func);

	bool						startCapture(BMDDisplayMode displayMode, IDeckLinkScreenPreviewCallback* screenPreviewCallback, bool applyDetectedInputMode);
	void						stopCapture(void);

	com_ptr<IDeckLink>					getDeckLinkInstance() const { return m_deckLink; }
	com_ptr<IDeckLinkInput>				getDeckLinkInput() const { return m_deckLinkInput; }
	com_ptr<IDeckLinkConfiguration>		getDeckLinkConfiguration() const { return m_deckLinkConfig; }
	com_ptr<IDeckLinkProfileManager>	getProfileManager() const { return m_deckLinkProfileManager; }

	// IUnknown interface
	HRESULT		QueryInterface (REFIID iid, LPVOID *ppv) override;
	ULONG		AddRef(void) override;
	ULONG		Release(void) override;

	// IDeckLinkInputCallback interface
	HRESULT		VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags) override;
	HRESULT		VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override;

private:
	QObject*							m_owner;
	std::atomic<ULONG>					m_refCount;
	//
	QString								m_deviceName;
	com_ptr<IDeckLink>					m_deckLink;
	com_ptr<IDeckLinkInput>				m_deckLinkInput;
	com_ptr<IDeckLinkConfiguration>		m_deckLinkConfig;
	com_ptr<IDeckLinkHDMIInputEDID>		m_deckLinkHDMIInputEDID;
	com_ptr<IDeckLinkProfileManager>	m_deckLinkProfileManager;

	bool								m_supportsFormatDetection;
	bool								m_currentlyCapturing;
	bool								m_applyDetectedInputMode;
	int64_t								m_supportedInputConnections;
	//
	static void	GetAncillaryDataFromFrame(IDeckLinkVideoInputFrame* frame, BMDTimecodeFormat format, QString* timecodeString, QString* userBitsString);
	static void	GetMetadataFromFrame(IDeckLinkVideoInputFrame* videoFrame, MetadataStruct* metadata);
};

class DeckLinkInputFormatChangedEvent : public QEvent
{
public:
	DeckLinkInputFormatChangedEvent(BMDDisplayMode displayMode);
	virtual ~DeckLinkInputFormatChangedEvent() {}

	BMDDisplayMode DisplayMode() const { return m_displayMode; }

private:
	BMDDisplayMode m_displayMode;
};

class DeckLinkInputFrameArrivedEvent : public QEvent
{
public:
	DeckLinkInputFrameArrivedEvent(AncillaryDataStruct* ancillaryData, MetadataStruct* metadata, bool signalValid);
	virtual ~DeckLinkInputFrameArrivedEvent() {}

	AncillaryDataStruct*	AncillaryData(void) const { return m_ancillaryData; }
	MetadataStruct*			Metadata(void) const { return m_metadata; }
	bool					SignalValid(void) const { return m_signalValid; }

private:
	AncillaryDataStruct*	m_ancillaryData;
	MetadataStruct*			m_metadata;
	bool					m_signalValid;
};

