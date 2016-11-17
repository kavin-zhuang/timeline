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

using namespace Gdiplus;

#pragma warning(disable:4996)

#pragma comment(lib,"gdiplus")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Windowscodecs.lib")

static TCHAR winclass[] = TEXT("mainwindow");
static TCHAR wintitle[] = TEXT("demo");
static int win_width = 800;
static int win_height = 600;
HWND mainwindow;

static IWICImagingFactory *pWICFactory = NULL;
static ID2D1Factory1* pD2DFactory = NULL;

static ID2D1HwndRenderTarget *pRenderTarget = NULL;
static ID2D1RenderTarget *pWicRenderTarget = NULL;

static ID2D1SolidColorBrush *pBlackBrush = NULL;
static ID2D1SolidColorBrush *pWhiteBrush = NULL;

static IWICBitmap *pWicBitmap = NULL;
static ID2D1Bitmap *pBitmap = NULL;

static int pos = 0;

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

  return 0;
}

static void draw_wic_bitmap(void)
{
  /* WARNING: render works between begin & end procedure */

  pWicRenderTarget->BeginDraw();

  pWicRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Green));
  pWicRenderTarget->DrawRectangle(D2D1::RectF(pos, pos, pos + 100, pos + 100), pWhiteBrush, 1);

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
}

static int CreateD2DResource(HWND hwnd)
{
  HRESULT hr;
  RECT rc;

  /* 创立图像处理工厂 */
  create_d2d_factory(hwnd, win_width, win_height);

  /* 创建图片资源工厂 */
  create_wic_factory(win_width, win_height);

  /* 图像处理工厂：绘图器 */
  create_wic_bitmap_render_target();

  return 0;
}

static void DrawRectangle(HWND hwnd)
{
  HRESULT hr;

  pRenderTarget->BeginDraw();

  // clear canvas, not validate now
  pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

  /* 绘图中... */
  draw_wic_bitmap();

  /* 图像转换成可以显示的方式 */
  create_d2d_bitmap();

  pRenderTarget->DrawBitmap(pBitmap, D2D1::RectF(0, 0, win_width, win_height));

  /* NOTE: Flush the content to windows */
  pRenderTarget->EndDraw();

  pBitmap->Release();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  HDC         hdc;
  PAINTSTRUCT ps;
  RECT rect;

  switch (uMsg)
  {
  case WM_TIMER:
    pos ++;
    if (pos > 400)
      pos = 0;
    GetClientRect(hwnd, &rect);
    InvalidateRect(hwnd, &rect, TRUE);
    return 0;
  case WM_COMMAND:
    GetClientRect(hwnd, &rect);
    InvalidateRect(hwnd, &rect, TRUE);
    return 0;
  case WM_CREATE:
    SetTimer(hwnd, NULL, 50, NULL);
    if (S_OK != CreateD2DResource(hwnd)) {
      printf("D2D Init failed\n");
    }
    return 0;
  case WM_PAINT:
    hdc = BeginPaint(hwnd, &ps);
    DrawRectangle(hwnd);
    EndPaint(hwnd, &ps);
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
