#pragma once

#include <Windows.h>
#include <utility>

namespace vibedbg::utils {

// RAII wrapper for Windows HANDLE objects
class HandleWrapper {
public:
    explicit HandleWrapper(HANDLE handle = INVALID_HANDLE_VALUE) noexcept
        : handle_(handle) {}
    
    ~HandleWrapper() noexcept {
        close();
    }
    
    // Non-copyable but movable
    HandleWrapper(const HandleWrapper&) = delete;
    HandleWrapper& operator=(const HandleWrapper&) = delete;
    
    HandleWrapper(HandleWrapper&& other) noexcept
        : handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE)) {}
    
    HandleWrapper& operator=(HandleWrapper&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
        }
        return *this;
    }
    
    // Access methods
    HANDLE get() const noexcept { return handle_; }
    HANDLE* get_address_of() noexcept { return &handle_; }
    
    bool is_valid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }
    
    explicit operator bool() const noexcept {
        return is_valid();
    }
    
    // Release ownership without closing
    HANDLE release() noexcept {
        return std::exchange(handle_, INVALID_HANDLE_VALUE);
    }
    
    // Reset to new handle (closes current if valid)
    void reset(HANDLE new_handle = INVALID_HANDLE_VALUE) noexcept {
        if (handle_ != new_handle) {
            close();
            handle_ = new_handle;
        }
    }
    
    // Close handle manually
    void close() noexcept {
        if (is_valid()) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_;
};

// Helper function to create HandleWrapper
inline HandleWrapper make_handle(HANDLE handle) noexcept {
    return HandleWrapper(handle);
}

} // namespace vibedbg::utils