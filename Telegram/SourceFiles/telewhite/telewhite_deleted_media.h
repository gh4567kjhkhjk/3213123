/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace Telewhite {

// Photos of deleted messages live in a separate encrypted database of the
// account, they are captured (or downloaded) when the message is deleted.
void RememberDeletedPhoto(not_null<HistoryItem*> item);

// The callback is called on the main thread with empty bytes when
// the photo wasn't saved.
void LoadDeletedPhoto(
	not_null<Main::Session*> session,
	FullMsgId id,
	Fn<void(QByteArray)> done);

void ForgetDeletedPhotos(
	not_null<Main::Session*> session,
	const std::vector<FullMsgId> &ids);
void ClearDeletedPhotos(not_null<Main::Session*> session);

} // namespace Telewhite
