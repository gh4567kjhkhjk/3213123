/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;

namespace Ui::Menu {
struct MenuCallback;
} // namespace Ui::Menu

namespace Window {
class SessionController;
} // namespace Window

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
};

void RememberDeleted(not_null<HistoryItem*> item);
[[nodiscard]] bool HasDeleted(not_null<PeerData*> peer);
[[nodiscard]] std::vector<DeletedMessage> Deleted(not_null<PeerData*> peer);
void ClearDeleted(not_null<PeerData*> peer);

void RememberEditVersion(not_null<HistoryItem*> item, EditVersion version);
[[nodiscard]] std::vector<EditVersion> EditVersions(
	not_null<HistoryItem*> item);

void AddDeletedMessagesAction(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	const Ui::Menu::MenuCallback &addAction);

} // namespace Telewhite
