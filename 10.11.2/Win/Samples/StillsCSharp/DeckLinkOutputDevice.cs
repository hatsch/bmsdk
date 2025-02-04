﻿/* -LICENSE-START-
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

using System;
using System.Collections.Generic;

using DeckLinkAPI;

namespace StillsCSharp
{
    class DeckLinkOutputInvalidException : Exception { }
    class DeckLinkOutputNotEnabledException : Exception { }

    public class DeckLinkOutputFrameCompletitonEventArgs : EventArgs
    {
        public readonly _BMDOutputFrameCompletionResult completionResult;

        public DeckLinkOutputFrameCompletitonEventArgs(_BMDOutputFrameCompletionResult completionResult)
        {
            this.completionResult = completionResult;
        }
    }

    public class DeckLinkOutputDevice : DeckLinkDevice, IDeckLinkVideoOutputCallback, IDeckLinkAudioOutputCallback, IEnumerable<IDeckLinkDisplayMode>
    {
        private IDeckLinkOutput m_deckLinkOutput;

        private long m_frameDuration;
        private long m_frameTimescale;

        private bool m_videoOutputEnabled = false;
        
        public DeckLinkOutputDevice(IDeckLink deckLink) : base(deckLink)
        {
            if (!PlaybackDevice)
                throw new DeckLinkOutputInvalidException();

            // Query output interface
            m_deckLinkOutput = (IDeckLinkOutput)deckLink;
            
            // Provide the delegate to the audio and video output interfaces
            m_deckLinkOutput.SetScheduledFrameCompletionCallback(this);
            m_deckLinkOutput.SetAudioCallback(this);
        }

        public event EventHandler<DeckLinkOutputFrameCompletitonEventArgs> VideoFrameCompletedHandler;
        public event EventHandler PlaybackStoppedHandler;
        public event EventHandler AudioOutputRequestedHandler;
        
        public IDeckLinkOutput DeckLinkOutput
        {
            get { return m_deckLinkOutput; }
        }

        public double FrameDurationMs 
        {
            get { return 1000 * (double) m_frameDuration / (double) m_frameTimescale; }
        }

        IEnumerator<IDeckLinkDisplayMode> IEnumerable<IDeckLinkDisplayMode>.GetEnumerator()
        {
            IDeckLinkDisplayModeIterator displayModeIterator;
            m_deckLinkOutput.GetDisplayModeIterator(out displayModeIterator);
            return new DisplayModeEnum(displayModeIterator);
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            throw new InvalidOperationException();
        }

        public void EnableAudioOutput(uint audioChannelCount, _BMDAudioSampleType audioSampleDepth)
        {
            // Set the audio output mode
            m_deckLinkOutput.EnableAudioOutput(_BMDAudioSampleRate.bmdAudioSampleRate48kHz, audioSampleDepth, audioChannelCount, _BMDAudioOutputStreamType.bmdAudioOutputStreamContinuous);

            // Begin audio preroll.  This will begin calling our audio callback, which will start the DeckLink output stream.
            m_deckLinkOutput.BeginAudioPreroll();
        }

        public void EnableVideoOutput(IDeckLinkDisplayMode videoDisplayMode)
        {
            videoDisplayMode.GetFrameRate(out m_frameDuration, out m_frameTimescale);

            // Set the video output mode
            m_deckLinkOutput.EnableVideoOutput(videoDisplayMode.GetDisplayMode(), _BMDVideoOutputFlags.bmdVideoOutputFlagDefault);

            m_videoOutputEnabled = true;
        }

        public void DisableVideoOutput()
        {
            m_deckLinkOutput.DisableVideoOutput();

            m_videoOutputEnabled = false;
        }

        public void DisableAudioOutput()
        {
            m_deckLinkOutput.DisableAudioOutput();
        }

        public void DisplayVideoFrame(IDeckLinkVideoFrame videoFrame)
        {
            if (!m_videoOutputEnabled)
                throw new DeckLinkOutputNotEnabledException();

            m_deckLinkOutput.DisplayVideoFrameSync(videoFrame);
        }

        public bool IsVideoModeSupported(IDeckLinkDisplayMode displayMode, _BMDPixelFormat pixelFormat)
        {
            _BMDDisplayModeSupport displayModeSupport;
            IDeckLinkDisplayMode resultDisplayMode;

            m_deckLinkOutput.DoesSupportVideoMode(displayMode.GetDisplayMode(), pixelFormat, _BMDVideoOutputFlags.bmdVideoOutputFlagDefault, out displayModeSupport, out resultDisplayMode);

            return (displayModeSupport != _BMDDisplayModeSupport.bmdDisplayModeNotSupported);
        }

        #region callbacks
        // Explicit implementation of IDeckLinkVideoOutputCallback and IDeckLinkAudioOutputCallback
        void IDeckLinkVideoOutputCallback.ScheduledFrameCompleted(IDeckLinkVideoFrame completedFrame, _BMDOutputFrameCompletionResult result)
        {
            // When a video frame has been completed, generate event to schedule next frame
            var handler = VideoFrameCompletedHandler;

            // Check whether any subscribers to VideoFrameCompletedHandler event
            if (handler != null)
            {
                handler(this, new DeckLinkOutputFrameCompletitonEventArgs(result));
            }
        }

        void IDeckLinkVideoOutputCallback.ScheduledPlaybackHasStopped()
        {
            var handler = PlaybackStoppedHandler;

            // Check whether any subscribers to PlaybackStoppedHandler event
            if (handler != null)
            {
                handler(this, EventArgs.Empty);
            }
        }

        void IDeckLinkAudioOutputCallback.RenderAudioSamples(int preroll)
        {
            // Provide further audio samples to the DeckLink API until our preferred buffer waterlevel is reached
            var handler = AudioOutputRequestedHandler;

            // Check whether any subscribers to AudioOutputRequestedHandler event
            if (handler != null)
            {
                handler(this, EventArgs.Empty);
            }

            if (preroll != 0)
            {
                m_deckLinkOutput.StartScheduledPlayback(0, 100, 1.0);
            }
        }
        #endregion
    }

}
