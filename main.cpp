/*
* main.cpp
* Copyright (C) 2016.11, jinfeng <kavin.zhuang@gmail.com>
*
* 思路：
* 1. 创建WIC位图，创建WIC的Render（包括属于该Render的画刷）绘画
* 2. 将WIC位图转换成D2D位图，注意，这不是指针操作，使用HWND的Render显示D2D位图后，释放D2D位图
* 3. 更新WIC位图，重复第2步
*/

/* Windows API */
#include <Windows.h>

/* GDI+ */
#include <gdiplus.h>

/* Direct2D */
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <dwrite.h>

/* WICBitmap */
#include <wincodec.h>

/* ISO C99 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>

/*
*==============================================================================
* Namespaces
*==============================================================================
*/

using namespace Gdiplus;

/*
*==============================================================================
* Compiler Control
*==============================================================================
*/

#pragma warning(disable:4996)

/*
*==============================================================================
* Librarys
*==============================================================================
*/

#pragma comment(lib,"gdiplus")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Windowscodecs.lib")

/*
*==============================================================================
* Defines
*==============================================================================
*/

#define TRACE(fmt, ...) \
  printf("[%s:%d] " fmt, __FUNCTION__, __LINE__, ##__VAR_ARGS__);

/*
*==============================================================================
* Types
*==============================================================================
*/

typedef struct _timeline {
  uint64_t start;
  uint64_t stop;
  uint32_t task;
} timeline_t;

/*
*==============================================================================
* Local Varibles
*==============================================================================
*/

static TCHAR winclass[] = TEXT("mainwindow");
static TCHAR wintitle[] = TEXT("demo");
static int win_width = 800;
static int win_height = 600;
HWND mainwindow;

static IWICImagingFactory *pWICFactory = NULL;
static ID2D1Factory1* pD2DFactory = NULL;
static IDWriteFactory *pDWriteFactory = NULL;

static ID2D1HwndRenderTarget *pRenderTarget = NULL;
static ID2D1RenderTarget *pWicRenderTarget = NULL;

static ID2D1SolidColorBrush *pBlackBrush = NULL;
static ID2D1SolidColorBrush *pWhiteBrush = NULL;
static ID2D1SolidColorBrush *pRedBrush = NULL;
static ID2D1SolidColorBrush *pWicBlackBrush = NULL;

static IDWriteTextFormat *pTextFormat = NULL;

static IWICBitmap *pWicBitmap = NULL;
static ID2D1Bitmap *pBitmap = NULL;
static ID2D1Bitmap *pBitmapPart = NULL;

static int fps = 0;

static int pos_display = 0;

static int data_count = 200;
static timeline_t *origin_data = NULL;
static timeline_t *win_data = NULL;

static wchar_t text_info_array[128];

static float data_ratio = 1.0;

static int image_width;
static int image_height;

/*
*==============================================================================
* Local Functions
*==============================================================================
*/

static void generate_random_data(void)
{
  int i;

  origin_data = (timeline_t *)malloc(data_count * sizeof(timeline_t));
  if (NULL == origin_data) {
    printf("malloc for data failed\n");
    return;
  }

  uint64_t pos_x = 0;

  for (i = 0; i < data_count; i++) {
    origin_data[i].task = rand() % 255;
    origin_data[i].start = pos_x;
    // Fix
    pos_x += 10;
    origin_data[i].stop = pos_x;
    pos_x += 0;

    //printf("%d ", origin_data[i].task);
  }
}

static void scan_data_to_get_ratio(void)
{
  int i;

  int num = data_count;

  if (NULL == origin_data) {
    printf("origin_data is NULL\n");
    return;
  }

  win_data = (timeline_t *)malloc(num * sizeof(timeline_t));
  if (NULL == win_data) {
    printf("malloc for data failed\n");
    return;
  }

  uint64_t start_pos = 0xFFFFFFFF;
  uint64_t end_pos = 0;
  for (i = 0; i < num; i++) {
    if (origin_data[i].start < start_pos) {
      start_pos = origin_data[i].start;
    }
    if (origin_data[i].stop > end_pos) {
      end_pos = origin_data[i].stop;
    }
  }

#ifdef FULL_SHOW
  uint64_t diff = end_pos - start_pos;

  printf("diff = %lld\n", diff);
  
  data_ratio = diff / (float)win_width;

  printf("ratio = %f\n", data_ratio);
#endif

  for (i = 0; i < num; i++) {
    win_data[i].start = (origin_data[i].start - start_pos) / data_ratio;
    win_data[i].stop = (origin_data[i].stop - start_pos) / data_ratio;
    win_data[i].task = origin_data[i].task;

    /* BUG: what happends if value > 0xFFFFFFFF */
    //printf("%d : %d %d\n", win_data[i].task, win_data[i].start, win_data[i].stop);
  }

  // generate the pixel data width & height, for static large image
  for (i = 0; i < num; i++) {
    if (win_data[i].start < start_pos) {
      start_pos = win_data[i].start;
    }
    if (win_data[i].stop > end_pos) {
      end_pos = win_data[i].stop;
    }
  }

  printf("start : %lld, stop : %lld\n", start_pos, end_pos);

  image_width = end_pos - start_pos;

  // Qustion
  image_height = 600;

  printf("image: %d x %d\n", image_width, image_height);
}

static int create_d2d_factory(HWND hwnd, UINT32 width, UINT32 height)
{
  HRESULT hr;
  RECT rc;

  /* Init Direct2D Engine
  *
  * NOTE: Work in Single Thread Mode
  */
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
  if (S_OK != hr) {
    printf("2D Factory Inited failed\n");
    return S_FALSE;
  }

  GetClientRect(hwnd, &rc);

  /* create Direct2D Render Target for window */
  hr = pD2DFactory->CreateHwndRenderTarget(
    D2D1::RenderTargetProperties(),
    D2D1::HwndRenderTargetProperties(
    hwnd,
    D2D1::SizeU(width, height)
    ),
    &pRenderTarget
    );
  if (S_OK != hr) {
    return S_FALSE;
  }

  /* create Direct2D brush */
  hr = pRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::Black),
    &pBlackBrush
    );
  if (S_OK != hr) {
    return S_FALSE;
  }
}

static int create_text_factory(void)
{
  HRESULT hr;

  hr = DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    (IUnknown**)&pDWriteFactory);
  if (S_OK != hr) {
    printf("DWrite Factory Inited failed\n");
    return S_FALSE;
  }

  hr = pDWriteFactory->CreateTextFormat(
    L"segoe print",
    NULL,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    17.0f,
    L"en-us", //locale
    &pTextFormat);
  if (S_OK != hr) {
    printf("TextFormat Inited failed\n");
    return S_FALSE;
  }

  // Center the text horizontally and vertically.
  //pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  //pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

static int create_wic_factory(UINT32 width, UINT32 height)
{
  HRESULT hr;

  CoInitialize(NULL);

  /* WIC Init
  *
  * WARNING: CLSID_WICImagingFactory not work on Win7
  * MSDN-"CLSID_WICImagingFactory: breaking change since VS11 Beta on Windows"
  */
  hr = CoCreateInstance(
    CLSID_WICImagingFactory1,
    NULL,
    CLSCTX_INPROC_SERVER,
    IID_IWICImagingFactory,
    (LPVOID*)&pWICFactory
    );
  if (S_OK != hr) {
    printf("err = %x\n", GetLastError());
    return S_FALSE;
  }

  /* WARNING: will error if parameters not correct. */
  hr = pWICFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &pWicBitmap);
  if (S_OK != hr) {
    printf("%d err = %x\n", __LINE__, GetLastError());
    return S_FALSE;
  }
}

static int create_wic_bitmap_render_target(void)
{
  HRESULT hr;

  /* Create Render target for WIC bitmap */
  hr = pD2DFactory->CreateWicBitmapRenderTarget(
    pWicBitmap,
    D2D1::RenderTargetProperties(),
    &pWicRenderTarget);
  if (S_OK != hr) {
    printf("%d err = %x\n", __LINE__, GetLastError());
    return S_FALSE;
  }
  printf("create WIC Bitmap Render Target OK\n");

  /* WARNING: the brush should be created by correct render target. */
  hr = pWicRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::White),
    &pWhiteBrush
    );
  if (S_OK != hr) {
    return S_FALSE;
  }

  hr = pWicRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::Red),
    &pRedBrush
    );
  if (S_OK != hr) {
    return S_FALSE;
  }

  hr = pWicRenderTarget->CreateSolidColorBrush(
    D2D1::ColorF(D2D1::ColorF::Black),
    &pWicBlackBrush
    );
  if (S_OK != hr) {
    return S_FALSE;
  }

  return 0;
}

static void draw_line(int y, int x1, int x2)
{
  pWicRenderTarget->DrawLine(D2D1::Point2F(x1, y), D2D1::Point2F(x2, y), pRedBrush);
}

static void draw_wic_bitmap(void)
{
  /* WARNING: render works between begin & end procedure */

  pWicRenderTarget->BeginDraw();

  pWicRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

  //pWicRenderTarget->DrawRectangle(D2D1::RectF(pos, pos, pos + 100, pos + 100), pWhiteBrush, 1);

  D2D1_SIZE_F renderTargetSize = pRenderTarget->GetSize();

  pWicRenderTarget->DrawText(
    text_info_array,
    ARRAYSIZE(text_info_array) - 1,
    pTextFormat,
    D2D1::RectF(0, 10, 120, 20),
    pWicBlackBrush);

  wchar_t testinfo[64];

  pWicRenderTarget->DrawLine(D2D1::Point2F(0, 0), D2D1::Point2F(image_width, image_height), pRedBrush);

#if 0
  for (int i = 0; i < data_count; i++) {
    //draw_line(win_data[i].task, win_data[i].start, win_data[i].stop);
    //draw_line(i%10+100, 10 * i, 10 * i + 10);

    wsprintf(testinfo, L"%d", i);
    pWicRenderTarget->DrawText(
      testinfo,
      ARRAYSIZE(testinfo) - 1,
      pTextFormat,
      D2D1::RectF(i, 100, 20, 20),
      pWicBlackBrush);
  }
#endif

  pWicRenderTarget->EndDraw();
}

static int create_d2d_bitmap(void)
{
  HRESULT hr;

  /* Create D2D Bitmap */
  hr = pRenderTarget->CreateBitmapFromWicBitmap(pWicBitmap, &pBitmap);
  if (S_OK != hr) {
    printf("err = %x\n", GetLastError());
    return S_FALSE;
  }

  /* Create D2D Bitmap for part */
  hr = pRenderTarget->CreateBitmapFromWicBitmap(pWicBitmap, &pBitmapPart);
  if (S_OK != hr) {
    printf("err = %x\n", GetLastError());
    return S_FALSE;
  }
}

static int CreateD2DResource(HWND hwnd)
{
  HRESULT hr;
  RECT rc;

  /* 创立图像处理工厂 */
  create_d2d_factory(hwnd, win_width, win_height);

  /* 创建字体资源工厂 */
  create_text_factory();

  image_height = win_height;

  /* 创建图片资源工厂 */
  create_wic_factory(image_width, image_height);

  /* 图像处理工厂：绘图器 */
  create_wic_bitmap_render_target();

  /* 绘图中... */
  draw_wic_bitmap();

  /* 图像转换成可以显示的方式 */
  create_d2d_bitmap();

  return 0;
}

static void DrawRectangle(HWND hwnd)
{
  HRESULT hr;

  pRenderTarget->BeginDraw();

  // clear canvas, not validate now
  pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

  D2D1_RECT_U src = { pos_display, 0, pos_display+win_width, win_height};
  D2D1_POINT_2U point = { 0, 0 };

  pBitmapPart->CopyFromBitmap(&point, pBitmap, &src);

  pRenderTarget->DrawBitmap(pBitmapPart, D2D1::RectF(0, 0, win_width, win_height));

  //pRenderTarget->DrawBitmap(pBitmap, D2D1::RectF(0, 0, win_width, win_height));

  /* NOTE: Flush the content to windows */
  pRenderTarget->EndDraw();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  HDC         hdc;
  PAINTSTRUCT ps;
  RECT rect;

  static int pretick = 0;

  switch (uMsg)
  {
  case WM_CREATE:
    SetTimer(hwnd, NULL, 10, NULL);
    if (S_OK != CreateD2DResource(hwnd)) {
      printf("D2D Init failed\n");
    }
    return 0;
  case WM_TIMER:
    pos_display++;
    if (pos_display >= image_width - win_width)
      pos_display = 0;
    GetClientRect(hwnd, &rect);
    InvalidateRect(hwnd, &rect, FALSE);
    return 0;
  case WM_COMMAND:
    GetClientRect(hwnd, &rect);
    InvalidateRect(hwnd, &rect, FALSE);
    return 0;
  case WM_PAINT:
    if (GetTickCount() - pretick > 1000) {
      //printf("fps: %d\n", fps);
      fps = 0;
      pretick = GetTickCount();
    }
    hdc = BeginPaint(hwnd, &ps);
    DrawRectangle(hwnd);
    EndPaint(hwnd, &ps);
    fps++;
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static int register_window(HINSTANCE hInstance)
{
  WNDCLASSEX wce = { 0 };

  wce.cbSize = sizeof(wce);
  wce.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wce.hCursor = LoadCursor(NULL, IDC_ARROW);
  wce.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wce.hInstance = hInstance;
  wce.lpfnWndProc = WndProc;
  wce.lpszClassName = winclass;
  wce.style = CS_HREDRAW | CS_VREDRAW;
  if (!RegisterClassEx(&wce)) {
    return S_FALSE;
  }

  return S_OK;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
  MSG msg;
  HWND hwnd;

  /* Console for DEBUG */
  AllocConsole();
  freopen("CONOUT$", "w", stdout);

  printf("Hello World\n");

  swprintf(text_info_array, L"ratio: %0.2f\nfps: %d", data_ratio, fps);

  srand((unsigned)time(NULL));

  generate_random_data();

  /* 可以显示LOGO后，先处理后台数据 */
  scan_data_to_get_ratio();

  /* GDI+ Init */
  ULONG_PTR gdiplusToken;
  GdiplusStartupInput gdiplusInput;
  GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL);

  /* Windows Register */
  if (0 != register_window(hInstance)) {
    return S_FALSE;
  }

  /* Create Windows & Do Message Loop */
  hwnd = CreateWindowEx(0, winclass, wintitle, WS_OVERLAPPEDWINDOW,
    500, 300, win_width, win_height,
    NULL, NULL, hInstance, NULL);
  if (NULL == hwnd) {
    return S_FALSE;
  }

  // register window handle
  mainwindow = hwnd;

  ShowWindow(hwnd, nShowCmd);
  UpdateWindow(hwnd);

  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  GdiplusShutdown(gdiplusToken);

  return msg.wParam;
}
