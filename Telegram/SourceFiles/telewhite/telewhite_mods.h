/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;

namespace Core {
class Settings;
} // namespace Core

namespace Telewhite {

enum class Mod {
	GhostRead,
	GhostTyping,
	GhostOnline,
	GhostStories,
	SaveDeleted,
	EditHistory,
	ForwardNoAuthor,
	MultiForward,
	LocalPins,
	HideSponsored,
	HideEmojiStatus,
	PreciseLastSeen,
	RegistrationDate,
	Snowfall,

	kCount,
};

void LoadMods(not_null<Core::Settings*> settings);

[[nodiscard]] bool Enabled(Mod mod);
[[nodiscard]] rpl::producer<bool> EnabledValue(Mod mod);
void SetEnabled(Mod mod, bool enabled);

// Ghost mode keeps "listened" of voice / round messages local,
// but self-destructing media are still reported as opened.
[[nodiscard]] bool SkipContentsRead(not_null<HistoryItem*> item);

[[nodiscard]] QDate EstimateRegistrationDate(uint64 userId);
[[nodiscard]] QString RegistrationDateText(uint64 userId);
[[nodiscard]] QString FormatExactDateTime(TimeId date);

} // namespace Telewhite
