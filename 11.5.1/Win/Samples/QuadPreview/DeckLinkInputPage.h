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

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <functional>

#include "DeckLinkInputDevice.h"
#include "DeckLinkOpenGLWidget.h"
#include "com_ptr.h"

class DeckLinkInputPage : public QWidget
{
	Q_OBJECT

public:
	DeckLinkInputPage();
	virtual ~DeckLinkInputPage();

	void setPreviewSize(QSize previewSize);

	void customEvent(QEvent* event) override;

	void startCapture(void);

	void addDevice(com_ptr<IDeckLink>& deckLink, bool deviceIsActive);
	void removeDevice(com_ptr<IDeckLink>& deckLink);
	void enableDevice(com_ptr<IDeckLink>& deckLink, bool enable);
	bool releaseDeviceIfSelected(com_ptr<IDeckLink>& deckLink);

	DeckLinkOpenGLWidget*			getPreviewView(void) const { return m_previewView; }
	com_ptr<DeckLinkInputDevice>	getSelectedDevice(void) const { return m_selectedDevice; }

public slots:
	void inputDeviceChanged(int selectedDeviceIndex);
	void inputConnectionChanged(int selectedConnectionIndex);
	void videoFormatChanged(int selectedVideoFormatIndex);
	void autoDetectChanged(int autoDetectState);
	void requestedDeviceGranted(com_ptr<IDeckLink>& device);

signals:
	void requestDeckLink(com_ptr<IDeckLink>& device);
	void requestDeckLinkIfAvailable(com_ptr<IDeckLink>& device);
	void relinquishDeckLink(com_ptr<IDeckLink>& device);

private:
	void restartCapture(void);
	void detectedVideoFormatChanged(BMDDisplayMode displayMode);
	void selectedDeviceChanged(void);
	void refreshInputConnectionMenu(void);
	void refreshDisplayModeMenu(void);

	com_ptr<DeckLinkInputDevice>	m_selectedDevice;
	DeckLinkOpenGLWidget*			m_previewView;

	QFormLayout*	m_formLayout;
	QComboBox*		m_deviceListCombo;
	QComboBox*		m_inputConnectionCombo;
	QComboBox*		m_videoFormatCombo;
	QCheckBox*		m_autoDetectCheckBox;

};
