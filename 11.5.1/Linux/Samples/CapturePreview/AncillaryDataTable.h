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

#include <QAbstractTableModel>
#include <QMutex>
#include <QStringList>

enum class AncillaryHeader : int { Types, Values };
const int kAncillaryTableColumnCount = 2;

const QStringList kAncillaryDataTypes = {
	"VITC Timecode field 1",
	"VITC User bits field 1",
	"VITC Timecode field 2",
	"VITC User bits field 2",
	"RP188 VITC1 Timecode",
	"RP188 VITC1 User bits",
	"RP188 VITC2 Timecode",
	"RP188 VITC2 User bits",
	"RP188 LTC Timecode",
	"RP188 LTC User bits",
	"RP188 HFRTC Timecode",
	"RP188 HFRTC User bits",
};

const QStringList kMetadataTypes = {
	"Static HDR Electro-optical Transfer Function",
	"Static HDR Display Primaries Red X",
	"Static HDR Display Primaries Red Y",
	"Static HDR Display Primaries Green X",
	"Static HDR Display Primaries Green Y",
	"Static HDR Display Primaries Blue X",
	"Static HDR Display Primaries Blue Y",
	"Static HDR White Point X",
	"Static HDR White Point Y",
	"Static HDR Max Display Mastering Luminance",
	"Static HDR Min Display Mastering Luminance",
	"Static HDR Max Content Light Level",
	"Static HDR Max Frame Average Light Level",
	"Static Colorspace",
};

typedef struct {
	// VITC timecodes and user bits for field 1 & 2
	QString vitcF1Timecode;
	QString vitcF1UserBits;
	QString vitcF2Timecode;
	QString vitcF2UserBits;

	// RP188 timecodes and user bits (VITC1, VITC2, LTC and HFRTC)
	QString rp188vitc1Timecode;
	QString rp188vitc1UserBits;
	QString rp188vitc2Timecode;
	QString rp188vitc2UserBits;
	QString rp188ltcTimecode;
	QString rp188ltcUserBits;
	QString rp188hfrtcTimecode;
	QString rp188hfrtcUserBits;
} AncillaryDataStruct;

typedef struct {
	QString electroOpticalTransferFunction;
	QString displayPrimariesRedX;
	QString displayPrimariesRedY;
	QString displayPrimariesGreenX;
	QString displayPrimariesGreenY;
	QString displayPrimariesBlueX;
	QString displayPrimariesBlueY;
	QString whitePointX;
	QString whitePointY;
	QString maxDisplayMasteringLuminance;
	QString minDisplayMasteringLuminance;
	QString maximumContentLightLevel;
	QString maximumFrameAverageLightLevel;
	QString colorspace;
} MetadataStruct;

class AncillaryDataTable : public QAbstractTableModel
{
	Q_OBJECT

public:
	AncillaryDataTable(QObject* parent = nullptr);
	virtual ~AncillaryDataTable() {}

	void UpdateFrameData(AncillaryDataStruct* newAncData, MetadataStruct* newMetadata);

	// QAbstractTableModel methods
	int			rowCount(const QModelIndex& parent = QModelIndex()) const override { Q_UNUSED(parent); return kAncillaryDataTypes.size() + kMetadataTypes.size(); }
	int			columnCount(const QModelIndex& parent = QModelIndex()) const override { Q_UNUSED(parent); return kAncillaryTableColumnCount; }
	QVariant	data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant	headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
	QMutex			m_updateMutex;
	QStringList		m_ancillaryDataValues;
	QStringList		m_metadataValues;
};

