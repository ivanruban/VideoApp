#ifndef _TIMESTAMPPROVIDER_HPP
#define _TIMESTAMPPROVIDER_HPP

#include <stdint.h>
#include <stdio.h>
#include <vector>

class TimeStampProvider
{
public:
   int init(const char* fname);
   void deinit();
   int getFrameTimeStamp(const uint32_t frameNumber);

private:
   std::vector<int> timeStamps;
};

#endif // _TIMESTAMPPROVIDER_HPP
