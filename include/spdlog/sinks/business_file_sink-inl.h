// Copyright(c) 2025-present.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/business_file_sink.h>
#endif

#include <spdlog/common.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/fmt/fmt.h>

#include <cerrno>
#include <ctime>
#include <mutex>
#include <string>
#include <tuple>

namespace spdlog {
namespace sinks {

template <typename Mutex>
SPDLOG_INLINE business_file_sink<Mutex>::business_file_sink(
    filename_t base_dir,
    std::size_t max_file_size,
    std::size_t max_files,
    const file_event_handlers &event_handlers)
    : base_dir_(std::move(base_dir)),
      max_size_(max_file_size),
      max_files_(max_files),
      event_handlers_(event_handlers) {
    if (max_file_size == 0) {
        throw_spdlog_ex("business_file_sink constructor: max_size arg cannot be zero");
    }
    if (max_files == 0) {
        throw_spdlog_ex("business_file_sink constructor: max_files arg cannot be zero");
    }
}

template <typename Mutex>
SPDLOG_INLINE filename_t business_file_sink<Mutex>::calc_filename_(const filename_t &filename,
                                                                    std::size_t index) {
    if (index == 0U) {
        return filename;
    }

    filename_t basename;
    filename_t ext;
    std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
    return fmt_lib::format(SPDLOG_FMT_STRING(SPDLOG_FILENAME_T("{}.{}{}")), basename, index, ext);
}

template <typename Mutex>
SPDLOG_INLINE typename business_file_sink<Mutex>::business_sink_data&
business_file_sink<Mutex>::get_or_create_sink_(business_type type) {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    
    auto it = business_sinks_.find(type);
    if (it == business_sinks_.end()) {
        // Create new sink for this business type
        std::string type_str = business_type_to_string(type);
        filename_t filename = fmt_lib::format(
            SPDLOG_FMT_STRING(SPDLOG_FILENAME_T("{}/{}")),
            base_dir_, 
            SPDLOG_FILENAME_T(type_str + ".log"));
        
        auto sink_data = details::make_unique<business_sink_data>(filename, event_handlers_);
        sink_data->file_helper.open(filename, true);  // truncate if exists on first open
        sink_data->current_size = sink_data->file_helper.size();
        
        auto* ptr = sink_data.get();
        business_sinks_[type] = std::move(sink_data);
        return *ptr;
    }
    return *it->second;
}

template <typename Mutex>
SPDLOG_INLINE void business_file_sink<Mutex>::log_to_business(business_type type,
                                                               const details::log_msg &msg) {
    auto& sink_data = get_or_create_sink_(type);
    
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    
    memory_buf_t formatted;
    base_sink<Mutex>::formatter_->format(msg, formatted);
    auto new_size = sink_data.current_size + formatted.size();

    // Rotate if the new estimated file size exceeds max size
    if (new_size > max_size_) {
        sink_data.file_helper.flush();
        if (sink_data.file_helper.size() > 0) {
            rotate_(type);
            new_size = formatted.size();
        }
    }
    
    sink_data.file_helper.write(formatted);
    sink_data.current_size = new_size;
}

template <typename Mutex>
SPDLOG_INLINE filename_t business_file_sink<Mutex>::get_filename(business_type type) {
    auto& sink_data = get_or_create_sink_(type);
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    return sink_data.file_helper.filename();
}

template <typename Mutex>
SPDLOG_INLINE void business_file_sink<Mutex>::rotate_business(business_type type) {
    auto& sink_data = get_or_create_sink_(type);
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    rotate_(type);
}

template <typename Mutex>
SPDLOG_INLINE void business_file_sink<Mutex>::set_max_size(std::size_t max_size) {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    if (max_size == 0) {
        throw_spdlog_ex("business_file_sink set_max_size: max_size arg cannot be zero");
    }
    max_size_ = max_size;
}

template <typename Mutex>
SPDLOG_INLINE void business_file_sink<Mutex>::set_max_files(std::size_t max_files) {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    if (max_files == 0) {
        throw_spdlog_ex("business_file_sink set_max_files: max_files arg cannot be zero");
    }
    max_files_ = max_files;
}

template <typename Mutex>
SPDLOG_INLINE void business_file_sink<Mutex>::sink_it_(const details::log_msg &msg) {
    // Default behavior: log to unknown business type
    log_to_business(business_type::unknown, msg);
}

template <typename Mutex>
SPDLOG_INLINE void business_file_sink<Mutex>::flush_() {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    for (auto it = business_sinks_.begin(); it != business_sinks_.end(); ++it) {
        it->second->file_helper.flush();
    }
}

template <typename Mutex>
SPDLOG_INLINE void business_file_sink<Mutex>::rotate_(business_type type) {
    using details::os::filename_to_str;
    using details::os::path_exists;

    auto it = business_sinks_.find(type);
    if (it == business_sinks_.end()) {
        return;
    }
    
    auto& sink_data = *it->second;
    filename_t base_filename = sink_data.base_filename;
    
    sink_data.file_helper.close();
    for (auto i = max_files_; i > 0; --i) {
        filename_t src = calc_filename_(base_filename, i - 1);
        if (!path_exists(src)) {
            continue;
        }
        filename_t target = calc_filename_(base_filename, i);

        if (!rename_file_(src, target)) {
            // If failed, try again after a small delay (Windows workaround)
            details::os::sleep_for_millis(100);
            if (!rename_file_(src, target)) {
                sink_data.file_helper.reopen(true);
                sink_data.current_size = 0;
                throw_spdlog_ex("business_file_sink: failed renaming " + filename_to_str(src) +
                                    " to " + filename_to_str(target),
                                errno);
            }
        }
    }
    sink_data.file_helper.reopen(true);
    sink_data.current_size = 0;
}

template <typename Mutex>
SPDLOG_INLINE bool business_file_sink<Mutex>::rename_file_(const filename_t &src_filename,
                                                            const filename_t &target_filename) {
    // Try to delete the target file in case it already exists
    (void)details::os::remove(target_filename);
    return details::os::rename(src_filename, target_filename) == 0;
}

}  // namespace sinks

//
// business_logger implementation
//
SPDLOG_INLINE business_logger::business_logger(const std::string& logger_name,
                                                const filename_t& base_dir,
                                                std::size_t max_file_size,
                                                std::size_t max_files) 
    : name_(logger_name), level_(level::info) {
    
    sink_ = std::make_shared<sinks::business_file_sink_mt>(base_dir, max_file_size, max_files);
}

SPDLOG_INLINE void business_logger::log(sinks::business_type type, 
                                        level::level_enum lvl, 
                                        const std::string& msg) {
    if (lvl < level_) {
        return;
    }
    
    details::log_msg log_msg;
    log_msg.logger_name = string_view_t(name_.data(), name_.size());
    log_msg.level = lvl;
    log_msg.time = log_clock::now();
    log_msg.payload = string_view_t(msg.data(), msg.size());
    
    sink_->log_to_business(type, log_msg);
}

SPDLOG_INLINE void business_logger::trace(sinks::business_type type, const std::string& msg) {
    log(type, level::trace, msg);
}

SPDLOG_INLINE void business_logger::debug(sinks::business_type type, const std::string& msg) {
    log(type, level::debug, msg);
}

SPDLOG_INLINE void business_logger::info(sinks::business_type type, const std::string& msg) {
    log(type, level::info, msg);
}

SPDLOG_INLINE void business_logger::warn(sinks::business_type type, const std::string& msg) {
    log(type, level::warn, msg);
}

SPDLOG_INLINE void business_logger::error(sinks::business_type type, const std::string& msg) {
    log(type, level::err, msg);
}

SPDLOG_INLINE void business_logger::critical(sinks::business_type type, const std::string& msg) {
    log(type, level::critical, msg);
}

SPDLOG_INLINE void business_logger::set_level(level::level_enum lvl) {
    level_ = lvl;
}

SPDLOG_INLINE void business_logger::set_pattern(const std::string& pattern) {
    auto formatter = details::make_unique<pattern_formatter>(pattern);
    sink_->set_formatter(std::move(formatter));
}

}  // namespace spdlog
