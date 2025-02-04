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

#import "CoreMediaPCMAudioBuffer.h"

CoreMediaPCMAudioBuffer::CoreMediaPCMAudioBuffer(CMSampleBufferRef sampleBuffer) :
m_sampleBuffer(sampleBuffer)
{
	if (m_sampleBuffer)
		CFRetain(m_sampleBuffer);
}

CoreMediaPCMAudioBuffer::~CoreMediaPCMAudioBuffer()
{
	if (m_sampleBuffer)
	{
		CFRelease(m_sampleBuffer);
		m_sampleBuffer = nullptr;
	}
}


bool CoreMediaPCMAudioBuffer::read(const AudioBufferReader& audioBufferReader)
{
	CMBlockBufferRef	blockBuffer;
	AudioBufferList		audioBufferList;
	bool				success = false;

	if (!m_sampleBuffer)
		return success;

	const AudioStreamBasicDescription* asbd = CMAudioFormatDescriptionGetStreamBasicDescription(CMSampleBufferGetFormatDescription(m_sampleBuffer));
	
	CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(m_sampleBuffer,
															nullptr,
															&audioBufferList,
															sizeof(audioBufferList),
															nullptr,
															nullptr,
															0,
															&blockBuffer);

	for (uint32_t bufferCount = 0; bufferCount < audioBufferList.mNumberBuffers; ++bufferCount)
	{
		void*		dataBuffer	= audioBufferList.mBuffers[bufferCount].mData;
		uint32_t	frameCount	= audioBufferList.mBuffers[bufferCount].mDataByteSize / asbd->mBytesPerFrame;

		if (!dataBuffer || frameCount == 0)
			continue;

		// Process the audio data
		success = audioBufferReader(dataBuffer, frameCount);
		if (!success)
			break;
	}

	CFRelease(blockBuffer);
	return success;
}
