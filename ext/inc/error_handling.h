#pragma once

#include <string>
#include <Windows.h>
#include <stdexcept>
#include <system_error>

namespace vibedbg::utils {

/**
 * @brief Custom exception class for VibeDbg-specific errors.
 */
class VibeDbgException : public std::runtime_error {
public:
    explicit VibeDbgException(const std::string& message) 
        : std::runtime_error(message) {}
    
    explicit VibeDbgException(const char* message) 
        : std::runtime_error(message) {}
};

/**
 * @brief Exception class for WinDbg-specific errors.
 */
class WinDbgException : public VibeDbgException {
public:
    explicit WinDbgException(const std::string& message, HRESULT hr = S_OK) 
        : VibeDbgException(message), hr_(hr) {}
    
    explicit WinDbgException(const char* message, HRESULT hr = S_OK) 
        : VibeDbgException(message), hr_(hr) {}
    
    HRESULT get_hr() const noexcept { return hr_; }

private:
    HRESULT hr_;
};

/**
 * @brief Exception class for communication errors.
 */
class CommunicationException : public VibeDbgException {
public:
    explicit CommunicationException(const std::string& message) 
        : VibeDbgException(message) {}
    
    explicit CommunicationException(const char* message) 
        : VibeDbgException(message) {}
};

/**
 * @brief RAII wrapper for automatic error handling.
 */
class ScopedErrorHandler {
public:
    ScopedErrorHandler(const std::string& context) : context_(context) {}
    
    ~ScopedErrorHandler() {
        if (std::uncaught_exceptions() > 0) {
            try {
                LOG_ERROR(context_, "Unhandled exception in destructor");
            } catch (...) {
                // Prevent exceptions from destructor
            }
        }
    }
    
    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        try {
            return func();
        } catch (const WinDbgException& e) {
            LOG_ERROR(context_, std::format("WinDbg error: {} (HRESULT: 0x{:08X})", e.what(), e.get_hr()));
            throw;
        } catch (const CommunicationException& e) {
            LOG_ERROR(context_, std::format("Communication error: {}", e.what()));
            throw;
        } catch (const VibeDbgException& e) {
            LOG_ERROR(context_, std::format("VibeDbg error: {}", e.what()));
            throw;
        } catch (const std::exception& e) {
            LOG_ERROR(context_, std::format("Standard exception: {}", e.what()));
            throw;
        } catch (...) {
            LOG_ERROR(context_, "Unknown exception");
            throw;
        }
    }

private:
    std::string context_;
};

/**
 * @brief Macro for automatic error handling in a scope.
 */
#define SCOPED_ERROR_HANDLER(context) \
    vibedbg::utils::ScopedErrorHandler _error_handler(context)

/**
 * @brief Macro for executing code with automatic error handling.
 */
#define EXECUTE_WITH_ERROR_HANDLING(context, code) \
    do { \
        SCOPED_ERROR_HANDLER(context); \
        _error_handler.execute([&]() { code; }); \
    } while(0)

class ErrorHandler {
public:
    // Error formatting
    static std::string format_error(HRESULT hr);
    static std::string format_win32_error(DWORD error);
    static std::string get_last_error_string();
    
    // Logging functions
    static void log_error(const std::string& message);
    static void log_warning(const std::string& message);
    static void log_info(const std::string& message);
    
    // Exception-safe operations
    template<typename Func>
    static auto safe_execute(const std::string& context, Func&& func) -> decltype(func()) {
        ScopedErrorHandler handler(context);
        return handler.execute(std::forward<Func>(func));
    }
    
    // HRESULT checking
    static void check_hr(HRESULT hr, const std::string& context = "");
    static void check_hr_throw(HRESULT hr, const std::string& context = "");
};

} // namespace vibedbg::utils