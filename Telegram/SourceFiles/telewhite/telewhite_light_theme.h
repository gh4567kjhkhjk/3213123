/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Telewhite {

// The default palette with violet accents, the rest comes from fallbacks.
inline constexpr auto kLightPalette = std::string_view(R"PALETTE(
windowBgActive: #6f4cd7;
windowActiveTextFg: #4b24bf;
activeButtonBgOver: #6845cf;
activeButtonBgRipple: #532dc3;
activeButtonSecondaryFg: #dbd0fb;
activeLineFg: #6744d1;
lightButtonBgOver: #eae5f8;
lightButtonBgRipple: #d6ccf3;
filterInputBorderFg: #8260e7;
botKbPrimaryBg: #5935c3cc;
dialogsBgActive: #6d4cce;
dialogsDraftFgActive: #d4caf3;
historyUnreadBarFg: #6f5aad;
msgInBgSelected: #d5c6ee;
msgSelectOverlay: #6341c84c;
msgStickerOverlay: #6341c87f;
msgInShadowSelected: #7c5cb329;
msgInDateFgSelected: #8d71be;
msgFileInBgOver: #7959d8;
msgFileInBgSelected: #835bc9;
msgWaveformInActiveSelected: #835bc9;
msgWaveformInInactiveSelected: #b7a1dc;
overviewPhotoSelectOverlay: #6f4cd733;
mainMenuCloudBg: #5332b4;
mediaPlayerDisabledFg: #b5a3e9;
sideBarTextFgActive: #8f6fef;
sideBarIconFgActive: #8a69ec;
sideBarBadgeBg: #8a69ec;
dialogsMentionIconFg: #6f4cd7;
introCoverTopBg: #461dc2;
introCoverBottomBg: #6e47e2;
introCoverIconsFg: #8c6af3;
introCoverPlaneTrace: #8c6af369;
dialogsSentIconFg: #6f52c4;
dialogsSentIconFgOver: #684db8;
historyOutIconFg: #674cb8;
historyOutIconFgSelected: #6a45aa;
historySendingOutIconFg: #a292d2;
msgOutBg: #e6defd;
msgOutBgSelected: #c4b7db;
msgOutServiceFg: #4b2da3;
msgOutServiceFgSelected: #644699;
msgOutShadow: #5c3ac31d;
msgOutShadowSelected: #6037a722;
msgOutDateFg: #7a66b5;
msgOutDateFgSelected: #7856b2;
msgOutReplyBarColor: #6d54b8;
msgOutMonoFg: #5a4598;
msgFileThumbLinkOutFg: #4f31a8;
msgFileThumbLinkOutFgSelected: #5a31a2;
msgFileOutBg: #775fbe;
msgFileOutBgSelected: #7250ac;
msgWaveformOutActive: #765ebd;
msgWaveformOutActiveSelected: #836bad;
msgWaveformOutInactive: #bfb3e2;
msgWaveformOutInactiveSelected: #a391c3;
)PALETTE");

} // namespace Telewhite
