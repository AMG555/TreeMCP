#include "native_window.h"

#ifdef _WIN32
#include <shlobj.h>
#include <iostream>

namespace TreeMCP {

static HWND g_hwnd = NULL;
static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2* g_webview = nullptr;

typedef HRESULT (STDAPICALLTYPE *CreateEnvironmentFn)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler
);

static const IID IID_IUnknown_Custom = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

// Pure COM implementation of ControllerCompletedHandler
class ControllerHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    ControllerHandler(const std::wstring& url) : url_(url), ref_count_(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (InlineIsEqualGUID(riid, IID_IUnknown_Custom) ||
            InlineIsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&ref_count_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) override {
        if (FAILED(errorCode) || !controller) return errorCode;

        g_controller = controller;
        g_controller->AddRef();
        g_controller->get_CoreWebView2(&g_webview);

        if (g_webview) {
            ICoreWebView2Controller2* controller2 = nullptr;
            if (SUCCEEDED(g_controller->QueryInterface(IID_ICoreWebView2Controller2, (void**)&controller2)) && controller2) {
                COREWEBVIEW2_COLOR darkColor{ 255, 24, 24, 27 };
                controller2->put_DefaultBackgroundColor(darkColor);
                controller2->Release();
            }

            RECT bounds;
            GetClientRect(g_hwnd, &bounds);
            g_controller->put_Bounds(bounds);
            g_webview->Navigate(url_.c_str());
        }
        return S_OK;
    }

private:
    std::wstring url_;
    LONG ref_count_;
};

// Pure COM implementation of EnvironmentCompletedHandler
class EnvironmentHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    EnvironmentHandler(const std::wstring& url) : url_(url), ref_count_(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (InlineIsEqualGUID(riid, IID_IUnknown_Custom) ||
            InlineIsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&ref_count_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override {
        if (FAILED(errorCode) || !env) return errorCode;

        ControllerHandler* ctrlHandler = new ControllerHandler(url_);
        env->CreateCoreWebView2Controller(g_hwnd, ctrlHandler);
        ctrlHandler->Release();
        return S_OK;
    }

private:
    std::wstring url_;
    LONG ref_count_;
};

LRESULT CALLBACK NativeWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE: {
            if (g_controller) {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                g_controller->put_Bounds(bounds);
            }
            return 0;
        }
        case WM_SETFOCUS: {
            if (g_controller) {
                g_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

void NativeWindow::close() {
    if (g_hwnd) {
        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    }
}

bool NativeWindow::run_native_window(const std::string& url, int width, int height) {
    HINSTANCE hInstance = GetModuleHandleW(NULL);

    // Register Win32 Window Class
    const wchar_t CLASS_NAME[] = L"TreeMCP_NativeAppWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = CLASS_NAME;

    HICON hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;

    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - width) / 2;
    int posY = (screenH - height) / 2;
    if (posX < 0) posX = 0;
    if (posY < 0) posY = 0;

    g_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"TreeMCP - High Speed Disk Space Analyzer & Hardened MCP Server",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        posX, posY, width, height,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hwnd) return false;

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, NULL, 0);
    std::wstring wurl(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wurl[0], wlen);

    wchar_t local_app_data[MAX_PATH];
    std::wstring user_data_folder;
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data, MAX_PATH) > 0) {
        user_data_folder = std::wstring(local_app_data) + L"\\TreeMCP\\native_profile";
        CreateDirectoryW(user_data_folder.c_str(), NULL);
    }

    HMODULE hLoader = LoadLibraryW(L"WebView2Loader.dll");
    if (!hLoader) {
        hLoader = LoadLibraryW(L"C:\\Program Files\\Microsoft Office\\root\\Office16\\WebView2Loader.dll");
    }

    if (hLoader) {
        CreateEnvironmentFn pCreateEnv = (CreateEnvironmentFn)GetProcAddress(hLoader, "CreateCoreWebView2EnvironmentWithOptions");
        if (pCreateEnv) {
            EnvironmentHandler* envHandler = new EnvironmentHandler(wurl);
            pCreateEnv(
                NULL,
                user_data_folder.empty() ? NULL : user_data_folder.c_str(),
                NULL,
                envHandler
            );
            envHandler->Release();
        }
    }

    MSG msg{};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_webview) {
        g_webview->Release();
        g_webview = nullptr;
    }
    if (g_controller) {
        g_controller->Release();
        g_controller = nullptr;
    }
    g_hwnd = NULL;

    return true;
}

} // namespace TreeMCP
#endif
