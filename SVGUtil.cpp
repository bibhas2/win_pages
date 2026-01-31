#include "SVGUtil.h"
#include <sstream>

void SVGGElement::render(ID2D1DeviceContext* pContext) {
	//Save the old transform
	D2D1_MATRIX_3X2_F oldTransform;

	pContext->GetTransform(&oldTransform);

	//Combine all transforms
	D2D1_MATRIX_3X2_F combinedTransform = D2D1::Matrix3x2F::Identity();

	for (const auto& t : transforms) {
		combinedTransform = combinedTransform * t;
	}

	pContext->SetTransform(combinedTransform);

	//Render all child elements
	for (const auto& child : children) {
		child->render(pContext);
	}

	pContext->SetTransform(oldTransform);
}

void SVGPathElement::buildPath(ID2D1Factory* pFactory, const wchar_t* pathData) {
	pFactory->CreatePathGeometry(&pathGeometry);

	CComPtr<ID2D1GeometrySink> pSink;

	pathGeometry->Open(&pSink);

	std::wstringstream ws(pathData);
	wchar_t cmd = 0, last_cmd = 0;

	while (true) {
		ws >> cmd;

		if (cmd != L'L' && cmd != L'Z' && cmd != L'M') {
			ws.unget();

			if (last_cmd == L'M') {
				cmd = L'L';
			}
			else {
				cmd = last_cmd;
			}
		}

		if (cmd == L'Z') {
			//End of path
			break;
		}

		float x = 0.0, y = 0.0;

		if (cmd == L'M') {
			ws >> x >> y;

			pSink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
		}
		else if (cmd == L'L') {
			ws >> x >> y;

			pSink->AddLine(D2D1::Point2F(x, y));
		}

		//std::wcout << cmd << " " << x << " " << y << std::endl;

		last_cmd = cmd;
	}

	pSink->Close();
}

void SVGPathElement::render(ID2D1DeviceContext* pContext) {
	pContext->FillGeometry(pathGeometry, fillBrush);
}

void SVGRectElement::render(ID2D1DeviceContext* pContext) {
	pContext->FillRectangle(
		D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
		fillBrush
	);
	pContext->DrawRectangle(
		D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
		strokeBrush
	);
}

bool SVGUtil::init(HWND _wnd)
{
	wnd = _wnd;

	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
	
	if (!SUCCEEDED(hr)) {
		return false;
	}

	// Create Render Target
	RECT rc;

	GetClientRect(_wnd, &rc);

	hr = pFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(
			_wnd,
			D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
		),
		&pRenderTarget
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Get the device context from the render target
	hr = pRenderTarget->QueryInterface(IID_PPV_ARGS(&pDeviceContext));

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::Black),
		&defaultStrokeBrush
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f),
		&defaultFillBrush
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	return true;
}

// Resize the render target when the window size changes
void SVGUtil::resize()
{
	RECT rc;

	GetClientRect(wnd, &rc);
	pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
}

// Render the loaded bitmap onto the window
void SVGUtil::render()
{
	pDeviceContext->BeginDraw();
	pDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::Beige));

	CComPtr<ID2D1SolidColorBrush> pBrush;

	HRESULT hr = pDeviceContext->CreateSolidColorBrush(
		D2D1::ColorF(0xE9/256.0f, 0x71/256.0f, 0x32/256.0f),
		&pBrush
	);
	
	if (!SUCCEEDED(hr)) {
		return;
	}

	D2D1_STROKE_STYLE_PROPERTIES strokeStyleProps = D2D1::StrokeStyleProperties();
	
	//Set stroke to dots
	strokeStyleProps.dashStyle = D2D1_DASH_STYLE_DASH;

	CComPtr<ID2D1StrokeStyle> pStrokeStyle;

	hr = pFactory->CreateStrokeStyle(
		strokeStyleProps,
		nullptr,
		0,
		&pStrokeStyle
	);

	if (!SUCCEEDED(hr)) {
		return;
	}

	pDeviceContext->SetTransform(D2D1::Matrix3x2F::Translation(-1211.0f, -981.0f));

	pDeviceContext->FillRectangle(
		D2D1::RectF(1214.5, 1074.5, 1732.0f, 1436.0f),
		pBrush
	);

	pDeviceContext->EndDraw();
}

