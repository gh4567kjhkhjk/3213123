/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_streamer_mode.h"

#include "platform/platform_specific.h"
#include "telewhite/telewhite_mods.h"

#include <QtWidgets/QApplication>

namespace Telewhite {
namespace {

constexpr auto kRequestedProperty = "telewhiteCaptureProtectionRequested";

class WinIdChangeFilter final : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

};

void Refresh(not_null<QWidget*> window) {
	Platform::SetWindowScreenshotProtection(
		window,
		window->property(kRequestedProperty).toBool());
}

// WinIdChange comes for new native windows and for the ones Qt recreates
// on window flag changes, which drops the display affinity.
bool WinIdChangeFilter::eventFilter(QObject *watched, QEvent *event) {
	if (event->type() == QEvent::WinIdChange && watched->isWidgetType()) {
		const auto widget = static_cast<QWidget*>(watched);
		if (widget->isWindow()) {
			Refresh(widget);
		}
	}
	return QObject::eventFilter(watched, event);
}

} // namespace

void SetupStreamerMode(rpl::lifetime &lifetime) {
	const auto filter = lifetime.make_state<WinIdChangeFilter>();
	EnabledValue(
		Mod::StreamerMode
	) | rpl::on_next([=](bool enabled) {
		if (enabled) {
			qApp->installEventFilter(filter);
		} else {
			qApp->removeEventFilter(filter);
		}
		for (const auto widget : QApplication::topLevelWidgets()) {
			Refresh(widget);
		}
	}, lifetime);
}

bool StreamerModeProtection(not_null<QWidget*> window, bool requested) {
	if (window->property(kRequestedProperty).toBool() != requested) {
		window->setProperty(kRequestedProperty, requested);
	}
	return requested || Enabled(Mod::StreamerMode);
}

} // namespace Telewhite
