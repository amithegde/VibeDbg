// pch.h: This is a precompiled header file.
#ifndef PCH_H
#define PCH_H

#include "framework.h"

// WinDBG extension headers with appropriate definitions
#define KDEXT_64BIT
#include <Windows.h>
#include <DbgEng.h>
#include <WDBGEXTS.H>
#include <atlcomcli.h>

// Standard C++ headers
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <memory>
#include <chrono>
#include <format>
#include <algorithm>
#include <cctype>
#include <span>
#include <shared_mutex>

// JSON library
#include "../inc/json.h"

// Logging system
#include "../inc/logging.h"

// Forward declaration for ExtensionApis - defined in extension_main.cpp
extern WINDBG_EXTENSION_APIS64 ExtensionApis;

#endif //PCH_H