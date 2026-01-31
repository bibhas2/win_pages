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


void check_throw(HRESULT hr) {
    if (!SUCCEEDED(hr)) {
        throw hr;
    }
}

void test_xml() {
	CComPtr<IXmlReader> pReader;

    HRESULT hr = ::CreateXmlReader(__uuidof(IXmlReader), (void**)&pReader, NULL);
    check_throw(hr);

    //Read XML from test.xml file
	CComPtr<IStream> pFileStream;
	hr = SHCreateStreamOnFileEx(L"test.xml", STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &pFileStream);
	check_throw(hr);
	hr = pReader->SetInput(pFileStream);
	check_throw(hr);

    while (true) {
        XmlNodeType nodeType;

        hr = pReader->Read(&nodeType);

        if (hr == S_FALSE) {
            break; //End of file
        }

        check_throw(hr);

        if (nodeType == XmlNodeType_Element) {
            const wchar_t* pwszLocalName = NULL;
            hr = pReader->GetLocalName(&pwszLocalName, NULL);
            check_throw(hr);
            //For demonstration, we just show a message box with the element name
            OutputDebugStringW(pwszLocalName);
			OutputDebugStringW(L"\n");

			//Print the font-family attribute if it exists
			const wchar_t* pwszValue = NULL;
			hr = pReader->MoveToAttributeByName(L"font-family", NULL);

            if (hr == S_FALSE) {
                continue; //Attribute not found
			}

			check_throw(hr);

			hr = pReader->GetValue(&pwszValue, NULL);
            check_throw(hr);

            OutputDebugStringW(L"\tfont-family: ");
            OutputDebugStringW(pwszValue);
            OutputDebugStringW(L"\n");
        }
        else if (nodeType == XmlNodeType_Text) {
			const wchar_t* pwszValue = NULL;

			hr = pReader->GetValue(&pwszValue, NULL);
			check_throw(hr);

            OutputDebugStringW(L"\tValue: ");
            OutputDebugStringW(pwszValue);
            OutputDebugStringW(L"\n");
        }
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

            test_xml();
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