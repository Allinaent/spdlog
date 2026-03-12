// Copyright(c) 2025-present.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <memory>

namespace spdlog {
namespace sinks {

// Business type enumeration
enum class business_type {
    screen_recorder = 0,    // 录制屏幕
    desktop_open = 1,       // 打开桌面
    keyboard_recorder = 2,  // 录制键盘
    sound_recorder = 3,     // 录制声音
    unknown = 4
};

// Hash function for business_type
struct business_type_hash {
    std::size_t operator()(const business_type& type) const {
        return static_cast<std::size_t>(type);
    }
};

// Convert business_type to string
inline std::string business_type_to_string(business_type type) {
    switch (type) {
        case business_type::screen_recorder:   return "screen_recorder";
        case business_type::desktop_open:      return "desktop_open";
        case business_type::keyboard_recorder: return "keyboard_recorder";
        case business_type::sound_recorder:    return "sound_recorder";
        default:                               return "unknown";
    }
}

// Convert string to business_type
inline business_type string_to_business_type(const std::string& str) {
    if (str == "screen_recorder")   return business_type::screen_recorder;
    if (str == "desktop_open")      return business_type::desktop_open;
    if (str == "keyboard_recorder") return business_type::keyboard_recorder;
    if (str == "sound_recorder")    return business_type::sound_recorder;
    return business_type::unknown;
}

//
// Business file sink - manages separate rotating log files for different business types
//
template <typename Mutex>
class business_file_sink final : public base_sink<Mutex> {
public:
    business_file_sink(filename_t base_dir,
                       std::size_t max_file_size = 30 * 1024 * 1024,  // 30MB default
                       std::size_t max_files = 3,
                       const file_event_handlers &event_handlers = {});
    
    ~business_file_sink() override = default;

    // Write log to specific business file
    void log_to_business(business_type type, const details::log_msg &msg);
    
    // Get current log file path for a business type
    filename_t get_filename(business_type type);
    
    // Force rotation for a specific business
    void rotate_business(business_type type);
    
    // Set max file size for all business types
    void set_max_size(std::size_t max_size);
    
    // Set max files for all business types
    void set_max_files(std::size_t max_files);

protected:
    void sink_it_(const details::log_msg &msg) override;
    void flush_() override;

private:
    // Internal rotating file helper for each business type
    struct business_sink_data {
        filename_t base_filename;
        std::size_t current_size;
        details::file_helper file_helper;
        
        business_sink_data() : current_size(0) {}
        business_sink_data(const filename_t& filename, const file_event_handlers& handlers)
            : base_filename(filename), current_size(0), file_helper(handlers) {}
    };

    void rotate_(business_type type);
    filename_t calc_filename_(const filename_t &filename, std::size_t index);
    bool rename_file_(const filename_t &src_filename, const filename_t &target_filename);
    business_sink_data& get_or_create_sink_(business_type type);

    filename_t base_dir_;
    std::size_t max_size_;
    std::size_t max_files_;
    file_event_handlers event_handlers_;
    std::unordered_map<business_type, std::unique_ptr<business_sink_data>, business_type_hash> business_sinks_;
};

using business_file_sink_mt = business_file_sink<std::mutex>;
using business_file_sink_st = business_file_sink<details::null_mutex>;

}  // namespace sinks

//
// Business logger - provides convenient interface for business logging
//
class business_logger {
public:
    business_logger(const std::string& logger_name,
                    const filename_t& base_dir,
                    std::size_t max_file_size = 30 * 1024 * 1024,
                    std::size_t max_files = 3);

    // Log to specific business type
    void log(sinks::business_type type, level::level_enum lvl, const std::string& msg);
    void trace(sinks::business_type type, const std::string& msg);
    void debug(sinks::business_type type, const std::string& msg);
    void info(sinks::business_type type, const std::string& msg);
    void warn(sinks::business_type type, const std::string& msg);
    void error(sinks::business_type type, const std::string& msg);
    void critical(sinks::business_type type, const std::string& msg);

    // Set log level
    void set_level(level::level_enum lvl);
    
    // Set pattern
    void set_pattern(const std::string& pattern);

    // Get underlying sink
    std::shared_ptr<sinks::business_file_sink_mt> sink() const { return sink_; }

private:
    std::string name_;
    std::shared_ptr<sinks::business_file_sink_mt> sink_;
    level::level_enum level_;
};

//
// factory functions
//
template <typename Factory = spdlog::synchronous_factory>
std::shared_ptr<logger> business_logger_mt(const std::string &logger_name,
                                           const filename_t &base_dir,
                                           size_t max_file_size = 30 * 1024 * 1024,
                                           size_t max_files = 3,
                                           const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::business_file_sink_mt>(
        logger_name, base_dir, max_file_size, max_files, event_handlers);
}

}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "business_file_sink-inl.h"
#endif
