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

#include <atomic>
#include <functional>
#include "com_ptr.h"
#include "CapturePreviewEvents.h"
#include "DeckLinkAPI.h"

class ProfileCallback : public IDeckLinkProfileCallback
{
	using ProfileChangingCallback = std::function<void(void)>;

public:
	ProfileCallback(QObject* owner);
	virtual ~ProfileCallback() = default;

	// IDeckLinkProfileCallback interface
	HRESULT		ProfileChanging(IDeckLinkProfile *profileToBeActivated, bool streamsWillBeForcedToStop) override;
	HRESULT		ProfileActivated(IDeckLinkProfile *activatedProfile) override;

	// IUnknown interface
	HRESULT		QueryInterface(REFIID iid, LPVOID *ppv) override;
	ULONG		AddRef() override;
	ULONG		Release() override;

	void		onProfileChanging(const ProfileChangingCallback& callback) { m_profileChangingCallback = callback; }

private:
	QObject*					m_owner;
	std::atomic<ULONG>			m_refCount;
	ProfileChangingCallback		m_profileChangingCallback;
};

class ProfileActivatedEvent : public QEvent
{
public:
	ProfileActivatedEvent(IDeckLinkProfile* deckLinkProfile) :
		QEvent(kProfileActivatedEvent), m_deckLinkProfile(deckLinkProfile) {}
	virtual ~ProfileActivatedEvent() = default;

	com_ptr<IDeckLinkProfile> deckLinkProfile() const { return m_deckLinkProfile; }

private:
	com_ptr<IDeckLinkProfile> m_deckLinkProfile;
};
