// win_pages.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "win_pages.h"
#include <commdlg.h>
#include <mgui.h>
#include "SVGUtil.h"
#include <shobjidl.h>
#include <xmllite.h>

#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")
//We need dxguid.lib for some of the CLSID and IID definitions
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "dwrite.lib")

void check_throw(HRESULT hr) {
    if (!SUCCEEDED(hr)) {
        throw hr;
    }
}

class MainWindow : public CFrame {
	SVGUtil svgUtil;
public:
    
    void create() {
        CFrame::create("Image Viewer", 800, 600, IDC_WINPAGES);

		svgUtil.init(getWindow());
    }
    void onClose() override {
        CWindow::stop();
	}
    void onCommand(int id, int type, CWindow* source) override {
        if (id == ID_FILE_OPEN) {
            std::wstring filename;

            if (!openFileName(L"Open SVG File",
                { { L"SVG Files", L"*.svg" }, { L"All Files", L"*.*" } },
                filename)) {
                return;
			}

			svgUtil.parse(filename.c_str());

			svgUtil.redraw();
        }
        else if (id == IDM_EXIT) {
            onClose();
        }
	}

    bool handleEvent(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_PAINT:
            PAINTSTRUCT ps;

            //We must call BeginPaint and EndPaint to validate the
            //invalidated region, or else we will get continuous
            //WM_PAINT messages.
            BeginPaint(m_wnd, &ps);
            svgUtil.render();
            EndPaint(m_wnd, &ps);
            break;
        case WM_SIZE:
            svgUtil.resize();
            break;
        case WM_ERASEBKGND:
			//Handle background erase to avoid flickering 
            //during resizing and move
            break;
        default:
            return CWindow::handleEvent(message, wParam, lParam);
        }

        return true;
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    CWindow::init(hInstance, IDC_WINPAGES);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (!SUCCEEDED(hr)) {
        return FALSE;
    }

    MainWindow mainWin;

    mainWin.create();
    mainWin.show();

    CWindow::loop();

    CoUninitialize();

    return 0;
}