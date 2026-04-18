#pragma once

#include "MulticastBroadcasterBase.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace phu {
namespace network {

/**
 * CommandBroadcasterBase: shared helpers for command-style multicast channels.
 *
 * Centralizes:
 * - receiver-side self/target-group filtering
 * - listener vector management under mutex
 * - listener dispatch fan-out
 */
class CommandBroadcasterBase : public MulticastBroadcasterBase {
  protected:
    CommandBroadcasterBase(const char* multicastGroup, int port)
        : MulticastBroadcasterBase(multicastGroup, port) {}

    static bool shouldAcceptFromPeer(uint32_t senderInstanceID, uint32_t localInstanceID) {
        return senderInstanceID != localInstanceID;
    }

    static bool shouldAcceptForGroup(const std::string& targetGroup, const std::string& ownGroup) {
        return targetGroup == "all" || targetGroup == ownGroup;
    }

    template <typename ListenerT>
    static void addListener(std::mutex& listenerMutex, std::vector<ListenerT*>& listeners, ListenerT* listener) {
        std::lock_guard<std::mutex> lock(listenerMutex);
        if (std::find(listeners.begin(), listeners.end(), listener) == listeners.end()) {
            listeners.push_back(listener);
        }
    }

    template <typename ListenerT>
    static void removeListener(std::mutex& listenerMutex, std::vector<ListenerT*>& listeners, ListenerT* listener) {
        std::lock_guard<std::mutex> lock(listenerMutex);
        listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
    }

    template <typename ListenerT, typename DispatchFn>
    static void dispatchToListeners(std::mutex& listenerMutex, std::vector<ListenerT*>& listeners, DispatchFn dispatchFn) {
        std::lock_guard<std::mutex> lock(listenerMutex);
        for (auto* listener : listeners) {
            dispatchFn(*listener);
        }
    }
};

} // namespace network
} // namespace phu
