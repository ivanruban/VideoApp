#define _WIN32_WINNT  _WIN32_WINNT_WINXP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <time.h>

#include "TimeStampClient.hpp"

int main(int argc, const char *argv[])
{
   TimeStampClient client;

   if(argc < 2)
   {
      printf("Use: %s <address> <port>\n", argv[0]);
      return 0;
   }
   const char *address = argv[1];
   int port = atoi(argv[2]);//agreed 23456

   int err = client.init(address, port);
   if(0 != err)
   {
      printf("Can't connect to '%s:%i'\n", address, port);
      return 0;
   }

   client.sendStartCmd();

   TCommStruct msg;
   while(1)
   {
      err = client.receiveCmd(&msg);
      if(0 == err)
      {
         printf("cmd: %u data: %u\n", msg.command, msg.timestamp);
         if(eStop == msg.command)
         {
            break;
         }
      }
      else
      {
         printf("receiveCmd() failed(%i)!\n", err);
         break;
      }
   }

   client.deinit();
   return 0;
}
