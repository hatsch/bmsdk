/* -LICENSE-START-
** Copyright (c) 2020 Blackmagic Design
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

#include "stdafx.h"
#include "DeckLinkOutputDevice.h"

DeckLinkOutputDevice::DeckLinkOutputDevice(CComPtr<IDeckLink>& deckLink) : 
	m_deckLink(deckLink),
	m_deckLinkOutput(deckLink),
	m_deckLinkConfiguration(deckLink),
	m_videoPrerollSize(30),
	m_audioWaterLevel(48000),
	m_refCount(1)
{
}

bool DeckLinkOutputDevice::init()
{
	CComBSTR deviceNameBSTR;

	// Check output interface exists
	if (!m_deckLinkOutput)
		return false;

	// Get configuration interface
	if (!m_deckLinkConfiguration)
		return false;

	// Get device name
	if (m_deckLink->GetDisplayName(&deviceNameBSTR) == S_OK)
	{
		m_deviceName = CString(deviceNameBSTR);
	}
	else
	{
		m_deviceName = _T("DeckLink");
	}

	return true;
}

HRESULT	DeckLinkOutputDevice::QueryInterface(REFIID iid, LPVOID *ppv)
{
	HRESULT result = E_NOINTERFACE;

	if (!ppv)
		return E_INVALIDARG;

	// Initialise the return result
	*ppv = nullptr;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if (iid == IID_IUnknown)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (iid == IID_IDeckLinkVideoOutputCallback)
	{
		*ppv = static_cast<IDeckLinkVideoOutputCallback*>(this);
		AddRef();
		result = S_OK;
	}
	else if (iid == IID_IDeckLinkAudioOutputCallback)
	{
		*ppv = static_cast<IDeckLinkAudioOutputCallback*>(this);
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG DeckLinkOutputDevice::AddRef(void)
{
	return ++m_refCount;
}

ULONG DeckLinkOutputDevice::Release(void)
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

HRESULT	DeckLinkOutputDevice::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	// Notify subscriber that scheduled frame is completed, so it can schedule more frames
	if (m_scheduledFrameCompletedCallback)
		m_scheduledFrameCompletedCallback();

	return S_OK;
}

HRESULT	DeckLinkOutputDevice::ScheduledPlaybackHasStopped()
{
	// Notify subscriber that playback has stopped, so it can disable output
	if (m_scheduledPlaybackStoppedCallback)
		m_scheduledPlaybackStoppedCallback();

	return S_OK;
}

HRESULT	DeckLinkOutputDevice::RenderAudioSamples(BOOL preroll)
{
	unsigned int bufferedAudioSampleCount;

	// Check audio water level and request further samples if required
	if (m_deckLinkOutput->GetBufferedAudioSampleFrameCount(&bufferedAudioSampleCount) != S_OK)
	{
		if (m_errorListener)
			m_errorListener(OutputDeviceError::GetBufferedAudioSampleCountFailed);
		return E_FAIL;
	}

	if (m_renderAudioSamplesCallback && (bufferedAudioSampleCount < m_audioWaterLevel))
		// Notify subscriber to provide more audio samples to reach water level
		m_renderAudioSamplesCallback(m_audioWaterLevel - bufferedAudioSampleCount);

	if (preroll)
	{
		// Ensure that both audio and video preroll have sufficient samples, then commence scheduled playback
		unsigned int bufferedVideoFrameCount;

		if (m_deckLinkOutput->GetBufferedVideoFrameCount(&bufferedVideoFrameCount) != S_OK)
		{
			if (m_errorListener)
				m_errorListener(OutputDeviceError::GetBufferedVideoFrameCountFailed);
			return E_FAIL;
		}

		if ((bufferedAudioSampleCount >= m_audioWaterLevel) && (bufferedVideoFrameCount >= m_videoPrerollSize))
		{
			// Start audio and video output
			m_deckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
		}
	}

	return S_OK;
}

void DeckLinkOutputDevice::queryDisplayModes(QueryDisplayModeFunc func)
{
	CComPtr<IDeckLinkDisplayModeIterator>	displayModeIterator;
	CComPtr<IDeckLinkDisplayMode>			displayMode;

	if (!func)
		return;

	if (m_deckLinkOutput->GetDisplayModeIterator(&displayModeIterator) != S_OK)
		return;

	while (displayModeIterator->Next(&displayMode) == S_OK)
	{
		func(displayMode);
		displayMode.Release();
	}
}
