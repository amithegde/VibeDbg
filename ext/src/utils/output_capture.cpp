#include "pch.h"
#include "output_capture.h"
#include "constants.h"

OutputCapture::OutputCapture() : ref_count_(1) {
}

OutputCapture::~OutputCapture() = default;

STDMETHODIMP OutputCapture::QueryInterface(REFIID InterfaceId, PVOID* Interface) {
    if (!Interface) {
        return E_POINTER;
    }

    *Interface = nullptr;

    if (IsEqualIID(InterfaceId, __uuidof(IUnknown)) ||
        IsEqualIID(InterfaceId, __uuidof(IDebugOutputCallbacks))) {
        *Interface = this;
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) OutputCapture::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) OutputCapture::Release() {
    LONG result = InterlockedDecrement(&ref_count_);
    if (result == 0) {
        delete this;
    }
    return result;
}

STDMETHODIMP OutputCapture::Output([[maybe_unused]] ULONG Mask, PCSTR Text) {
    if (!Text) {
        return S_OK;
    }

    std::lock_guard<std::mutex> lock(output_mutex_);
    
    // Prevent output buffer from growing too large
    if (output_.size() + strlen(Text) > Constants::MAX_OUTPUT_SIZE) {
        output_ += "\n[Output truncated - maximum size exceeded]\n";
        return S_OK;
    }

    ProcessOutputText(std::string(Text));
    return S_OK;
}

std::string OutputCapture::GetOutput() const {
    std::lock_guard<std::mutex> lock(output_mutex_);
    return output_;
}

void OutputCapture::Clear() {
    std::lock_guard<std::mutex> lock(output_mutex_);
    output_.clear();
    extension_error_ = false;
    export_error_ = false;
}

void OutputCapture::ProcessOutputText(const std::string& text) {
    if (IsWarningMessage(text)) {
        output_ += "Note: " + text + "\n";
    } else if (IsExtensionError(text)) {
        if (!extension_error_) {
            output_ += FormatErrorMessage(text);
            extension_error_ = true;
        }
    } else if (IsExportError(text)) {
        if (!export_error_) {
            output_ += FormatErrorMessage(text);
            export_error_ = true;
        }
    } else {
        output_ += text;
    }
}

bool OutputCapture::IsWarningMessage(const std::string& text) {
    return text.find("WARNING: .cache forcedecodeuser is not enabled") != std::string::npos;
}

bool OutputCapture::IsExtensionError(const std::string& text) {
    return text.find("is not extension gallery command") != std::string::npos;
}

bool OutputCapture::IsExportError(const std::string& text) {
    return text.find("No export") != std::string::npos && text.find("found") != std::string::npos;
}

std::string OutputCapture::FormatErrorMessage(const std::string& text) {
    if (IsExtensionError(text)) {
        auto pos = text.find(" is not extension gallery command");
        if (pos != std::string::npos) {
            auto cmdName = text.substr(0, pos);
            if (cmdName == "modinfo") {
                return "Note: The !modinfo command is not available. Using alternative lmv command instead.\n";
            } else {
                return "Error: Command '" + cmdName + "' is not available. Make sure the required extension is loaded.\n";
            }
        }
    } else if (IsExportError(text)) {
        auto pos = text.find(" found");
        if (pos != std::string::npos) {
            auto cmdName = text.substr(9, pos - 9);  // Extract name after "No export "
            return "Note: Command '" + cmdName + "' is not available in the current debugging context.\n";
        }
    }
    return text;
}

// OutputCaptureHelper implementation
OutputCaptureHelper::OutputCaptureHelper(IDebugClient* debug_client) 
    : debug_client_(debug_client), output_capture_(nullptr), previous_callbacks_(nullptr) {
    
    if (!debug_client_) {
        return;
    }

    // Create output capture instance
    output_capture_ = new OutputCapture();
    
    // Get current callbacks to restore later
    debug_client_->GetOutputCallbacks(&previous_callbacks_);
    
    // Set our capture callbacks
    debug_client_->SetOutputCallbacks(output_capture_);
}

OutputCaptureHelper::~OutputCaptureHelper() {
    if (debug_client_) {
        // Restore previous callbacks
        debug_client_->SetOutputCallbacks(previous_callbacks_);
        
        if (previous_callbacks_) {
            previous_callbacks_->Release();
        }
    }
    
    if (output_capture_) {
        output_capture_->Release();
    }
}

std::string OutputCaptureHelper::GetCapturedOutput() const {
    if (output_capture_) {
        return output_capture_->GetOutput();
    }
    return "";
}