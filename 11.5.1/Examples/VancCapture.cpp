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

#include <atomic>
#include <map>
#include <vector>
#include "platform.h"

 // Video mode parameters
const BMDDisplayMode			kDisplayMode = bmdModeHD1080i50;
const BMDPixelFormat			kPixelFormat = bmdFormat10BitYUV;
const BMDAncillaryPacketFormat	kAncillaryFormat = bmdAncillaryPacketFormatUInt8;

struct AncillaryPacket
{
	INT8_UNSIGNED				m_DID;
	INT8_UNSIGNED				m_SDID;
	std::vector<INT8_UNSIGNED>	m_data;
	bool operator!=(const AncillaryPacket& other) const
	{
		return std::tie(m_DID, m_SDID, m_data) != std::tie(other.m_DID, other.m_SDID, other.m_data);
	}
};

// The input callback class
class InputCallback : public IDeckLinkInputCallback
{
public:
	InputCallback(IDeckLinkInput *deckLinkInput) :
		m_deckLinkInput(deckLinkInput),
		m_refCount(1)
	{
	}

	// IDeckLinkInputCallback interface
	HRESULT		STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags) override;
	HRESULT		STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override;

	// IUnknown interface
	HRESULT		STDMETHODCALLTYPE QueryInterface (REFIID iid, LPVOID *ppv) override
	{
		HRESULT result = S_OK;

		if (ppv == nullptr)
			return E_INVALIDARG;

		// Obtain the IUnknown interface and compare it the provided REFIID
		if (iid == IID_IUnknown)
		{
			*ppv = static_cast<IUnknown*>(this);
			AddRef();
		}
		else if (iid == IID_IDeckLinkInputCallback)
		{
			*ppv = static_cast<IDeckLinkInputCallback*>(this);
			AddRef();
		}
		else
		{
			*ppv = nullptr;
			result = E_NOINTERFACE;
		}

		return result;
	}

	ULONG		STDMETHODCALLTYPE AddRef() override
	{
		return ++m_refCount;
	}

	ULONG		STDMETHODCALLTYPE Release() override
	{
		ULONG newRefValue = --m_refCount;

		if (newRefValue == 0)
			delete this;

		return newRefValue;
	}

private:
	IDeckLinkInput*								m_deckLinkInput;
	std::map<INT32_UNSIGNED, AncillaryPacket>	m_prevAncillaryPackets;
	std::atomic<ULONG>							m_refCount;
};

HRESULT InputCallback::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
	return S_OK;
}

HRESULT InputCallback::VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket)
{
	std::map<INT32_UNSIGNED, AncillaryPacket>	capturedAncillaryPackets;
	IDeckLinkVideoFrameAncillaryPackets*		videoFrameAncillaryPackets	= nullptr;
	IDeckLinkAncillaryPacketIterator*			ancillaryPacketIterator		= nullptr;
	IDeckLinkAncillaryPacket*					ancillaryPacket				= nullptr;
	HRESULT										result;

	if (!videoFrame || (videoFrame->GetFlags() & bmdFrameHasNoInputSource))
		return S_OK;

	result = videoFrame->QueryInterface(IID_IDeckLinkVideoFrameAncillaryPackets, (void**)&videoFrameAncillaryPackets);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkVideoFrameAncillaryPackets interface - result = %08x\n", result);
		goto bail;
	}

	if (videoFrameAncillaryPackets->GetPacketIterator(&ancillaryPacketIterator) != S_OK)
	{
		fprintf(stderr, "Could not get ancillary packet iterator\n");
		goto bail;
	}

	while (ancillaryPacketIterator->Next(&ancillaryPacket) == S_OK)
	{
		INT32_UNSIGNED	ancillaryBufferSize;
		INT8_UNSIGNED*	ancillaryBufferPtr;

		if (ancillaryPacket->GetBytes(kAncillaryFormat, (const void**)&ancillaryBufferPtr, &ancillaryBufferSize) == S_OK)
		{
			std::vector<INT8_UNSIGNED> ancillaryBuffer(ancillaryBufferPtr, ancillaryBufferPtr + ancillaryBufferSize);
			capturedAncillaryPackets[ancillaryPacket->GetLineNumber()] = { ancillaryPacket->GetDID(), ancillaryPacket->GetSDID(), std::move(ancillaryBuffer) };
		}

		ancillaryPacket->Release();
	}

	// Check any VANC lines that no longer have data
	for (auto& ancillaryPacketIter : m_prevAncillaryPackets)
	{
		auto vancLineSearch = capturedAncillaryPackets.find(ancillaryPacketIter.first);
		if (vancLineSearch == capturedAncillaryPackets.end())
		{
			printf("Line %d:\t<empty>\n", ancillaryPacketIter.first);
		}
	}

	// Check VANC lines that have new or modified data 
	for (auto& ancillaryPacketIter : capturedAncillaryPackets)
	{
		auto vancLineSearch = m_prevAncillaryPackets.find(ancillaryPacketIter.first);
		if ((vancLineSearch == m_prevAncillaryPackets.end()) || (vancLineSearch->second != ancillaryPacketIter.second))
		{
			printf("Line %d:\t", ancillaryPacketIter.first);
			printf("DID: %02x; ", ancillaryPacketIter.second.m_DID);
			printf("SDID: %02x; ", ancillaryPacketIter.second.m_SDID);
			printf("Data:");
			for (int i = 0; i < (int)ancillaryPacketIter.second.m_data.size(); i++)
				printf(" %02x", ancillaryPacketIter.second.m_data[i]);
			printf("\n");
		}
	}

	m_prevAncillaryPackets = std::move(capturedAncillaryPackets);

bail:
	if (ancillaryPacketIterator != nullptr)
		ancillaryPacketIterator->Release();

	if (videoFrameAncillaryPackets != nullptr)
		videoFrameAncillaryPackets->Release();

	return S_OK;
}

int		main (int argc, char** argv)
{
	IDeckLinkIterator*	deckLinkIterator		= nullptr;
	IDeckLink*			deckLink				= nullptr;
	IDeckLinkInput*		deckLinkInput			= nullptr;
	InputCallback*		deckLinkInputCallback	= nullptr;

	HRESULT				result;
	INT8_UNSIGNED		returnCode = 1;
	
	Initialize();

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	if (GetDeckLinkIterator(&deckLinkIterator) != S_OK)
	{
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		goto bail;
	}

	// Obtain the first DeckLink device
	result = deckLinkIterator->Next(&deckLink);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not find DeckLink device - result = %08x\n", result);
		goto bail;
	}

	// Obtain the input interface for the DeckLink device
	result = deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&deckLinkInput);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkInput interface - result = %08x\n", result);
		goto bail;
	}

	// Create an instance of DeckLink Input callback
	deckLinkInputCallback = new InputCallback(deckLinkInput);
	if (deckLinkInputCallback == nullptr)
	{
		fprintf(stderr, "Could not create DeckLink input callback object\n");
		goto bail;
	}

	// Set the callback object to the DeckLink device's input interface
	result = deckLinkInput->SetCallback(deckLinkInputCallback);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not set callback - result = %08x\n", result);
		goto bail;
	}

	// Enable video input with a default video mode and the automatic format detection feature enabled
	result = deckLinkInput->EnableVideoInput(kDisplayMode, kPixelFormat, bmdVideoInputFlagDefault);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not enable video input - result = %08x\n", result);
		goto bail;
	}

	// Start capture
	result = deckLinkInput->StartStreams();
	if (result != S_OK)
	{
		fprintf(stderr, "Could not start capture - result = %08x\n", result);
		goto bail;
	}

	printf("Capturing... Press <RETURN> to exit\n");

	getchar();

	printf("Exiting.\n");

	// Stop capture
	result = deckLinkInput->StopStreams();

	// Disable the video input interface
	result = deckLinkInput->DisableVideoInput();

	// return success
	returnCode = 0;

	// Release resources
bail:
	// Release the video input interface
	if (deckLinkInput != nullptr)
		deckLinkInput->Release();

	// Release the Decklink object
	if (deckLink != nullptr)
		deckLink->Release();

	// Release the DeckLink iterator
	if (deckLinkIterator != nullptr)
		deckLinkIterator->Release();

	// Release the DeckLink Input callback object
	if (deckLinkInputCallback != nullptr)
		deckLinkInputCallback->Release();
 
	return returnCode;
}
