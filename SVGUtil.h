#pragma once

#include <d2d1_2.h>
#include <wincodec.h>
#include <atlbase.h>
#include <vector>
#include <string>
#include <memory>

struct SVGGraphicsElement {
	std::wstring tagName;
	CComPtr<ID2D1SolidColorBrush> fillBrush;
	CComPtr<ID2D1SolidColorBrush> strokeBrush;
	std::vector<std::unique_ptr<SVGGraphicsElement>> children;
	std::vector<D2D1_MATRIX_3X2_F> transforms;
	std::vector<float> points;

	virtual void render(ID2D1DeviceContext* pContext) = 0;
};

struct SVGGElement : public SVGGraphicsElement {
	void render(ID2D1DeviceContext* pContext);
};

struct SVGRectElement : public SVGGraphicsElement {
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGPathElement : public SVGGraphicsElement {
	CComPtr<ID2D1PathGeometry> pathGeometry;

	void buildPath(ID2D1Factory* pFactory, const wchar_t* pathData);
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGUtil
{
	HWND wnd;
	CComPtr<ID2D1Factory> pFactory;
	CComPtr<ID2D1HwndRenderTarget> pRenderTarget;
	CComPtr<ID2D1DeviceContext> pDeviceContext;
	CComPtr<ID2D1SolidColorBrush> defaultFillBrush;
	CComPtr<ID2D1SolidColorBrush> defaultStrokeBrush;

	bool init(HWND wnd);
	void resize();
	void render();
};

