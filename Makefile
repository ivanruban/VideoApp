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

all: videoserver timeserver

videoserver : videoserver.cpp H264_Decoder.cpp TimeStampClient.cpp
	$(GCC) -o videoserver.exe videoserver.cpp H264_Decoder.cpp TimeStampClient.cpp $(FLAGS) $(INCLUDE) $(LIBRARY) $(LIBAV) $(WINLIBRARY)

timeserver : timeserver.cpp
	$(GCC) -o timeserver.exe timeserver.cpp $(FLAGS) $(INCLUDE) $(LIBRARY) 

clean:
	rm -r *.o
