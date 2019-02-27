#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <ratio>

#include "half_pixel.hpp"
#include "mv.hpp"
#include "motion_estimator.hpp"
#include "resource.h"

namespace chrono = std::chrono;
using std::make_unique;
using std::max;
using std::memcpy;
using std::min;
using std::ofstream;
using std::round;
using std::unique_ptr;

extern int g_VFVAPIVersion;

template<typename T>
inline static T clamp(T value, T a, T b) {
	if (value >= b)
		return b;

	if (value <= a)
		return a;

	return value;
}

inline static uint8 RGBToY(uint32 rgb) {
	const auto r = (rgb >> 16) & 0xFF;
	const auto g = (rgb >> 8) & 0xFF;
	const auto b = rgb & 0xFF;

	return static_cast<uint8>(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
}

inline static void RGBToYUV(uint8 r, uint8 g, uint8 b, uint8& y, int16& u, int16& v) {
	y = static_cast<uint8>(0.299 * r + 0.587 * g + 0.114 * b + 0.5);
	u = static_cast<int16>(-0.14713 * r - 0.28886 * g + 0.436 * b);
	v = static_cast<int16>(0.615 * r - 0.51499 * g - 0.10001 * b);
}

inline static void YUVToRGB(uint8 y, int16 u, int16 v, uint8& r, uint8& g, uint8& b) {
	int r_ = static_cast<int>(y + 1.13983 * v);
	int g_ = static_cast<int>(y - 0.39465 * u - 0.5806 * v);
	int b_ = static_cast<int>(y + 2.03211 * u);

	r = static_cast<uint8>(clamp(r_, 0, 255));
	g = static_cast<uint8>(clamp(g_, 0, 255));
	b = static_cast<uint8>(clamp(b_, 0, 255));
}

inline static double PSNR(double MSE, sint32 w, sint32 h) {
	return 10 * log10(w * h * 255.0 * 255.0 / MSE);
}

enum class OutputType : int {
	SOURCE,
	RESIDUAL_BEFORE_MC,
	RESIDUAL_AFTER_MC,
	COMPENSATED
};

struct FilterTemplateConfig {
	OutputType output_type;
	bool show_vectors;
	bool draw_nothing;
	bool measure_psnr;
	uint8 quality;
	bool use_half_pixel;

	FilterTemplateConfig()
		: output_type(OutputType::SOURCE)
		, show_vectors(false)
		, draw_nothing(false)
		, measure_psnr(false)
		, quality(100)
		, use_half_pixel(false) {
	}
};

class FilterTemplateDialog : public VDXVideoFilterDialog {
public:
	FilterTemplateDialog(FilterTemplateConfig& config, IVDXFilterPreview* preview)
		: config(config)
		, preview(preview)
		, not_user_input(false) {
	}

	bool Show(VDXHWND parent) {
		return 0 != VDXVideoFilterDialog::Show(nullptr, MAKEINTRESOURCE(IDD_FILTER_CONFIG), reinterpret_cast<HWND>(parent));
	}

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	FilterTemplateConfig& config;
	IVDXFilterPreview* preview;

	bool not_user_input;
};

static int OutputTypeToID(OutputType output_type) {
	switch (output_type) {
	default:
	case OutputType::SOURCE:
		return IDC_RADIO_SHOWSOURCE;

	case OutputType::RESIDUAL_BEFORE_MC:
		return IDC_RADIO_SHOWBEFORE;

	case OutputType::RESIDUAL_AFTER_MC:
		return IDC_RADIO_SHOWAFTER;

	case OutputType::COMPENSATED:
		return IDC_RADIO_SHOWCOMPENSATED;
	}
}

INT_PTR FilterTemplateDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		SendDlgItemMessage(mhdlg, IDC_SLIDER_QUALITY, TBM_SETRANGEMIN, true, 0);
		SendDlgItemMessage(mhdlg, IDC_SLIDER_QUALITY, TBM_SETRANGEMAX, true, 100);
		SendDlgItemMessage(mhdlg, IDC_SLIDER_QUALITY, TBM_SETLINESIZE, 0, 1);
		SendDlgItemMessage(mhdlg, IDC_SLIDER_QUALITY, TBM_SETPAGESIZE, 0, 5);
		SendDlgItemMessage(mhdlg, IDC_SLIDER_QUALITY, TBM_SETTICFREQ, 5, 0);

		SetDlgItemInt(mhdlg, IDC_EDIT_QUALITY, config.quality, FALSE);

		CheckRadioButton(mhdlg,
		                 IDC_RADIO_SHOWSOURCE,
		                 IDC_RADIO_SHOWCOMPENSATED,
		                 OutputTypeToID(config.output_type));

		CheckDlgButton(mhdlg, IDC_CHECK_SHOWVECTORS, config.show_vectors ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_CHECK_DRAWNOTHING, config.draw_nothing ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_CHECK_PSNR, config.measure_psnr ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_CHECK_HALFPIXEL, config.use_half_pixel ? BST_CHECKED : BST_UNCHECKED);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			EndDialog(mhdlg, true);
			return TRUE;

		case IDCANCEL:
			EndDialog(mhdlg, false);
			return TRUE;

		case IDC_EDIT_QUALITY:
			if (HIWORD(wParam) == EN_UPDATE) {
				auto val = GetDlgItemInt(mhdlg, IDC_EDIT_QUALITY, nullptr, FALSE);

				if (val > 100) {
					val = 100;
					SetDlgItemInt(mhdlg, IDC_EDIT_QUALITY, val, FALSE);
				}

				not_user_input = true;
				SendDlgItemMessage(mhdlg, IDC_SLIDER_QUALITY, TBM_SETPOS, TRUE, val);
				not_user_input = false;

				config.quality = val;
			}

			return TRUE;

		case IDC_RADIO_SHOWSOURCE:
			if (HIWORD(wParam) == BN_CLICKED && !!IsDlgButtonChecked(mhdlg, IDC_RADIO_SHOWSOURCE))
				config.output_type = OutputType::SOURCE;

			return TRUE;

		case IDC_RADIO_SHOWBEFORE:
			if (HIWORD(wParam) == BN_CLICKED && !!IsDlgButtonChecked(mhdlg, IDC_RADIO_SHOWBEFORE))
				config.output_type = OutputType::RESIDUAL_BEFORE_MC;

			return TRUE;

		case IDC_RADIO_SHOWAFTER:
			if (HIWORD(wParam) == BN_CLICKED && !!IsDlgButtonChecked(mhdlg, IDC_RADIO_SHOWAFTER))
				config.output_type = OutputType::RESIDUAL_AFTER_MC;

			return TRUE;

		case IDC_RADIO_SHOWCOMPENSATED:
			if (HIWORD(wParam) == BN_CLICKED && !!IsDlgButtonChecked(mhdlg, IDC_RADIO_SHOWCOMPENSATED))
				config.output_type = OutputType::COMPENSATED;

			return TRUE;

		case IDC_CHECK_SHOWVECTORS:
			if (HIWORD(wParam) == BN_CLICKED)
				config.show_vectors = !!IsDlgButtonChecked(mhdlg, IDC_CHECK_SHOWVECTORS);

			return TRUE;

		case IDC_CHECK_DRAWNOTHING:
			if (HIWORD(wParam) == BN_CLICKED)
				config.draw_nothing = !!IsDlgButtonChecked(mhdlg, IDC_CHECK_DRAWNOTHING);

			return TRUE;

		case IDC_CHECK_PSNR:
			if (HIWORD(wParam) == BN_CLICKED)
				config.measure_psnr = !!IsDlgButtonChecked(mhdlg, IDC_CHECK_PSNR);

			return TRUE;

		case IDC_CHECK_HALFPIXEL:
			if (HIWORD(wParam) == BN_CLICKED)
				config.use_half_pixel = !!IsDlgButtonChecked(mhdlg, IDC_CHECK_HALFPIXEL);

			return TRUE;
		}

	case WM_NOTIFY:
		if (LOWORD(wParam) == IDC_SLIDER_QUALITY) {
			if (!not_user_input)
				SetDlgItemInt(mhdlg,
				              IDC_EDIT_QUALITY,
				              SendDlgItemMessage(mhdlg,
				                                 IDC_SLIDER_QUALITY,
				                                 TBM_GETPOS,
				                                 0,
				                                 0),
				              FALSE);
		}
	}

	return FALSE;
}

class FilterTemplate : public VDXVideoFilter {
public:
	FilterTemplate();
	FilterTemplate(const FilterTemplate& other);

	virtual uint32 GetParams();
	virtual void Start();
	virtual void Run();
	virtual void End();
	virtual bool Configure(VDXHWND hwnd);
	virtual void GetScriptString(char* buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

	void ProcessRGB32(void* dst, ptrdiff_t dst_pitch, const void* src, ptrdiff_t src_pitch);
	void CopyFromSrc(const uint8* src, ptrdiff_t src_pitch);
	void FillBorders();
	void EstimateMotion();
	void DrawOutput(uint8* dst, ptrdiff_t dst_pitch);
	void CompensateMotion();
	void CopyToDst(uint8* dst, ptrdiff_t dst_pitch, const uint8* p_Y, ptrdiff_t Y_gap, const int16* p_U, const int16* p_V);
	void DrawLine(uint8* dst, ptrdiff_t dst_pitch, sint32 x1, sint32 y1, sint32 x2, sint32 y2);
	void MeasurePSNR();

	sint32 width, height;
	sint32 width_ext, height_ext;
	sint32 num_blocks_hor, num_blocks_vert;
	unique_ptr<uint8[]> cur_Y;
	unique_ptr<int16[]> cur_U, cur_V;
	unique_ptr<uint8[]> prev_Y;
	unique_ptr<int16[]> prev_U, prev_V;
	unique_ptr<uint8[]> prev_Y_up, prev_Y_left, prev_Y_upleft;
	unique_ptr<int16[]> prev_U_up, prev_U_left, prev_U_upleft;
	unique_ptr<int16[]> prev_V_up, prev_V_left, prev_V_upleft;
	unique_ptr<uint8[]> cur_Y_MC;
	unique_ptr<int16[]> cur_U_MC, cur_V_MC;

	unique_ptr<MotionEstimator> me;
	unique_ptr<MV[]> vectors;

	bool measured_psnr;

	FilterTemplateConfig config;

	ofstream perf_file, psnr_file;
	double /*total_rgbtoyuv, total_borders, */total_me/*, total_output, total_copy*/;
	double total_y_psnr, total_u_psnr, total_v_psnr;
	unsigned frame_count;
};

VDXVF_BEGIN_SCRIPT_METHODS(FilterTemplate)
VDXVF_DEFINE_SCRIPT_METHOD(FilterTemplate, ScriptConfig, "iiiiii")
VDXVF_END_SCRIPT_METHODS()

FilterTemplate::FilterTemplate() : VDXVideoFilter() {
}

FilterTemplate::FilterTemplate(const FilterTemplate& other)
	: VDXVideoFilter(other)
	, width(other.width)
	, height(other.height)
	, num_blocks_hor(other.num_blocks_hor)
	, num_blocks_vert(other.num_blocks_vert)
	, width_ext(other.width_ext)
	, height_ext(other.height_ext)
	, cur_Y(new uint8[width_ext * height_ext])
	, cur_U(new int16[width * height])
	, cur_V(new int16[width * height])
	, config(other.config) {
	if (other.prev_Y) {
		prev_Y = make_unique<uint8[]>(width_ext * height_ext);
		prev_U = make_unique<int16[]>(width * height);
		prev_V = make_unique<int16[]>(width * height);

		memcpy(prev_Y.get(), other.prev_Y.get(), width_ext * height_ext);
		memcpy(prev_U.get(), other.prev_U.get(), width * height * 2);
		memcpy(prev_V.get(), other.prev_V.get(), width * height * 2);
	}

	if (other.prev_Y_up) {
		prev_Y_up = make_unique<uint8[]>(width_ext * height_ext);
		prev_Y_left = make_unique<uint8[]>(width_ext * height_ext);
		prev_Y_upleft = make_unique<uint8[]>(width_ext * height_ext);
		prev_U_up = make_unique<int16[]>(width * height);
		prev_U_left = make_unique<int16[]>(width * height);
		prev_U_upleft = make_unique<int16[]>(width * height);
		prev_V_up = make_unique<int16[]>(width * height);
		prev_V_left = make_unique<int16[]>(width * height);
		prev_V_upleft = make_unique<int16[]>(width * height);

		memcpy(prev_Y_up.get(), other.prev_Y_up.get(), width_ext * height_ext);
		memcpy(prev_Y_left.get(), other.prev_Y_left.get(), width_ext * height_ext);
		memcpy(prev_Y_upleft.get(), other.prev_Y_upleft.get(), width_ext * height_ext);
		memcpy(prev_U_up.get(), other.prev_U_up.get(), width * height * 2);
		memcpy(prev_U_left.get(), other.prev_U_left.get(), width * height * 2);
		memcpy(prev_U_upleft.get(), other.prev_U_upleft.get(), width * height * 2);
		memcpy(prev_V_up.get(), other.prev_V_up.get(), width * height * 2);
		memcpy(prev_V_left.get(), other.prev_V_left.get(), width * height * 2);
		memcpy(prev_V_upleft.get(), other.prev_V_upleft.get(), width * height * 2);
	}
}

uint32 FilterTemplate::GetParams() {
	if (g_VFVAPIVersion >= 12) {
		switch (fa->src.mpPixmapLayout->format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
		}
	}

	fa->dst.offset = 0;
	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_NEEDS_LAST;
}

void FilterTemplate::Start() {
	if (g_VFVAPIVersion >= 12) {
		const VDXPixmap& pxsrc = *fa->src.mpPixmap;

		width = pxsrc.w;
		height = pxsrc.h;
	} else {
		width = fa->src.w;
		height = fa->src.h;
	}

	width_ext = width + 2 * MotionEstimator::BORDER;
	height_ext = height + 2 * MotionEstimator::BORDER;

	num_blocks_hor = (width + MotionEstimator::BLOCK_SIZE - 1) / MotionEstimator::BLOCK_SIZE;
	num_blocks_vert = (height + MotionEstimator::BLOCK_SIZE - 1) / MotionEstimator::BLOCK_SIZE;

	cur_Y = make_unique<uint8[]>(width_ext * height_ext);
	cur_U = make_unique<int16[]>(width * height);
	cur_V = make_unique<int16[]>(width * height);
	prev_Y.reset();
	prev_U.reset();
	prev_V.reset();
	prev_Y_up.reset();
	prev_Y_left.reset();
	prev_Y_upleft.reset();
	prev_U_up.reset();
	prev_U_left.reset();
	prev_U_upleft.reset();
	prev_V_up.reset();
	prev_V_left.reset();
	prev_V_upleft.reset();
	cur_Y_MC.reset();
	cur_U_MC.reset();
	cur_V_MC.reset();

	me = make_unique<MotionEstimator>(width, height, config.quality, config.use_half_pixel);
	vectors = make_unique<MV[]>(num_blocks_hor * num_blocks_vert);

	perf_file.open("ME_performance.log", std::ios::app);

	if (config.measure_psnr) {
		psnr_file.open("ME_PSNR.log", std::ios::app);

		if (psnr_file)
			psnr_file << "\n\n#: YPSNR, UPSNR, VPSNR\n";
	}

	//total_rgbtoyuv = 0.0;
	//total_borders = 0.0;
	//total_output = 0.0;
	total_me = 0.0;
	//total_copy = 0.0;
	
	total_y_psnr = 0.0;
	total_u_psnr = 0.0;
	total_v_psnr = 0.0;

	frame_count = 0;
}

void FilterTemplate::Run() {
	if (g_VFVAPIVersion >= 12) {
		const VDXPixmap& pxdst = *fa->dst.mpPixmap;
		const VDXPixmap& pxsrc = *fa->src.mpPixmap;

		ProcessRGB32(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch);
	} else {
		ProcessRGB32(fa->dst.data, fa->dst.pitch, fa->src.data, fa->src.pitch);
	}
}

void FilterTemplate::End() {
	if (!perf_file)
		return;

	// frame_count > 2 is to prevent spamming the log.
	// VirtualDub likes to call the filter for one or two frames.
	if (frame_count > 2) {
		perf_file.precision(6);
		perf_file.setf(std::ios::fixed);
		//perf_file << "RGB to YUV: " << total_rgbtoyuv / frame_count << '\n';
		//perf_file << "Borders: " << total_borders / frame_count << '\n';
		//perf_file << "ME: " << total_me / frame_count << '\n';
		//perf_file << "Output: " << total_output / frame_count << '\n';
		//perf_file << "Copy: " << total_copy / frame_count << '\n';
		perf_file << "Average ME time (ms per frame): " << total_me / frame_count << '\n';

		if (config.measure_psnr) {
			perf_file << "Average Y PSNR: " << total_y_psnr / (frame_count - 1) << '\n';
			perf_file << "Average U PSNR: " << total_u_psnr / (frame_count - 1) << '\n';
			perf_file << "Average V PSNR: " << total_v_psnr / (frame_count - 1) << '\n';
		}

		perf_file << "Frame count: " << frame_count << '\n';
		perf_file << "\n\n";
	}

	perf_file.close();
	psnr_file.close();
}

bool FilterTemplate::Configure(VDXHWND hwnd) {
	FilterTemplateConfig old_config(config);
	FilterTemplateDialog dialog(config, fa->ifp);

	if (dialog.Show(hwnd))
		return true;

	config = old_config;
	return false;
}

void FilterTemplate::GetScriptString(char* buf, int maxlen) {
	SafePrintf(buf,
	           maxlen,
	           "Config(%d, %d, %d, %d, %d, %d)",
	           static_cast<int>(config.output_type),
	           config.show_vectors ? 1 : 0,
	           config.draw_nothing ? 1 : 0,
	           config.measure_psnr ? 1 : 0,
	           config.quality,
	           config.use_half_pixel ? 1 : 0);
}

void FilterTemplate::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	config.output_type = static_cast<OutputType>(clamp(argv[0].asInt(), 0, 3));
	config.show_vectors = !!argv[1].asInt();
	config.draw_nothing = !!argv[2].asInt();
	config.measure_psnr = !!argv[3].asInt();
	config.quality = clamp(argv[4].asInt(), 0, 100);
	config.use_half_pixel = !!argv[5].asInt();
}

void FilterTemplate::ProcessRGB32(void* dst0, ptrdiff_t dst_pitch, const void* src0, ptrdiff_t src_pitch) {
	const uint8* src = static_cast<const uint8*>(src0);
	uint8* dst = static_cast<uint8*>(dst0);

	// Fill in cur_{Y,U,V}.
	//auto start = chrono::steady_clock::now();
	CopyFromSrc(src, src_pitch);
	//auto end = chrono::steady_clock::now();
	//total_rgbtoyuv += chrono::duration<double, std::milli>(end - start).count();

	// Fill in the borders.
	//start = chrono::steady_clock::now();
	FillBorders();
	//end = chrono::steady_clock::now();
	//total_borders += chrono::duration<double, std::milli>(end - start).count();

	// On the first frame, copy cur_{Y,U,V} to prev_{Y,U,V}.
	if (!prev_Y || !prev_U || !prev_V) {
		prev_Y = make_unique<uint8[]>(width_ext * height_ext);
		prev_U = make_unique<int16[]>(width * height);
		prev_V = make_unique<int16[]>(width * height);

		memcpy(prev_Y.get(), cur_Y.get(), width_ext * height_ext);
		memcpy(prev_U.get(), cur_U.get(), width * height * 2);
		memcpy(prev_V.get(), cur_V.get(), width * height * 2);
	}

	// Half-pixel shifts.
	if (config.use_half_pixel) {
		if (!prev_Y_up) {
			prev_Y_up = make_unique<uint8[]>(width_ext * height_ext);
			prev_Y_left = make_unique<uint8[]>(width_ext * height_ext);
			prev_Y_upleft = make_unique<uint8[]>(width_ext * height_ext);
			prev_U_up = make_unique<int16[]>(width * height);
			prev_U_left = make_unique<int16[]>(width * height);
			prev_U_upleft = make_unique<int16[]>(width * height);
			prev_V_up = make_unique<int16[]>(width * height);
			prev_V_left = make_unique<int16[]>(width * height);
			prev_V_upleft = make_unique<int16[]>(width * height);
		}

		memcpy(prev_Y_up.get(), prev_Y.get(), width_ext * height_ext);
		memcpy(prev_Y_left.get(), prev_Y.get(), width_ext * height_ext);
		memcpy(prev_Y_upleft.get(), prev_Y.get(), width_ext * height_ext);

		HalfpixelShiftHorz(prev_Y_left.get(), width_ext, height_ext, false);
		HalfpixelShift(prev_Y_up.get(), width_ext, height_ext, false);
		HalfpixelShift(prev_Y_upleft.get(), width_ext, height_ext, false);
		HalfpixelShiftHorz(prev_Y_upleft.get(), width_ext, height_ext, false);

		memcpy(prev_U_up.get(), prev_U.get(), width * height * 2);
		memcpy(prev_U_left.get(), prev_U.get(), width * height * 2);
		memcpy(prev_U_upleft.get(), prev_U.get(), width * height * 2);

		HalfpixelShiftHorz(prev_U_left.get(), width, height, false);
		HalfpixelShift(prev_U_up.get(), width, height, false);
		HalfpixelShift(prev_U_upleft.get(), width, height, false);
		HalfpixelShiftHorz(prev_U_upleft.get(), width, height, false);

		memcpy(prev_V_up.get(), prev_V.get(), width * height * 2);
		memcpy(prev_V_left.get(), prev_V.get(), width * height * 2);
		memcpy(prev_V_upleft.get(), prev_V.get(), width * height * 2);

		HalfpixelShiftHorz(prev_V_left.get(), width, height, false);
		HalfpixelShift(prev_V_up.get(), width, height, false);
		HalfpixelShift(prev_V_upleft.get(), width, height, false);
		HalfpixelShiftHorz(prev_V_upleft.get(), width, height, false);
	}

	// Call the motion estimator.
	EstimateMotion();

	// Flag that we measured psnr in DrawOutput.
	measured_psnr = false;
	
	// Fill in the output.
	//start = chrono::steady_clock::now();
	if (!config.draw_nothing)
		DrawOutput(dst, dst_pitch);
	//end = chrono::steady_clock::now();
	//total_output += chrono::duration<double, std::milli>(end - start).count();

	// Measure PSNR here if we didn't do it before.
	if (config.measure_psnr && !measured_psnr) {
		if (!cur_Y_MC || !cur_U_MC || !cur_V_MC) {
			cur_Y_MC = make_unique<uint8[]>(width * height);
			cur_U_MC = make_unique<int16[]>(width * height);
			cur_V_MC = make_unique<int16[]>(width * height);
		}

		CompensateMotion();
		MeasurePSNR();
	}

	// Copy cur_{Y,U,V} to prev_{Y,U,V}.
	//start = chrono::steady_clock::now();
	memcpy(prev_Y.get(), cur_Y.get(), width_ext * height_ext);
	memcpy(prev_U.get(), cur_U.get(), width * height * 2);
	memcpy(prev_V.get(), cur_V.get(), width * height * 2);
	//end = chrono::steady_clock::now();
	//total_copy += chrono::duration<double, std::milli>(end - start).count();

	++frame_count;
}

void FilterTemplate::CopyFromSrc(const uint8* src, ptrdiff_t src_pitch) {
	auto p_cur_Y = cur_Y.get() + width_ext * MotionEstimator::BORDER + MotionEstimator::BORDER;
	auto p_cur_U = cur_U.get();
	auto p_cur_V = cur_V.get();

	for (sint32 y = 0; y < height; ++y) {
		auto p_src = src;

		for (sint32 x = 0; x < width; ++x) {
			auto b = p_src[0];
			auto g = p_src[1];
			auto r = p_src[2];

			RGBToYUV(r, g, b, *p_cur_Y, *p_cur_U, *p_cur_V);

			++p_cur_Y;
			++p_cur_U;
			++p_cur_V;
			p_src += 4;
		}

		p_cur_Y += 2 * MotionEstimator::BORDER;
		src += src_pitch;
	}
}

void FilterTemplate::FillBorders() {
	// Left and right borders.
	auto p_cur_Y = cur_Y.get() + width_ext * MotionEstimator::BORDER;

	for (sint32 y = 0; y < height; ++y) {
		memset(p_cur_Y, p_cur_Y[MotionEstimator::BORDER], MotionEstimator::BORDER);
		p_cur_Y += MotionEstimator::BORDER + width;
		memset(p_cur_Y, p_cur_Y[-1], MotionEstimator::BORDER);
		p_cur_Y += MotionEstimator::BORDER;
	}

	// Top and bottom borders.
	p_cur_Y = cur_Y.get();
	auto p_cur_Y_row = p_cur_Y + width_ext * MotionEstimator::BORDER;

	for (sint32 y = 0; y < MotionEstimator::BORDER; ++y) {
		memcpy(p_cur_Y, p_cur_Y_row, width_ext);
		p_cur_Y += width_ext;
	}

	p_cur_Y += width_ext * height;
	p_cur_Y_row = p_cur_Y - width_ext;

	for (sint32 y = 0; y < MotionEstimator::BORDER; ++y) {
		memcpy(p_cur_Y, p_cur_Y_row, width_ext);
		p_cur_Y += width_ext;
	}
}

void FilterTemplate::EstimateMotion() {
	const auto start = chrono::steady_clock::now();

	me->Estimate(cur_Y.get(),
	             prev_Y.get(),
	             prev_Y_up.get(),
	             prev_Y_left.get(),
	             prev_Y_upleft.get(),
	             vectors.get());

	const auto end = chrono::steady_clock::now();
	total_me += chrono::duration<double, std::milli>(end - start).count();
}

void FilterTemplate::DrawOutput(uint8* dst, ptrdiff_t dst_pitch) {
	const uint8* p_Y;
	const int16* p_U;
	const int16* p_V;
	ptrdiff_t Y_gap;

	if (config.output_type == OutputType::SOURCE) {
		p_Y = cur_Y.get() + width_ext * MotionEstimator::BORDER + MotionEstimator::BORDER;
		p_U = cur_U.get();
		p_V = cur_V.get();
		Y_gap = 2 * MotionEstimator::BORDER;
	} else {
		if (!cur_Y_MC || !cur_U_MC || !cur_V_MC) {
			cur_Y_MC = make_unique<uint8[]>(width * height);
			cur_U_MC = make_unique<int16[]>(width * height);
			cur_V_MC = make_unique<int16[]>(width * height);
		}

		if (config.output_type != OutputType::RESIDUAL_BEFORE_MC || config.measure_psnr)
			CompensateMotion();

		if (config.measure_psnr) {
			MeasurePSNR();
			measured_psnr = true;
		}

		if (config.output_type == OutputType::RESIDUAL_BEFORE_MC) {
			// We don't use the compensated frame here, simply copy the previous one.
			auto prev = prev_Y.get() + width_ext * MotionEstimator::BORDER + MotionEstimator::BORDER;;
			auto p_Y_MC = cur_Y_MC.get();

			for (sint32 y = 0; y < height; ++y) {
				memcpy(p_Y_MC, prev, width);
				prev += width_ext;
				p_Y_MC += width;
			}

			memcpy(cur_U_MC.get(), prev_U.get(), width * height * 2);
			memcpy(cur_V_MC.get(), prev_V.get(), width * height * 2);
		}

		// For residuals, subtract the current frame.
		if (config.output_type == OutputType::RESIDUAL_BEFORE_MC
			|| config.output_type == OutputType::RESIDUAL_AFTER_MC) {
			auto p_Y_MC = cur_Y_MC.get();
			auto p_U_MC = cur_U_MC.get();
			auto p_V_MC = cur_V_MC.get();

			auto p_Y_cur = cur_Y.get() + width_ext * MotionEstimator::BORDER + MotionEstimator::BORDER;
			auto p_U_cur = cur_U.get();
			auto p_V_cur = cur_V.get();

			for (sint32 y = 0; y < height; ++y) {
				for (sint32 x = 0; x < width; ++x) {
					*p_Y_MC = static_cast<uint8>(clamp(128 + (int{*p_Y_MC} - *p_Y_cur) * 3, 0, 255));
					*p_U_MC = (*p_U_MC - *p_U_cur) * 3;
					*p_V_MC = (*p_V_MC - *p_V_cur) * 3;

					++p_Y_MC;
					++p_U_MC;
					++p_V_MC;
					++p_Y_cur;
					++p_U_cur;
					++p_V_cur;
				}

				p_Y_cur += 2 * MotionEstimator::BORDER;
			}
		}

		p_Y = cur_Y_MC.get();
		p_U = cur_U_MC.get();
		p_V = cur_V_MC.get();
		Y_gap = 0;
	}

	CopyToDst(dst, dst_pitch, p_Y, Y_gap, p_U, p_V);

	if (config.show_vectors) {
		for (sint32 i = 0; i < num_blocks_vert; ++i) {
			for (sint32 j = 0; j < num_blocks_hor; ++j) {
				auto mv = vectors[i * num_blocks_hor + j];

				if (!mv.IsSplit()) {
					DrawLine(dst,
					         dst_pitch,
					         j * MotionEstimator::BLOCK_SIZE + MotionEstimator::BLOCK_SIZE / 2,
					         i * MotionEstimator::BLOCK_SIZE + MotionEstimator::BLOCK_SIZE / 2,
					         j * MotionEstimator::BLOCK_SIZE + MotionEstimator::BLOCK_SIZE / 2 + mv.x,
					         i * MotionEstimator::BLOCK_SIZE + MotionEstimator::BLOCK_SIZE / 2 + mv.y);
				} else {
					for (int h = 0; h < 4; ++h) {
						const auto mv_ = mv.SubVector(h);
						const auto x = j * MotionEstimator::BLOCK_SIZE + MotionEstimator::BLOCK_SIZE / 2
							+ (h & 1 ? 1 : -1) * (MotionEstimator::BLOCK_SIZE / 4);
						const auto y = i * MotionEstimator::BLOCK_SIZE + MotionEstimator::BLOCK_SIZE / 2
							+ (h > 1 ? 1 : -1) * (MotionEstimator::BLOCK_SIZE / 4);

						DrawLine(dst,
						         dst_pitch,
						         x,
						         y,
						         x + mv_.x,
						         y + mv_.y);
					}
				}
			}
		}
	}
}

void FilterTemplate::CompensateMotion() {
	auto p_Y_MC = cur_Y_MC.get();
	auto p_U_MC = cur_U_MC.get();
	auto p_V_MC = cur_V_MC.get();

	for (sint32 y = 0; y < height; ++y) {
		for (sint32 x = 0; x < width; ++x) {
			const auto i = (y / MotionEstimator::BLOCK_SIZE);
			const auto j = (x / MotionEstimator::BLOCK_SIZE);
			auto mv = vectors[i * num_blocks_hor + j];

			if (mv.IsSplit()) {
				const auto h = (((y % MotionEstimator::BLOCK_SIZE) < (MotionEstimator::BLOCK_SIZE / 2)) ? 0 : 2)
					+ (((x % MotionEstimator::BLOCK_SIZE) < (MotionEstimator::BLOCK_SIZE / 2)) ? 0 : 1);
				mv = mv.SubVector(h);
			}

			uint8* p_Y;
			int16* p_U;
			int16* p_V;

			switch (mv.shift_dir) {
			default:
			case ShiftDir::NONE:
				p_Y = prev_Y.get();
				p_U = prev_U.get();
				p_V = prev_V.get();
				break;

			case ShiftDir::UP:
				p_Y = prev_Y_up.get();
				p_U = prev_U_up.get();
				p_V = prev_V_up.get();
				break;

			case ShiftDir::LEFT:
				p_Y = prev_Y_left.get();
				p_U = prev_U_left.get();
				p_V = prev_V_left.get();
				break;

			case ShiftDir::UPLEFT:
				p_Y = prev_Y_upleft.get();
				p_U = prev_U_upleft.get();
				p_V = prev_V_upleft.get();
				break;
			}

			p_Y += width_ext * MotionEstimator::BORDER + MotionEstimator::BORDER
				+ y * width_ext + x;
			p_U += y * width + x;
			p_V += y * width + x;

			int sh_x, sh_y;
			if (x + mv.x < 0)
				sh_x = -x;
			else if (x + mv.x >= width)
				sh_x = width - 1 - x;
			else
				sh_x = mv.x;

			if (y + mv.y < 0)
				sh_y = -y;
			else if (y + mv.y >= height)
				sh_y = height - 1 - y;
			else
				sh_y = mv.y;

			*p_Y_MC = p_Y[sh_y * width_ext + sh_x];
			*p_U_MC = p_U[sh_y * width + sh_x];
			*p_V_MC = p_V[sh_y * width + sh_x];

			++p_Y_MC;
			++p_U_MC;
			++p_V_MC;
		}
	}
}

void FilterTemplate::CopyToDst(uint8* dst, ptrdiff_t dst_pitch, const uint8* p_Y, ptrdiff_t Y_gap, const int16* p_U, const int16* p_V) {
	for (sint32 y = 0; y < height; ++y) {
		auto p_dst = dst;

		for (sint32 x = 0; x < width; ++x) {
			uint8 r, g, b;
			YUVToRGB(*p_Y, *p_U, *p_V, r, g, b);

			p_dst[0] = b;
			p_dst[1] = g;
			p_dst[2] = r;
			p_dst[3] = 0;

			++p_Y;
			++p_U;
			++p_V;
			p_dst += 4;
		}

		p_Y += Y_gap;
		dst += dst_pitch;
	}
}

// Mostly copied from the old template.
void FilterTemplate::DrawLine(uint8* dst, ptrdiff_t dst_pitch, sint32 x1, sint32 y1, sint32 x2, sint32 y2) {
	int x, y;
	uint32* cur;
	bool origin;
	bool point = x1 == x2 && y1 == y2;
	if (x1 == x2)
	{
		for (y = min(y1, y2); y <= max(y2, y1); y++)
		{
			x = x1;
			if (x < 0 || x >= width || y < 0 || y >= height)
				continue;
			origin = y == y1;
			cur = (uint32*)(dst + y * dst_pitch) + x;
			if (point)
			{
				*cur = 0x00ff00;
				return;
			}
			if (RGBToY(*cur) < 128)
			{
				if (origin)
				{
					*cur = 0xff0000;
					origin = false;
				}
				else
					*cur = 0xffffff;
			}
			else
			{
				if (origin)
				{
					*cur = 0xff0000;
					origin = false;
				}
				else
					*cur = 0x00;
			}
		}
	}
	else if (abs(x2 - x1) >= abs(y2 - y1))
	{
		for (x = min(x1, x2); x <= max(x2, x1); x++)
		{
			if (x < 0 || x >= width)
				continue;
			origin = x == x1;
			y = (x - x1) * (y2 - y1) / (x2 - x1) + y1;
			if (y < 0 || y >= height)
				continue;
			cur = (uint32*)(dst + y * dst_pitch) + x;
			if (point)
			{
				*cur = 0x00ff00;
				return;
			}
			if (RGBToY(*cur) < 128)
			{
				if (origin)
				{
					*cur = 0xff0000;
					origin = false;
				}
				else
					*cur = 0xffffff;
			}
			else
			{
				if (origin)
				{
					*cur = 0xff0000;
					origin = false;
				}
				else
					*cur = 0x000000;
			}
		}
	}
	else
	{
		for (y = min(y1, y2); y <= max(y2, y1); y++)
		{
			origin = y == y1;
			x = (y - y1) * (x2 - x1) / (y2 - y1) + x1;
			if (x < 0 || x >= width || y < 0 || y >= height)
				continue;
			cur = (uint32*)(dst + y * dst_pitch) + x;
			if (point)
			{
				*cur = 0x00ff00;
				return;
			}
			if (RGBToY(*cur) < 128)
			{
				if (origin)
				{
					*cur = 0xff0000;
					origin = false;
				}
				else
					*cur = 0xffffff;
			}
			else
			{
				if (origin)
				{
					*cur = 0xff0000;
					origin = false;
				}
				else
					*cur = 0x000000;
			}
		}
	}
}

void FilterTemplate::MeasurePSNR() {
	if (frame_count == 0)
		return;

	// Calculate MSE.
	double YMSE = 0.0, UMSE = 0.0, VMSE = 0.0;

	auto p_Y_MC = cur_Y_MC.get();
	auto p_U_MC = cur_U_MC.get();
	auto p_V_MC = cur_V_MC.get();

	auto p_Y_cur = cur_Y.get() + width_ext * MotionEstimator::BORDER + MotionEstimator::BORDER;
	auto p_U_cur = cur_U.get();
	auto p_V_cur = cur_V.get();

	for (sint32 y = 0; y < height; ++y) {
		for (sint32 x = 0; x < width; ++x) {
			auto diff = int{*p_Y_cur} - *p_Y_MC;
			YMSE += diff * diff;

			diff = int{*p_U_cur} - *p_U_MC;
			UMSE += diff * diff;

			diff = int{*p_V_cur} - *p_V_MC;
			VMSE += diff * diff;

			++p_Y_MC;
			++p_U_MC;
			++p_V_MC;
			++p_Y_cur;
			++p_U_cur;
			++p_V_cur;
		}

		p_Y_cur += 2 * MotionEstimator::BORDER;
	}

	// Calculate PSNR.
	const auto YPSNR = PSNR(YMSE, width, height);
	const auto UPSNR = PSNR(UMSE, width, height);
	const auto VPSNR = PSNR(VMSE, width, height);

	if (psnr_file)
		psnr_file << frame_count << ": " << YPSNR << ' ' << UPSNR << ' ' << VPSNR << '\n';

	total_y_psnr += YPSNR;
	total_u_psnr += UPSNR;
	total_v_psnr += VPSNR;
}

extern VDXFilterDefinition filterDef_template = VDXVideoFilterDefinition<FilterTemplate>(FILTER_AUTHOR, FILTER_NAME, "ME task filter");
