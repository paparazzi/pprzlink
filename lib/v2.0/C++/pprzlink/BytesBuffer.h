// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file BytesBuffer.h
 * @brief Shared byte storage for values, codecs and devices.
 * @ingroup messages
 */
#pragma once

#include <cstdint>
#include <vector>

namespace pprzlink {
  /// @brief Owned sequence of raw bytes, independent of device and event-loop APIs.
  /// @ingroup messages
  using BytesBuffer = std::vector<uint8_t>;
}
