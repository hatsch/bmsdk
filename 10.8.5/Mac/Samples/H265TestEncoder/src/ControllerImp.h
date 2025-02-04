/* -LICENSE-START-
 ** Copyright (c) 2015 Blackmagic Design
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

#include <vector>
#include <QObject>

class DeckLinkDeviceDiscovery;
class DeckLinkDevice;
class IDeckLink;

class ControllerImp : public QObject
{
	Q_OBJECT

public:

	enum 
	{
		kNoError = 0,
		kUnknownError = 1,
		kInvalidOpError = 2,
	};

	ControllerImp();
	virtual ~ControllerImp();

	void	init(QObject* uiDelegate);

	int		stopCapture(bool deleteFile = false);
	void	addDevice(IDeckLink* device);
	void	removeDevice(IDeckLink* device);

	void	newDeviceSelected();

	void	showErrorMessage(const QString& title, const QString& msg);

	bool	shouldRestartCaptureWithNewVideoMode();
	void	selectDetectedVideoModeWithIndex(uint32_t index);

	QObject* m_uiDelegate;
	DeckLinkDeviceDiscovery* m_deckLinkDiscovery;
	
	typedef std::vector<DeckLinkDevice*> DeviceList;
	DeviceList m_deviceList;
	int m_selectedDevice;
	int m_selectedMode;
	int m_currentBitrate;

public slots:
	int		startStopCapture();
	int		startStopCapture(uint32_t);
	int		changeTargetRate(int rate);
	void	onDisplayErrorMessage(const QString&, const QString&);

signals:
	void	recordingStarted(QString, uint32_t);
	void	recordingFinished(void);
	void	displayErrorMessage(const QString&, const QString&);
	void	restartCapture(uint32_t);

private:
	int		startCapture(uint32_t);

private slots:
	void	restartCaptureRequested(uint32_t);
};
