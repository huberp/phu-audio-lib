#pragma once

#include "MulticastBroadcasterBase.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace phu {
namespace network {

/**
 * StatefulBroadcaster: thin reusable base for broadcasters that keep mutable
 * state per remote instance ID.
 *
 * It provides shared map/mutex helper operations used by concrete broadcasters
 * (Sample/Spectrum/Ctrl) while still allowing each broadcaster to define its
 * own remote-state type and pruning logic.
 */
class StatefulBroadcaster : public MulticastBroadcasterBase {
  protected:
    StatefulBroadcaster(const char* multicastGroup, int port)
        : MulticastBroadcasterBase(multicastGroup, port) {}

    template <typename RemoteState>
    static void clearRemoteStates(std::mutex& mutex, std::map<uint32_t, RemoteState>& states) {
        std::lock_guard<std::mutex> lock(mutex);
        states.clear();
    }

    template <typename RemoteState, typename PrunePredicate>
    static void getRemoteStates(std::mutex& mutex, std::map<uint32_t, RemoteState>& states,
                                std::vector<RemoteState>& out, PrunePredicate shouldPrune) {
        out.clear();

        std::lock_guard<std::mutex> lock(mutex);
        auto it = states.begin();
        while (it != states.end()) {
            if (shouldPrune(it->second)) {
                it = states.erase(it);
            } else {
                out.push_back(it->second);
                ++it;
            }
        }
    }

    template <typename RemoteState>
    static int getNumRemoteStates(std::mutex& mutex, const std::map<uint32_t, RemoteState>& states) {
        std::lock_guard<std::mutex> lock(mutex);
        return static_cast<int>(states.size());
    }
};

} // namespace network
} // namespace phu
