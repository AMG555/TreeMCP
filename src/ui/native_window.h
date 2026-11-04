#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl.h>
#include "../../deps/include/WebView2.h"
#include <string>

using namespace Microsoft::WRL;

namespace TreeMCP {

class NativeWindow {
public:
    static bool run_native_window(const std::string& url, int width = 1280, int height = 820);
    static void close();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace TreeMCP
#endif
