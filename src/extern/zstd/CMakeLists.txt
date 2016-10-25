SET(HEADERS
    zstd.h
    common/bitstream.h
    common/error_private.h
    common/error_public.h
    common/fse.h
    common/huf.h
    common/mem.h
    common/xxhash.h
    common/zbuff.h
    common/zstd_internal.h
    compress/zstd_opt.h
    dictBuilder/divsufsort.h
    dictBuilder/zdict.h)

SET(SOURCES
    common/entropy_common.c
    common/fse_decompress.c
    common/xxhash.c
    common/zstd_common.c
    compress/fse_compress.c
    compress/huf_compress.c
    compress/zbuff_compress.c
    compress/zstd_compress.c
    decompress/huf_decompress.c
    decompress/zbuff_decompress.c
    decompress/zstd_decompress.c
    dictBuilder/divsufsort.c
    dictBuilder/zdict.c)

ADD_LIBRARY(zstd STATIC ${SOURCES} ${HEADERS})

TARGET_INCLUDE_DIRECTORIES(zstd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        common)
SET_PROPERTY (TARGET zstd PROPERTY POSITION_INDEPENDENT_CODE TRUE)
