/**
 * @file logger.h
 * @brief Lightweight, generic logging interface.
 *
 * This logger is designed for:
 *  - low overhead
 *  - deterministic formatting
 *  - compile-time log level filtering
 *
 * Logging is performed synchronously to stderr and is intended for debugging,
 * diagnostics, and educational clarity—not high-performance production logging.
 *
 * Log statements at levels above LOG_LEVEL are compiled out entirely.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Log level numeric constants
 * @brief Compile-time numeric values for log severity levels.
 *
 * Lower values represent more verbose logging.
 * These values are used both by the log_level_t enum and by
 * compile-time filtering via LOG_LEVEL.
 *
 */
#define LOG_LEVEL_TRACE 10 /**< Most verbose: trace-level diagnostics. */
#define LOG_LEVEL_DEBUG 20 /**< Debug messages useful during development. */
#define LOG_LEVEL_INFO 30  /**< Informational messages (default). */
#define LOG_LEVEL_WARN 40  /**< Warnings: unexpected but recoverable states. */
#define LOG_LEVEL_ERROR 50 /**< Errors: operation failed. */
#define LOG_LEVEL_CRIT \
    60 /**< Critical errors: loader likely cannot continue. */

/**
 * @def LOG_LEVEL
 * @brief Compile-time maximum verbosity level.
 *
 * Log macros whose severity is numerically greater than LOG_LEVEL
 * are compiled out entirely.
 *
 * Example:
 *   make LOG=LOG_LEVEL_DEBUG
 */
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

/**
 * @enum log_level_t
 * @brief Severity levels for logging output.
 *
 * Log levels are ordered from least to most severe. Filtering may be applied at
 * compile time or runtime.
 */
typedef enum {
    LOG_TRACE = LOG_LEVEL_TRACE,
    LOG_DEBUG = LOG_LEVEL_DEBUG,
    LOG_INFO = LOG_LEVEL_INFO,
    LOG_WARN = LOG_LEVEL_WARN,
    LOG_ERROR = LOG_LEVEL_ERROR,
    LOG_CRIT = LOG_LEVEL_CRIT,
} log_level_t;

/**
 * @def LOGGER_PRINTF_FMT
 * @brief Enable printf-style format checking for a function.
 *
 * This macro expands to a compiler attribute on GCC/Clang and
 * is a no-op on other compilers.
 *
 * @param fmt_index 1-based index of the format string argument.
 * @param first_arg 1-based index of the first variadic argument.
 */
#if defined(__GNUC__) || defined(__clang__)
#define LOGGER_PRINTF_FMT(fmt_index, first_arg) \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define LOGGER_PRINTF_FMT(fmt_index, first_arg)
#endif

/**
 * @brief Output a formatted log message with source context.
 *
 * Formats and writes a log message to stderr, including header meta data:
 * - timestamp (seconds.milliseconds since epoch)
 * - log severity
 * - source file and line number
 * - calling function name
 *
 * This function is intended to be wrapped by logging macros that automatically
 * supply file, line, and function context.
 *
 * @param lvl  Log severity level confirming message importance.
 * @param file Source file name (usually __FILE__).
 * @param line Source line number (usually __LINE__).
 * @param func Calling function name (usually __func__).
 * @param fmt  printf-style format string.
 * @param ...  Additional arguments referenced by fmt.
 */
void log_msg(log_level_t lvl,
             const char* file,
             int line,
             const char* func,
             const char* fmt,
             ...) LOGGER_PRINTF_FMT(5, 6);

/*
 * Compile-time log level filtering
 *
 * Log macros whose severity is greater than LOG_LEVEL are compiled out entirely
 * (no code is generated and arguments are not evaluated).
 *
 * This makes it safe to write:
 *
 *     DEBUG("x=%d", expensive_call());
 *
 * as long as DEBUG is disabled at compile time.
 *
 * All log macros expect at least a format string. Calling a log macro with no
 * arguments is undefined behavior.
 *
 * __func__ is a C99 feature and is supported by all modern compilers
 * targeted by locus.
 */

#if LOG_LEVEL <= LOG_LEVEL_TRACE
/** @brief Output a TRACE-level log message. */
#define TRACE(...)                                   \
    log_msg(LOG_TRACE, __FILE__, __LINE__, __func__, \
            __VA_ARGS__) /**< Trace log macro. */
#else
#define TRACE(...) \
    ((void)0) /**< TRACE-level logging disabled at compile time. */
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define DEBUG(...)                                   \
    log_msg(LOG_DEBUG, __FILE__, __LINE__, __func__, \
            __VA_ARGS__) /**< Debug log macro. */
#else
#define DEBUG(...) \
    ((void)0) /**< DEBUG-level logging disabled at compile time. */
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define INFO(...)                                   \
    log_msg(LOG_INFO, __FILE__, __LINE__, __func__, \
            __VA_ARGS__) /**< Info log macro. */
#else
#define INFO(...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define WARN(...)                                   \
    log_msg(LOG_WARN, __FILE__, __LINE__, __func__, \
            __VA_ARGS__) /**< Warning log macro. */
#else
#define WARN(...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define ERROR(...)                                   \
    log_msg(LOG_ERROR, __FILE__, __LINE__, __func__, \
            __VA_ARGS__) /**< Error log macro. */
#else
#define ERROR(...) ((void)0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_CRIT
#define CRIT(...)                                   \
    log_msg(LOG_CRIT, __FILE__, __LINE__, __func__, \
            __VA_ARGS__) /**< Critical log macro. */
#else
#define CRIT(...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
