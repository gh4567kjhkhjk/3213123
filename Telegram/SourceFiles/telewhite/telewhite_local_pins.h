/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class History;

namespace Dialogs {
class Entry;
} // namespace Dialogs

namespace Window {
class SessionController;
} // namespace Window

namespace Telewhite {

// 1-based position among chats pinned only on this computer, or 0.
[[nodiscard]] int LocalPinIndex(not_null<const Dialogs::Entry*> entry);
[[nodiscard]] bool IsLocallyPinned(not_null<const Dialogs::Entry*> entry);
[[nodiscard]] bool IsPinnedInList(
	not_null<const Dialogs::Entry*> entry,
	FilterId filterId);
void SetLocallyPinned(not_null<History*> history, bool pinned);

[[nodiscard]] bool PinLocallyOverLimit(
	not_null<Window::SessionController*> controller,
	not_null<History*> history);

} // namespace Telewhite
