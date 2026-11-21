// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file TransportStatistics.h
 * @brief Counters describing frame reception, rejection and recovery.
 * @ingroup transports
 *
 * Counters describe successful messages and discarded input rather than radio acknowledgements or application delivery.
 */

#pragma once
#include <cstdint>

namespace pprzlink {
  /// @brief Cumulative receive counters maintained by a frame decoder or transport.
  /// @ingroup transports
  /// Counts exclude radio status events and do not imply application delivery.
  struct TransportStatistics {
    uint64_t receivedMessages = 0; ///< Successfully decoded message frames.
    uint64_t receivedMessageBytes = 0; ///< Valid frame bytes, including the transport envelope.
    uint64_t checksumErrors = 0; ///< Candidates rejected because their checksum was invalid.
    uint64_t lengthErrors = 0; ///< Candidates rejected because their declared length was invalid.
    uint64_t decodingErrors = 0; ///< Valid envelopes containing malformed or unknown payloads.
    uint64_t discardedBytes = 0; ///< Noise, rejected frames and explicitly discarded partial input.
  };
}
