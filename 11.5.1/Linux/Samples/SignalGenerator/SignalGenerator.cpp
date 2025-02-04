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

#include "SignalGenerator.h"
#include "SignalGeneratorEvents.h"
#include "DeckLinkOutputDevice.h"
#include "DeckLinkDeviceDiscovery.h"
#include "DeckLinkOpenGLWidget.h"
#include "ProfileCallback.h"

#include <map>
#include <math.h>
#include <stdio.h>

const uint32_t		kAudioWaterlevel = 48000;

// SD 75% Colour Bars
static uint32_t gSD75pcColourBars[8] =
{
	0xeb80eb80, 0xa28ea22c, 0x832c839c, 0x703a7048,
	0x54c654b8, 0x41d44164, 0x237223d4, 0x10801080
};

// HD 75% Colour Bars
static uint32_t gHD75pcColourBars[8] =
{
	0xeb80eb80, 0xa888a82c, 0x912c9193, 0x8534853f,
	0x3fcc3fc1, 0x33d4336d, 0x1c781cd4, 0x10801080
};

// Audio channels supported
static const int gAudioChannels[] = { 2, 8, 16 };

// Supported pixel formats map to string representation and boolean if RGB format
static const std::map<BMDPixelFormat, std::pair<QString, bool>> kPixelFormats =
{
	std::make_pair(bmdFormat8BitYUV,	std::make_pair(QString("8-bit YUV"), false)),
	std::make_pair(bmdFormat10BitYUV,	std::make_pair(QString("10-bit YUV"), false)),
	std::make_pair(bmdFormat8BitARGB,	std::make_pair(QString("8-bit RGB"), true)),
	std::make_pair(bmdFormat10BitRGB,	std::make_pair(QString("10-bit RGB"), true)),
};

SignalGenerator::SignalGenerator() : QDialog(),
	running(false),
	selectedDisplayMode(bmdModeUnknown),
	selectedPixelFormat(bmdFormatUnspecified),
	audioBuffer(nullptr),
	scheduledPlaybackStopped(false)
{
	ui = new Ui::SignalGeneratorDialog();
	ui->setupUi(this);

	layout = new QGridLayout(ui->previewContainer);
	layout->setMargin(0);

	previewView = new DeckLinkOpenGLWidget(this);
	previewView->resize(ui->previewContainer->size());
	previewView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(previewView, 0, 0, 0, 0);
	previewView->clear();

	ui->outputSignalPopup->addItem("Pip", QVariant::fromValue((int)kOutputSignalPip));
	ui->outputSignalPopup->addItem("Dropout", QVariant::fromValue((int)kOutputSignalDrop));
	
	ui->audioSampleDepthPopup->addItem("16", QVariant::fromValue(16));
	ui->audioSampleDepthPopup->addItem("32", QVariant::fromValue(32));

	connect(ui->startButton, &QPushButton::clicked, this, &SignalGenerator::toggleStart);
	connect(ui->videoFormatPopup, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SignalGenerator::videoFormatChanged);
	connect(ui->outputDevicePopup, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SignalGenerator::outputDeviceChanged);
	enableInterface(false);
	show();
}

void SignalGenerator::setup()
{
	//
	// Create and initialise DeckLink device discovery object
	deckLinkDiscovery = make_com_ptr<DeckLinkDeviceDiscovery>(this);
	if (deckLinkDiscovery)
	{
		if (!deckLinkDiscovery->enable())
		{
			QMessageBox::critical(this, "This application requires the DeckLink drivers installed.", "Please install the Blackmagic DeckLink drivers to use the features of this application.");
		}
	}

	profileCallback = make_com_ptr<ProfileCallback>(this);
	if (profileCallback)
		profileCallback->onProfileChanging(std::bind(&SignalGenerator::haltStreams, this));
}

void SignalGenerator::customEvent(QEvent *event)
{
	if (event->type() == kAddDeviceEvent)
	{
		DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		com_ptr<IDeckLink> deckLink(discoveryEvent->deckLink());
		addDevice(deckLink);
	}

	else if (event->type() == kRemoveDeviceEvent)
	{
		DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		com_ptr<IDeckLink> deckLink(discoveryEvent->deckLink());
		removeDevice(deckLink);
	}

	else if (event->type() == kProfileActivatedEvent)
	{
		ProfileActivatedEvent* profileEvent = dynamic_cast<ProfileActivatedEvent*>(event);
		com_ptr<IDeckLinkProfile> deckLinkProfile(profileEvent->deckLinkProfile());
		updateProfile(deckLinkProfile);
	}
}

void SignalGenerator::closeEvent(QCloseEvent *)
{
	if (running)
		stopRunning();

	if (selectedDevice)
	{
		if (selectedDevice->getProfileManager())
			selectedDevice->getProfileManager()->SetCallback(nullptr);

		selectedDevice->getDeviceOutput()->SetScreenPreviewCallback(nullptr);
	}

	if (deckLinkDiscovery)
		deckLinkDiscovery->disable();
}

void SignalGenerator::enableInterface(bool enable)
{
	// Set the enable state of user interface elements
	for (auto& combobox : ui->groupBox->findChildren<QComboBox*>())
	{
		combobox->setEnabled(enable);
	}
}

void SignalGenerator::toggleStart()
{
	if (running == false)
		startRunning();
	else
		stopRunning();
}

void SignalGenerator::refreshDisplayModeMenu(void)
{
	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	ui->videoFormatPopup->clear();

	selectedDevice->queryDisplayModes([this](com_ptr<IDeckLinkDisplayMode>& displayMode)
	{
		const char*		modeName;

		if (displayMode->GetName(&modeName) == S_OK)
		{
			ui->videoFormatPopup->addItem(QString(modeName), QVariant::fromValue((uint64_t)displayMode->GetDisplayMode()));
			free((void*)modeName);
		}
	});

	ui->videoFormatPopup->setCurrentIndex(0);
	ui->startButton->setEnabled(ui->videoFormatPopup->count() != 0);
}

void SignalGenerator::refreshPixelFormatMenu(void)
{
	// Populate the pixel format mode combo with a list of pixel formats supported by the installed DeckLink card
	ui->pixelFormatPopup->clear();

	for (auto& pixelFormat : kPixelFormats)
	{
		bool supported = false;
		QString pixelFormatString;

		std::tie(pixelFormatString, std::ignore) = pixelFormat.second;

		HRESULT hr = selectedDevice->getDeviceOutput()->DoesSupportVideoMode(bmdVideoConnectionUnspecified, selectedDisplayMode, 
								pixelFormat.first, bmdNoVideoOutputConversion, bmdSupportedVideoModeDefault, nullptr, &supported);
		if (hr != S_OK || ! supported)
			continue;

		ui->pixelFormatPopup->addItem(pixelFormatString, QVariant::fromValue((unsigned int)pixelFormat.first));
	}

	ui->pixelFormatPopup->setCurrentIndex(0);
}

void SignalGenerator::refreshAudioChannelMenu(void)
{
	com_ptr<IDeckLinkProfileAttributes>	deckLinkAttributes(IID_IDeckLinkProfileAttributes, selectedDevice->getDeckLinkInstance());
	int64_t								maxAudioChannels;

	if (!deckLinkAttributes)
		return;

	// Get max number of audio channels supported by DeckLink device
	if (deckLinkAttributes->GetInt(BMDDeckLinkMaximumAudioChannels, &maxAudioChannels) != S_OK)
		return;

	ui->audioChannelPopup->clear();

	// Scan through Audio channel popup menu and disable invalid entries
	for (unsigned int i = 0; i < sizeof(gAudioChannels)/sizeof(*gAudioChannels); i++)
	{
		if (maxAudioChannels < (int64_t)gAudioChannels[i])
			break;
			
		QVariant audioChannelVariant = QVariant::fromValue(gAudioChannels[i]);

		ui->audioChannelPopup->addItem(audioChannelVariant.toString(), audioChannelVariant);
	}
	
	ui->audioChannelPopup->setCurrentIndex(ui->audioChannelPopup->count() - 1);
}

void SignalGenerator::addDevice(com_ptr<IDeckLink>& deckLink)
{
	com_ptr<DeckLinkOutputDevice> outputDevice = make_com_ptr<DeckLinkOutputDevice>(this, deckLink);

	if (!outputDevice->getDeviceOutput())
	{
		// Device does not have IDeckLinkOutput interface, eg it is a DeckLink Mini Recorder
		return;
	}

	// Store output device to map to maintain reference
	outputDevices[(intptr_t)deckLink.get()] = outputDevice;

	// Add this DeckLink device to the device list
	ui->outputDevicePopup->addItem(outputDevice->getDeviceName(), QVariant::fromValue<intptr_t>((intptr_t)deckLink.get()));

	if (ui->outputDevicePopup->count() == 1)
	{
		// We have added our first item, refresh and enable UI
		ui->outputDevicePopup->setCurrentIndex(0);
		outputDeviceChanged(0);

		enableInterface(true);
		ui->startButton->setText("Start");
	}
}

void SignalGenerator::removeDevice(com_ptr<IDeckLink>& deckLink)
{
	// If device to remove is selected device, stop playback if active
	if (selectedDevice->getDeckLinkInstance().get() == deckLink.get())
	{
		if (running)
			stopRunning();
		selectedDevice = nullptr;
	}

	// Find the combo box entry to remove (there may be multiple entries with the same name, but each
	// will have a different data pointer).
	for (int i = 0; i < ui->outputDevicePopup->count(); ++i)
	{
		if (deckLink.get() == (IDeckLink*)((ui->outputDevicePopup->itemData(i)).value<void*>()))
		{
			ui->outputDevicePopup->removeItem(i);
			break;
		}
	}

	// Dereference output device by removing from list 
	auto iter = outputDevices.find((intptr_t)deckLink.get());
	if (iter != outputDevices.end())
	{
		outputDevices.erase(iter);
	}

	// Check how many devices are left
	if (ui->outputDevicePopup->count() == 0)
	{
		// We have removed the last device, disable the interface.
		enableInterface(false);
	}
	else if (!selectedDevice)
	{
		// The device that was removed was the one selected in the UI.
		// Select the first available device in the list and reset the UI.
		ui->outputDevicePopup->setCurrentIndex(0);
		outputDeviceChanged(0);
	}
}

void SignalGenerator::playbackStopped()
{
	// Notify waiting process that scheduled playback has stopped
	{
		std::lock_guard<std::mutex> lock(mutex);
		scheduledPlaybackStopped = true;
	}
	stopPlaybackCondition.notify_one();
}

void SignalGenerator::haltStreams(void)
{
	// Profile is changing, stop playback if running
	if (running)
		stopRunning();
}

void SignalGenerator::updateProfile(com_ptr<IDeckLinkProfile>& /* newProfile */)
{
	// Update the video mode popup menu based on new profile
	refreshDisplayModeMenu();

	// Update the audio channels popup menu based on new profile
	refreshAudioChannelMenu();
}

com_ptr<IDeckLinkMutableVideoFrame> SignalGenerator::CreateOutputFrame(FillFrameFunction fillFrame)
{
	com_ptr<IDeckLinkOutput>				deckLinkOutput;
	com_ptr<IDeckLinkMutableVideoFrame>		referenceFrame;
	com_ptr<IDeckLinkMutableVideoFrame>		scheduleFrame;
	HRESULT									hr;
	int										bytesPerRow;
	int										referenceBytesPerRow;
	com_ptr<IDeckLinkVideoConversion>		frameConverter;

	bytesPerRow = GetRowBytes(selectedPixelFormat, frameWidth);
	referenceBytesPerRow = GetRowBytes(bmdFormat8BitYUV, frameWidth);

	deckLinkOutput = selectedDevice->getDeviceOutput();

	frameConverter = CreateVideoConversionInstance();
	if (!frameConverter)
		goto bail;

	hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, referenceBytesPerRow, bmdFormat8BitYUV, bmdFrameFlagDefault, referenceFrame.releaseAndGetAddressOf());
	if (hr != S_OK)
		goto bail;

	fillFrame(referenceFrame);

	if (selectedPixelFormat == bmdFormat8BitYUV)
	{
		// Frame is already 8-bit YUV, no conversion required
		scheduleFrame = referenceFrame;
	}
	else
	{
		hr = deckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, bytesPerRow, selectedPixelFormat, bmdFrameFlagDefault, scheduleFrame.releaseAndGetAddressOf());
		if (hr != S_OK)
			goto bail;

		hr = frameConverter->ConvertFrame(referenceFrame.get(), scheduleFrame.get());
		if (hr != S_OK)
			goto bail;
	}

bail:
	return scheduleFrame;
}

void SignalGenerator::startRunning()
{
	com_ptr<IDeckLinkOutput>			deckLinkOutput		= selectedDevice->getDeviceOutput();
	com_ptr<IDeckLinkDisplayMode>		displayMode			= nullptr;
	com_ptr<IDeckLinkProfileAttributes>	deckLinkAttributes(IID_IDeckLinkProfileAttributes, selectedDevice->getDeckLinkInstance());
	bool								success				= false;
	BMDVideoOutputFlags					videoOutputFlags	= 0;
	bool								output444;
	HRESULT								result;
	QVariant 							v;
	
	if (!deckLinkAttributes)
		goto bail;

	// When a scheduled video frame is complete, schedule next frame
	selectedDevice->onScheduledFrameCompleted(std::bind(&SignalGenerator::scheduleNextFrame, this, false));

	// Provide further audio samples to the DeckLink API until our preferred buffer waterlevel is reached
	selectedDevice->onRenderAudioSamples(std::bind(&SignalGenerator::writeNextAudioSamples, this));

	// Register when scheduled playback stopped callback, to know when to disable video output
	selectedDevice->onScheduledPlaybackStopped(std::bind(&SignalGenerator::playbackStopped, this));

	// Provide the delegate to the audio and video output interfaces
	if (deckLinkOutput->SetScheduledFrameCompletionCallback(selectedDevice.get()) != S_OK)
		goto bail;

	if (deckLinkOutput->SetAudioCallback(selectedDevice.get()) != S_OK)
		goto bail;

	if (deckLinkOutput->SetScreenPreviewCallback(previewView->delegate()) != S_OK)
		goto bail;

	// Determine the audio and video properties for the output stream
	v = ui->outputSignalPopup->itemData(ui->outputSignalPopup->currentIndex());
	outputSignal = (OutputSignal)v.value<int>();
	
	v = ui->audioChannelPopup->itemData(ui->audioChannelPopup->currentIndex());
	audioChannelCount = v.value<int>();
	
	v = ui->audioSampleDepthPopup->itemData(ui->audioSampleDepthPopup->currentIndex());
	audioSampleDepth = v.value<int>();
	audioSampleRate = bmdAudioSampleRate48kHz;
	
	// Get the IDeckLinkDisplayMode object associated with the selected display mode
	if (deckLinkOutput->GetDisplayMode(selectedDisplayMode, displayMode.releaseAndGetAddressOf()) != S_OK)
		goto bail;

	frameWidth = displayMode->GetWidth();
	frameHeight = displayMode->GetHeight();
	
	displayMode->GetFrameRate(&frameDuration, &frameTimescale);
	// Calculate the number of frames per second, rounded up to the nearest integer.  For example, for NTSC (29.97 FPS), framesPerSecond == 30.
	framesPerSecond = (frameTimescale + (frameDuration-1))  /  frameDuration;
	
	// m-rate frame rates with multiple 30-frame counting should implement Drop Frames compensation, refer to SMPTE 12-1
	if (frameDuration == 1001 && frameTimescale % 30000 == 0)
		dropFrames = 2 * (frameTimescale / 30000);
	else
		dropFrames = 0;

	// Check whether HFRTC is supported by the selected device
	if (deckLinkAttributes->GetFlag(BMDDeckLinkSupportsHighFrameRateTimecode, &hfrtcSupported) != S_OK)
	{
		hfrtcSupported = false;
	}

	if (selectedDisplayMode == bmdModeNTSC ||
			selectedDisplayMode == bmdModeNTSC2398 ||
			selectedDisplayMode == bmdModePAL)
	{
		timeCodeFormat = bmdTimecodeVITC;
		videoOutputFlags |= bmdVideoOutputVITC;
	}
	else
	{
		timeCodeFormat = bmdTimecodeRP188Any;
		videoOutputFlags |= bmdVideoOutputRP188;
	}

	timeCode = std::unique_ptr<Timecode>(new Timecode(framesPerSecond, dropFrames));

	selectedPixelFormat = (BMDPixelFormat)ui->pixelFormatPopup->itemData(ui->pixelFormatPopup->currentIndex()).value<int>();
	
	// Set the output to 444 if RGB mode is selected
	try
	{
		std::tie(std::ignore, output444) = kPixelFormats.at(selectedPixelFormat);
	}
	catch (std::out_of_range)
	{
		goto bail;
	}

	result = selectedDevice->getDeviceConfiguration()->SetFlag(bmdDeckLinkConfig444SDIVideoOutput, output444);
	// If a device without SDI output is used (eg Intensity Pro 4K), then SetFlags will return E_NOTIMPL
	if ((result != S_OK) && (result != E_NOTIMPL))
		goto bail;

	// Set the video output mode
	if (deckLinkOutput->EnableVideoOutput(selectedDisplayMode, videoOutputFlags) != S_OK)
		goto bail;
	
	// Set the audio output mode
	if (deckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, audioSampleDepth, audioChannelCount, bmdAudioOutputStreamTimestamped) != S_OK)
		goto bail;
	
	// Generate one second of audio tone
	audioSamplesPerFrame = ((audioSampleRate * frameDuration) / frameTimescale);
	audioBufferSampleLength = (framesPerSecond * audioSampleRate * frameDuration) / frameTimescale;
	audioBuffer = malloc(audioBufferSampleLength * audioChannelCount * (audioSampleDepth / 8));
	if (audioBuffer == nullptr)
		goto bail;
	FillSine(audioBuffer, audioBufferSampleLength, audioChannelCount, audioSampleDepth);
	
	// Generate a frame of black
	videoFrameBlack = CreateOutputFrame(FillBlack);
	
	// Generate a frame of colour bars
	videoFrameBars = CreateOutputFrame(FillColorBars);
	
	// Begin video preroll by scheduling a second of frames in hardware
	totalFramesScheduled = 0;
	for (unsigned int i = 0; i < framesPerSecond; i++)
		scheduleNextFrame(true);
	
	// Begin audio preroll.  This will begin calling our audio callback, which will start the DeckLink output stream.
	totalAudioSecondsScheduled = 0;
	if (deckLinkOutput->BeginAudioPreroll() != S_OK)
		goto bail;
	
	// Success; update the UI
	running = true;
	ui->startButton->setText("Stop");
	// Disable the user interface while running (prevent the user from making changes to the output signal)
	enableInterface(false);
	scheduledPlaybackStopped = false;

	success = true;
	
bail:
	if(!success)
	{
		QMessageBox::critical(this, "Failed to start output", "Failed to start output");
		// *** Error-handling code.  Cleanup any resources that were allocated. *** //
		stopRunning();
	}
}

void SignalGenerator::stopRunning()
{
	com_ptr<IDeckLinkOutput> deckLinkOutput = selectedDevice->getDeviceOutput();

	if (running)
	{
		// Stop the audio and video output streams immediately
		deckLinkOutput->StopScheduledPlayback(0, nullptr, 0);

		// Wait until IDeckLinkVideoOutputCallback::ScheduledPlaybackHasStopped callback
		std::unique_lock<std::mutex> lock(mutex);
		stopPlaybackCondition.wait(lock, [this] { return scheduledPlaybackStopped; } );
	}

	// Deregister output callbacks
	deckLinkOutput->SetScheduledFrameCompletionCallback(nullptr);
	deckLinkOutput->SetAudioCallback(nullptr);
	deckLinkOutput->SetScreenPreviewCallback(nullptr);

	deckLinkOutput->DisableAudioOutput();
	deckLinkOutput->DisableVideoOutput();
	
	if (audioBuffer != nullptr)
		free(audioBuffer);
	audioBuffer = nullptr;
	
	selectedDevice->onScheduledFrameCompleted(nullptr);
	selectedDevice->onRenderAudioSamples(nullptr);
	selectedDevice->onScheduledPlaybackStopped(nullptr);

	// Success; update the UI
	running = false;
	ui->startButton->setText("Start");
	enableInterface(true);
}


void SignalGenerator::scheduleNextFrame(bool prerolling)
{
	HRESULT									result = S_OK;
	com_ptr<IDeckLinkMutableVideoFrame>		currentFrame;
	com_ptr<IDeckLinkOutput>				deckLinkOutput = nullptr;
	com_ptr<IDeckLinkDisplayMode>			outputDisplayMode = nullptr;
	bool									setVITC1Timecode = false;
	bool									setVITC2Timecode = false;

	deckLinkOutput = selectedDevice->getDeviceOutput();

	if (prerolling == false)
	{
		// If not prerolling, make sure that playback is still active
		if (running == false)
			return;
	}
	
	if (outputSignal == kOutputSignalPip)
	{
		if ((totalFramesScheduled % framesPerSecond) == 0)
			currentFrame = videoFrameBars;
		else
			currentFrame = videoFrameBlack;
	}
	else
	{
		if ((totalFramesScheduled % framesPerSecond) == 0)
			currentFrame = videoFrameBlack;
		else
			currentFrame = videoFrameBars;
	}
	
	if (timeCodeFormat == bmdTimecodeVITC)
	{
		result = currentFrame->SetTimecodeFromComponents(bmdTimecodeVITC,
														 timeCode->hours(),
														 timeCode->minutes(),
														 timeCode->seconds(),
														 timeCode->frames(),
														 bmdTimecodeFlagDefault);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not set VITC timecode on frame - result = %08x\n", result);
			return;
		}
	}
	else
	{
		int frames = timeCode->frames();

		if (hfrtcSupported)
		{
			result = currentFrame->SetTimecodeFromComponents(bmdTimecodeRP188HighFrameRate,
														   timeCode->hours(),
														   timeCode->minutes(),
														   timeCode->seconds(),
														   frames,
														   bmdTimecodeFlagDefault);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not set HFRTC timecode on frame - result = %08x\n", result);
				return;
			}
		}

		result = deckLinkOutput->GetDisplayMode(selectedDisplayMode, outputDisplayMode.releaseAndGetAddressOf());
		if (result != S_OK)
		{
			fprintf(stderr, "Could not get output display mode - result = %08x\n", result);
			return;			
		}

		if (outputDisplayMode->GetFieldDominance() != bmdProgressiveFrame)
		{
			// An interlaced or PsF frame has both VITC1 and VITC2 set with the same timecode value (SMPTE ST 12-2:2014 7.2)
			setVITC1Timecode = true;
			setVITC2Timecode = true;
		}
		else if (framesPerSecond <= 30)
		{
			// If this isn't a High-P mode, then just use VITC1 (SMPTE ST 12-2:2014 7.2)
			setVITC1Timecode = true;
		}
		else if (framesPerSecond <= 60)
		{
			// If this is a High-P mode then use VITC1 on even frames and VITC2 on odd frames. This is done because the
			// frames field of the RP188 VITC timecode cannot hold values greater than 30 (SMPTE ST 12-2:2014 7.2, 9.2)
			if ((frames & 1) == 0)
				setVITC1Timecode = true;
			else
				setVITC2Timecode = true;

			frames >>= 1;
		}

		if (setVITC1Timecode)
		{
			result = currentFrame->SetTimecodeFromComponents(bmdTimecodeRP188VITC1,
															 timeCode->hours(),
															 timeCode->minutes(),
															 timeCode->seconds(),
															 frames,
															 bmdTimecodeFlagDefault);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not set VITC1 timecode on interlaced frame - result = %08x\n", result);
				return;
			}
		}

		if (setVITC2Timecode)
		{
			// The VITC2 timecode also has the field mark flag set
			result = currentFrame->SetTimecodeFromComponents(bmdTimecodeRP188VITC2,
															 timeCode->hours(),
															 timeCode->minutes(),
															 timeCode->seconds(),
															 frames,
															 bmdTimecodeFieldMark);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not set VITC1 timecode on interlaced frame - result = %08x\n", result);
				return;
			}
		}
	}

	printf("Output frame: %02d:%02d:%02d:%03d\n", timeCode->hours(), timeCode->minutes(), timeCode->seconds(), timeCode->frames());

	result = deckLinkOutput->ScheduleVideoFrame(currentFrame.get(), (totalFramesScheduled * frameDuration), frameDuration, frameTimescale);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not schedule video output frame - result = %08x\n", result);
		return;
	}

	totalFramesScheduled += 1;
	timeCode->update();
}

void SignalGenerator::writeNextAudioSamples()
{
	// Write one second of audio to the DeckLink API.
	
	if (outputSignal == kOutputSignalPip)
	{
		// Schedule one-frame of audio tone
		if (selectedDevice->getDeviceOutput()->ScheduleAudioSamples(audioBuffer, audioSamplesPerFrame, (totalAudioSecondsScheduled * audioBufferSampleLength), audioSampleRate, nullptr) != S_OK)
			return;
	}
	else
	{
		// Schedule one-second (minus one frame) of audio tone
		if (selectedDevice->getDeviceOutput()->ScheduleAudioSamples(audioBuffer, (audioBufferSampleLength - audioSamplesPerFrame), (totalAudioSecondsScheduled * audioBufferSampleLength) + audioSamplesPerFrame, audioSampleRate, nullptr) != S_OK)
			return;
	}
	
	totalAudioSecondsScheduled += 1;
}

void SignalGenerator::outputDeviceChanged(int selectedDeviceIndex)
{
	if (selectedDeviceIndex == -1)
		return;

	// Release profile callback from existing selected device
	if ((selectedDevice) && (selectedDevice->getProfileManager()))
		selectedDevice->getProfileManager()->SetCallback(nullptr);

	QVariant selectedDeviceVariant = ui->outputDevicePopup->itemData(selectedDeviceIndex);
	intptr_t deckLinkPtr = (intptr_t)selectedDeviceVariant.value<intptr_t>();

	// Find output device based on IDeckLink* object
	auto iter = outputDevices.find(deckLinkPtr);
	if (iter == outputDevices.end())
		return;

	selectedDevice = iter->second;

	// Register profile callback with newly selected device's profile manager
	if (selectedDevice->getProfileManager())
		selectedDevice->getProfileManager()->SetCallback(profileCallback.get());

	// Update the video mode popup menu
	refreshDisplayModeMenu();
	
	// Update the audio channels popup menu
	refreshAudioChannelMenu();
}

void SignalGenerator::videoFormatChanged(int videoFormatIndex)
{
	if (videoFormatIndex == -1)
		return;

	selectedDisplayMode = (BMDDisplayMode)ui->videoFormatPopup->itemData(videoFormatIndex).value<uint64_t>();

	// Update pixel format popup menu
	refreshPixelFormatMenu();
}

/*****************************************/

int		GetRowBytes(BMDPixelFormat pixelFormat, uint32_t frameWidth)
{
	int bytesPerRow;

	// Refer to DeckLink SDK Manual - 2.7.4 Pixel Formats
	switch (pixelFormat)
	{
	case bmdFormat8BitYUV:
		bytesPerRow = frameWidth * 2;
		break;

	case bmdFormat10BitYUV:
		bytesPerRow = ((frameWidth + 47) / 48) * 128;
		break;

	case bmdFormat10BitRGB:
		bytesPerRow = ((frameWidth + 63) / 64) * 256;
		break;

	case bmdFormat8BitARGB:
	case bmdFormat8BitBGRA:
	default:
		bytesPerRow = frameWidth * 4;
		break;
	}

	return bytesPerRow;
}

void	FillSine (void* audioBuffer, uint32_t samplesToWrite, uint32_t channels, uint32_t sampleDepth)
{
	if (sampleDepth == 16)
	{
		int16_t*		nextBuffer;
		
		nextBuffer = (int16_t*)audioBuffer;
		for (uint32_t i = 0; i < samplesToWrite; i++)
		{
			int16_t		sample;
			
			sample = (int16_t)(24576.0 * sin((i * 2.0 * M_PI) / 48.0));
			for (uint32_t ch = 0; ch < channels; ch++)
				*(nextBuffer++) = sample;
		}
	}
	else if (sampleDepth == 32)
	{
		int32_t*		nextBuffer;
		
		nextBuffer = (int32_t*)audioBuffer;
		for (uint32_t i = 0; i < samplesToWrite; i++)
		{
			int32_t		sample;
			
			sample = (int32_t)(1610612736.0 * sin((i * 2.0 * M_PI) / 48.0));
			for (uint32_t ch = 0; ch < channels; ch++)
				*(nextBuffer++) = sample;
		}
	}
}

void	FillColorBars (com_ptr<IDeckLinkMutableVideoFrame>& theFrame)
{
	uint32_t*		nextWord;
	uint32_t		width;
	uint32_t		height;
	uint32_t*		bars;
	
	theFrame->GetBytes((void**)&nextWord);
	width = theFrame->GetWidth();
	height = theFrame->GetHeight();
	
	if (width > 720)
	{
		bars = gHD75pcColourBars;
	}
	else
	{
		bars = gSD75pcColourBars;
	}

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x+=2)
		{
			*(nextWord++) = bars[(x * 8) / width];
		}
	}
}

void	FillBlack (com_ptr<IDeckLinkMutableVideoFrame>& theFrame)
{
	uint32_t*		nextWord;
	uint32_t		width;
	uint32_t		height;
	uint32_t		wordsRemaining;
	
	theFrame->GetBytes((void**)&nextWord);
	width = theFrame->GetWidth();
	height = theFrame->GetHeight();
	
	wordsRemaining = (width*2 * height) / 4;
	
	while (wordsRemaining-- > 0)
		*(nextWord++) = 0x10801080;
}
