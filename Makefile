MINGW_ROOT = C:/MinGW
GCC   = "$(MINGW_ROOT)/bin/gcc"

INCLUDE = -I"$(MINGW_ROOT)/include" -I"."  -I"import/include"
LIBRARY = -L"$(MINGW_ROOT)/lib" -L"import/lib" -lws2_32 -lstdc++

WINLIBRARY = -mwindows

LIBAV = import/lib/libavcodec.dll.a\
        import/lib/libavformat.dll.a\
        import/lib/libavutil.dll.a\
        import/lib/libswscale.dll.a

FLAGS = -Wall -Os -static-libgcc -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS 

all: videoserver testclient

videoserver : videoserver.cpp H264_Decoder.cpp TimeStampProvider.cpp CommandServer.cpp
	$(GCC) -o videoserver.exe videoserver.cpp H264_Decoder.cpp TimeStampProvider.cpp CommandServer.cpp $(FLAGS) $(INCLUDE) $(LIBRARY) $(LIBAV) $(WINLIBRARY)

testclient : testclient.cpp
	$(GCC) -o testclient.exe testclient.cpp TimeStampClient.cpp $(FLAGS) $(INCLUDE) $(LIBRARY) 

clean:
	rm -r *.o
