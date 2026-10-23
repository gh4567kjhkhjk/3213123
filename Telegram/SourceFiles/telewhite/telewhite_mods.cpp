/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_mods.h"

#include "base/unixtime.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_media_types.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"

namespace Telewhite {
namespace {

constexpr auto kModsCount = int(Mod::kCount);

[[nodiscard]] std::string_view Key(Mod mod) {
	switch (mod) {
	case Mod::GhostRead: return "telewhite-ghost-read";
	case Mod::GhostTyping: return "telewhite-ghost-typing";
	case Mod::GhostOnline: return "telewhite-ghost-online";
	case Mod::GhostStories: return "telewhite-ghost-stories";
	case Mod::SaveDeleted: return "telewhite-save-deleted";
	case Mod::EditHistory: return "telewhite-edit-history";
	case Mod::ForwardNoAuthor: return "telewhite-forward-no-author";
	case Mod::MultiForward: return "telewhite-multi-forward";
	case Mod::LocalPins: return "telewhite-local-pins";
	case Mod::HideSponsored: return "telewhite-hide-sponsored";
	case Mod::HideEmojiStatus: return "telewhite-hide-emoji-status";
	case Mod::PreciseLastSeen: return "telewhite-precise-last-seen";
	case Mod::RegistrationDate: return "telewhite-registration-date";
	case Mod::Snowfall: return "telewhite-snowfall";
	case Mod::kCount: break;
	}
	Unexpected("Mod in Telewhite::Key.");
}

[[nodiscard]] std::array<rpl::variable<bool>, kModsCount> &Values() {
	static auto result = std::array<rpl::variable<bool>, kModsCount>();
	return result;
}

struct RegistrationPoint {
	uint64 id = 0;
	int64 date = 0;
};

// Approximate (user id -> registration unixtime) checkpoints.
constexpr auto kRegistrationPoints = std::array<RegistrationPoint, 33>{ {
	{ 1000000ULL, 1376438400 },
	{ 2768409ULL, 1383264000 },
	{ 7679610ULL, 1388448000 },
	{ 11538514ULL, 1391212000 },
	{ 15835244ULL, 1392940000 },
	{ 23646077ULL, 1393459000 },
	{ 38015510ULL, 1393632000 },
	{ 44634663ULL, 1399334000 },
	{ 54845238ULL, 1411257000 },
	{ 63263518ULL, 1414454000 },
	{ 101260938ULL, 1425600000 },
	{ 122600695ULL, 1437782000 },
	{ 133909606ULL, 1444176000 },
	{ 157242073ULL, 1446768000 },
	{ 171295414ULL, 1457481000 },
	{ 181783990ULL, 1460246000 },
	{ 222021233ULL, 1465344000 },
	{ 278941742ULL, 1473465000 },
	{ 294851037ULL, 1479600000 },
	{ 328594461ULL, 1482969000 },
	{ 352940995ULL, 1487894000 },
	{ 369669043ULL, 1490918000 },
	{ 400169472ULL, 1501459000 },
	{ 805158066ULL, 1563208000 },
	{ 1974255900ULL, 1634000000 },
	{ 2147483647ULL, 1638316800 },
	{ 5000000000ULL, 1648771200 },
	{ 5500000000ULL, 1661990400 },
	{ 6000000000ULL, 1677628800 },
	{ 6500000000ULL, 1693526400 },
	{ 7000000000ULL, 1709251200 },
	{ 7500000000ULL, 1725148800 },
	{ 8000000000ULL, 1740787200 },
} };

} // namespace

void LoadMods(not_null<Core::Settings*> settings) {
	auto &values = Values();
	for (auto i = 0; i != kModsCount; ++i) {
		values[i] = settings->readPref<bool>(Key(Mod(i)), false);
	}
}

bool Enabled(Mod mod) {
	return Values()[int(mod)].current();
}

rpl::producer<bool> EnabledValue(Mod mod) {
	return Values()[int(mod)].value();
}

void SetEnabled(Mod mod, bool enabled) {
	Values()[int(mod)] = enabled;
	Core::App().settings().writePref<bool>(Key(mod), enabled);
	Core::App().saveSettingsDelayed();
}

QDate EstimateRegistrationDate(uint64 userId) {
	if (!userId) {
		return QDate();
	}
	const auto &points = kRegistrationPoints;
	auto date = int64(0);
	if (userId <= points.front().id) {
		date = points.front().date;
	} else if (userId >= points.back().id) {
		const auto &a = points[points.size() - 2];
		const auto &b = points.back();
		const auto slope = double(b.date - a.date) / double(b.id - a.id);
		date = b.date + int64(slope * double(userId - b.id));
	} else {
		for (auto i = 1; i != int(points.size()); ++i) {
			const auto &b = points[i];
			if (userId > b.id) {
				continue;
			}
			const auto &a = points[i - 1];
			const auto part = double(userId - a.id) / double(b.id - a.id);
			date = a.date + int64(part * double(b.date - a.date));
			break;
		}
	}
	date = std::min(date, int64(base::unixtime::now()));
	return base::unixtime::parse(TimeId(date)).date();
}

QString RegistrationDateText(uint64 userId) {
	const auto date = EstimateRegistrationDate(userId);
	if (!date.isValid()) {
		return QString();
	}
	return u"~ "_q + langMonthOfYearFull(date.month(), date.year());
}

QString FormatExactDateTime(TimeId date) {
	return base::unixtime::parse(date).toString(u"dd.MM.yyyy HH:mm:ss"_q);
}

bool SkipContentsRead(not_null<HistoryItem*> item) {
	if (!Enabled(Mod::GhostRead) || !item->isIncomingUnreadMedia()) {
		return false;
	}
	const auto media = item->media();
	return !media || !media->ttlSeconds();
}

} // namespace Telewhite
