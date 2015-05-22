#include "H264_Decoder.hpp"
#include "TimeStampClient.hpp"

#include <windows.h>

double getTimeDiff(LARGE_INTEGER t)
{
   LARGE_INTEGER frequency;// ticks per second
   LARGE_INTEGER now;

   QueryPerformanceCounter(&now);
   QueryPerformanceFrequency(&frequency);

   double elapsedTime = (now.QuadPart - t.QuadPart) * 1000.0 / frequency.QuadPart;

   return elapsedTime;
}

void ErrorBox(const char *msg, ...)
{
   char errmsg[256];
   va_list va;

   va_start(va, msg);
   vsnprintf(errmsg, sizeof(errmsg), msg, va);
   va_end(va);

   MessageBox(NULL, errmsg, "Error", MB_ICONWARNING | MB_DEFBUTTON2);
}

void fatalError(const char *msg, ...)
{
   char errmsg[256];
   va_list va;

   va_start(va, msg);
   vsnprintf(errmsg, sizeof(errmsg), msg, va);
   va_end(va);

   MessageBox(NULL, errmsg, "Error", MB_ICONWARNING | MB_DEFBUTTON2);
   exit(EXIT_FAILURE);
}


static struct AplicationContext
{
   double fps;
   int width;
   int height;
   H264_Decoder decoder;
   void *pixels;//decoder output buffer allocated with the CreateDIBSection

   char timeServerAddr[64];
   int timeServerPort;

   // Global Windows/Drawing variables
   HBITMAP hbmp;
   HANDLE hTickThread;
   HWND hwnd;
   HDC hdcMem;
} ctx;


DWORD WINAPI tickThreadProc(HANDLE handle)
{
   LARGE_INTEGER lastFrameDisplayed;

   TimeStampClient client;
   bool timeServerConnected;
   int err = client.init(ctx.timeServerAddr, ctx.timeServerPort);
   if(0 != err)
   {
      ErrorBox("Can't connect to server %s:%i", ctx.timeServerAddr, ctx.timeServerPort);
      timeServerConnected = false;
   }
   else
   {
      timeServerConnected = true;
   }

   Sleep(100);
   ShowWindow(ctx.hwnd, SW_SHOW);

   HDC hdc = GetDC( ctx.hwnd );
   ctx.hdcMem = CreateCompatibleDC( hdc );
   HBITMAP hbmOld = (HBITMAP)SelectObject( ctx.hdcMem, ctx.hbmp );

   double msPerFrame = 1000.0 / ctx.fps;
   QueryPerformanceCounter(&lastFrameDisplayed);

   uint32_t frameTimeStampMS = 0u;
   for ( ;; )
   {
      Frame frame;
      if(0 == ctx.decoder.getFrameRGB(&frame))
      {
         if(0u == frameTimeStampMS)
         {
            if(timeServerConnected)
            {
               client.sendStartCmd();
            }
         }

         BitBlt(hdc, 0, 0, ctx.width, ctx.height, ctx.hdcMem, 0, 0, SRCCOPY);

         if(timeServerConnected)
         {
            client.sendTimeStamp(frameTimeStampMS);
         }
         frameTimeStampMS+=(uint32_t)msPerFrame;

         double timeSpendMs = getTimeDiff(lastFrameDisplayed);
         QueryPerformanceCounter(&lastFrameDisplayed);

         int timeToSleepMS = msPerFrame - timeSpendMs;
         if(timeToSleepMS > 1)
         {
            Sleep( timeToSleepMS );
         }
      }
      else
      {
         if(timeServerConnected)
         {
            client.sendStopCmd();
         }

         ctx.decoder.seekToStart();
         frameTimeStampMS = 0u;
      }
   }

   SelectObject( ctx.hdcMem, hbmOld );
   DeleteDC( hdc );
   return 0;
}

void MakeSurface(HWND hwnd)
{
   /* Use CreateDIBSection to make a HBITMAP which can be quickly
   * blitted to a surface while giving 100% fast access to pixels
   * before blit.
   */
   // Desired bitmap properties
   BITMAPINFO bmi;
   bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
   bmi.bmiHeader.biWidth = ctx.width;
   bmi.bmiHeader.biHeight =  -ctx.height; // Order pixels from top to bottom
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 24; // last byte not used, 32 bit for alignment
   bmi.bmiHeader.biCompression = BI_RGB;
   bmi.bmiHeader.biSizeImage = 0;
   bmi.bmiHeader.biXPelsPerMeter = 0;
   bmi.bmiHeader.biYPelsPerMeter = 0;
   bmi.bmiHeader.biClrUsed = 0;
   bmi.bmiHeader.biClrImportant = 0;
   bmi.bmiColors[0].rgbBlue = 0;
   bmi.bmiColors[0].rgbGreen = 0;
   bmi.bmiColors[0].rgbRed = 0;
   bmi.bmiColors[0].rgbReserved = 0;
   HDC hdc = GetDC( ctx.hwnd );
   // Create DIB section to always give direct access to pixels
   ctx.hbmp = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, &ctx.pixels, NULL, 0 );
   DeleteDC( hdc );

   int size = ctx.width*ctx.height*3;
   if(0 != ctx.decoder.setOutputBuffer((uint8_t *)ctx.pixels, size))
   {
      fatalError("decoder.setOutputBuffer(%i) failed!", size);
   }

   ctx.hTickThread = CreateThread(NULL, 0, &tickThreadProc, &ctx, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch ( msg )
   {
      case WM_CREATE:
      {
         MakeSurface(hwnd);
      }
      break;

      case WM_CLOSE:
      {
         DestroyWindow(hwnd);
      }
      break;

      case WM_KEYDOWN:
        switch (wParam)
        {
          case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;
        }
        break;

      case WM_DESTROY:
      {
         TerminateThread(ctx.hTickThread, 0);
         ctx.decoder.free();
         PostQuitMessage(0);
      }
      break;

      default:
         return DefWindowProc( hwnd, msg, wParam, lParam );
   }
   return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
   char videoFile[64];
   char timeServerPort[20];

   int argCount = sscanf(lpCmdLine,"%s %s %s", videoFile, ctx.timeServerAddr, timeServerPort);
   if(3 != argCount)
   {
      fatalError("Specify the cmd line as:'<videoFilePath> <timeStampServer> <timeStampPort>'");
      return 0;
   }
   ctx.timeServerPort = atoi(timeServerPort);

   WNDCLASSEX wc;
   wc.cbClsExtra = 0;
   wc.cbWndExtra = 0;
   wc.cbSize = sizeof( WNDCLASSEX );
   wc.hbrBackground = CreateSolidBrush( 0 );
   wc.hCursor = LoadCursor( NULL, IDC_ARROW );
   wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
   wc.hIconSm = LoadIcon( NULL, IDI_APPLICATION );
   wc.hInstance = hInstance;
   wc.lpfnWndProc = WndProc;
   wc.lpszClassName = videoFile;
   wc.lpszMenuName = NULL;
   wc.style = 0;

   if(!RegisterClassEx(&wc))
   {
      fatalError("Failed to register window class.");
      return 0;
   }

   int err = ctx.decoder.load(videoFile, ctx.width, ctx.height, ctx.fps);
   if(0 != err)
   {
      fatalError("Can't load file: %s\n", videoFile);
      return 0;
   }

   HMONITOR hmon = MonitorFromWindow(ctx.hwnd, MONITOR_DEFAULTTONEAREST);
   MONITORINFO mi = { sizeof(mi) };
   if (!GetMonitorInfo(hmon, &mi))
   {
      fatalError("GetMonitorInfo() failed");
      return 0;
   }

   ctx.hwnd = CreateWindowEx( WS_EX_APPWINDOW
                             ,videoFile
                             ,videoFile
                             ,WS_POPUP | WS_VISIBLE
                             ,mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left
                             ,mi.rcMonitor.bottom - mi.rcMonitor.top
                             ,NULL, NULL, hInstance, NULL );

   MSG msg;
   while ( GetMessage(&msg, 0, 0, 0) > 0 )
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   return 0;
}
