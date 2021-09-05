PROJECT(zlib VERSION 1.2.8)

SET(ZLIB_SOURCES
	adler32.c	compress.c
	crc32.c    	deflate.c
	gzclose.c   gzlib.c
	gzread.c    gzwrite.c
	inflate.c   infback.c
	inftrees.c  inffast.c
	trees.c     uncompr.c
	zutil.c)

SET(ZLIB_HEADERS
	crc32.h		deflate.h
	gzguts.h	inffast.h
	inffixed.h	inflate.h
	inftrees.h	trees.h
	zconf.h		zlib.h
	zutil.h)

ADD_LIBRARY(zlib STATIC ${ZLIB_SOURCES} ${ZLIB_HEADERS})
TARGET_INCLUDE_DIRECTORIES(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

IF(MSVC)
	target_compile_options(zlib PRIVATE /wd4267 /wd4131 /wd4244 /wd4127 /wd4996)
	target_compile_definitions(zlib PRIVATE -D_CRT_SECURE_NO_WARNINGS)
ENDIF()

INCLUDE(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(vsnprintf NIV_HAS_VSNPRINTF)

IF(NIV_HAS_VSNPRINTF)
	target_compile_definitions(zlib PRIVATE HAVE_VSNPRINTF)
ENDIF()

# Disable all trace output
target_compile_definitions(zlib PRIVATE -Dverbose=-1)