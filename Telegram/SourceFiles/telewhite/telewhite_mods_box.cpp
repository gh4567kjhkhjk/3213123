/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_mods_box.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "settings.h"
#include "lang/lang_keys.h"
#include "telewhite/telewhite_amoled_theme.h"
#include "telewhite/telewhite_light_theme.h"
#include "telewhite/telewhite_mods.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/themes/window_theme.h"
#include "window/themes/window_themes_embedded.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

namespace Telewhite {
namespace {

void ApplyThemeFile(const QString &name, const QByteArray &content) {
	const auto folder = cWorkingDir() + u"tdata"_q;
	QDir().mkpath(folder);
	const auto path = folder + u"/"_q + name;
	auto file = QFile(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	const auto written = file.write(content);
	file.close();
	if (written != content.size()) {
		return;
	}
	if (Window::Theme::Apply(path)) {
		Window::Theme::KeepApplied();
	}
}

void ApplyAmoledTheme() {
	ApplyThemeFile(
		u"telewhite-amoled.tdesktop-theme"_q,
		QByteArray::fromRawData(
			reinterpret_cast<const char*>(kAmoledThemeData),
			sizeof(kAmoledThemeData)));
}

void ApplyLightTheme() {
	ApplyThemeFile(
		u"telewhite-light.tdesktop-palette"_q,
		QByteArray(kLightPalette.data(), kLightPalette.size()));
}

void ApplyDarkTheme(std::shared_ptr<Ui::Show> show) {
	using namespace Window::Theme;
	const auto schemes = EmbeddedThemes();
	const auto night = ranges::find(
		schemes,
		EmbeddedType::Night,
		&EmbeddedScheme::type);
	if (night == end(schemes)) {
		return;
	}
	const auto apply = [path = night->path] {
		auto &settings = Core::App().settings();
		settings.themesAccentColors().set(
			EmbeddedType::Night,
			QColor(0x7a, 0x5c, 0xcf));
		settings.setSystemAccentColorEnabled(false);
		Core::App().saveSettingsDelayed();
		if (IsNightMode()) {
			ApplyDefaultWithPath(path);
		} else {
			ToggleNightMode(path);
		}
		KeepApplied();
	};
	if (IsNightMode() || Background()->nightModeChangeAllowed()) {
		apply();
		return;
	}
	show->show(Ui::MakeConfirmBox({
		.text = tr::lng_settings_auto_night_warning(),
		.confirmed = [=](Fn<void()> close) {
			Core::App().settings().setSystemDarkModeEnabled(false);
			Core::App().saveSettingsDelayed();
			apply();
			close();
		},
		.confirmText = tr::lng_settings_auto_night_disable(),
	}));
}

void AddToggle(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		bool initial,
		Fn<void(bool)> callback) {
	const auto button = container->add(object_ptr<Ui::SettingsButton>(
		container,
		std::move(text),
		st::settingsButtonNoIcon));
	button->toggleOn(rpl::single(initial));
	button->toggledChanges(
	) | rpl::on_next(std::move(callback), button->lifetime());
}

void AddModToggle(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		Mod mod) {
	AddToggle(container, std::move(text), Enabled(mod), [=](bool value) {
		SetEnabled(mod, value);
	});
}

void AddSection(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> title) {
	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, std::move(title));
}

} // namespace

void ModsBox(not_null<Ui::GenericBox*> box) {
	box->setStyle(st::layerBox);
	box->setTitle(tr::lng_telewhite_mods_title());
	box->setWidth(st::boxWideWidth);

	const auto container = box->verticalLayout();

	AddSection(container, tr::lng_telewhite_mods_ghost());
	AddModToggle(
		container,
		tr::lng_telewhite_mods_ghost_read(),
		Mod::GhostRead);
	AddModToggle(
		container,
		tr::lng_telewhite_mods_ghost_typing(),
		Mod::GhostTyping);
	AddModToggle(
		container,
		tr::lng_telewhite_mods_ghost_online(),
		Mod::GhostOnline);
	AddModToggle(
		container,
		tr::lng_telewhite_mods_ghost_stories(),
		Mod::GhostStories);
	Ui::AddSkip(container);
	Ui::AddDividerText(container, tr::lng_telewhite_mods_ghost_about());

	AddSection(container, tr::lng_telewhite_mods_messages());
	AddModToggle(
		container,
		tr::lng_telewhite_mods_save_deleted(),
		Mod::SaveDeleted);
	AddModToggle(
		container,
		tr::lng_telewhite_mods_edit_history(),
		Mod::EditHistory);
	AddModToggle(
		container,
		tr::lng_telewhite_mods_forward_no_author(),
		Mod::ForwardNoAuthor);
	AddModToggle(
		container,
		tr::lng_telewhite_mods_multi_forward(),
		Mod::MultiForward);
	Ui::AddSkip(container);
	Ui::AddDividerText(container, tr::lng_telewhite_mods_messages_about());

	AddSection(container, tr::lng_telewhite_mods_chats());
	AddModToggle(
		container,
		tr::lng_telewhite_mods_local_pins(),
		Mod::LocalPins);
	Ui::AddSkip(container);
	Ui::AddDividerText(container, tr::lng_telewhite_mods_local_pins_about());

	AddSection(container, tr::lng_telewhite_mods_clean());
	AddToggle(
		container,
		tr::lng_settings_chat_hide_stories(),
		Core::App().settings().hideStories(),
		[](bool value) {
			Core::App().settings().setHideStories(value);
			Core::App().saveSettingsDelayed();
		});
	AddModToggle(
		container,
		tr::lng_telewhite_mods_hide_sponsored(),
		Mod::HideSponsored);
	AddModToggle(
		container,
		tr::lng_telewhite_mods_hide_emoji_status(),
		Mod::HideEmojiStatus);

	AddSection(container, tr::lng_telewhite_mods_info());
	AddModToggle(
		container,
		tr::lng_telewhite_mods_precise_last_seen(),
		Mod::PreciseLastSeen);
	AddToggle(
		container,
		tr::lng_settings_chat_show_seconds(),
		Core::App().settings().showSeconds(),
		[](bool value) {
			Core::App().settings().setShowSeconds(value);
			Core::App().saveSettingsDelayed();
		});
	AddModToggle(
		container,
		tr::lng_telewhite_mods_registration_date(),
		Mod::RegistrationDate);

	AddSection(container, tr::lng_telewhite_mods_design());
	AddModToggle(
		container,
		tr::lng_telewhite_mods_snowfall(),
		Mod::Snowfall);
	container->add(object_ptr<Ui::SettingsButton>(
		container,
		tr::lng_telewhite_mods_light(),
		st::settingsButtonNoIcon
	))->addClickHandler(ApplyLightTheme);
	container->add(object_ptr<Ui::SettingsButton>(
		container,
		tr::lng_telewhite_mods_dark(),
		st::settingsButtonNoIcon
	))->addClickHandler([=] { ApplyDarkTheme(box->uiShow()); });
	container->add(object_ptr<Ui::SettingsButton>(
		container,
		tr::lng_telewhite_mods_amoled(),
		st::settingsButtonNoIcon
	))->addClickHandler(ApplyAmoledTheme);
	Ui::AddSkip(container);
	Ui::AddDividerText(container, tr::lng_telewhite_mods_restart_about());

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace Telewhite
