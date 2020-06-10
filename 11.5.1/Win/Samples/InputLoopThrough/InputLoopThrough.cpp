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

//*************************************************************************************/
// Note to developers:
//
// The InputLoopThough sample demonstrates how to capture a video frame on an input port
// and reschedule to output port.  The goal of the sample is to allow developers to
// integrate their own video pipeline, and to determine the overall latency of the pipeline.
//
// In this sample, we define three latency measurements:
// * Input latency: The measured time from start of frame on the wire to
//     IDeckLinkInputCallback::VideoInputFrameArrived callback
// * Processing latency: The measured time from the IDeckLinkInputCallback::VideoInputFrameArrived
//     callback until the call to IDeckLinkOutput::ScheduleVideoFrame.
// * Output latency: The measured time for a frame to be scheduled by DeckLinkOutput::ScheduleVideoFrame
//     to the start of frame output on the wire, per IDeckLinkVideoOutputCallback::ScheduledFrameCompleted
//     callback
//
// Performance considerations:
// * It is recommended that your input source and playback are locked to the same reference,
//     otherwise the output latency will vary between runs.  This will impact the minimum you
//     can set your video output preroll size, defined by constant kOutputVideoPreroll
// * If the video processing pipeline is long, then you will need to increase the number of
//     worker threads for concurrent processing.  The sample defines a dispatch queue, whose
//     number of threads is defined by constant kDispatcherThreadCount
// * If there is large variance in the video processing latency, then it is recommended that
//     the preroll is increased to reduce the risk of late or dropped frames on output
//
// Additional considerations:
// * Ensure that a valid input source is provided with a display mode that is supported by
//     both input and output devices
// * Out of the box, the video processing thread, defined by function processVideo(),
//     injects a random sleep time into the pipeline.  The time's mean and standard
//     deviation can be adjusted by constants kProcessingAdditionalTimeMean and
//     kProcessingAdditionalTimeStdDev respectively
// * The sample has 2 console output modes of operation, defined by constant kPrintRollingAverage
//   - When set to true, a rolling average of latency is displayed to stdout with ms
//     interval defined by constant kRollingAverageUpdateRateMs with rolling average
//     sample could defined by constant kRollingAverageSampleCount
//   - When set to false, the latency for every output frame is displayed to stdout
//   - In both modes or operation, a full statistical summary is displayed when application
//     completes
//*************************************************************************************/


#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>

#include "DeckLinkInputDevice.h"
#include "DeckLinkOutputDevice.h"
#include "DispatchQueue.h"
#include "SampleQueue.h"
#include "LatencyStatistics.h"
#include "DeckLinkAPI.h"
#include "com_ptr.h"
#include "platform.h"

const BMDAudioSampleType	kAudioSampleType			= bmdAudioSampleType32bitInteger;
const BMDDisplayMode		kInitialDisplayMode			= bmdModeNTSC;
const BMDPixelFormat		kInitialPixelFormat			= bmdFormat10BitYUV;
const uint32_t				kDefaultAudioChannelCount	= 16;
const bool					kWaitForReferenceToLock		= true;		// True if reference lock should be waited for before starting capture/playback

const int					kOutputVideoPreroll			= 1;		// number of output preroll frames
const int					kVideoDispatcherThreadCount	= 3;		// number of threads used by video processing dispatcher
const int					kAudioDispatcherThreadCount	= 2;		// number of threads used by audio processing dispatcher
const int					kPrintDispatcherThreadCount	= 1;		// number of threads used by print stdout dispatcher

const BMDTimeScale			kDisplayTimescale			= 100000;	// Display latency in 10 us timescale
const int					kMilliSecPrecision			= kDisplayTimescale / 1000;

const bool					kPrintRollingAverage		= true;		// If true, display latency as rolling average, if false print latency for each frame
const int					kRollingAverageSampleCount	= 300;		// Number of samples for calculating rolling average of latency
const long					kRollingAverageUpdateRateMs	= 2000;		// Print rolling average every 2 seconds

const double				kProcessingAdditionalTimeMean		= 5.0;		// Mean additional time injected into video processing thread (ms)
const double				kProcessingAdditionalTimeStdDev		= 0.1;		// Standard deviation of time injected into video processing thread (ms)

// Output frame completion result pair = { Completion result string, frame output boolean}
const std::map<BMDOutputFrameCompletionResult, std::pair<const char*, bool>> kOutputCompletionResults
{
	{ bmdOutputFrameCompleted,		std::make_pair("completed",			true) },
	{ bmdOutputFrameDisplayedLate,	std::make_pair("displayed late",	true) },
	{ bmdOutputFrameDropped,		std::make_pair("dropped",			false) },
	{ bmdOutputFrameFlushed,		std::make_pair("flushed",			false) },
};

// List of known pixel formats and their matching display names
const std::map<BMDPixelFormat, const char*> kPixelFormats =
{
	{ bmdFormat8BitYUV,		"8-bit YUV" },
	{ bmdFormat10BitYUV,	"10-bit YUV" },
	{ bmdFormat8BitARGB,	"8-bit ARGB" },
	{ bmdFormat8BitBGRA,	"8-bit BGRA" },
	{ bmdFormat10BitRGB,	"10-bit RGB" },
	{ bmdFormat12BitRGB,	"12-bit RGB" },
	{ bmdFormat12BitRGBLE,	"12-bit RGBLE" },
	{ bmdFormat10BitRGBXLE,	"10-bit RGBXLE" },
	{ bmdFormat10BitRGBX,	"10-bit RGBX" },
};

struct ThreadNotifier
{
	std::mutex mutex;
	std::condition_variable condition;

	ThreadNotifier() :
		m_notified(false)
	{ }

	void reset()
	{
		std::lock_guard<std::mutex> lock(mutex);
		m_notified = false;
	}

	void notify()
	{
		std::lock_guard<std::mutex> lock(mutex);
		m_notified = true;
		condition.notify_all();
	}

	bool isNotified()
	{
		std::lock_guard<std::mutex> lock(mutex);
		return m_notified;
	}

	bool isNotifiedLocked()
	{
		return m_notified;
	}

private:
	bool m_notified;
};

uint32_t														g_audioChannelCount = kDefaultAudioChannelCount;

LatencyStatistics												g_videoInputLatencyStatistics(kRollingAverageSampleCount);
LatencyStatistics												g_videoProcessingLatencyStatistics(kRollingAverageSampleCount);
LatencyStatistics												g_videoOutputLatencyStatistics(kRollingAverageSampleCount);
LatencyStatistics												g_audioProcessingLatencyStatistics(kRollingAverageSampleCount);

std::map<BMDOutputFrameCompletionResult, int>					g_frameCompletionResultCount;
int 															g_outputFrameCount = 0;
int																g_droppedOnCaptureFrameCount = 0;

std::default_random_engine 										g_randomEngine;
std::normal_distribution<double> 								g_sleepDistribution(kProcessingAdditionalTimeMean, kProcessingAdditionalTimeStdDev);

ThreadNotifier													g_printRollingAverageNotifier;
ThreadNotifier													g_loopThroughSessionNotifier;

struct FormatDescription
{
	BMDDisplayMode displayMode;
	bool is3D;
	BMDPixelFormat pixelFormat;
};

bool operator==(const FormatDescription& desc1, const FormatDescription& desc2)
{
	return std::tie(desc1.displayMode, desc1.pixelFormat, desc1.is3D) == std::tie(desc2.displayMode, desc2.pixelFormat, desc2.is3D);
}

bool operator!=(const FormatDescription& desc1, const FormatDescription& desc2)
{
	return !operator==(desc1, desc2);
}

template<typename... Args>
void dispatch_printf(DispatchQueue& dispatchQueue, const char* format, Args... args)
{
	int size = snprintf(NULL, 0, format, args...);
	char* buf = new char[size + 1];
	snprintf(buf, size + 1, format, args...);

	dispatchQueue.dispatch([=]
	{
		fprintf(stdout, "%s", buf);
		delete [] buf;
	});
}

void processVideo(std::shared_ptr<LoopThroughVideoFrame>& videoFrame, com_ptr<DeckLinkOutputDevice>& deckLinkOutput)
{
	// Main video processing function, it is intended to invoke with DispatchQueue to allow multi-threading of incoming frames
	// Inputs:	videoFrame - input/output video frame with stream time
	//			deckLinkOutput - reference to IDeckLinkOutput
	// At end of function, queue output frame for scheduling by calling deckLinkOutput->scheduleVideoFrame
	//
	// Developers are encouraged to insert their own processing test code in this function, by default we will simply forward the LoopThroughVideoFrame object.
	// The input frame may be replaced by another IDeckLinkVideoFrame object for output by calling LoopThroughVideoFrame::setVideoFrame()

	// Check playback is active, if it is inactive, it is likely that the incoming display mode is not supported by output
	if (!deckLinkOutput->isPlaybackActive())
		return;

	// Simulate doing something by using a busy wait loop
	// This is more precise than sleeping
	int delay = (int)std::round(g_sleepDistribution(g_randomEngine) * 1000);
	auto target = std::chrono::steady_clock::now() + std::chrono::microseconds(delay);
	uint32_t i = 0;
	while (std::chrono::steady_clock::now() < target)
		++i;

	// At end of function, remember to queue your output frame
	deckLinkOutput->scheduleVideoFrame(std::move(videoFrame));
}


void processAudio(std::shared_ptr<LoopThroughAudioPacket>& audioPacket, com_ptr<DeckLinkOutputDevice>& deckLinkOutput)
{
	// Main audio processing function, it is intended to invoke with DispatchQueue to allow multi-threading of incoming packets
	// Inputs:	inputAudioPacket - input audio packet with stream time
	//			deckLinkOutput - reference to IDeckLinkOutput
	// At end of function, queue output frame for scheduling by calling deckLinkOutput->scheduleAudioBuffer
	//
	// Developers are encouraged to insert their own processing test code in this function, by default we will simply forward the LoopThroughAudioPacket object.
	// The input audio packet may be replaced by another void* buffer for output by calling LoopThroughAudioPacket::setAudioPacket()

	// Check playback is active, if it is inactive, it is likely that the incoming display mode is not supported by output
	if (!deckLinkOutput->isPlaybackActive())
		return;

	// Simulate doing something by using a busy wait loop
	// This is more precise than sleeping
	int delay = (int)std::round(g_sleepDistribution(g_randomEngine) * 1000);
	auto target = std::chrono::steady_clock::now() + std::chrono::microseconds(delay);
	uint32_t i = 0;
	while (std::chrono::steady_clock::now() < target)
		++i;
	
	// At end of function, remember to queue your output audio packet
	deckLinkOutput->scheduleAudioPacket(std::move(audioPacket));
}

std::string getDeckLinkDisplayName(com_ptr<IDeckLink> deckLink)
{
	dlstring_t		displayName;
	std::string		displayNameString;

	if (deckLink->GetDisplayName(&displayName) == S_OK)
	{
		displayNameString = DlToStdString(displayName);
		DeleteString(displayName);
	}
	else
	{
		displayNameString = "Unknown";
	}

	return displayNameString;
}

void printDroppedCaptureFrame(BMDTimeValue streamTime, BMDTimeValue frameDuration, DispatchQueue& printDispatchQueue)
{
	++g_droppedOnCaptureFrameCount;

	if (!kPrintRollingAverage)
		dispatch_printf(printDispatchQueue, "Frame %d (dropped);\n", streamTime / frameDuration);
}

void printOutputCompletionResult(std::shared_ptr<LoopThroughVideoFrame> completedFrame, DispatchQueue& printDispatchQueue)
{
	const char*		completionResultString;
	bool			frameDisplayed;
	try
	{
		std::tie(completionResultString, frameDisplayed) = kOutputCompletionResults.at(completedFrame->getOutputCompletionResult());
	}
	catch (std::out_of_range)
	{
		fprintf(stderr, "Unexpected video frame output completion result\n");
		return;
	}

	if (frameDisplayed)
	{
		dispatch_printf(printDispatchQueue,
						"Frame %d (%s); Latency: Input = %.2f ms, Processing = %.2f ms, Output = %.2f ms\n",
						completedFrame->getVideoStreamTime() / completedFrame->getVideoFrameDuration(), completionResultString,
						(double)completedFrame->getInputLatency() / kMilliSecPrecision,
						(double)completedFrame->getProcessingLatency() / kMilliSecPrecision,
						(double)completedFrame->getOutputLatency() / kMilliSecPrecision);
	}
	else
	{
		dispatch_printf(printDispatchQueue, "Frame %d (%s);\n", completedFrame->getVideoStreamTime() / completedFrame->getVideoFrameDuration(), completionResultString);
	}
}

void updateCompletedFrameLatency(std::shared_ptr<LoopThroughVideoFrame> completedFrame, DispatchQueue& printDispatchQueue)
{
	bool frameDisplayed;
	try
	{
		std::tie(std::ignore, frameDisplayed) = kOutputCompletionResults.at(completedFrame->getOutputCompletionResult());
	}
	catch (std::out_of_range)
	{
		fprintf(stderr, "Unexpected video frame output completion result\n");
		return;
	}
	
	if (frameDisplayed)
	{
		g_videoInputLatencyStatistics.addSample(completedFrame->getInputLatency());
		g_videoProcessingLatencyStatistics.addSample(completedFrame->getProcessingLatency());
		g_videoOutputLatencyStatistics.addSample(completedFrame->getOutputLatency());
	}
	
	g_outputFrameCount++;
	++g_frameCompletionResultCount[completedFrame->getOutputCompletionResult()];
	
	if (!kPrintRollingAverage)
	{
		printOutputCompletionResult(std::move(completedFrame), printDispatchQueue);
	}
}

void printRollingAverage(DispatchQueue& printDispatchQueue)
{
	std::chrono::milliseconds	printRollingAveragePeriod(kRollingAverageUpdateRateMs);
	
	while (true)
	{
		std::unique_lock<std::mutex> lock(g_printRollingAverageNotifier.mutex);
		if (!g_printRollingAverageNotifier.condition.wait_for(lock, printRollingAveragePeriod, [] { return g_printRollingAverageNotifier.isNotifiedLocked(); }))
		{
			// Timeout, print rolling average
			dispatch_printf(printDispatchQueue,
							"%d frames output; Average latency: Input = %.2f ms, Processing = %.2f ms, Output = %.2f ms\n",
							g_outputFrameCount,
							(double)g_videoInputLatencyStatistics.getRollingAverage() / kMilliSecPrecision,
							(double)g_videoProcessingLatencyStatistics.getRollingAverage() / kMilliSecPrecision,
							(double)g_videoOutputLatencyStatistics.getRollingAverage() / kMilliSecPrecision);
		}
		else
		{
			break;
		}
	}
}

void printOutputSummary(DispatchQueue& printDispatchQueue)
{
	int displayedFrames = 0;
	dispatch_printf(printDispatchQueue, "\nFrames dropped on capture: %d\n", g_droppedOnCaptureFrameCount);
	for (auto completionResultIter : kOutputCompletionResults)
	{
		const char* completionResultString;
		bool frameDisplayed;
		
		std::tie(completionResultString, frameDisplayed) = completionResultIter.second;
		
		auto completionCountIter = g_frameCompletionResultCount.find(completionResultIter.first);
		int frameCount = (completionCountIter != g_frameCompletionResultCount.end()) ? completionCountIter->second : 0;
		dispatch_printf(printDispatchQueue, "Frames %s: %d\n", completionResultString, frameCount);
		if (frameDisplayed)
			displayedFrames += frameCount;
	}
	if (displayedFrames > 0)
	{
		BMDTimeValue mean;
		BMDTimeValue stddev;
		
		std::tie(mean, stddev) = g_videoInputLatencyStatistics.getMeanAndStdDev();
		dispatch_printf(printDispatchQueue,
						"\nVideo Input Latency:\t\tMinimum = %6.2f ms, Maximum = %6.2f ms, Mean = %6.2f ms, StdDev = %.2f ms\n",
						(double)g_videoInputLatencyStatistics.getMinimum() / kMilliSecPrecision,
						(double)g_videoInputLatencyStatistics.getMaximum() / kMilliSecPrecision,
						(double)mean / kMilliSecPrecision,
						(double)stddev / kMilliSecPrecision);
		
		std::tie(mean, stddev) = g_videoProcessingLatencyStatistics.getMeanAndStdDev();
		dispatch_printf(printDispatchQueue,
						"Video Processing Latency:\tMinimum = %6.2f ms, Maximum = %6.2f ms, Mean = %6.2f ms, StdDev = %.2f ms\n",
						(double)g_videoProcessingLatencyStatistics.getMinimum() / kMilliSecPrecision,
						(double)g_videoProcessingLatencyStatistics.getMaximum() / kMilliSecPrecision,
						(double)mean / kMilliSecPrecision,
						(double)stddev / kMilliSecPrecision);

		std::tie(mean, stddev) = g_videoOutputLatencyStatistics.getMeanAndStdDev();
		dispatch_printf(printDispatchQueue,
						"Video Output Latency:\t\tMinimum = %6.2f ms, Maximum = %6.2f ms, Mean = %6.2f ms, StdDev = %.2f ms\n",
						(double)g_videoOutputLatencyStatistics.getMinimum() / kMilliSecPrecision,
						(double)g_videoOutputLatencyStatistics.getMaximum() / kMilliSecPrecision,
						(double)mean / kMilliSecPrecision,
						(double)stddev / kMilliSecPrecision);
		
		std::tie(mean, stddev) = g_audioProcessingLatencyStatistics.getMeanAndStdDev();
		dispatch_printf(printDispatchQueue,
						"Audio Processing Latency:\tMinimum = %6.2f ms, Maximum = %6.2f ms, Mean = %6.2f ms, StdDev = %.2f ms\n",
						(double)g_audioProcessingLatencyStatistics.getMinimum() / kMilliSecPrecision,
						(double)g_audioProcessingLatencyStatistics.getMaximum() / kMilliSecPrecision,
						(double)mean / kMilliSecPrecision,
						(double)stddev / kMilliSecPrecision);	}
}

void printReferenceStatus(com_ptr<DeckLinkOutputDevice>& deckLinkOutput, DispatchQueue& printDispatchQueue)
{
	BMDDisplayMode referenceSignalDisplayMode;

	if (deckLinkOutput->getReferenceSignalMode(&referenceSignalDisplayMode))
	{
		if (referenceSignalDisplayMode != bmdModeUnknown)
		{
			com_ptr<IDeckLinkDisplayMode>	referenceDeckLinkDisplayMode;
			dlstring_t						referenceDisplayModeName;

			deckLinkOutput->getDeckLinkOutput()->GetDisplayMode(referenceSignalDisplayMode, referenceDeckLinkDisplayMode.releaseAndGetAddressOf());

			referenceDeckLinkDisplayMode->GetName(&referenceDisplayModeName);
			dispatch_printf(printDispatchQueue, "Reference signal locked to %s\n", DlToCString(referenceDisplayModeName));
			DeleteString(referenceDisplayModeName);
		}
		else
		{
			dispatch_printf(printDispatchQueue, "Reference signal locked\n");
		}
	}
	else
	{
		dispatch_printf(printDispatchQueue, "Warning: Reference signal not locked, this will result in an indeterminate latency between runs.\n");
	}
}

HRESULT InputLoopThrough(void)
{
	HRESULT								result = S_OK;

	com_ptr<IDeckLinkIterator>			deckLinkIterator;
	com_ptr<IDeckLink>					deckLink;
	com_ptr<DeckLinkInputDevice>		deckLinkInput;
	com_ptr<DeckLinkOutputDevice>		deckLinkOutput;

	DispatchQueue 						videoDispatchQueue(kVideoDispatcherThreadCount);
	DispatchQueue 						audioDispatchQueue(kAudioDispatcherThreadCount);
	DispatchQueue						printDispatchQueue(kPrintDispatcherThreadCount);
	
	std::thread							printRollingAverageThread;

	result = GetDeckLinkIterator(deckLinkIterator.releaseAndGetAddressOf());
	if (result != S_OK)
		return result;

	while (deckLinkIterator->Next(deckLink.releaseAndGetAddressOf()) == S_OK)
	{
		com_ptr<IDeckLinkProfileAttributes>		deckLinkAttributes(IID_IDeckLinkProfileAttributes, deckLink);
		int64_t									duplexMode;
		int64_t									videoIOSupport;
		int64_t									maxAudioChannels;

		if (!deckLinkAttributes)
		{
			fprintf(stderr, "Could not obtain the IDeckLinkAttributes interface - result %08x\n", result);
			deckLink = nullptr;
			continue;
		}

		// Check whether device is an active state
		if ((deckLinkAttributes->GetInt(BMDDeckLinkDuplex, &duplexMode) != S_OK) ||
			((BMDDuplexMode)duplexMode == bmdDuplexInactive))
		{
			deckLink = nullptr;
			continue;
		}

		// Get the IO support for device
		if (deckLinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &videoIOSupport) == S_OK)
		{
			if (deckLinkAttributes->GetInt(BMDDeckLinkMaximumAudioChannels, &maxAudioChannels) != S_OK)
			{
				deckLink = nullptr;
				continue;
			}

			if (!deckLinkInput && (((BMDVideoIOSupport)videoIOSupport & bmdDeviceSupportsCapture) != 0))
			{
				dlbool_t	supportsInputFormatDetection;

				// For scope of sample, only use input devices that support input format detection
				if ((deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &supportsInputFormatDetection) == S_OK) &&
					supportsInputFormatDetection)
				{
					try
					{
						deckLinkInput = make_com_ptr<DeckLinkInputDevice>(deckLink, kDisplayTimescale);
					}
					catch (const std::exception& e)
					{
						fprintf(stderr, "%s\n", e.what());
						continue;
					}
					g_audioChannelCount = std::min((uint32_t)maxAudioChannels, g_audioChannelCount);
					dispatch_printf(printDispatchQueue, "Using input device: %s\n", getDeckLinkDisplayName(deckLink).c_str());
				}

				// If input device is half duplex, skip output discovery
				if (!deckLinkOutput && ((BMDDuplexMode)duplexMode == bmdDuplexHalf))
					continue;
			}

			if (!deckLinkOutput && (((BMDVideoIOSupport)videoIOSupport & bmdDeviceSupportsPlayback) != 0))
			{
				int64_t minimumPrerollFrames;
				if (deckLinkAttributes->GetInt(BMDDeckLinkMinimumPrerollFrames, &minimumPrerollFrames) != S_OK)
				{
					fprintf(stderr, "Failed to get the minumum required number of pre-roll frames\n");
					continue;
				}

				if (kOutputVideoPreroll < minimumPrerollFrames)
				{
					dispatch_printf(printDispatchQueue, "Warning: Specified video output preroll size is smaller than the minimum supported size; Changing preroll size from %d to %d.\n", kOutputVideoPreroll, minimumPrerollFrames);
				}
				
				int prerollFrames = std::max((int)minimumPrerollFrames, kOutputVideoPreroll);

				try
				{
					deckLinkOutput = make_com_ptr<DeckLinkOutputDevice>(deckLink, prerollFrames, kDisplayTimescale);
				}
				catch (const std::exception& e)
				{
					fprintf(stderr, "%s\n", e.what());
					continue;
				}
				g_audioChannelCount = std::min((uint32_t)maxAudioChannels, g_audioChannelCount);
				
				dispatch_printf(printDispatchQueue, "Using output device: %s\n", getDeckLinkDisplayName(deckLink).c_str());
			}
		}

		deckLink = nullptr;

		if (deckLinkInput && deckLinkOutput)
			break;
	}

	if (!deckLinkInput || !deckLinkOutput)
	{
		fprintf(stderr, "Unable to find both active input and output devices\n");
		return E_FAIL;
	}

	std::mutex formatDescMutex;
	FormatDescription formatDesc = { kInitialDisplayMode, false, kInitialPixelFormat };

	// Monitor for keypress when user wants to exit
	std::thread userInputThread = std::thread([&] {
		getchar();
		deckLinkOutput->cancelWaitForReference();
		g_loopThroughSessionNotifier.notify();
	});

	while (!g_loopThroughSessionNotifier.isNotified())
	{
		FormatDescription currentFormatDesc;
		{
			std::lock_guard<std::mutex> lock(formatDescMutex);
			currentFormatDesc = formatDesc;
		}

		// Register input callbacks
		deckLinkInput->onVideoFormatChange([&](BMDDisplayMode displayMode, bool is3D, BMDPixelFormat pixelFormat)
		{
			{
				std::lock_guard<std::mutex> lock(formatDescMutex);
				formatDesc.displayMode = displayMode;
				formatDesc.is3D = is3D;
				formatDesc.pixelFormat = pixelFormat;
			}
			deckLinkOutput->cancelWaitForReference();
			g_loopThroughSessionNotifier.condition.notify_all();
		});

		deckLinkInput->onVideoInputArrived([&](std::shared_ptr<LoopThroughVideoFrame> videoFrame) { videoDispatchQueue.dispatch(processVideo, videoFrame, deckLinkOutput); });
		deckLinkInput->onAudioInputArrived([&](std::shared_ptr<LoopThroughAudioPacket> audioPacket) { audioDispatchQueue.dispatch(processAudio, audioPacket, deckLinkOutput); });
		deckLinkInput->onVideoInputFrameDropped([&](BMDTimeValue streamTime, BMDTimeValue frameDuration, BMDTimeScale) { printDroppedCaptureFrame(streamTime, frameDuration, std::ref(printDispatchQueue)); });

		// Register output callbacks
		deckLinkOutput->onScheduledFrameCompleted([&](std::shared_ptr<LoopThroughVideoFrame> videoFrame) { updateCompletedFrameLatency(videoFrame, std::ref(printDispatchQueue)); });
		deckLinkOutput->onAudioPacketScheduled([&](std::shared_ptr<LoopThroughAudioPacket> audioPacket) { g_audioProcessingLatencyStatistics.addSample(audioPacket->getProcessingLatency()); });

		if (!deckLinkInput->startCapture(currentFormatDesc.displayMode, currentFormatDesc.is3D, currentFormatDesc.pixelFormat, kAudioSampleType, g_audioChannelCount))
		{
			fprintf(stderr, "Unable to enable input on the selected device\n");
			return E_ACCESSDENIED;
		}

		if (kWaitForReferenceToLock)
			dispatch_printf(printDispatchQueue, "Waiting for reference to lock...\n");

		if (!deckLinkOutput->startPlayback(currentFormatDesc.displayMode, currentFormatDesc.is3D, currentFormatDesc.pixelFormat, kAudioSampleType, g_audioChannelCount, kWaitForReferenceToLock))
		{
			std::lock_guard<std::mutex> lock(formatDescMutex);
			if (!g_loopThroughSessionNotifier.isNotified() && formatDesc == currentFormatDesc)
			{
				fprintf(stderr, "Unable to enable output on the selected device\n");
				return E_ACCESSDENIED;
			}
		}

		deckLinkInput->setReadyForCapture();

		printReferenceStatus(deckLinkOutput, printDispatchQueue);

		dispatch_printf(printDispatchQueue, "Starting input loop-through, press <RETURN> to stop/exit\n");

		if (kPrintRollingAverage)
		{
			g_printRollingAverageNotifier.reset();
			printRollingAverageThread = std::thread(printRollingAverage, std::ref(printDispatchQueue));
		}

		{
			std::unique_lock<std::mutex> lock(g_loopThroughSessionNotifier.mutex);

			g_loopThroughSessionNotifier.condition.wait(lock, [&] {
				return g_loopThroughSessionNotifier.isNotifiedLocked() || formatDesc != currentFormatDesc;
			});
		}

		// If we are in rolling average mode, cancel thread
		if (kPrintRollingAverage)
		{
			g_printRollingAverageNotifier.notify();
		
			if (printRollingAverageThread.joinable())
				printRollingAverageThread.join();
		}
	
		deckLinkInput->stopCapture();
		deckLinkOutput->stopPlayback();

		printOutputSummary(printDispatchQueue);

		// Reset statistics
		g_videoInputLatencyStatistics.reset();
		g_videoProcessingLatencyStatistics.reset();
		g_videoOutputLatencyStatistics.reset();
		g_audioProcessingLatencyStatistics.reset();

		g_frameCompletionResultCount.clear();
		g_outputFrameCount = 0;
		g_droppedOnCaptureFrameCount = 0;

		if (formatDesc != currentFormatDesc)
		{
			std::lock_guard<std::mutex> lock(formatDescMutex);

			com_ptr<IDeckLinkDisplayMode>	deckLinkDisplayMode;
			dlstring_t						displayModeNameStr;

			deckLinkOutput->getDeckLinkOutput()->GetDisplayMode(formatDesc.displayMode, deckLinkDisplayMode.releaseAndGetAddressOf());

			if (deckLinkDisplayMode->GetName(&displayModeNameStr) == S_OK)
			{
				try
				{
					dispatch_printf(printDispatchQueue,
									"\nLoop-through video format changed to %s %s%s\n",
									DlToCString(displayModeNameStr),
									formatDesc.is3D ? "3D " : "",
									kPixelFormats.at(formatDesc.pixelFormat));
				}
				catch (std::out_of_range) {}

				DeleteString(displayModeNameStr);
			}
			else
			{
				fprintf(stderr, "Unable to get new video format name\n");
			}
		}
	}

	if (userInputThread.joinable())
		userInputThread.join();

	dispatch_printf(printDispatchQueue, "\nInputLoopThrough complete\n\n");

	return result;
}

int main(int argc, const char * argv[])
{
	HRESULT		result;
	int			exitStatus = EXIT_FAILURE;

	// Initialize COM on this thread
	result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(result))
	{
		fprintf(stderr, "Initialization of COM failed - result = %08x.\n", result);
		return EXIT_FAILURE;
	}

	result = InputLoopThrough();
	if (result == S_OK)
		exitStatus = EXIT_SUCCESS;;

	// Shutdown COM on this thread
	CoUninitialize();

	return exitStatus;
}
