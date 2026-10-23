/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_archive.h"

#include "base/timer.h"
#include "base/unixtime.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_photo.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "storage/storage_account.h"
#include "telewhite/telewhite_deleted_media.h"

#include <QtCore/QDataStream>

namespace Telewhite {
namespace {

constexpr auto kArchiveVersion = quint32(2);
constexpr auto kDeletedPhotoFlag = quint32(0x01);
constexpr auto kDeletedNoForwardsFlag = quint32(0x02);
constexpr auto kMaxDeleted = 5000;
constexpr auto kMaxEdited = 3000;
constexpr auto kMaxVersions = 50;
constexpr auto kSaveDelay = crl::time(3000);

void WriteText(QDataStream &stream, const TextWithEntities &text) {
	stream
		<< text.text
		<< TextUtilities::SerializeTags(
			TextUtilities::ConvertEntitiesToTextTags(text.entities));
}

[[nodiscard]] TextWithEntities ReadText(QDataStream &stream) {
	auto text = QString();
	auto tags = QByteArray();
	stream >> text >> tags;
	auto entities = TextUtilities::ConvertTextTagsToEntities(
		TextUtilities::DeserializeTags(tags, text.size()));
	return { std::move(text), std::move(entities) };
}

class Archive final {
public:
	explicit Archive(not_null<Main::Session*> session);

	[[nodiscard]] bool mark(FullMsgId id);
	[[nodiscard]] bool marked(FullMsgId id) const;

	void rememberDeleted(PeerId peerId, DeletedMessage message);
	[[nodiscard]] bool hasDeleted(PeerId peerId);
	[[nodiscard]] std::vector<DeletedMessage> deleted(PeerId peerId);
	void clearDeleted(PeerId peerId);

	void rememberEditVersion(FullMsgId id, EditVersion version);
	[[nodiscard]] std::vector<EditVersion> editVersions(FullMsgId id);

	void flush();

private:
	struct DeletedEntry {
		PeerId peerId = 0;
		DeletedMessage message;
	};

	void load();
	void changed();
	void save(bool sync);
	[[nodiscard]] QByteArray serialize() const;
	[[nodiscard]] bool deserialize(const QByteArray &serialized);

	const not_null<Main::Session*> _session;
	base::Timer _saveTimer;
	base::flat_set<FullMsgId> _marked;
	std::deque<DeletedEntry> _deleted;
	base::flat_map<FullMsgId, std::vector<EditVersion>> _edits;
	bool _loaded = false;
	bool _dirty = false;

};

Archive::Archive(not_null<Main::Session*> session)
: _session(session)
, _saveTimer([=] { save(false); }) {
}

void Archive::load() {
	if (_loaded) {
		return;
	}
	_loaded = true;
	const auto serialized = _session->local().readTelewhiteData(
		Storage::TelewhiteData::Archive);
	if (!serialized.isEmpty() && !deserialize(serialized)) {
		_deleted.clear();
		_edits.clear();
	}
}

void Archive::changed() {
	_dirty = true;
	if (!_saveTimer.isActive()) {
		_saveTimer.callOnce(kSaveDelay);
	}
}

void Archive::save(bool sync) {
	if (!_dirty) {
		return;
	}
	_dirty = false;
	_saveTimer.cancel();
	_session->local().writeTelewhiteData(
		Storage::TelewhiteData::Archive,
		serialize(),
		sync);
}

void Archive::flush() {
	save(true);
}

bool Archive::mark(FullMsgId id) {
	return _marked.emplace(id).second;
}

bool Archive::marked(FullMsgId id) const {
	return _marked.contains(id);
}

void Archive::rememberDeleted(PeerId peerId, DeletedMessage message) {
	load();
	_deleted.push_back({ .peerId = peerId, .message = std::move(message) });
	auto evicted = std::vector<FullMsgId>();
	while (int(_deleted.size()) > kMaxDeleted) {
		const auto &front = _deleted.front();
		if (front.message.photo) {
			evicted.emplace_back(front.peerId, front.message.msgId);
		}
		_deleted.pop_front();
	}
	if (!evicted.empty()) {
		ForgetDeletedPhotos(_session, evicted);
	}
	changed();
}

bool Archive::hasDeleted(PeerId peerId) {
	load();
	return ranges::contains(_deleted, peerId, &DeletedEntry::peerId);
}

std::vector<DeletedMessage> Archive::deleted(PeerId peerId) {
	load();
	auto result = std::vector<DeletedMessage>();
	for (const auto &entry : _deleted) {
		if (entry.peerId == peerId) {
			result.push_back(entry.message);
		}
	}
	return result;
}

void Archive::clearDeleted(PeerId peerId) {
	load();
	auto photos = std::vector<FullMsgId>();
	for (const auto &entry : _deleted) {
		if (entry.peerId == peerId && entry.message.photo) {
			photos.emplace_back(peerId, entry.message.msgId);
		}
	}
	if (!photos.empty()) {
		ForgetDeletedPhotos(_session, photos);
	}
	const auto was = _deleted.size();
	_deleted.erase(
		ranges::remove(_deleted, peerId, &DeletedEntry::peerId),
		end(_deleted));
	if (_deleted.size() != was) {
		changed();
	}
}

void Archive::rememberEditVersion(FullMsgId id, EditVersion version) {
	load();
	auto &list = _edits[id];
	list.push_back(std::move(version));
	if (int(list.size()) > kMaxVersions) {
		list.erase(begin(list));
	}
	if (int(_edits.size()) > kMaxEdited) {
		auto oldest = end(_edits);
		for (auto i = begin(_edits); i != end(_edits); ++i) {
			if (i->first != id
				&& (oldest == end(_edits)
					|| i->second.back().date < oldest->second.back().date)) {
				oldest = i;
			}
		}
		if (oldest != end(_edits)) {
			_edits.erase(oldest);
		}
	}
	changed();
}

std::vector<EditVersion> Archive::editVersions(FullMsgId id) {
	load();
	const auto i = _edits.find(id);
	return (i != end(_edits)) ? i->second : std::vector<EditVersion>();
}

QByteArray Archive::serialize() const {
	auto result = QByteArray();
	auto stream = QDataStream(&result, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_1);
	stream
		<< kArchiveVersion
		<< quint64(_session->userId().bare)
		<< quint32(_deleted.size());
	for (const auto &[peerId, message] : _deleted) {
		stream
			<< SerializePeerId(peerId)
			<< qint64(message.msgId.bare)
			<< qint32(message.date)
			<< qint32(message.deletedAt)
			<< SerializePeerId(message.fromId)
			<< message.fromName;
		WriteText(stream, message.text);
		stream
			<< message.media
			<< quint32((message.photo ? kDeletedPhotoFlag : 0)
				| (message.noForwards ? kDeletedNoForwardsFlag : 0));
	}
	stream << quint32(_edits.size());
	for (const auto &[id, versions] : _edits) {
		stream
			<< SerializePeerId(id.peer)
			<< qint64(id.msg.bare)
			<< quint32(versions.size());
		for (const auto &version : versions) {
			stream << qint32(version.date);
			WriteText(stream, version.text);
		}
	}
	return result;
}

bool Archive::deserialize(const QByteArray &serialized) {
	auto stream = QDataStream(serialized);
	stream.setVersion(QDataStream::Qt_5_1);
	auto version = quint32();
	auto userId = quint64();
	auto deletedCount = quint32();
	stream >> version >> userId >> deletedCount;
	if (stream.status() != QDataStream::Ok
		|| !version
		|| version > kArchiveVersion
		|| userId != _session->userId().bare
		|| deletedCount > quint32(kMaxDeleted)) {
		return false;
	}
	for (auto i = quint32(); i != deletedCount; ++i) {
		auto peerId = quint64();
		auto msgId = qint64();
		auto date = qint32();
		auto deletedAt = qint32();
		auto fromId = quint64();
		auto entry = DeletedEntry();
		stream
			>> peerId
			>> msgId
			>> date
			>> deletedAt
			>> fromId
			>> entry.message.fromName;
		entry.message.text = ReadText(stream);
		stream >> entry.message.media;
		auto flags = quint32();
		if (version > 1) {
			stream >> flags;
		}
		if (stream.status() != QDataStream::Ok) {
			return false;
		}
		entry.peerId = DeserializePeerId(peerId);
		entry.message.msgId = MsgId(msgId);
		entry.message.date = TimeId(date);
		entry.message.deletedAt = TimeId(deletedAt);
		entry.message.fromId = DeserializePeerId(fromId);
		entry.message.photo = (flags & kDeletedPhotoFlag);
		entry.message.noForwards = (flags & kDeletedNoForwardsFlag);
		_deleted.push_back(std::move(entry));
	}
	auto editsCount = quint32();
	stream >> editsCount;
	if (stream.status() != QDataStream::Ok
		|| editsCount > quint32(kMaxEdited)) {
		return false;
	}
	for (auto i = quint32(); i != editsCount; ++i) {
		auto peerId = quint64();
		auto msgId = qint64();
		auto count = quint32();
		stream >> peerId >> msgId >> count;
		if (stream.status() != QDataStream::Ok
			|| !count
			|| count > quint32(kMaxVersions)) {
			return false;
		}
		auto versions = std::vector<EditVersion>();
		versions.reserve(count);
		for (auto j = quint32(); j != count; ++j) {
			auto date = qint32();
			stream >> date;
			auto text = ReadText(stream);
			if (stream.status() != QDataStream::Ok) {
				return false;
			}
			versions.push_back({
				.text = std::move(text),
				.date = TimeId(date),
			});
		}
		_edits.emplace(
			FullMsgId(DeserializePeerId(peerId), MsgId(msgId)),
			std::move(versions));
	}
	return true;
}

using Archives = base::flat_map<
	not_null<Main::Session*>,
	std::unique_ptr<Archive>>;

[[nodiscard]] Archives &AllArchives() {
	static auto result = Archives();
	return result;
}

[[nodiscard]] not_null<Archive*> ArchiveFor(
		not_null<Main::Session*> session) {
	auto &archives = AllArchives();
	if (const auto i = archives.find(session); i != end(archives)) {
		return i->second.get();
	}
	const auto result = archives.emplace(
		session,
		std::make_unique<Archive>(session)).first->second.get();
	session->lifetime().add([=] {
		auto &archives = AllArchives();
		const auto i = archives.find(session);
		if (i != end(archives)) {
			const auto archive = std::move(i->second);
			archives.erase(i);
			archive->flush();
		}
	});
	return result;
}

[[nodiscard]] not_null<Archive*> ArchiveFor(not_null<HistoryItem*> item) {
	return ArchiveFor(&item->history()->session());
}

} // namespace

bool MarkDeleted(not_null<HistoryItem*> item) {
	if (!item->isRegular()
		|| item->isService()
		|| !ArchiveFor(item)->mark(item->fullId())) {
		return false;
	}
	const auto media = item->media();
	const auto photo = media ? media->photo() : nullptr;
	const auto from = item->from();
	const auto peer = item->history()->peer;
	const auto noForwards = item->forbidsSaving() || !peer->allowsForwarding();
	const auto withPhoto = photo && !photo->isNull();
	ArchiveFor(item)->rememberDeleted(peer->id, {
		.msgId = item->id,
		.date = item->date(),
		.deletedAt = base::unixtime::now(),
		.fromId = from->id,
		.fromName = from->name(),
		.text = item->originalText(),
		.media = media ? media->pinnedTextSubstring() : QString(),
		.photo = withPhoto,
		.noForwards = noForwards,
	});
	if (withPhoto && !noForwards) {
		RememberDeletedPhoto(item);
	}
	return true;
}

bool IsMarkedDeleted(not_null<const HistoryItem*> item) {
	return ArchiveFor(&item->history()->session())->marked(item->fullId());
}

std::optional<DeletedMessage> FindDeleted(not_null<HistoryItem*> item) {
	const auto list = Deleted(item->history()->peer);
	const auto i = ranges::find(list, item->id, &DeletedMessage::msgId);
	return (i != end(list)) ? std::make_optional(*i) : std::nullopt;
}

bool HasDeleted(not_null<PeerData*> peer) {
	return ArchiveFor(&peer->session())->hasDeleted(peer->id);
}

std::vector<DeletedMessage> Deleted(not_null<PeerData*> peer) {
	return ArchiveFor(&peer->session())->deleted(peer->id);
}

void ClearDeleted(not_null<PeerData*> peer) {
	ArchiveFor(&peer->session())->clearDeleted(peer->id);
}

void RememberEditVersion(not_null<HistoryItem*> item, EditVersion version) {
	ArchiveFor(item)->rememberEditVersion(item->fullId(), std::move(version));
}

std::vector<EditVersion> EditVersions(not_null<HistoryItem*> item) {
	return ArchiveFor(item)->editVersions(item->fullId());
}

} // namespace Telewhite
