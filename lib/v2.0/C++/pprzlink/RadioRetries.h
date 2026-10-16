// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef PPRZLINKCPP_RADIORETRIES_H
#define PPRZLINKCPP_RADIORETRIES_H

namespace pprzlink {
  /// @brief Per-frame radio ACK/retry policy, separate from application resends.
  /// @ingroup messages
  /// MessageDefinition stores Enabled or Disabled. An XBee send can select
  /// FromDefinition to use that message's XML radio_retry hint.
  enum class RadioRetries {
    Enabled, ///< Normal radio transmit options, subject to modem configuration.
    Disabled, ///< Disable the radio ACK and associated retries for this frame.
    FromDefinition ///< Use the message definition; missing radio_retry means Enabled.
  };
}

#endif // PPRZLINKCPP_RADIORETRIES_H
