#include "pch.h"
#include "../../inc/error_handling.h"
#include <format>

using namespace vibedbg::utils;

std::string ErrorHandler::format_error(HRESULT hr) {
    return std::format("HRESULT error: 0x{:08X}", static_cast<uint32_t>(hr));
}

std::string ErrorHandler::format_win32_error(DWORD error) {
    LPSTR message_buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr);
    
    if (size == 0 || !message_buffer) {
        return std::format("Unknown error: {}", error);
    }
    
    std::string message(message_buffer, size);
    LocalFree(message_buffer);
    
    // Remove trailing whitespace
    while (!message.empty() && std::isspace(message.back())) {
        message.pop_back();
    }
    
    return message;
}

std::string ErrorHandler::get_last_error_string() {
    return format_win32_error(GetLastError());
}

void ErrorHandler::log_error(const std::string& message) {
    LOG_ERROR("ErrorHandler", message);
}

void ErrorHandler::log_warning(const std::string& message) {
    LOG_WARNING("ErrorHandler", message);
}

void ErrorHandler::log_info(const std::string& message) {
    LOG_INFO("ErrorHandler", message);
}

void ErrorHandler::check_hr(HRESULT hr, const std::string& context) {
    if (FAILED(hr)) {
        std::string error_msg = std::format("HRESULT check failed: 0x{:08X}", static_cast<uint32_t>(hr));
        if (!context.empty()) {
            error_msg = std::format("{} - {}", context, error_msg);
        }
        log_error(error_msg);
    }
}

void ErrorHandler::check_hr_throw(HRESULT hr, const std::string& context) {
    if (FAILED(hr)) {
        std::string error_msg = std::format("HRESULT check failed: 0x{:08X}", static_cast<uint32_t>(hr));
        if (!context.empty()) {
            error_msg = std::format("{} - {}", context, error_msg);
        }
        throw WinDbgException(error_msg, hr);
    }
}