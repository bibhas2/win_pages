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
	std::wstring tag_name;
	float stroke_width = 1.0f;
	CComPtr<ID2D1SolidColorBrush> fill_brush;
	CComPtr<ID2D1SolidColorBrush> stroke_brush;
	CComPtr<ID2D1StrokeStyle> stroke_style;
	std::vector<std::shared_ptr<SVGGraphicsElement>> children;
	std::optional<D2D1_MATRIX_3X2_F> combined_transform;
	std::vector<float> points;
	std::map<std::wstring, std::wstring> styles;

	virtual void render_tree(ID2D1DeviceContext* pContext);
	virtual void render(ID2D1DeviceContext* pContext) {};
	virtual void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory);
	bool get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value);
	void get_style_computed(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, const std::wstring& style_name, std::wstring& style_value, const std::wstring& default_value);
};

struct SVGGElement : public SVGGraphicsElement {
	void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory) override;
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
	CComPtr<ID2D1PathGeometry> path_geometry;

	void buildPath(ID2D1Factory* pD2DFactory, const std::wstring_view& pathData);
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGTextElement : public SVGGraphicsElement {
	std::wstring text_content;
	CComPtr<IDWriteFactory> pDWriteFactory;
	CComPtr<IDWriteTextFormat> textFormat;
	CComPtr<IDWriteTextLayout> textLayout;
	float baseline = 0.0f;

	void configure_presentation_style(const std::vector<std::shared_ptr<SVGGraphicsElement>>& parent_stack, ID2D1DeviceContext* pDeviceContext, ID2D1Factory* pD2DFactory) override;
	void render(ID2D1DeviceContext* pContext) override;
};

struct SVGUtil
{
	HWND wnd;
	CComPtr<ID2D1Factory> pD2DFactory;
	CComPtr<IDWriteFactory> pDWriteFactory;
	CComPtr<ID2D1HwndRenderTarget> pRenderTarget;
	CComPtr<ID2D1DeviceContext> pDeviceContext;
	CComPtr<ID2D1SolidColorBrush> defaultFillBrush;
	CComPtr<ID2D1SolidColorBrush> defaultStrokeBrush;
	CComPtr<IDWriteTextFormat> defaultTextFormat;
	std::shared_ptr<SVGGraphicsElement> root_element;

	bool init(HWND wnd);
	void resize();
	void render();
	void redraw();
	bool parse(const wchar_t* fileName);
};

