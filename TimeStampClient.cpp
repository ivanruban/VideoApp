#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "TimeStampClient.hpp"

int TimeStampClient::init(const char* addr, const int port)
{
   int err = WSAStartup(MAKEWORD(2,2), &wsaData);
   if (err != 0)
   {
      fprintf(stderr, "WSAStartup failed with error: %d\n", err);
      return EIO;
   }

   sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if(sClient == INVALID_SOCKET)
   {
      fprintf(stderr,"socket() failed\n");
      WSACleanup();
      return EIO;
   }

   memset(&sinClient, 0, sizeof(sinClient));
   sinClient.sin_family = AF_INET;
   sinClient.sin_addr.s_addr = inet_addr(addr);
   sinClient.sin_port = htons(port);

   if(connect(sClient, (LPSOCKADDR)&sinClient, sizeof(sinClient)) == SOCKET_ERROR)
   {
      WSACleanup();
      return EIO;
   }

   return 0;
}


void TimeStampClient::deinit()
{
   closesocket(sClient);
   WSACleanup();
}

int TimeStampClient::receiveCmd(TCommStruct *msg)
{
   int res;

   res = recv(sClient, (char*)msg, sizeof(TCommStruct), 0);
   if (res == sizeof(TCommStruct))
   {
      return 0;
   }

   return EIO;
}


int TimeStampClient::sendStartCmd()
{
   int err;
   TCommStruct msg = {(uint32_t)eStart, 0};

   err = send(sClient, (const char*)&msg, sizeof(msg), 0);
   if(err == SOCKET_ERROR)
   {
      return EIO;
   }

   return 0;
}

int TimeStampClient::sendStopCmd()
{
   int err;
   TCommStruct msg = {(uint32_t)eStop, 0};

   err = send(sClient, (const char*)&msg, sizeof(msg), 0);
   if(err == SOCKET_ERROR)
   {
      return EIO;
   }

   return 0;
}

int TimeStampClient::sendTimeStamp(const uint32_t timestamp)
{
   int err;
   TCommStruct msg = {(uint32_t)eTimeStamp, timestamp};

   err = send(sClient, (const char*)&msg, sizeof(msg), 0);
   if(err == SOCKET_ERROR)
   {
      return EIO;
   }

   return 0;
}
