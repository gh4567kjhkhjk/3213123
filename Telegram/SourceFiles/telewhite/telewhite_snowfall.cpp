/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_snowfall.h"

#include "base/random.h"
#include "base/timer.h"
#include "telewhite/telewhite_mods.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/style/style_core_scale.h"

#include <cmath>

namespace Telewhite {
namespace {

constexpr auto kFrameDelay = crl::time(33);
constexpr auto kRaiseEveryFrames = 30;
constexpr auto kFlakesPerMegapixel = 90.;
constexpr auto kMaxFlakes = 220;

[[nodiscard]] double RandomUnit() {
	return base::RandomValue<uint32>() / double(0xFFFFFFFFU);
}

class SnowOverlay final : public Ui::RpWidget {
public:
	explicit SnowOverlay(not_null<Ui::RpWidget*> parent);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	struct Flake {
		double x = 0.;
		double y = 0.;
		double speed = 0.;
		double sway = 0.;
		double phase = 0.;
		double radius = 0.;
		double opacity = 0.;
	};

	void setActive(bool active);
	void refill();
	void respawn(Flake &flake, bool anywhere);
	void tick();
	[[nodiscard]] QRect flakeRect(const Flake &flake) const;

	std::vector<Flake> _flakes;
	base::Timer _timer;
	crl::time _last = 0;
	int _frame = 0;
	bool _active = false;

};

SnowOverlay::SnowOverlay(not_null<Ui::RpWidget*> parent)
: RpWidget(parent.get())
, _timer([=] { tick(); }) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setAttribute(Qt::WA_NoSystemBackground);
	hide();

	parent->sizeValue(
	) | rpl::on_next([=](QSize size) {
		setGeometry(QRect(QPoint(), size));
		refill();
	}, lifetime());

	EnabledValue(
		Mod::Snowfall
	) | rpl::on_next([=](bool enabled) {
		setActive(enabled);
	}, lifetime());
}

void SnowOverlay::setActive(bool active) {
	if (_active == active) {
		return;
	}
	_active = active;
	if (_active) {
		refill();
		show();
		raise();
		_last = crl::now();
		_timer.callEach(kFrameDelay);
	} else {
		_timer.cancel();
		_flakes.clear();
		hide();
	}
}

void SnowOverlay::refill() {
	if (!_active || width() <= 0 || height() <= 0) {
		return;
	}
	const auto area = double(width()) * double(height()) / 1000000.;
	const auto count = std::clamp(
		int(area * kFlakesPerMegapixel),
		12,
		kMaxFlakes);
	const auto was = int(_flakes.size());
	_flakes.resize(count);
	for (auto i = was; i < count; ++i) {
		respawn(_flakes[i], true);
	}
	update();
}

void SnowOverlay::respawn(Flake &flake, bool anywhere) {
	const auto unit = style::Scale() / 100.;
	flake.radius = (1.2 + RandomUnit() * 2.3) * unit;
	flake.x = RandomUnit() * width();
	flake.y = anywhere
		? (RandomUnit() * height())
		: -(flake.radius * 2. + RandomUnit() * 20. * unit);
	flake.speed = (18. + RandomUnit() * 38.) * unit * (flake.radius / unit);
	flake.sway = (6. + RandomUnit() * 18.) * unit;
	flake.phase = RandomUnit() * 6.2831853;
	flake.opacity = 0.45 + RandomUnit() * 0.5;
}

QRect SnowOverlay::flakeRect(const Flake &flake) const {
	const auto x = flake.x + std::sin(flake.phase) * flake.sway;
	const auto r = int(std::ceil(flake.radius)) + 2;
	return QRect(int(x) - r, int(flake.y) - r, r * 2 + 1, r * 2 + 1);
}

void SnowOverlay::tick() {
	const auto topLevel = window();
	if (!topLevel
		|| !topLevel->isVisible()
		|| topLevel->isMinimized()
		|| !isVisible()) {
		_last = crl::now();
		return;
	}
	const auto now = crl::now();
	const auto dt = std::clamp((now - _last) / 1000., 0., 0.1);
	_last = now;

	auto region = QRegion();
	for (auto &flake : _flakes) {
		region += flakeRect(flake);
		flake.y += flake.speed * dt;
		flake.phase += dt * 1.3;
		if (flake.y - flake.radius > height()) {
			respawn(flake, false);
		}
		region += flakeRect(flake);
	}
	if (++_frame >= kRaiseEveryFrames) {
		_frame = 0;
		raise();
	}
	update(region);
}

void SnowOverlay::paintEvent(QPaintEvent *e) {
	if (_flakes.empty()) {
		return;
	}
	auto p = QPainter(this);
	PainterHighQualityEnabler hq(p);
	p.setPen(Qt::NoPen);
	const auto clip = e->rect();
	for (const auto &flake : _flakes) {
		const auto rect = flakeRect(flake);
		if (!rect.intersects(clip)) {
			continue;
		}
		const auto x = flake.x + std::sin(flake.phase) * flake.sway;
		p.setOpacity(flake.opacity);
		p.setBrush(QColor(255, 255, 255));
		p.drawEllipse(QPointF(x, flake.y), flake.radius, flake.radius);
	}
}

} // namespace

void SetupSnowfall(not_null<Ui::RpWidget*> parent) {
	Ui::CreateChild<SnowOverlay>(parent.get());
}

} // namespace Telewhite
