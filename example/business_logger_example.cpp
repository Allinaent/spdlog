#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <map>
#include <string>
#include <iostream>

// 业务类型枚举
enum class BusinessType {
    SCREEN_RECORD,    // 录制屏幕
    DESKTOP_OPEN,     // 打开桌面
    KEYBOARD_RECORD,  // 录制键盘
    AUDIO_RECORD      // 录制声音
};

// 业务日志管理器
class BusinessLoggerManager {
public:
    // 获取单例实例
    static BusinessLoggerManager& getInstance() {
        static BusinessLoggerManager instance;
        return instance;
    }

    // 获取指定业务的日志记录器
    std::shared_ptr<spdlog::logger> getLogger(BusinessType type) {
        auto it = loggers_.find(type);
        if (it != loggers_.end()) {
            return it->second;
        }

        // 创建新的日志记录器
        std::string logger_name = getBusinessName(type);
        std::string filename = "logs/" + logger_name + ".log";
        
        // 每个日志文件最大30MB，最多保留3个文件
        const size_t max_size = 30 * 1024 * 1024; // 30MB
        const size_t max_files = 3;

        auto logger = spdlog::rotating_logger_mt(logger_name, filename, max_size, max_files);
        loggers_[type] = logger;
        return logger;
    }

    // 根据业务类型获取业务名称
    std::string getBusinessName(BusinessType type) {
        switch (type) {
            case BusinessType::SCREEN_RECORD:
                return "screen_record";
            case BusinessType::DESKTOP_OPEN:
                return "desktop_open";
            case BusinessType::KEYBOARD_RECORD:
                return "keyboard_record";
            case BusinessType::AUDIO_RECORD:
                return "audio_record";
            default:
                return "unknown";
        }
    }

private:
    BusinessLoggerManager() = default;
    ~BusinessLoggerManager() = default;

    // 禁用拷贝构造和赋值操作
    BusinessLoggerManager(const BusinessLoggerManager&) = delete;
    BusinessLoggerManager& operator=(const BusinessLoggerManager&) = delete;

    // 存储不同业务的日志记录器
    std::map<BusinessType, std::shared_ptr<spdlog::logger>> loggers_;
};

// 业务日志宏，方便使用
#define BUSINESS_LOG(type, level, ...) \
    BusinessLoggerManager::getInstance().getLogger(type)->level(__VA_ARGS__)

// 业务日志记录函数
void logBusiness(BusinessType type, const std::string& message) {
    BusinessLoggerManager::getInstance().getLogger(type)->info(message);
}

int main() {
    try {
        // 测试不同业务的日志记录
        BUSINESS_LOG(BusinessType::SCREEN_RECORD, info, "开始录制屏幕");
        BUSINESS_LOG(BusinessType::SCREEN_RECORD, info, "录制屏幕中...");
        BUSINESS_LOG(BusinessType::SCREEN_RECORD, info, "结束录制屏幕");

        BUSINESS_LOG(BusinessType::DESKTOP_OPEN, info, "打开桌面");
        BUSINESS_LOG(BusinessType::DESKTOP_OPEN, info, "桌面已打开");

        BUSINESS_LOG(BusinessType::KEYBOARD_RECORD, info, "开始录制键盘");
        BUSINESS_LOG(BusinessType::KEYBOARD_RECORD, info, "录制键盘中...");
        BUSINESS_LOG(BusinessType::KEYBOARD_RECORD, info, "结束录制键盘");

        BUSINESS_LOG(BusinessType::AUDIO_RECORD, info, "开始录制声音");
        BUSINESS_LOG(BusinessType::AUDIO_RECORD, info, "录制声音中...");
        BUSINESS_LOG(BusinessType::AUDIO_RECORD, info, "结束录制声音");

        // 使用函数形式记录日志
        logBusiness(BusinessType::SCREEN_RECORD, "使用函数形式记录屏幕录制日志");

        std::cout << "日志记录完成，请查看logs目录下的日志文件" << std::endl;

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "日志初始化失败: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
