#ifndef _TIMESTAMPCLIENT_HPP
#define _TIMESTAMPCLIENT_HPP

#include <stdint.h>

#include <ws2tcpip.h>
#include <windows.h>

class TimeStampClient
{
public:
   int init(const char* addr, const int port);
   void deinit();
   int sendTimeStamp(const uint32_t timestamp);
   int sendStartCmd();
   int sendStopCmd();

private:
   WSADATA wsaData;
   SOCKET sClient;
   SOCKADDR_IN sinClient;
};

#endif // _TIMESTAMPCLIENT_HPP
