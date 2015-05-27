#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "TimeStampProvider.hpp"

int TimeStampProvider::init(const char* fname)
{
   FILE *file = fopen(fname, "r");
   if(NULL == file)
      return errno;

   char buf[256];
   while (fgets(buf, sizeof(buf), file) != NULL)
   {
      timeStamps.push_back(atoi(buf)/1000);//convert to ms
   }
   fclose(file);
   return 0;
}

void TimeStampProvider::deinit()
{
   timeStamps.clear();
}


int TimeStampProvider::getFrameTimeStamp(const uint32_t frameNumber)
{
   if(frameNumber < timeStamps.size())
      return timeStamps[frameNumber];

   return -1;
}
