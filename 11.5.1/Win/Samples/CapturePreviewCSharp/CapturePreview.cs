﻿/* -LICENSE-START-
** Copyright (c) 2017 Blackmagic Design
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
using System.Windows.Forms;
using System.Runtime.InteropServices;
using DeckLinkAPI;

namespace CapturePreviewCSharp
{
    public partial class CapturePreview : Form
    {
        private DeckLinkDeviceDiscovery     m_deckLinkDiscovery;
        private DeckLinkDevice              m_selectedDevice;

        public CapturePreview()
        {
            InitializeComponent();

            m_deckLinkDiscovery = new DeckLinkDeviceDiscovery();

            previewWindow.InitD3D();

            m_deckLinkDiscovery.DeviceArrived += new DeckLinkDiscoveryHandler((d) => this.Invoke((Action)(() => AddDevice(d))));
            m_deckLinkDiscovery.DeviceRemoved += new DeckLinkDiscoveryHandler((d) => this.Invoke((Action)(() => RemoveDevice(d))));
        }

        void AddDevice(IDeckLink decklinkDevice)
        {
            DeckLinkDevice deckLink = new DeckLinkDevice(decklinkDevice);

            if (deckLink.deckLinkInput != null)
            {
                comboBoxInputDevice.BeginUpdate();
                comboBoxInputDevice.Items.Add(new StringObjectPair<DeckLinkDevice>(deckLink.deviceName, deckLink));
                comboBoxInputDevice.EndUpdate();

                if (comboBoxInputDevice.Items.Count == 1)
                {
                    comboBoxInputDevice.SelectedIndex = 0;

                    EnableInterface(true);
                    buttonStartStop.Enabled = true;
                }
            }
        }

        void RemoveDevice(IDeckLink decklinkDevice)
        {
            // Stop capture if the selected device was removed
            if (m_selectedDevice != null && m_selectedDevice.deckLink == decklinkDevice)
            {
                if (m_selectedDevice.isCapturing)
                    StopCapture();

                comboBoxInputDevice.SelectedIndex = -1;
                m_selectedDevice = null;
            }

            // Remove the device from the dropdown
            comboBoxInputDevice.BeginUpdate();
            foreach (StringObjectPair<DeckLinkDevice> item in comboBoxInputDevice.Items)
            {
                if (item.value.deckLink == decklinkDevice)
                {
                    comboBoxInputDevice.Items.Remove(item);
                    break;
                }
            }
            comboBoxInputDevice.EndUpdate();

            if (comboBoxInputDevice.Items.Count == 0)
            {
                buttonStartStop.Enabled = false;
                EnableInterface(false);
            }
        }

        private void CapturePreview_Load(object sender, EventArgs e)
        {
            buttonStartStop.Enabled = false;
            EnableInterface(false);

            m_deckLinkDiscovery.Enable();
        }

        private void buttonStartStop_Click(object sender, EventArgs e)
        {
            if (m_selectedDevice == null)
                return;

            if (m_selectedDevice.isCapturing)
                StopCapture();
            else
                StartCapture();
        }

        private void StartCapture()
        {
            if (comboBoxVideoFormat.SelectedIndex < 0)
                return;

            var displayMode = ((DisplayModeEntry)comboBoxVideoFormat.SelectedItem).displayMode;

            m_selectedDevice.InputSignalChanged += new DeckLinkInputSignalHandler((v) => this.Invoke((Action)(() => { labelInvalidInput.Visible = v; })));
            m_selectedDevice.InputFormatChanged += new DeckLinkFormatChangedHandler((m) => this.Invoke((Action)(() => { DisplayModeChanged(m); })));

            if (m_selectedDevice != null)
                m_selectedDevice.StartCapture(displayMode, previewWindow, checkBoxAutodetectFormat.Checked);

            // Update UI
            buttonStartStop.Text = "Stop Capture";
            EnableInterface(false);
        }

        private void DisplayModeChanged(IDeckLinkDisplayMode newDisplayMode)
        {
            foreach (DisplayModeEntry item in comboBoxVideoFormat.Items)
            {
                if (item.displayMode.GetDisplayMode() == newDisplayMode.GetDisplayMode())
                    comboBoxVideoFormat.SelectedItem = item;
            }
        }

        private void StopCapture()
        {
            if (m_selectedDevice != null)
                m_selectedDevice.StopCapture();

            // Update UI
            buttonStartStop.Text = "Start Capture";
            EnableInterface(true);
            labelInvalidInput.Visible = false;
        }

        private void comboBoxInputDevice_SelectedValueChanged(object sender, EventArgs e)
        {
            m_selectedDevice = null;

            if (comboBoxInputDevice.SelectedIndex < 0)
                return;

            m_selectedDevice = ((StringObjectPair<DeckLinkDevice>)comboBoxInputDevice.SelectedItem).value;

            // Update the video mode popup menu
            RefreshVideoModeList();

            // Enable the interface
            EnableInterface(true);

            checkBoxAutodetectFormat.Checked = m_selectedDevice.supportsFormatDetection;
        }

        private void EnableInterface(bool enabled)
        {
            comboBoxInputDevice.Enabled = enabled;
            comboBoxVideoFormat.Enabled = enabled;

            checkBoxAutodetectFormat.Enabled = enabled;
            if (enabled && m_selectedDevice != null && !m_selectedDevice.supportsFormatDetection)
            {
                checkBoxAutodetectFormat.Enabled = false;
                checkBoxAutodetectFormat.Checked = false;
            }
        }

        private void RefreshVideoModeList()
        {
            comboBoxVideoFormat.BeginUpdate();
            comboBoxVideoFormat.Items.Clear();

            foreach (IDeckLinkDisplayMode displayMode in m_selectedDevice)
                comboBoxVideoFormat.Items.Add(new DisplayModeEntry(displayMode));

            comboBoxVideoFormat.SelectedIndex = 0;
            comboBoxVideoFormat.EndUpdate();
        }

        /// <summary>
        /// Used for putting the IDeckLinkDisplayMode objects into the video format
        /// combo box.
        /// </summary>
        struct DisplayModeEntry
        {
            public IDeckLinkDisplayMode displayMode;

            public DisplayModeEntry(IDeckLinkDisplayMode displayMode)
            {
                this.displayMode = displayMode;
            }

            public override string ToString()
            {
                string str;

                displayMode.GetName(out str);

                return str;
            }
        }

        /// <summary>
        /// Used for putting other object types into combo boxes.
        /// </summary>
        struct StringObjectPair<T>
        {
            public string name;
            public T value;

            public StringObjectPair(string name, T value)
            {
                this.name = name;
                this.value = value;
            }

            public override string ToString()
            {
                return name;
            }
        }
    }
}
