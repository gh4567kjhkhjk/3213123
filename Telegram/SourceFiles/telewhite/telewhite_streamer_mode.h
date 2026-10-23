/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Telewhite {

void SetupStreamerMode(rpl::lifetime &lifetime);

// Platform code passes the protection a window asked for and applies
// the result: streamer mode keeps every window hidden from capture and
// the remembered request is restored once the mode is turned off.
[[nodiscard]] bool StreamerModeProtection(
	not_null<QWidget*> window,
	bool requested);

} // namespace Telewhite
