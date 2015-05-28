#include "CommandServer.hpp"
#include "H264_Decoder.hpp"
#include "TimeStampProvider.hpp"

#include <windows.h>

#define  WM_START (WM_USER + 1)
#define  WM_STOP (WM_USER + 2)

static struct AplicationContext
{
   CommandServer commandServer;

   void *pixels;//decoder output buffer allocated with the CreateDIBSection

   char timeStampFile[64];
   char videoStreamFile[64];
   char timeServerPort[32];
   char idleAnimation[64];

   bool idleVideoRunning;
   HANDLE idleThread;
   bool streamingRunning;
   HANDLE streamingThread;
   HANDLE commandServerThread;

   // Global Windows/Drawing variables
   HBITMAP hbmp;
   HWND hwnd;
   HDC hdcMem;
} ctx;

void MakeSurface(HWND hwnd, int w, int h)
{
   /* Use CreateDIBSection to make a HBITMAP which can be quickly
   * blitted to a surface while giving 100% fast access to pixels
   * before blit.
   */
   // Desired bitmap properties
   BITMAPINFO bmi;
   bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
   bmi.bmiHeader.biWidth = w;
   bmi.bmiHeader.biHeight =  -h; // Order pixels from top to bottom
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
}


void DeleteSurface(HWND hwnd)
{
   DeleteObject((void*)ctx.hbmp);
}

double getTimeDiff(LARGE_INTEGER t)
{
   LARGE_INTEGER frequency;// ticks per second
   LARGE_INTEGER now;

   QueryPerformanceCounter(&now);
   QueryPerformanceFrequency(&frequency);

   double elapsedTime = (now.QuadPart - t.QuadPart) * 1000.0 / frequency.QuadPart;

   return elapsedTime;
}

static DWORD joinThread(HANDLE hThread)
{
   DWORD dwExitCode;

   WaitForSingleObject(hThread, INFINITE);
   GetExitCodeThread(hThread, &dwExitCode);
   CloseHandle(hThread);

   return dwExitCode;
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

DWORD WINAPI commandServerThread(HANDLE handle)
{
   int res;

   while(1)
   {
      res = ctx.commandServer.waitForStartCommand();
      if(0 == res)
      {
         LRESULT err = SendMessage(ctx.hwnd, WM_START, 0, 0);
         if(0 != err)
         {
            ErrorBox("Failed to send the WM_START message");
         }
      }
   }

   return 0;
}


DWORD WINAPI playIdleVideo(HANDLE handle)
{
   H264_Decoder decoder;
   const char *fname = (const char *)handle;
   int w;
   int h;
   double framerate;

   int err = decoder.load(fname, w, h, framerate);
   if(0 != err)
   {
      fatalError("Can't load file: %s\n", fname);
      return 0;
   }
   if(framerate < 1.0)
   {
      framerate = 25;
   }

   MakeSurface(ctx.hwnd, w, h);

   int size = w*h*3;
   if(0 != decoder.setOutputBuffer((uint8_t *)ctx.pixels, size))
   {
      fatalError("decoder.setOutputBuffer(%i) failed!", size);
   }

   HDC hdc = GetDC( ctx.hwnd );
   ctx.hdcMem = CreateCompatibleDC( hdc );
   HBITMAP hbmOld = (HBITMAP)SelectObject( ctx.hdcMem, ctx.hbmp );

   double msPerFrame = 1000.0 / framerate;

   LARGE_INTEGER frameProcessingStarted;
   while(ctx.idleVideoRunning)
   {
      QueryPerformanceCounter(&frameProcessingStarted);
      Frame frame;
      if(0 == decoder.getFrameRGB(&frame))
      {
         BitBlt(hdc, 0, 0, w, h, ctx.hdcMem, 0, 0, SRCCOPY);
         double timeSpendMs = getTimeDiff(frameProcessingStarted);

         int timeToSleepMS = msPerFrame - timeSpendMs;
         if(timeToSleepMS > 1)
         {
            Sleep( timeToSleepMS );
         }
      }
      else
      {
         decoder.seekToStart();
      }
   }

   decoder.free();
   SelectObject( ctx.hdcMem, hbmOld );
   DeleteDC( hdc );
   DeleteSurface(ctx.hwnd);

   return 0;
}

DWORD WINAPI streamVideoWithTimeStamp(HANDLE handle)
{
   TimeStampProvider  timeStamps;
   int err = timeStamps.init(ctx.timeStampFile);
   if(0 != err)
   {
      fatalError("Can't read time stamp file: '%s'", ctx.timeStampFile);
   }

   int w;
   int h;
   double framerate;
   H264_Decoder decoder;
   err = decoder.load(ctx.videoStreamFile, w, h, framerate);
   if(0 != err)
   {
      fatalError("Can't load file: %s\n", ctx.videoStreamFile);
      return 0;
   }

   MakeSurface(ctx.hwnd, w, h);

   int size = w*h*3;
   if(0 != decoder.setOutputBuffer((uint8_t *)ctx.pixels, size))
   {
      fatalError("decoder.setOutputBuffer(%i) failed!", size);
   }

   HDC hdc = GetDC(ctx.hwnd);
   ctx.hdcMem = CreateCompatibleDC(hdc);
   HBITMAP hbmOld = (HBITMAP)SelectObject(ctx.hdcMem, ctx.hbmp);

   int frameTimeStampMS;
   LARGE_INTEGER frameProcessingStarted;
   for (int frameNumber=0; ctx.streamingRunning ;frameNumber++)
   {
      QueryPerformanceCounter(&frameProcessingStarted);
      Frame frame;
      if(0 == decoder.getFrameRGB(&frame))
      {
         frameTimeStampMS = timeStamps.getFrameTimeStamp(frameNumber);
         if(-1 == frameTimeStampMS)
         {
            break;
         }
         BitBlt(hdc, 0, 0, w, h, ctx.hdcMem, 0, 0, SRCCOPY);

         err = ctx.commandServer.sendTimeStamp(frameTimeStampMS);
         if(0 != err)
         {
            //exit streaming thread if connection to client is lost
            break;
         }

         double timeSpendMs = getTimeDiff(frameProcessingStarted);

         int nextFrameTimeStampMs = timeStamps.getFrameTimeStamp(frameNumber+1);
         if(-1 == nextFrameTimeStampMs)
         {
            break;//last frame
         }
         double delay = nextFrameTimeStampMs - frameTimeStampMS;
         int timeToSleepMS = delay - timeSpendMs;
         if(timeToSleepMS > 1)
         {
            Sleep( timeToSleepMS );
         }
      }
      else
      {
         break;
      }
   }

   ctx.commandServer.sendStopCmd();
   timeStamps.deinit();

   decoder.free();
   SelectObject(ctx.hdcMem, hbmOld);
   DeleteDC(hdc);
   DeleteSurface(ctx.hwnd);

   //notify that streaming thread is finished
   SendMessage(ctx.hwnd, WM_STOP, 0, 0);

   return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   switch ( msg )
   {
      case WM_STOP:
      {
         if(NULL != ctx.streamingThread)
         {
            ctx.streamingRunning = false;
            CloseHandle(ctx.streamingThread);
            ctx.streamingThread = NULL;
         }

         ctx.idleVideoRunning = true;
         ctx.idleThread = CreateThread(NULL, 0, &playIdleVideo, (void*)ctx.idleAnimation, 0, 0);
         if(NULL == ctx.idleThread)
         {
            ctx.idleVideoRunning = false;
            fatalError("Can't create idle thread");
            return 0;
         }
      }
      break;

      case WM_START:
      {
         if(ctx.streamingRunning)
         {
            fprintf(stderr, "START command received while stream is already displayed\n");
            return 0;
         }

         if(ctx.idleVideoRunning && (NULL != ctx.idleThread))
         {
            ctx.idleVideoRunning = false;
            joinThread(ctx.idleThread);
            ctx.idleThread = NULL;
         }

         ctx.streamingRunning = true;
         ctx.streamingThread = CreateThread(NULL, 0, &streamVideoWithTimeStamp, (void*)&ctx, 0, 0);
         if(NULL == ctx.streamingThread)
         {
            ctx.streamingRunning = false;
            fatalError("Can't create streaming thread");
            return 0;
         }
       }
      break;

      case WM_CREATE:
      {
         ctx.idleVideoRunning = true;
         ctx.idleThread = CreateThread(NULL, 0, &playIdleVideo, (void*)ctx.idleAnimation, 0, 0);
         if(NULL == ctx.idleThread)
         {
            ctx.idleVideoRunning = false;
            fatalError("Can't create idle thread");
            return 0;
         }
      }
      break;

      case WM_CLOSE:
      {
         if(NULL != ctx.idleThread)
         {
            TerminateThread(ctx.idleThread, 0);
            ctx.idleThread = NULL;
         }
         if(NULL != ctx.streamingThread)
         {
            TerminateThread(ctx.streamingThread, 0);
            ctx.streamingThread = NULL;
         }
         if(NULL != ctx.commandServerThread)
         {
            TerminateThread(ctx.commandServerThread, 0);
            ctx.commandServerThread = NULL;
         }
         DestroyWindow(hwnd);
      }
      break;

      case WM_KEYDOWN:
      switch (wParam)
      {
         case VK_ESCAPE:
            ctx.commandServer.sendStopCmd();
            PostQuitMessage(0);
            return 0;

         case VK_SPACE:
            if(ctx.streamingRunning && (NULL != ctx.streamingThread))
            {
               ctx.streamingRunning = false;
               CloseHandle(ctx.streamingThread);
               ctx.streamingThread = NULL;
            }
            if(!ctx.idleVideoRunning)
            {
               ctx.idleVideoRunning = true;
               ctx.idleThread = CreateThread(NULL, 0, &playIdleVideo, (void*)ctx.idleAnimation, 0, 0);
               if(NULL == ctx.idleThread)
               {
                  ctx.idleVideoRunning = false;
                  fatalError("Can't create idle thread");
                  return 0;
               }
            }
            return 0;
      }
      break;

      case WM_DESTROY:
      {
         ctx.commandServer.sendStopCmd();
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
   ctx.idleVideoRunning = false;
   ctx.streamingRunning = false;
   ctx.idleThread = NULL;
   ctx.streamingThread = NULL;
   ctx.commandServerThread = NULL;

   int argCount = sscanf(lpCmdLine,"%s %s %s %s", ctx.videoStreamFile, ctx.timeStampFile, ctx.timeServerPort, ctx.idleAnimation);
   if(4 != argCount)
   {
      fatalError("Specify the cmd line as:'<videoFilePath> <timeStampFile> <timeStampPort> <idleAnimation>'");
      return 0;
   }

   int err = ctx.commandServer.init(ctx.timeServerPort);
   if(0 != err)
   {
      fatalError("Failed to init TCP server on port %s", ctx.timeServerPort);
      return 0;
   }

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
   wc.lpszClassName = ctx.videoStreamFile;
   wc.lpszMenuName = NULL;
   wc.style = 0;

   if(!RegisterClassEx(&wc))
   {
      fatalError("Failed to register window class.");
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
                             ,ctx.videoStreamFile
                             ,ctx.videoStreamFile
                             ,WS_POPUP | WS_VISIBLE | WS_CAPTION |WS_MAXIMIZEBOX
                             ,mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left
                             ,mi.rcMonitor.bottom - mi.rcMonitor.top
                             ,NULL, NULL, hInstance, NULL );

   ctx.commandServerThread = CreateThread(NULL, 0, &commandServerThread, (void*)&ctx, 0, 0);
   if(NULL == ctx.commandServerThread)
   {
      fatalError("Can't create command server thread");
      return 0;
   }

   MSG msg;
   while ( GetMessage(&msg, 0, 0, 0) > 0 )
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
   return 0;
}
