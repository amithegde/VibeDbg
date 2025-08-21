#include "pch.h"
#include "windbg_helpers.h"
#include "constants.h"
#include "../core/extension_impl.h"

using namespace vibedbg::utils;
using namespace vibedbg::core;

// Output callback class to capture WinDbg command output
class OutputCallback : public IDebugOutputCallbacks {
public:
    OutputCallback() : ref_count_(1) {}
    
    // IUnknown methods
    STDMETHOD_(ULONG, AddRef)() override {
        return InterlockedIncrement(&ref_count_);
    }
    
    STDMETHOD_(ULONG, Release)() override {
        ULONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) {
            delete this;
        }
        return count;
    }
    
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == __uuidof(IDebugOutputCallbacks)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    
    // IDebugOutputCallbacks method
    STDMETHOD(Output)(ULONG mask, PCSTR text) override {
        if (text && (mask & DEBUG_OUTPUT_NORMAL)) {
            output_stream_ << text;
        }
        return S_OK;
    }
    
    std::string GetOutput() const {
        return output_stream_.str();
    }
    
    void ClearOutput() {
        output_stream_.str("");
        output_stream_.clear();
    }
    
private:
    LONG ref_count_;
    std::ostringstream output_stream_;
};

std::string WinDbgHelpers::execute_command(std::string_view command, HRESULT* error) {
    return execute_command_with_timeout(command, std::chrono::milliseconds(30000), error);
}

std::string WinDbgHelpers::execute_command_with_timeout(
    std::string_view command, 
    [[maybe_unused]] std::chrono::milliseconds timeout,
    HRESULT* error) {
    
    if (error) *error = S_OK;
    
    auto* debug_control = get_debug_control();
    auto* debug_client = get_debug_client();
    if (!debug_control || !debug_client) {
        if (error) *error = E_FAIL;
        return "";
    }
    
    // Create output callback to capture command output
    OutputCallback* output_callback = new OutputCallback();
    
    // Get current output callbacks to restore later
    IDebugOutputCallbacks* old_callbacks = nullptr;
    debug_client->GetOutputCallbacks(&old_callbacks);
    
    // Set our output callback
    HRESULT hr = debug_client->SetOutputCallbacks(output_callback);
    if (FAILED(hr)) {
        output_callback->Release();
        if (old_callbacks) old_callbacks->Release();
        if (error) *error = hr;
        return "";
    }
    
    // Execute the command
    std::string command_str(command);
    hr = debug_control->Execute(DEBUG_OUTCTL_THIS_CLIENT,
                               command_str.c_str(),
                               DEBUG_EXECUTE_DEFAULT);
    
    // Get the captured output
    std::string output = output_callback->GetOutput();
    
    // Restore original output callbacks
    debug_client->SetOutputCallbacks(old_callbacks);
    if (old_callbacks) old_callbacks->Release();
    output_callback->Release();
    
    if (FAILED(hr)) {
        if (error) *error = hr;
        return "";
    }
    
    // Return the actual command output
    return output;
}

std::string WinDbgHelpers::capture_command_output(std::string_view command, HRESULT* error) {
    // Now we have proper output capture
    return execute_command_with_timeout(command, std::chrono::milliseconds(30000), error);
}


bool WinDbgHelpers::is_user_mode_debugging() {
    return true;
}

bool WinDbgHelpers::is_live_debugging() {
    return true;
}

uint32_t WinDbgHelpers::get_current_process_id(HRESULT* error) {
    if (error) *error = S_OK;
    
    auto* debug_control = get_debug_control();
    if (!debug_control) {
        if (error) *error = E_FAIL;
        return 0;
    }
    
    // For now, return a dummy value since we need IDebugSystemObjects interface
    // In a full implementation, we would get IDebugSystemObjects from debug_client
    if (error) *error = E_NOTIMPL;
    return 0;
}

uint32_t WinDbgHelpers::get_current_thread_id(HRESULT* error) {
    if (error) *error = S_OK;
    
    auto* debug_control = get_debug_control();
    if (!debug_control) {
        if (error) *error = E_FAIL;
        return 0;
    }
    
    // For now, return a dummy value since we need IDebugSystemObjects interface
    // In a full implementation, we would get IDebugSystemObjects from debug_client
    if (error) *error = E_NOTIMPL;
    return 0;
}

std::string WinDbgHelpers::get_current_process_name(HRESULT* error) {
    if (error) *error = S_OK;
    
    auto* debug_control = get_debug_control();
    if (!debug_control) {
        if (error) *error = E_FAIL;
        return "";
    }
    
    // For now, return a dummy value since we need IDebugSystemObjects interface
    // In a full implementation, we would get IDebugSystemObjects from debug_client
    if (error) *error = E_NOTIMPL;
    return "unknown_process";
}

std::vector<uint8_t> WinDbgHelpers::read_memory(
    uintptr_t address, 
    size_t size,
    HRESULT* error) {
    
    if (error) *error = S_OK;
    
    auto* debug_data_spaces = get_debug_data_spaces();
    if (!debug_data_spaces) {
        if (error) *error = E_FAIL;
        return {};
    }
    
    std::vector<uint8_t> buffer(size);
    ULONG bytes_read;
    
    HRESULT hr = debug_data_spaces->ReadVirtual(
        address, 
        buffer.data(), 
        static_cast<ULONG>(size), 
        &bytes_read);
    
    if (FAILED(hr)) {
        if (error) *error = hr;
        return {};
    }
    
    buffer.resize(bytes_read);
    return buffer;
}

HRESULT WinDbgHelpers::write_memory(
    uintptr_t address, 
    const std::vector<uint8_t>& data) {
    
    auto* debug_data_spaces = get_debug_data_spaces();
    if (!debug_data_spaces) {
        return E_FAIL;
    }
    
    ULONG bytes_written;
    HRESULT hr = debug_data_spaces->WriteVirtual(
        address,
        const_cast<uint8_t*>(data.data()),
        static_cast<ULONG>(data.size()),
        &bytes_written);
    
    return hr;
}

uintptr_t WinDbgHelpers::get_symbol_address(std::string_view symbol, HRESULT* error) {
    if (error) *error = S_OK;
    
    auto* debug_symbols = get_debug_symbols();
    if (!debug_symbols) {
        if (error) *error = E_FAIL;
        return 0;
    }
    
    ULONG64 address;
    std::string symbol_str(symbol);
    HRESULT hr = debug_symbols->GetOffsetByName(symbol_str.c_str(), &address);
    
    if (FAILED(hr)) {
        if (error) *error = hr;
        return 0;
    }
    
    return static_cast<uintptr_t>(address);
}

std::string WinDbgHelpers::get_symbol_name(uintptr_t address, HRESULT* error) {
    if (error) *error = S_OK;
    
    auto* debug_symbols = get_debug_symbols();
    if (!debug_symbols) {
        if (error) *error = E_FAIL;
        return "";
    }
    
    char name_buffer[256];
    ULONG name_size;
    ULONG64 displacement;
    
    HRESULT hr = debug_symbols->GetNameByOffset(
        address, 
        name_buffer, 
        sizeof(name_buffer), 
        &name_size, 
        &displacement);
    
    if (FAILED(hr)) {
        if (error) *error = hr;
        return "";
    }
    
    std::string result(name_buffer, name_size > 0 ? name_size - 1 : 0);
    if (displacement > 0) {
        result += std::format("+0x{:x}", displacement);
    }
    
    return result;
}

std::string WinDbgHelpers::format_windbg_error(HRESULT hr) {
    return std::format("HRESULT: 0x{:08x}", static_cast<uint32_t>(hr));
}

std::string WinDbgHelpers::format_last_error() {
    DWORD error = GetLastError();
    
    LPSTR message_buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr);
    
    std::string message(message_buffer, size);
    LocalFree(message_buffer);
    
    return trim_whitespace(message);
}

std::string WinDbgHelpers::trim_whitespace(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> WinDbgHelpers::split_lines(const std::string& str) {
    std::vector<std::string> lines;
    std::istringstream stream(str);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    return lines;
}

std::string WinDbgHelpers::join_lines(const std::vector<std::string>& lines) {
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            result += "\n";
        }
        result += lines[i];
    }
    return result;
}

// Private helper methods
IDebugControl* WinDbgHelpers::get_debug_control() {
    auto& extension = ExtensionImpl::get_instance();
    return extension.get_debug_control();
}

IDebugDataSpaces* WinDbgHelpers::get_debug_data_spaces() {
    auto& extension = ExtensionImpl::get_instance();
    return extension.get_debug_data_spaces();
}

IDebugSymbols* WinDbgHelpers::get_debug_symbols() {
    auto& extension = ExtensionImpl::get_instance();
    return extension.get_debug_symbols();
}

IDebugRegisters* WinDbgHelpers::get_debug_registers() {
    auto& extension = ExtensionImpl::get_instance();
    return extension.get_debug_registers();
}

IDebugClient* WinDbgHelpers::get_debug_client() {
    auto& extension = ExtensionImpl::get_instance();
    return extension.get_debug_client();
}

