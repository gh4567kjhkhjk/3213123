/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;

namespace Telewhite {

struct EditVersion {
	TextWithEntities text;
	TimeId date = 0;
};

struct DeletedMessage {
	MsgId msgId = 0;
	TimeId date = 0;
	TimeId deletedAt = 0;
	PeerId fromId = 0;
	QString fromName;
	TextWithEntities text;
	QString media;
	bool photo = false;
	bool noForwards = false;
};

// Keeps a deleted message in the chat with a mark and archives it,
// returns false if it was already marked.
bool MarkDeleted(not_null<HistoryItem*> item);
[[nodiscard]] bool IsMarkedDeleted(not_null<const HistoryItem*> item);
[[nodiscard]] std::optional<DeletedMessage> FindDeleted(
	not_null<HistoryItem*> item);

[[nodiscard]] bool HasDeleted(not_null<PeerData*> peer);
[[nodiscard]] std::vector<DeletedMessage> Deleted(not_null<PeerData*> peer);
void ClearDeleted(not_null<PeerData*> peer);

void RememberEditVersion(not_null<HistoryItem*> item, EditVersion version);
[[nodiscard]] std::vector<EditVersion> EditVersions(
	not_null<HistoryItem*> item);

} // namespace Telewhite
