/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_deleted_box.h"

#include "api/api_common.h"
#include "apiwrap.h"
#include "base/event_filter.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_peer.h"
#include "data/data_premium_limits.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "storage/localimageloader.h"
#include "storage/storage_media_prepare.h"
#include "telewhite/telewhite_archive.h"
#include "telewhite/telewhite_deleted_media.h"
#include "telewhite/telewhite_mods.h"
#include "ui/boxes/confirm_box.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/image/image_prepare.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/text/format_values.h"
#include "ui/text/text_utilities.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtGui/QCursor>

namespace Telewhite {
namespace {

constexpr auto kMaxShown = 300;
constexpr auto kThumbSide = 260;

[[nodiscard]] not_null<History*> SavedHistory(
		not_null<Main::Session*> session) {
	return session->data().history(session->user());
}

[[nodiscard]] Api::SendAction SavedAction(not_null<Main::Session*> session) {
	auto result = Api::SendAction(SavedHistory(session));
	result.clearDraft = false;
	return result;
}

[[nodiscard]] TextWithTags ToTags(const TextWithEntities &text) {
	return {
		text.text,
		TextUtilities::ConvertEntitiesToTextTags(text.entities),
	};
}

[[nodiscard]] TextWithEntities ForwardHeader(const DeletedMessage &message) {
	return Ui::Text::Italic(tr::lng_telewhite_deleted_forward_header(
		tr::now,
		lt_name,
		message.fromName,
		lt_date,
		FormatExactDateTime(message.date)));
}

[[nodiscard]] TextWithEntities ForwardText(
		const DeletedMessage &message,
		bool mediaLabel) {
	auto result = ForwardHeader(message);
	if (mediaLabel && !message.media.isEmpty()) {
		result.append(u"\n"_q).append(
			Ui::Text::Italic(u"["_q + message.media + u"]"_q));
	}
	if (!message.text.text.isEmpty()) {
		result.append(u"\n"_q).append(message.text);
	}
	return result;
}

void SendText(
		not_null<Main::Session*> session,
		const TextWithEntities &text) {
	auto message = Api::MessageToSend(SavedAction(session));
	message.textWithTags = ToTags(text);
	session->api().sendMessage(std::move(message));
}

[[nodiscard]] bool SendPhoto(
		not_null<Main::Session*> session,
		QByteArray bytes,
		const DeletedMessage &message) {
	auto image = Images::Read({ .content = bytes }).image;
	if (image.isNull()) {
		return false;
	}
	auto list = Storage::PrepareMediaFromImage(
		std::move(image),
		std::move(bytes),
		st::sendMediaPreviewSize);
	if (list.files.empty()) {
		return false;
	}
	auto caption = ForwardText(message, false);
	auto rest = TextWithEntities();
	const auto limit = Data::PremiumLimits(session).captionLengthCurrent();
	if (caption.text.size() > limit) {
		caption = ForwardHeader(message);
		rest = message.text;
	}
	list.files.front().caption = ToTags(caption);
	session->api().sendFiles(
		std::move(list),
		SendMediaType::Photo,
		nullptr,
		SavedAction(session));
	if (!rest.text.isEmpty()) {
		SendText(session, rest);
	}
	return true;
}

// One by one, so that the uploads are queued in the original order.
void SendPhotos(
		base::weak_ptr<Main::Session> weak,
		PeerId peerId,
		std::shared_ptr<std::vector<DeletedMessage>> list,
		int index) {
	const auto session = weak.get();
	if (!session || index >= int(list->size())) {
		return;
	}
	const auto id = FullMsgId(peerId, (*list)[index].msgId);
	LoadDeletedPhoto(session, id, [=](QByteArray bytes) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		const auto &message = (*list)[index];
		if (bytes.isEmpty() || !SendPhoto(strong, std::move(bytes), message)) {
			SendText(strong, ForwardText(message, true));
		}
		SendPhotos(weak, peerId, list, index + 1);
	});
}

void OpenPhoto(not_null<Main::Session*> session, FullMsgId id) {
	LoadDeletedPhoto(session, id, [=](QByteArray bytes) {
		if (bytes.isEmpty()) {
			return;
		}
		const auto path = QDir::tempPath()
			+ u"/telewhite_%1_%2.jpg"_q.arg(id.peer.value).arg(id.msg.bare);
		auto file = QFile(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return;
		}
		const auto written = file.write(bytes);
		file.close();
		if (written == bytes.size()) {
			File::Launch(path);
		}
	});
}

void SavePhoto(not_null<Main::Session*> session, FullMsgId id) {
	LoadDeletedPhoto(session, id, [=](QByteArray bytes) {
		if (bytes.isEmpty()) {
			return;
		}
		FileDialog::GetWritePath(
			Core::App().getFileDialogParent(),
			tr::lng_save_photo(tr::now),
			u"JPEG Image (*.jpg);;"_q + FileDialog::AllFilesFilter(),
			filedialogDefaultName(u"photo"_q, u".jpg"_q),
			[=](QString &&result) {
				if (result.isEmpty()) {
					return;
				}
				auto file = QFile(result);
				if (file.open(QIODevice::WriteOnly)) {
					file.write(bytes);
				}
			});
	});
}

class EntryHeader final : public Ui::RpWidget {
public:
	EntryHeader(QWidget *parent, QString name, QString time);

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;

private:
	const QString _name;
	const QString _time;

};

class PhotoThumb final : public Ui::RpWidget {
public:
	PhotoThumb(
		QWidget *parent,
		not_null<Main::Session*> session,
		FullMsgId id,
		Fn<void()> open);

	[[nodiscard]] bool available() const;

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	void ready(QImage image);
	[[nodiscard]] QSize thumbSize(int width) const;

	const Fn<void()> _open;
	QImage _original;
	QImage _prepared;
	bool _loading = true;

};

struct EntryState {
	DeletedMessage message;
	PhotoThumb *photo = nullptr;
	base::unique_qptr<Ui::PopupMenu> menu;
};

EntryHeader::EntryHeader(QWidget *parent, QString name, QString time)
: RpWidget(parent)
, _name(std::move(name))
, _time(std::move(time)) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

int EntryHeader::resizeGetHeight(int newWidth) {
	return st::semiboldFont->height;
}

void EntryHeader::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto skip = st::normalFont->spacew * 2;
	const auto timeWidth = st::normalFont->width(_time);
	const auto name = st::semiboldFont->elided(
		_name,
		std::max(width() - timeWidth - skip, 0));
	p.setFont(st::semiboldFont);
	p.setPen(st::windowBoldFg);
	p.drawTextLeft(0, 0, width(), name);
	p.setFont(st::normalFont);
	p.setPen(st::windowSubTextFg);
	p.drawTextLeft(
		st::semiboldFont->width(name) + skip,
		0,
		width(),
		_time,
		timeWidth);
}

PhotoThumb::PhotoThumb(
	QWidget *parent,
	not_null<Main::Session*> session,
	FullMsgId id,
	Fn<void()> open)
: RpWidget(parent)
, _open(std::move(open)) {
	const auto side = style::ConvertScale(kThumbSide)
		* style::DevicePixelRatio();
	const auto weak = QPointer<PhotoThumb>(this);
	LoadDeletedPhoto(session, id, [=](QByteArray bytes) {
		if (!weak) {
			return;
		} else if (bytes.isEmpty()) {
			ready(QImage());
			return;
		}
		crl::async([=, bytes = std::move(bytes)]() mutable {
			auto image = Images::Read({
				.content = std::move(bytes),
				.maxSize = QSize(side, side),
			}).image;
			crl::on_main(weak, [=, image = std::move(image)]() mutable {
				weak->ready(std::move(image));
			});
		});
	});
}

bool PhotoThumb::available() const {
	return !_original.isNull();
}

void PhotoThumb::ready(QImage image) {
	_loading = false;
	_original = std::move(image);
	_prepared = QImage();
	if (available() && _open) {
		setCursor(style::cur_pointer);
	}
	resizeToWidth(width());
	update();
}

QSize PhotoThumb::thumbSize(int width) const {
	const auto size = _original.size();
	if (size.isEmpty() || width <= 0) {
		return QSize();
	}
	const auto side = style::ConvertScale(kThumbSide);
	const auto scale = std::min({
		double(std::min(width, side)) / size.width(),
		double(side) / size.height(),
		1.,
	});
	return QSize(
		std::max(int(base::SafeRound(size.width() * scale)), 1),
		std::max(int(base::SafeRound(size.height() * scale)), 1));
}

int PhotoThumb::resizeGetHeight(int newWidth) {
	return available()
		? thumbSize(newWidth).height()
		: st::normalFont->height;
}

void PhotoThumb::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	if (_loading) {
		return;
	} else if (!available()) {
		p.setFont(st::normalFont);
		p.setPen(st::windowSubTextFg);
		p.drawTextLeft(
			0,
			0,
			width(),
			tr::lng_telewhite_deleted_photo_missing(tr::now));
		return;
	}
	const auto size = thumbSize(width());
	const auto ratio = style::DevicePixelRatio();
	if (_prepared.size() != size * ratio) {
		auto scaled = _original.scaled(
			size * ratio,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		scaled.setDevicePixelRatio(ratio);
		_prepared = Images::Round(std::move(scaled), ImageRoundRadius::Large);
	}
	p.drawImage(0, 0, _prepared);
}

void PhotoThumb::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton
		&& available()
		&& _open
		&& QRect(QPoint(), thumbSize(width())).contains(e->pos())) {
		_open();
	}
}

void FillEntryMenu(
		not_null<Ui::PopupMenu*> menu,
		not_null<Main::Session*> session,
		PeerId peerId,
		std::shared_ptr<Ui::Show> show,
		not_null<EntryState*> state) {
	const auto &message = state->message;
	const auto id = FullMsgId(peerId, message.msgId);
	if (!message.noForwards) {
		menu->addAction(tr::lng_telewhite_deleted_to_saved(tr::now), [=] {
			SendDeletedToSaved(session, peerId, { state->message });
			show->showToast(tr::lng_telewhite_deleted_sent(tr::now));
		}, &st::menuIconSavedMessages);
	}
	if (!message.text.text.isEmpty()) {
		menu->addAction(tr::lng_context_copy_text(tr::now), [=] {
			TextUtilities::SetClipboardText(
				TextForMimeData::Rich(base::duplicate(state->message.text)));
		}, &st::menuIconCopy);
	}
	if (state->photo && state->photo->available()) {
		menu->addAction(tr::lng_telewhite_deleted_open_photo(tr::now), [=] {
			OpenPhoto(session, id);
		}, &st::menuIconPhoto);
		menu->addAction(tr::lng_context_save_image(tr::now), [=] {
			SavePhoto(session, id);
		}, &st::menuIconSaveImage);
	}
}

void AddDayLabel(not_null<Ui::VerticalLayout*> container, QDate date) {
	const auto text = (date.year() == QDate::currentDate().year())
		? langDayOfMonthFull(date)
		: (langDayOfMonthFull(date) + ' ' + QString::number(date.year()));
	Ui::AddSkip(container);
	container->add(
		object_ptr<Ui::FlatLabel>(container, text, st::boxDividerLabel),
		st::boxRowPadding);
}

void AddEntry(
		not_null<Ui::VerticalLayout*> container,
		not_null<Main::Session*> session,
		PeerId peerId,
		std::shared_ptr<Ui::Show> show,
		const DeletedMessage &message) {
	Ui::AddSkip(container, st::defaultVerticalListSkip);
	const auto wrap = container->add(
		object_ptr<Ui::VerticalLayout>(container),
		st::boxRowPadding);
	const auto state = wrap->lifetime().make_state<EntryState>();
	state->message = message;

	const auto time = Ui::FormatTimeOfDay(
		base::unixtime::parse(message.date).time());
	wrap->add(object_ptr<EntryHeader>(wrap, message.fromName, time));
	const auto skip = QMargins(0, st::lineWidth * 2, 0, 0);
	if (!message.text.text.isEmpty()) {
		const auto label = wrap->add(
			object_ptr<Ui::FlatLabel>(
				wrap,
				rpl::single(message.text),
				st::boxLabel),
			skip);
		label->setSelectable(true);
		label->setContextMenuHook([=](Ui::FlatLabel::ContextMenuRequest r) {
			label->fillContextMenu(r);
			r.menu->addSeparator();
			FillEntryMenu(r.menu, session, peerId, show, state);
		});
	}
	if (message.photo) {
		const auto id = FullMsgId(peerId, message.msgId);
		auto open = message.noForwards
			? Fn<void()>()
			: Fn<void()>([=] { OpenPhoto(session, id); });
		state->photo = wrap->add(
			object_ptr<PhotoThumb>(wrap, session, id, std::move(open)),
			skip + QMargins(0, st::lineWidth * 4, 0, 0));
	} else if (!message.media.isEmpty()) {
		wrap->add(
			object_ptr<Ui::FlatLabel>(wrap, message.media, st::boxDividerLabel),
			skip);
	}
	base::install_event_filter(wrap, [=](not_null<QEvent*> e) {
		if (e->type() != QEvent::ContextMenu) {
			return base::EventFilterResult::Continue;
		}
		state->menu = base::make_unique_q<Ui::PopupMenu>(
			wrap,
			st::popupMenuWithIcons);
		FillEntryMenu(state->menu.get(), session, peerId, show, state);
		if (state->menu->empty()) {
			state->menu = nullptr;
		} else {
			state->menu->popup(QCursor::pos());
		}
		return base::EventFilterResult::Cancel;
	});
}

void DeletedMessagesBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer) {
	box->setTitle(tr::lng_telewhite_deleted_title());
	box->setWidth(st::boxWideWidth);

	const auto session = &peer->session();
	const auto peerId = peer->id;
	const auto show = box->uiShow();
	const auto container = box->verticalLayout();
	const auto list = Deleted(peer);
	if (ranges::contains(list, false, &DeletedMessage::noForwards)) {
		Settings::AddButtonWithIcon(
			container,
			tr::lng_telewhite_deleted_all_to_saved(),
			st::settingsButtonActive,
			{ &st::menuIconSavedMessages }
		)->setClickedCallback([=] {
			show->show(Ui::MakeConfirmBox({
				.text = tr::lng_telewhite_deleted_all_sure(),
				.confirmed = [=](Fn<void()> close) {
					SendDeletedToSaved(session, peerId, list);
					show->showToast(tr::lng_telewhite_deleted_sent(tr::now));
					close();
				},
				.confirmText = tr::lng_telewhite_deleted_send(),
			}));
		});
	}

	const auto count = int(list.size());
	const auto from = std::max(count - kMaxShown, 0);
	auto day = QDate();
	for (auto i = count; i != from;) {
		const auto &message = list[--i];
		const auto date = base::unixtime::parse(message.date).date();
		if (date != day) {
			day = date;
			AddDayLabel(container, date);
		}
		AddEntry(container, session, peerId, show, message);
	}
	Ui::AddSkip(container);

	const auto weak = base::make_weak(box);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->addLeftButton(tr::lng_telewhite_deleted_clear(), [=] {
		box->uiShow()->show(Ui::MakeConfirmBox({
			.text = tr::lng_telewhite_deleted_clear_sure(),
			.confirmed = [=](Fn<void()> close) {
				ClearDeleted(peer);
				close();
				if (const auto strong = weak.get()) {
					strong->closeBox();
				}
			},
			.confirmText = tr::lng_box_delete(),
			.confirmStyle = &st::attentionBoxButton,
		}));
	});
}

} // namespace

void SendDeletedToSaved(
		not_null<Main::Session*> session,
		PeerId peerId,
		std::vector<DeletedMessage> messages) {
	auto digest = TextWithEntities();
	auto photos = std::vector<DeletedMessage>();
	for (auto &message : messages) {
		if (message.noForwards) {
			continue;
		} else if (message.photo) {
			photos.push_back(std::move(message));
			continue;
		} else if (!digest.text.isEmpty()) {
			digest.append(u"\n\n"_q);
		}
		digest.append(ForwardText(message, true));
	}
	if (!digest.text.isEmpty()) {
		SendText(session, digest);
	}
	SendPhotos(
		base::make_weak(session),
		peerId,
		std::make_shared<std::vector<DeletedMessage>>(std::move(photos)),
		0);
}

void AddDeletedMessagesAction(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		const Ui::Menu::MenuCallback &addAction) {
	if (!HasDeleted(peer)) {
		return;
	}
	addAction(tr::lng_telewhite_deleted_title(tr::now), [=] {
		controller->show(Box(DeletedMessagesBox, peer));
	}, &st::menuIconDelete);
}

void AddDeletedMessageActions(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	if (!IsMarkedDeleted(item)) {
		return;
	}
	const auto message = FindDeleted(item);
	if (!message || message->noForwards) {
		return;
	}
	const auto session = &item->history()->session();
	const auto peerId = item->history()->peer->id;
	menu->addAction(tr::lng_telewhite_deleted_to_saved(tr::now), [=] {
		SendDeletedToSaved(session, peerId, { *message });
		controller->uiShow()->showToast(
			tr::lng_telewhite_deleted_sent(tr::now));
	}, &st::menuIconSavedMessages);
}

} // namespace Telewhite
