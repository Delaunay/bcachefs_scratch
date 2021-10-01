#ifndef BCACHE_FS_SRC_LOGGER_HEADER
#define BCACHE_FS_SRC_LOGGER_HEADER

#include <string>
#include <vector>
// Do not include spdlog directly
// only use the fmt header
#include <spdlog/fmt/bundled/core.h>

#include "version.h"

namespace bcachefs {

// Path to repository on current system
constexpr char __source_dir[] = _SOURCE_DIRECTORY;
// Length of the path so we can cut unimportant folders
constexpr int __size_src_dir = sizeof(_SOURCE_DIRECTORY) / sizeof(char);

struct CodeLocation {
    CodeLocation(std::string const &file, std::string const &fun, int line, std::string const &fun_long):
        filename(file.substr(__size_src_dir + 4)), function_name(fun), line(line), function_long(fun_long) {}

    std::string filename;
    std::string function_name;
    int         line;
    std::string function_long;
};

#define LOC bcachefs::CodeLocation(__FILE__, __FUNCTION__, __LINE__, __PRETTY_FUNCTION__)

enum class LogLevel
{
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL,
    OFF
};

// Show backtrace using spdlog
void show_log_backtrace();

// Show backtrace using execinfo
void show_backtrace();

// retrieve backtrace using execinfo
std::vector<std::string> get_backtrace(size_t size);

void spdlog_log(LogLevel level, const std::string &msg);

template <typename... Args>
void log(LogLevel level, CodeLocation const &loc, const char *fmt, const Args &...args) {
    auto msg = fmt::format("{}:{} {} - {}", loc.filename, loc.line, loc.function_name, fmt::format(fmt, args...));

    spdlog_log(level, msg);
}

#define BCACHEFS_LOGS 1

#if BCACHEFS_LOGS
#    define BCACHEFS_LOG_HELPER(level, ...) log(level, LOC, __VA_ARGS__)
#else
#    define BCACHEFS_LOG_HELPER(level, ...)
#endif

#define info(...)     BCACHEFS_LOG_HELPER(bcachefs::LogLevel::INFO, __VA_ARGS__)
#define warn(...)     BCACHEFS_LOG_HELPER(bcachefs::LogLevel::WARN, __VA_ARGS__)
#define debug(...)    BCACHEFS_LOG_HELPER(bcachefs::LogLevel::DEBUG, __VA_ARGS__)
#define error(...)    BCACHEFS_LOG_HELPER(bcachefs::LogLevel::ERROR, __VA_ARGS__)
#define critical(...) BCACHEFS_LOG_HELPER(bcachefs::LogLevel::CRITICAL, __VA_ARGS__)

// Exception that shows the backtrace when .what() is called
class Exception: public std::exception {
    public:
    template <typename... Args>
    Exception(const char *fmt, const Args &...args): message(fmt::format(fmt, args...).c_str()) {}

    const char *what() const noexcept final;

    private:
    const char *message;
};

// Make a simple exception
#define NEW_EXCEPTION(name)                                                    \
    class name: public bcachefs::Exception {                                   \
        public:                                                                \
        template <typename... Args>                                            \
        name(const char *fmt, const Args &...args): Exception(fmt, args...) {} \
    };
} // namespace bcachefs

#endif
