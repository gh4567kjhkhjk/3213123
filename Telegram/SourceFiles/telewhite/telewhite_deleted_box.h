/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Ui::Menu {
struct MenuCallback;
} // namespace Ui::Menu

namespace Window {
class SessionController;
} // namespace Window

namespace Telewhite {

struct DeletedMessage;

// Deleted messages are gone from the server, so they are sent to Saved
// Messages again as new messages: text messages in one digest first,
// then photos with captions.
void SendDeletedToSaved(
	not_null<Main::Session*> session,
	PeerId peerId,
	std::vector<DeletedMessage> messages);

void AddDeletedMessagesAction(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	const Ui::Menu::MenuCallback &addAction);

void AddDeletedMessageActions(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

} // namespace Telewhite
