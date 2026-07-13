#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace beamdrop::transfer {

enum class Stage {
    // A transfer task has started. Emitted exactly once per task before any file event.
    TaskStarted,
    // Bytes for the current file have been transferred. Emitted zero or more times per file.
    Transferring,
    // The current file has completed successfully. Emitted exactly once per file.
    FileCompleted,
    // All files in the transfer task have completed successfully. Emitted exactly once per task.
    TaskCompleted,
};

enum class ProgressDirection {
    Send,
    Receive,
};

struct ProgressEvent {
    ProgressDirection direction = ProgressDirection::Send;
    // Meaningful for file-level events. Empty for task-level events.
    std::string relative_path;
    // Meaningful for file-level events. Zero for task-level events.
    std::uint64_t current_file_bytes = 0;
    // Meaningful for file-level events. Zero for task-level events.
    std::uint64_t current_file_total_bytes = 0;
    // For file-level events: 1-based current file index. For TaskStarted: 0.
    // For TaskCompleted: file_count.
    std::size_t file_index = 0;
    // Total files in the transfer task.
    std::size_t file_count = 0;
    Stage stage = Stage::TaskStarted;
};

using ProgressCallback = std::function<void(const ProgressEvent &)>;

} // namespace beamdrop::transfer