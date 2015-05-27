#ifndef _COMMANDSERVER_HPP
#define _COMMANDSERVER_HPP

#define _WIN32_WINNT  _WIN32_WINNT_WINXP

#include <stdint.h>
#include <ws2tcpip.h>
#include <windows.h>

class CommandServer
{
public:
   int init(const char *port);
   void deinit();
   int waitForStartCommand();

   int sendStartCmd();
   int sendTimeStamp(const uint32_t timestamp);
   int sendStopCmd();

private:
   WSADATA wsaData;
   SOCKET ListenSocket;
   SOCKET ClientSocket;
};

#endif // _COMMANDSERVER_HPP
