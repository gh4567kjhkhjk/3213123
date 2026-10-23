/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_local_pins.h"

#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "dialogs/dialogs_entry.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "storage/storage_account.h"
#include "telewhite/telewhite_mods.h"
#include "window/window_session_controller.h"

#include <QtCore/QDataStream>

namespace Telewhite {
namespace {

constexpr auto kVersion = quint32(1);
constexpr auto kMaxLocalPins = 1000;

[[nodiscard]] QByteArray Serialize(
		UserId userId,
		const std::vector<PeerId> &order) {
	auto result = QByteArray();
	auto stream = QDataStream(&result, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_1);
	stream << kVersion << quint64(userId.bare) << quint32(order.size());
	for (const auto &peerId : order) {
		stream << SerializePeerId(peerId);
	}
	return result;
}

[[nodiscard]] std::vector<PeerId> Deserialize(
		UserId userId,
		const QByteArray &serialized) {
	if (serialized.isEmpty()) {
		return {};
	}
	auto stream = QDataStream(serialized);
	stream.setVersion(QDataStream::Qt_5_1);
	auto version = quint32();
	auto storedUserId = quint64();
	auto count = quint32();
	stream >> version >> storedUserId >> count;
	if (stream.status() != QDataStream::Ok
		|| version != kVersion
		|| storedUserId != userId.bare
		|| count > quint32(kMaxLocalPins)) {
		return {};
	}
	auto result = std::vector<PeerId>();
	result.reserve(count);
	for (auto i = quint32(); i != count; ++i) {
		auto peerId = quint64();
		stream >> peerId;
		if (stream.status() != QDataStream::Ok) {
			return {};
		}
		result.push_back(DeserializePeerId(peerId));
	}
	return result;
}

// Never dropped with the session: sort keys may be asked during teardown.
[[nodiscard]] std::vector<PeerId> &Order(not_null<Main::Session*> session) {
	static auto loaded = base::flat_map<uint64, std::vector<PeerId>>();
	const auto key = session->uniqueId();
	auto i = loaded.find(key);
	if (i == end(loaded)) {
		const auto serialized = session->local().readTelewhiteData(
			Storage::TelewhiteData::LocalPins);
		i = loaded.emplace(
			key,
			Deserialize(session->userId(), serialized)).first;
	}
	return i->second;
}

} // namespace

int LocalPinIndex(not_null<const Dialogs::Entry*> entry) {
	const auto history = entry->asHistory();
	if (!history) {
		return 0;
	}
	const auto &order = Order(&history->session());
	const auto i = ranges::find(order, history->peer->id);
	return (i != end(order)) ? int(i - begin(order)) + 1 : 0;
}

bool IsLocallyPinned(not_null<const Dialogs::Entry*> entry) {
	return LocalPinIndex(entry) != 0;
}

bool IsPinnedInList(
		not_null<const Dialogs::Entry*> entry,
		FilterId filterId) {
	return entry->isPinnedDialog(filterId)
		|| (!filterId && IsLocallyPinned(entry));
}

void SetLocallyPinned(not_null<History*> history, bool pinned) {
	const auto session = &history->session();
	auto &order = Order(session);
	const auto peerId = history->peer->id;
	const auto i = ranges::find(order, peerId);
	if (pinned == (i != end(order))) {
		return;
	} else if (pinned) {
		order.insert(begin(order), peerId);
	} else {
		order.erase(i);
	}
	session->local().writeTelewhiteData(
		Storage::TelewhiteData::LocalPins,
		Serialize(session->userId(), order),
		false);

	const auto resort = order;
	const auto owner = &history->owner();
	for (const auto &id : resort) {
		if (const auto pinnedHistory = owner->historyLoaded(id)) {
			pinnedHistory->updateChatListSortPosition();
		}
	}
	if (!pinned) {
		history->updateChatListSortPosition();
	}
}

bool PinLocallyOverLimit(
		not_null<Window::SessionController*> controller,
		not_null<History*> history) {
	if (!Enabled(Mod::LocalPins)
		|| int(Order(&history->session()).size()) >= kMaxLocalPins) {
		return false;
	}
	SetLocallyPinned(history, true);
	controller->content()->dialogsToUp();
	controller->showToast(tr::lng_telewhite_local_pin_toast(tr::now));
	return true;
}

} // namespace Telewhite
