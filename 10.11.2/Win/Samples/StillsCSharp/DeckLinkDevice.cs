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
using DeckLinkAPI;

namespace StillsCSharp
{
    public class DeckLinkDevice
    {
        private IDeckLink m_deckLink;

        public DeckLinkDevice(IDeckLink deckLink)
        {
            m_deckLink = deckLink;
        }

        public IDeckLink DeckLink
        {
            get { return m_deckLink; }
        }
        
        public string DeviceName
        {
            get
            {
                string deviceName;
                m_deckLink.GetDisplayName(out deviceName);
                return deviceName;
            }
        }

        public bool CaptureDevice
        {
            get { return IoSupport.HasFlag(_BMDVideoIOSupport.bmdDeviceSupportsCapture); }
        }

        public bool PlaybackDevice
        {
            get { return IoSupport.HasFlag(_BMDVideoIOSupport.bmdDeviceSupportsPlayback); }
        }

        public bool SupportsFormatDetection
        {
            get
            {
                int flag;
                var deckLinkAttributes = (IDeckLinkAttributes)m_deckLink;
                deckLinkAttributes.GetFlag(_BMDDeckLinkAttributeID.BMDDeckLinkSupportsInputFormatDetection, out flag);
                return flag != 0;
            }
        }

        private _BMDVideoIOSupport IoSupport
        {
            get
            {
                long ioSupportAttribute;

                var deckLinkAttributes = (IDeckLinkAttributes)m_deckLink;
                deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkVideoIOSupport, out ioSupportAttribute);
                return (_BMDVideoIOSupport)ioSupportAttribute;
            }
        }
    }
}
