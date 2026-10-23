/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_deleted_media.h"

#include "core/application.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "storage/cache/storage_cache_database.h"
#include "storage/storage_account.h"
#include "storage/storage_databases.h"
#include "ui/image/image.h"

#include <QtCore/QBuffer>

namespace Telewhite {
namespace {

constexpr auto kSizeLimit = int64(512) * 1024 * 1024;
constexpr auto kLoadTimeout = 10 * 60 * crl::time(1000);
constexpr auto kJpegQuality = 92;

[[nodiscard]] Storage::Cache::Key PhotoKey(FullMsgId id) {
	return { id.peer.value, uint64(id.msg.bare) };
}

[[nodiscard]] QByteArray PhotoBytes(not_null<Data::PhotoMedia*> media) {
	auto result = media->imageBytes(Data::PhotoSize::Large);
	if (!result.isEmpty()) {
		return result;
	}
	const auto image = media->image(Data::PhotoSize::Large);
	if (!image) {
		return result;
	}
	auto buffer = QBuffer(&result);
	image->original().save(&buffer, "JPG", kJpegQuality);
	return result;
}

class DeletedPhotos final {
public:
	explicit DeletedPhotos(not_null<Main::Session*> session);

	void remember(not_null<HistoryItem*> item);
	void load(FullMsgId id, Fn<void(QByteArray)> done);
	void forget(const std::vector<FullMsgId> &ids);
	void clear();

private:
	struct Pending {
		std::shared_ptr<Data::PhotoMedia> media;
		crl::time started = 0;
	};

	[[nodiscard]] Storage::Cache::Database &database();
	[[nodiscard]] QByteArray loadedBytes(FullMsgId id) const;
	void store(FullMsgId id, not_null<Data::PhotoMedia*> media);
	void checkPending();

	const not_null<Main::Session*> _session;
	std::optional<Storage::DatabasePointer> _database;
	base::flat_map<FullMsgId, Pending> _pending;
	rpl::lifetime _lifetime;

};

DeletedPhotos::DeletedPhotos(not_null<Main::Session*> session)
: _session(session) {
	_session->downloaderTaskFinished(
	) | rpl::filter([=] {
		return !_pending.empty();
	}) | rpl::on_next([=] {
		checkPending();
	}, _lifetime);
}

Storage::Cache::Database &DeletedPhotos::database() {
	if (!_database) {
		auto settings = Storage::Cache::Database::Settings();
		settings.clearOnWrongKey = true;
		settings.totalSizeLimit = kSizeLimit;
		settings.totalTimeLimit = 0;
		_database.emplace(Core::App().databases().get(
			_session->local().telewhiteMediaPath(),
			settings));
		(*_database)->open(_session->local().cacheKey());
	}
	return **_database;
}

void DeletedPhotos::remember(not_null<HistoryItem*> item) {
	const auto media = item->media();
	const auto photo = media ? media->photo() : nullptr;
	if (!photo || photo->isNull()) {
		return;
	}
	const auto id = item->fullId();
	auto view = photo->createMediaView();
	if (view->loaded()) {
		store(id, view.get());
		return;
	}
	photo->load(Data::PhotoSize::Large, id);
	_pending[id] = Pending{ std::move(view), crl::now() };
}

void DeletedPhotos::store(FullMsgId id, not_null<Data::PhotoMedia*> media) {
	auto bytes = PhotoBytes(media);
	if (!bytes.isEmpty()) {
		database().put(PhotoKey(id), std::move(bytes));
	}
}

void DeletedPhotos::checkPending() {
	const auto now = crl::now();
	for (auto i = begin(_pending); i != end(_pending);) {
		const auto media = i->second.media.get();
		if (media->loaded()) {
			store(i->first, media);
			i = _pending.erase(i);
		} else if (!media->owner()->loading(Data::PhotoSize::Large)
			|| (now - i->second.started > kLoadTimeout)) {
			i = _pending.erase(i);
		} else {
			++i;
		}
	}
}

QByteArray DeletedPhotos::loadedBytes(FullMsgId id) const {
	const auto item = _session->data().message(id);
	const auto media = item ? item->media() : nullptr;
	const auto photo = media ? media->photo() : nullptr;
	const auto view = photo ? photo->activeMediaView() : nullptr;
	return (view && view->loaded()) ? PhotoBytes(view.get()) : QByteArray();
}

void DeletedPhotos::load(FullMsgId id, Fn<void(QByteArray)> done) {
	// The message may still be shown in the chat with its photo loaded,
	// while the copy is not written to the database yet.
	if (auto bytes = loadedBytes(id); !bytes.isEmpty()) {
		done(std::move(bytes));
		return;
	}
	const auto weak = base::make_weak(_session);
	database().get(PhotoKey(id), [=](QByteArray &&bytes) {
		crl::on_main(weak, [=, bytes = std::move(bytes)]() mutable {
			done(std::move(bytes));
		});
	});
}

void DeletedPhotos::forget(const std::vector<FullMsgId> &ids) {
	for (const auto &id : ids) {
		_pending.remove(id);
		database().remove(PhotoKey(id));
	}
}

void DeletedPhotos::clear() {
	_pending.clear();
	auto &database = this->database();
	database.close();
	database.clear();
}

using AllDeletedPhotos = base::flat_map<
	not_null<Main::Session*>,
	std::unique_ptr<DeletedPhotos>>;

[[nodiscard]] AllDeletedPhotos &All() {
	static auto result = AllDeletedPhotos();
	return result;
}

[[nodiscard]] not_null<DeletedPhotos*> PhotosFor(
		not_null<Main::Session*> session) {
	auto &all = All();
	if (const auto i = all.find(session); i != end(all)) {
		return i->second.get();
	}
	const auto result = all.emplace(
		session,
		std::make_unique<DeletedPhotos>(session)).first->second.get();
	session->lifetime().add([=] {
		All().remove(session);
	});
	return result;
}

} // namespace

void RememberDeletedPhoto(not_null<HistoryItem*> item) {
	PhotosFor(&item->history()->session())->remember(item);
}

void LoadDeletedPhoto(
		not_null<Main::Session*> session,
		FullMsgId id,
		Fn<void(QByteArray)> done) {
	PhotosFor(session)->load(id, std::move(done));
}

void ForgetDeletedPhotos(
		not_null<Main::Session*> session,
		const std::vector<FullMsgId> &ids) {
	PhotosFor(session)->forget(ids);
}

void ClearDeletedPhotos(not_null<Main::Session*> session) {
	PhotosFor(session)->clear();
}

} // namespace Telewhite
