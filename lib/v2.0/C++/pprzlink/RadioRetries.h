// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef PPRZLINKCPP_RADIORETRIES_H
#define PPRZLINKCPP_RADIORETRIES_H

namespace pprzlink {
  /// @brief Per-frame radio ACK/retry policy, separate from application resends.
  /// @ingroup xbee
  /// This is an explicit send option. Broadcast destinations do not request
  /// MAC acknowledgements even with the normal Enabled transmit options.
  enum class RadioRetries {
    Enabled, ///< Normal radio transmit options, subject to modem configuration.
    Disabled ///< Disable the radio ACK and associated retries for this frame.
  };
}

#endif // PPRZLINKCPP_RADIORETRIES_H
