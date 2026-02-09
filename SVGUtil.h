#pragma once

#include <d2d1_2.h>
#include <wincodec.h>
#include <atlbase.h>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <map>
#include <dwrite.h>

struct SVGGraphicsElement {
	std::wstring tagName;
	float strokeWidth = 1.0f;
	float fillOpacity = 1.0f;
	CComPtr<ID2D1SolidColorBrush> fillBrush;
	CComPtr<ID2D1SolidColorBrush> strokeBrush;
	std::vector<std::shared_ptr<SVGGraphicsElement>> children;
	std::optional<D2D1_MATRIX_3X2_F> combinedTransform;
	std::vector<float> points;
	std::map<std::wstring, std::wstring> styles;

	virtual void render_tree(ID2D1DeviceContext* pContext);
	virtual void render(ID2D1DeviceContext* pContext) {};
	virtual void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext);
};

struct SVGGElement : public SVGGraphicsElement {
	void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext) override;
};

struct SVGRectElement : public SVGGraphicsElement {
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGCircleElement : public SVGGraphicsElement {
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGEllipseElement : public SVGGraphicsElement {
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGLineElement : public SVGGraphicsElement {
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGPathElement : public SVGGraphicsElement {
	CComPtr<ID2D1PathGeometry> pathGeometry;

	void buildPath(ID2D1Factory* pFactory, const std::wstring_view& pathData);
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGTextElement : public SVGGraphicsElement {
	std::wstring textContent;
	CComPtr<IDWriteTextFormat> textFormat;
	CComPtr<IDWriteTextLayout> textLayout;

	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGUtil
{
	HWND wnd;
	CComPtr<ID2D1Factory> pFactory;
	CComPtr<IDWriteFactory> pDWriteFactory;
	CComPtr<ID2D1HwndRenderTarget> pRenderTarget;
	CComPtr<ID2D1DeviceContext> pDeviceContext;
	CComPtr<ID2D1SolidColorBrush> defaultFillBrush;
	CComPtr<ID2D1SolidColorBrush> defaultStrokeBrush;
	CComPtr<IDWriteTextFormat> defaultTextFormat;
	std::shared_ptr<SVGGraphicsElement> rootElement;

	bool init(HWND wnd);
	void resize();
	void render();
	void redraw();
	bool parse(const wchar_t* fileName);
	CComPtr<IDWriteTextFormat> build_text_format(IDWriteFactory* pDWriteFactory, std::wstring_view family, std::wstring_view weight, std::wstring_view style, float size);
};

