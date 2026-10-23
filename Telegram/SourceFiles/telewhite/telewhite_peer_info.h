/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class PeerData;

namespace Telewhite {

// Exact creation time of a group or a channel, empty when unknown.
[[nodiscard]] rpl::producer<TextWithEntities> CreationDateValue(
	not_null<PeerData*> peer);

} // namespace Telewhite
