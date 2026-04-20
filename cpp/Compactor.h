#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <functional>

namespace turbo_db {

class MMapRegion;
class PersistentBPlusTree;
class SecureCryptoContext;

/**
 * Compactor — background vacuum / defragmentation process.
 *
 * Scans the mmap file for live records, rewrites them to a compact
 * temporary file, then atomically swaps the new file in via rename().
 *
 * The compactor runs during idle time (triggered by DBScheduler at
 * COMPACTION priority) and only when fragmentation exceeds the threshold.
 */
class Compactor {
public:
    // Trigger compaction when dead space > X% of total file size
    static constexpr double FRAGMENTATION_THRESHOLD = 0.30; // 30%

    explicit Compactor(std::string db_path, SecureCryptoContext* crypto = nullptr);

    /**
     * Compute fragmentation ratio from live_bytes vs total file size.
     * Call this before deciding whether to schedule compaction.
     */
    double getFragmentationRatio(size_t live_bytes, size_t total_file_bytes) const;

    /**
     * Returns true if compaction should be scheduled.
     */
    bool shouldCompact(size_t live_bytes, size_t total_file_bytes) const;

    /**
     * Main compaction routine — runs on the DBWorker thread.
     * Rebuilds the database file from live records only.
     * On success, atomically replaces the old file.
     * On failure, leaves the original file untouched.
     *
     * @param mmap    The current mmap region (will be re-opened after compaction)
     * @param tree    The B+ Tree index (updated to reflect new offsets)
     * @param onDone  Callback invoked on completion (success=true) or failure (success=false)
     */
    void runCompaction(
        MMapRegion* mmap,
        PersistentBPlusTree* tree,
        std::function<void(bool success, size_t bytes_saved)> onDone
    );

    /**
     * Track live bytes to compute fragmentation ratio over time.
     * Called by DBEngine after each insert / delete.
     */
    void trackLiveBytes(size_t delta, bool add);

    size_t getLiveBytes() const { return live_bytes_.load(std::memory_order_relaxed); }

private:
    std::string db_path_;
    SecureCryptoContext* crypto_;
    std::atomic<size_t> live_bytes_{0};
};

} // namespace turbo_db
