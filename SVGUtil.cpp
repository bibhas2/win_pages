#include "SVGUtil.h"
#include <sstream>
#include <string_view>
#include <stack>
#include <xmllite.h>

struct TransformFunction {
	std::wstring name;
	std::vector<float> values;
};

static void ltrim_str(std::wstring_view& source) {
	size_t pos = source.find_first_not_of(L" \t\r\n");

	if (pos == std::wstring_view::npos) {
		source.remove_prefix(source.length());
	}
	else {
		source.remove_prefix(pos);
	}
}

bool SVGUtil::get_rgba(std::wstring_view source, float& r, float& g, float& b, float& a) {
	if (source.empty()) {
		return false;
	}

	// Trim leading whitespace
	ltrim_str(source);

	if (source == L"none") {
		return false;
	}

	//See if it is a named color
	if (source[0] != L'#') {
		auto iter = namedColors.find(std::wstring(source));
		if (iter != namedColors.end()) {
			UINT32 color = iter->second;

			r = ((color >> 16) & 0xFF) / 255.0f;
			g = ((color >> 8) & 0xFF) / 255.0f;
			b = (color & 0xFF) / 255.0f;
			a = 1.0f;

			return true;
		}
	}

	//Parse #RRGGBBAA color from the source
	if (source.length() < 7 || source[0] != L'#') {
		return false;
	}

	std::wstring rStr(source.substr(1, 2));
	std::wstring gStr(source.substr(3, 2));
	std::wstring bStr(source.substr(5, 2));

	r = static_cast<float>(std::stoul(rStr, nullptr, 16)) / 255.0f;
	g = static_cast<float>(std::stoul(gStr, nullptr, 16)) / 255.0f;
	b = static_cast<float>(std::stoul(bStr, nullptr, 16)) / 255.0f;

	if (source.length() == 9) {
		std::wstring aStr(source.substr(7, 2));

		a = static_cast<float>(std::stoul(aStr, nullptr, 16)) / 255.0f;
	}
	else {
		a = 1.0f;
	}

	return true;
}

static bool get_transform_functions(const std::wstring_view& source, std::vector<TransformFunction>& functions) {
	size_t start = 0;

	while (true) {
		TransformFunction f;
		size_t pos = source.find(L'(', start);

		if (pos == std::wstring::npos) {
			//No more functions left
			return true;
		}

		std::wstring_view function_name = source.substr(start, pos - start);

		ltrim_str(function_name);

		if (function_name.empty()) {
			return false;
		}

		f.name = function_name;

		start = pos + 1;

		pos = source.find(L')', start);

		if (pos == std::wstring::npos) {
			return false;
		}

		std::wstring_view values = source.substr(start, pos - start);
		std::wstringstream ws;

		//Replace comma with spaces
		for (wchar_t ch : values) {
			if (ch == L',') {
				ws << L' ';
			} else {
				ws << ch;
			}
		}

		float v;

		while (ws >> v) {
			f.values.push_back(v);
		}

		functions.push_back(f);

		start = pos + 1;

		if (start >= source.length()) {
			break;
		}
	}

	return true;
}

void SVGGraphicsElement::render_tree(ID2D1DeviceContext* pContext) {
	OutputDebugStringW(L"Rendering element: ");
	OutputDebugStringW(tagName.c_str());
	OutputDebugStringW(L"\n");

	//Save the old transform
	D2D1_MATRIX_3X2_F oldTransform;

	if (combinedTransform) {
		OutputDebugStringW(L"Applying transform\n");
		pContext->GetTransform(&oldTransform);

		auto totalTransform = oldTransform * combinedTransform.value();

		pContext->SetTransform(totalTransform);
	}

	render(pContext);

	//Render all child elements
	for (const auto& child : children) {
		child->render_tree(pContext);
	}

	if (combinedTransform) {
		OutputDebugStringW(L"Restoring transform\n");
		pContext->SetTransform(oldTransform);
	}
}

bool build_transform_matrix(const std::wstring_view& transformStr, D2D1_MATRIX_3X2_F& outMatrix) {
	std::vector<TransformFunction> functions;

	if (!get_transform_functions(transformStr, functions)) {
		return false;
	}

	//Transformation functions are applied in reverse order
	for (auto it = functions.rbegin(); it != functions.rend(); ++it) {
		const auto& f = *it;

		if (f.name == L"translate") {
			if (f.values.size() == 1) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Translation(f.values[0], 0.0f);
			}
			else if (f.values.size() == 2) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Translation(f.values[0], f.values[1]);
			}
		}
		else if (f.name == L"scale") {
			if (f.values.size() == 1) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Scale(f.values[0], f.values[0]);
			}
			else if (f.values.size() == 2) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Scale(f.values[0], f.values[1]);
			}
		}
		else if (f.name == L"rotate") {
			if (f.values.size() == 1) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Rotation(f.values[0]);
			}
			else if (f.values.size() == 3) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Rotation(f.values[0], D2D1::Point2F(f.values[1], f.values[2]));
			}
		} else if (f.name == L"matrix") {
			if (f.values.size() == 6) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Scale(f.values[0], f.values[3]);
				outMatrix = outMatrix * D2D1::Matrix3x2F::Skew(f.values[1], f.values[2]);
				outMatrix = outMatrix * D2D1::Matrix3x2F::Translation(f.values[4], f.values[5]);

				/*D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F(
					f.values[0], f.values[1],
					f.values[2], f.values[3],
					f.values[4], f.values[5]
				);
				outMatrix = outMatrix * m;*/
			}
		}
		else if (f.name == L"skew") {
			//Apply skew transform
			if (f.values.size() == 2) {
				outMatrix = outMatrix * D2D1::Matrix3x2F::Skew(f.values[0], f.values[1]);
			}
		}
	}

	return true;
}

bool char_is_number(wchar_t ch) {
	return (ch >= 48 && ch <= 57) || (ch == L'.') || (ch == L'-');
}

void SVGPathElement::buildPath(ID2D1Factory* pFactory, const std::wstring_view& pathData) {
	pFactory->CreatePathGeometry(&pathGeometry);

	CComPtr<ID2D1GeometrySink> pSink;

	pathGeometry->Open(&pSink);

	//SVG spec is very leinent on path syntax. White spaces are
	//entirely optional. Numbers can either be separated by comma or spaces.
	//Here we normalize the path by properly separating commands and numbers by spaces.
	std::wstringstream ws;
	std::wstring_view spaces(L", \t\r\n");

	for (wchar_t ch : pathData) {
		if (spaces.find_first_of(ch) != std::wstring_view::npos) {
			//Normalize all spaces to single space
			ws << L' ';
		} else if (ch == L'-') {
			//Insert space before negative sign
			ws << L' ';
			ws << ch;
		}
		else if (ch == L',') {
			ws << L' ';
		}
		else {
			ws << ch;
		}
	}

	wchar_t cmd = 0, last_cmd = 0;
	bool is_in_figure = false;
	std::wstring_view supported_cmds(L"MLHVQTCSZz");
	float current_x = 0.0, current_y = 0.0;
	float last_ctrl_x = 0.0, last_ctrl_y = 0.0;

	while (!ws.eof()) {
		//Read command letter
		if (!(ws >> cmd)) {
			//End of stream
			break;
		}

		if (supported_cmds.find_first_of(cmd) == std::wstring_view::npos) {
			//We did not find a command. Put it back.
			ws.unget();

			//As per the SVG spec deduce the command from the last command
			if (last_cmd == L'M') {
				//Subsequent moveto pairs are treated as lineto commands
				cmd = L'L';
			}
			else {
				//Continue with the last command
				cmd = last_cmd;
			}
		}

		if (cmd == L'M') {
			float x = 0.0, y = 0.0;

			//If we are already in a figure, end it first
			if (is_in_figure) {
				pSink->EndFigure(D2D1_FIGURE_END_OPEN);
			}

			ws >> x >> y;

			pSink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
			is_in_figure = true;
			//Update current point
			current_x = x;
			current_y = y;
		}
		else if (cmd == L'L') {
			float x = 0.0, y = 0.0;

			ws >> x >> y;

			pSink->AddLine(D2D1::Point2F(x, y));

			//Update current point
			current_x = x;
			current_y = y;
		}
		else if (cmd == L'H') {
			float x = 0.0;

			ws >> x;

			pSink->AddLine(D2D1::Point2F(x, current_y));

			//Update current point
			current_x = x;
		}
		else if (cmd == L'V') {
			float y = 0.0;

			ws >> y;

			pSink->AddLine(D2D1::Point2F(current_x, y));

			//Update current point
			current_y = y;
		}
		else if (cmd == L'Q') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;

			ws >> x1 >> y1 >> x2 >> y2;

			pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2)));

			//Update current point
			current_x = x2;
			current_y = y2;
			last_ctrl_x = x1;
			last_ctrl_y = y1;
		}
		else if (cmd == L'T') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;

			ws >> x2 >> y2;

			//Calculate the control point by reflecting the last control point
			if (last_cmd == L'Q' || last_cmd == L'T') {
				x1 = 2 * current_x - last_ctrl_x;
				y1 = 2 * current_y - last_ctrl_y;
			} else {
				x1 = current_x;
				y1 = current_y;
			}

			pSink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2)));

			//Update current point
			current_x = x2;
			current_y = y2;
			last_ctrl_x = x1;
			last_ctrl_y = y1;
		} 
		else if (cmd == L'C') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, x3 = 0.0, y3 = 0.0;
			ws >> x1 >> y1 >> x2 >> y2 >> x3 >> y3;
			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), D2D1::Point2F(x3, y3)));

			//Update current point
			current_x = x3;
			current_y = y3;
			last_ctrl_x = x2;
			last_ctrl_y = y2;
		}
		else if (cmd == L'S') {
			float x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, x3 = 0.0, y3 = 0.0;
			ws >> x2 >> y2 >> x3 >> y3;

			//Calculate the first control point by reflecting the last control point
			if (last_cmd == L'C' || last_cmd == L'S') {
				x1 = 2 * current_x - last_ctrl_x;
				y1 = 2 * current_y - last_ctrl_y;
			}
			else {
				x1 = current_x;
				y1 = current_y;
			}

			pSink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), D2D1::Point2F(x3, y3)));

			//Update current point
			current_x = x3;
			current_y = y3;
			last_ctrl_x = x2;
			last_ctrl_y = y2;
		} else if (cmd == L'Z' || cmd == L'z') {
			//Close the current figure
			if (is_in_figure) {
				pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
				is_in_figure = false;
			}
		}

		last_cmd = cmd;
	}

	//End of path
	if (is_in_figure) {
		pSink->EndFigure(D2D1_FIGURE_END_OPEN);

		is_in_figure = false;
	}

	pSink->Close();
}

void SVGPathElement::render(ID2D1DeviceContext* pContext) {
	if (fillBrush) {
		OutputDebugStringW(L"FillGeometry\n");
		pContext->FillGeometry(pathGeometry, fillBrush);
	}
	if (strokeBrush) {
		OutputDebugStringW(L"DrawGeometry\n");

		pContext->DrawGeometry(pathGeometry, strokeBrush, strokeWidth);
	}
}

void SVGRectElement::render(ID2D1DeviceContext* pContext) {
	if (fillBrush) {
		pContext->FillRectangle(
			D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
			fillBrush
		);
	}
	if (strokeBrush) {
		pContext->DrawRectangle(
			D2D1::RectF(points[0], points[1], points[0] + points[2], points[1] + points[3]),
			strokeBrush,
			strokeWidth
		);
	}
}

void SVGCircleElement::render(ID2D1DeviceContext* pContext) {
	if (fillBrush) {
		pContext->FillEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[2]),
			fillBrush
		);
	}
	if (strokeBrush) {
		pContext->DrawEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[2]),
			strokeBrush,
			strokeWidth
		);
	}
}

//Render SVGEllipseElement
void SVGEllipseElement::render(ID2D1DeviceContext* pContext) {
	if (fillBrush) {
		pContext->FillEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[3]),
			fillBrush
		);
	}
	if (strokeBrush) {
		pContext->DrawEllipse(
			D2D1::Ellipse(D2D1::Point2F(points[0], points[1]), points[2], points[3]),
			strokeBrush,
			strokeWidth
		);
	}
}

void SVGLineElement::render(ID2D1DeviceContext* pContext) {
	if (strokeBrush) {
		pContext->DrawLine(
			D2D1::Point2F(points[0], points[1]),
			D2D1::Point2F(points[2], points[3]),
			strokeBrush,
			strokeWidth
		);
	}
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
		D2D1::ColorF(D2D1::ColorF::Black),
		&defaultFillBrush
	);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Initialize named colors
	namedColors[L"black"] = D2D1::ColorF::Black;
	namedColors[L"white"] = D2D1::ColorF::White;
	namedColors[L"red"] = D2D1::ColorF::Red;
	namedColors[L"green"] = D2D1::ColorF::Green;
	namedColors[L"blue"] = D2D1::ColorF::Blue;
	namedColors[L"orange"] = D2D1::ColorF::Orange;
	namedColors[L"pink"] = D2D1::ColorF::Pink;
	namedColors[L"yellow"] = D2D1::ColorF::Yellow;
	namedColors[L"brown"] = D2D1::ColorF::Brown;

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
	pDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White));

	if (rootElement) {
		//Render the SVG element tree
		rootElement->render_tree(pDeviceContext);
	}

	pDeviceContext->EndDraw();
}

bool get_element_name(IXmlReader *pReader, std::wstring_view& name) {
	const wchar_t* pwszLocalName = NULL;
	UINT len;

	HRESULT hr = pReader->GetLocalName(&pwszLocalName, &len);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	name = std::wstring_view(pwszLocalName, len);

	return true;
}

bool get_attribute(IXmlReader* pReader, const wchar_t* attr_name, std::wstring_view& attr_value) {
	const wchar_t* pwszValue = NULL;
	UINT len;

	HRESULT hr = pReader->MoveToAttributeByName(attr_name, NULL);

	if (hr == S_FALSE) {
		return false; //Attribute not found
	}

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pReader->GetValue(&pwszValue, &len);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	attr_value = std::wstring_view(pwszValue, len);

	return true;
}

bool get_attribute(IXmlReader* pReader, const wchar_t* attr_name, float& attr_value) {
	const wchar_t* pwszValue = NULL;
	UINT len;

	HRESULT hr = pReader->MoveToAttributeByName(attr_name, NULL);

	if (hr == S_FALSE) {
		return false; //Attribute not found
	}

	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pReader->GetValue(&pwszValue, &len);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	return swscanf_s(pwszValue, L"%f", &attr_value) == 1;
}

bool SVGUtil::parse(const wchar_t* fileName) {
	CComPtr<IXmlReader> pReader;

	HRESULT hr = ::CreateXmlReader(__uuidof(IXmlReader), (void**)&pReader, NULL);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	CComPtr<IStream> pFileStream;

	hr = SHCreateStreamOnFileEx(fileName, STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &pFileStream);
	
	if (!SUCCEEDED(hr)) {
		return false;
	}

	hr = pReader->SetInput(pFileStream);

	if (!SUCCEEDED(hr)) {
		return false;
	}

	//Clear any existing root element
	rootElement = nullptr;

	std::stack<std::shared_ptr<SVGGraphicsElement>> parent_stack;

	while (true) {
		XmlNodeType nodeType;

		hr = pReader->Read(&nodeType);

		if (hr == S_FALSE) {
			break; //End of file
		}

		if (!SUCCEEDED(hr)) {
			return false;
		}

		if (nodeType == XmlNodeType_Element) {
			//We must call IsEmptyElement before reading
			//any attributes!!!
			bool is_self_closing = pReader->IsEmptyElement();

			std::wstring_view element_name, attr_value;

			if (!get_element_name(pReader, element_name)) {
				return false;
			}

			std::shared_ptr<SVGGraphicsElement> parent_element;
			std::shared_ptr<SVGGraphicsElement> new_element;

			if (!parent_stack.empty()) {
				parent_element = parent_stack.top();
			}

			if (element_name == L"svg") {
				new_element = std::make_shared<SVGGraphicsElement>();

				//Set up default brushes
				new_element->fillBrush = defaultFillBrush;
				new_element->strokeBrush = nullptr;

				if (!rootElement)
				{
					//This is the root <svg> element
					rootElement = new_element;
				}
				else {
					//Inner svg elements have some special treatment
					float x = 0.0f, y = 0.0f, width = 100.0f, height = 100.0f;

					if (get_attribute(pReader, L"x", x) &&
						get_attribute(pReader, L"y", y)) {
						//Position the inner SVG element
						new_element->combinedTransform = D2D1::Matrix3x2F::Translation(x, y);
					}
				}
			}
			else if (element_name == L"rect") {
				float x, y, width, height;
				if (get_attribute(pReader, L"x", x) &&
					get_attribute(pReader, L"y", y) &&
					get_attribute(pReader, L"width", width) &&
					get_attribute(pReader, L"height", height)) {
					new_element = std::make_shared<SVGRectElement>();

					new_element->points.push_back(x);
					new_element->points.push_back(y);
					new_element->points.push_back(width);
					new_element->points.push_back(height);
				}
			}
			else if (element_name == L"circle") {
				float cx, cy, r;

				if (get_attribute(pReader, L"cx", cx) &&
					get_attribute(pReader, L"cy", cy) &&
					get_attribute(pReader, L"r", r)) {
					
					auto circle_element = std::make_shared<SVGCircleElement>();

					circle_element->points.push_back(cx);
					circle_element->points.push_back(cy);
					circle_element->points.push_back(r);

					new_element = circle_element;
				}
			}
			else if (element_name == L"ellipse") {
				float cx, cy, rx, ry;

				if (get_attribute(pReader, L"cx", cx) &&
					get_attribute(pReader, L"cy", cy) &&
					get_attribute(pReader, L"rx", rx) &&
					get_attribute(pReader, L"ry", ry)) {
					
					auto ellipse_element = std::make_shared<SVGEllipseElement>();

					ellipse_element->points.push_back(cx);
					ellipse_element->points.push_back(cy);
					ellipse_element->points.push_back(rx);
					ellipse_element->points.push_back(ry);

					new_element = ellipse_element;
				}
			}
			else if (element_name == L"path") {
				if (get_attribute(pReader, L"d", attr_value)) {
					auto path_element = std::make_shared<SVGPathElement>();

					path_element->buildPath(pFactory, attr_value);
					new_element = path_element;
				}
			} else if (element_name == L"group" || element_name == L"g") {
				new_element = std::make_shared<SVGGElement>();
			} else if (element_name == L"line") {
				float x1, y1, x2, y2;

				if (get_attribute(pReader, L"x1", x1) &&
					get_attribute(pReader, L"y1", y1) &&
					get_attribute(pReader, L"x2", x2) &&
					get_attribute(pReader, L"y2", y2)) {
					
					auto line_element = std::make_shared<SVGLineElement>();

					line_element->points.push_back(x1);
					line_element->points.push_back(y1);
					line_element->points.push_back(x2);
					line_element->points.push_back(y2);

					new_element = line_element;
				}
			}

			if (new_element) {
				new_element->tagName = element_name;

				if (get_attribute(pReader, L"transform", attr_value)) {
					D2D1_MATRIX_3X2_F trans = D2D1::Matrix3x2F::Identity();

					//If the element already has a transform (like inner <svg>), combine them
					if (new_element->combinedTransform)
					{
						trans = new_element->combinedTransform.value();
					}

					if (build_transform_matrix(attr_value, trans)) {
						new_element->combinedTransform = trans;
					}
				}

				//Set brushes
				if (get_attribute(pReader, L"stroke", attr_value)) {
					if (attr_value == L"none") {
						new_element->strokeBrush = nullptr;
					}
					else {
						float r, g, b, a;

						if (get_rgba(attr_value, r, g, b, a)) {
							CComPtr<ID2D1SolidColorBrush> brush;
							hr = pDeviceContext->CreateSolidColorBrush(
								D2D1::ColorF(r, g, b, a),
								&brush
							);

							if (SUCCEEDED(hr)) {
								new_element->strokeBrush = brush;
							}
						}
					}
				} else {
					if (parent_element) {
						//Inherit stroke brush from parent
						new_element->strokeBrush = parent_element->strokeBrush;
					}
				}

				//Get fill opacity
				float fillOpacity;

				if (get_attribute(pReader, L"fill-opacity", fillOpacity)) {
					new_element->fillOpacity = fillOpacity;
				}
				else {
					if (parent_element) {
						//Inherit fill opacity from parent
						new_element->fillOpacity = parent_element->fillOpacity;
					}
				}

				if (get_attribute(pReader, L"fill", attr_value)) {
					if (attr_value == L"none") {
						new_element->fillBrush = nullptr;
					}
					else {
						float r, g, b, a;

						if (get_rgba(attr_value, r, g, b, a)) {
							CComPtr<ID2D1SolidColorBrush> brush;

							hr = pDeviceContext->CreateSolidColorBrush(
								D2D1::ColorF(r, g, b, a * new_element->fillOpacity),
								&brush
							);
							if (SUCCEEDED(hr)) {
								new_element->fillBrush = brush;
							}
						}
					}
				} else {
					if (parent_element) {
						//Inherit fill brush from parent
						new_element->fillBrush = parent_element->fillBrush;
					}
				}

				//Get stroke width
				float strokeWidth;

				if (get_attribute(pReader, L"stroke-width", strokeWidth)) {
					new_element->strokeWidth = strokeWidth;
				} else {
					if (parent_element) {
						//Inherit stroke width from parent
						new_element->strokeWidth = parent_element->strokeWidth;
					}
					else {
						new_element->strokeWidth = 1.0f; //Default stroke width
					}
				}

				if (parent_element) {
					//Add the new element to its parent
					OutputDebugStringW(L"Parent::Child: ");
					OutputDebugStringW(parent_element->tagName.c_str());
					OutputDebugStringW(L"::");
					OutputDebugStringW(element_name.data());
					OutputDebugStringW(L"\n");
					parent_element->children.push_back(new_element);
				}
			}

			//Do not add self closing elements like <circle .../> to the parent stack
			if (!is_self_closing)
			{
				//Push the new element onto the stack
				//This may be null if the element is not supported
				OutputDebugStringW(L"Pushing to parent stack: ");
				OutputDebugStringW(element_name.data());
				OutputDebugStringW(L"\n");
				parent_stack.push(new_element);
			}
		}
		else if (nodeType == XmlNodeType_Text) {
			const wchar_t* pwszValue = NULL;

			hr = pReader->GetValue(&pwszValue, NULL);
			if (!SUCCEEDED(hr)) {
				return false;
			}
		}
		else if (nodeType == XmlNodeType_EndElement) {
			std::wstring_view element_name;

			if (!get_element_name(pReader, element_name)) {
				return false;
			}

			OutputDebugStringW(L"End Element: ");
			OutputDebugStringW(element_name.data());
			OutputDebugStringW(L"\n");

			if (!parent_stack.empty()) {
				parent_stack.pop();
			}
		}
	}

	//If we reach here, no <svg> element was found
	return false;
}

void SVGUtil::redraw()
{
	InvalidateRect(wnd, NULL, FALSE);
}