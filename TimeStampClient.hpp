#ifndef _TIMESTAMPCLIENT_HPP
#define _TIMESTAMPCLIENT_HPP

#include <stdint.h>

#include <ws2tcpip.h>
#include <windows.h>

typedef enum
{
  eStart = 0,
  eStop =  1,
  eTimeStamp = 2,
} eCommand;

typedef struct
{
  uint32_t command; //eCommand
  uint32_t timestamp; // timestamp in ms from start of video
} TCommStruct;


class TimeStampClient
{
public:
   int init(const char* addr, const int port);
   void deinit();
   int sendTimeStamp(const uint32_t timestamp);
   int sendStartCmd();
   int sendStopCmd();

   int receiveCmd(TCommStruct *msg);

private:
   WSADATA wsaData;
   SOCKET sClient;
   SOCKADDR_IN sinClient;
};

#endif // _TIMESTAMPCLIENT_HPP
