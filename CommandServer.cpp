#include "CommandServer.hpp"

#include "stdio.h"
#include "errno.h"

typedef enum
{
  eStart = 0,
  eStop =  1,
  eTimeStamp = 2,
} eCommand;

typedef struct
{
  uint32_t command; //eCommand
  uint32_t timestamp; // timestamp in ms
} TCommStruct;

int CommandServer::waitForStartCommand()
{
   int res;
   TCommStruct msg;

   ClientSocket = accept(ListenSocket, NULL, NULL);
   if (ClientSocket == INVALID_SOCKET)
   {
      fprintf(stderr,"accept failed with error: %d\n", WSAGetLastError());
      return EIO;
   }

   do
   {
      res = recv(ClientSocket, (char*)&msg, sizeof(msg), 0);
      if (res == sizeof(msg))
      {
         if(eStart == msg.command)
         {
            int err = sendStartCmd();
            if(0 == err)
            {
               return 0;
            }
         }
      }
      else if (res == 0)
         return EIO;
      else
      {
         return EIO;
      }
   } while(res > 0);

   return EIO;
}


int CommandServer::init(const char *port)
{
   int iResult;

   struct addrinfo *result = NULL;
   struct addrinfo hints;

   ClientSocket = INVALID_SOCKET;

   iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
   if (iResult != 0)
   {
      fprintf(stderr,"WSAStartup failed with error: %d\n", iResult);
      return EIO;
   }

   ZeroMemory(&hints, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = AI_PASSIVE;

   iResult = getaddrinfo(NULL, port, &hints, &result);
   if ( iResult != 0 )
   {
      fprintf(stderr,"getaddrinfo failed with error: %d\n", iResult);
      WSACleanup();
      return EIO;
   }

   ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
   if (ListenSocket == INVALID_SOCKET)
   {
      fprintf(stderr,"socket failed with error: %i\n", WSAGetLastError());
      freeaddrinfo(result);
      WSACleanup();
      return EIO;
   }

   iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
   if (iResult == SOCKET_ERROR)
   {
      fprintf(stderr,"bind failed with error: %d\n", WSAGetLastError());
      freeaddrinfo(result);
      closesocket(ListenSocket);
      WSACleanup();
      return EIO;
   }

   freeaddrinfo(result);

   iResult = listen(ListenSocket, SOMAXCONN);
   if (iResult == SOCKET_ERROR)
   {
      fprintf(stderr,"listen failed with error: %d\n", WSAGetLastError());
      closesocket(ListenSocket);
      WSACleanup();
      return EIO;
   }

   return 0;
}


void CommandServer::deinit()
{
   closesocket(ListenSocket);
   closesocket(ClientSocket);
   WSACleanup();
}

int CommandServer::sendStartCmd()
{
   int err;
   TCommStruct msg = {(uint32_t)eStart, 0};

   err = send(ClientSocket, (const char*)&msg, sizeof(msg), 0);
   if(err == SOCKET_ERROR)
   {
      return EIO;
   }

   return 0;
}


int CommandServer::sendStopCmd()
{
   int err;
   TCommStruct msg = {(uint32_t)eStop, 0};

   err = send(ClientSocket, (const char*)&msg, sizeof(msg), 0);
   if(err == SOCKET_ERROR)
   {
      return EIO;
   }

   return 0;
}


int CommandServer::sendTimeStamp(const uint32_t timestamp)
{
   int err;
   TCommStruct msg = {(uint32_t)eTimeStamp, timestamp};

   err = send(ClientSocket, (const char*)&msg, sizeof(msg), 0);
   if(err == SOCKET_ERROR)
   {
      return EIO;
   }

   return 0;
}

