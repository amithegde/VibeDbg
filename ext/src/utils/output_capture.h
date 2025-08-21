#pragma once

#include "pch.h"
#include <string>

/**
 * @class OutputCapture
 * @brief Implements IDebugOutputCallbacks to capture debugger output.
 */
class OutputCapture : public IDebugOutputCallbacks {
public:
    OutputCapture();
    virtual ~OutputCapture();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID InterfaceId, PVOID* Interface) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDebugOutputCallbacks methods
    STDMETHOD(Output)(ULONG Mask, PCSTR Text) override;

    // Get captured output
    std::string GetOutput() const;
    void Clear();

private:
    std::string output_;
    LONG ref_count_;
    mutable std::mutex output_mutex_;
    bool extension_error_{false};
    bool export_error_{false};
    
    // Helper methods for intelligent output processing
    void ProcessOutputText(const std::string& text);
    bool IsWarningMessage(const std::string& text);
    bool IsExtensionError(const std::string& text);
    bool IsExportError(const std::string& text);
    std::string FormatErrorMessage(const std::string& text);
};

/**
 * @class OutputCaptureHelper
 * @brief Helper class for automatic output capture management.
 */
class OutputCaptureHelper {
public:
    OutputCaptureHelper(IDebugClient* debug_client);
    ~OutputCaptureHelper();

    std::string GetCapturedOutput() const;

private:
    IDebugClient* debug_client_;
    OutputCapture* output_capture_;
    IDebugOutputCallbacks* previous_callbacks_;
};