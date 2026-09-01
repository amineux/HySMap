#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace hysmap {

[[nodiscard]] inline int effective_threads(int requested) {
    if (requested > 0) {
        return requested;
    }
    const unsigned hc = std::thread::hardware_concurrency();
    return static_cast<int>(std::max(1u, hc));
}

/// Static-ish work-stealing loop over [0, n). Deterministic if `fn` is.
template <typename Fn>
void parallel_for(int n, int threads, Fn&& fn) {
    if (n <= 0) {
        return;
    }
    // Tiny batches lose to thread-spawn overhead on this objective.
    const int t = (n < 32) ? 1 : std::max(1, std::min(threads, n));
    if (t <= 1) {
        for (int i = 0; i < n; ++i) {
            fn(i);
        }
        return;
    }
    std::atomic<int> next{0};
    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(t));
    for (int k = 0; k < t; ++k) {
        pool.emplace_back([&]() {
            while (true) {
                const int i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= n) {
                    break;
                }
                fn(i);
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }
}

}  // namespace hysmap
