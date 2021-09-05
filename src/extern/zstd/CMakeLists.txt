PROJECT(zstd VERSION 1.5.0)

SET(SOURCES
	common/debug.c
	common/entropy_common.c
	common/error_private.c
	common/fse_decompress.c
	common/pool.c
	common/threading.c
	common/xxhash.c
	common/zstd_common.c
	compress/fse_compress.c
	compress/hist.c
	compress/huf_compress.c
	compress/zstdmt_compress.c
	compress/zstd_compress.c
	compress/zstd_compress_literals.c
	compress/zstd_compress_sequences.c
	compress/zstd_compress_superblock.c
	compress/zstd_double_fast.c
	compress/zstd_fast.c
	compress/zstd_lazy.c
	compress/zstd_ldm.c
	compress/zstd_opt.c
	decompress/huf_decompress.c
	decompress/zstd_ddict.c
	decompress/zstd_decompress.c
	decompress/zstd_decompress_block.c
	dictBuilder/cover.c
	dictBuilder/divsufsort.c
	dictBuilder/fastcover.c
	dictBuilder/zdict.c)

SET(HEADERS
	zdict.h
	zstd.h
	zstd_errors.h
	common/bitstream.h
	common/compiler.h
	common/cpu.h
	common/debug.h
	common/error_private.h
	common/fse.h
	common/huf.h
	common/mem.h
	common/pool.h
	common/threading.h
	common/xxhash.h
	common/zstd_deps.h
	common/zstd_internal.h
	common/zstd_trace.h
	compress/hist.h
	compress/zstdmt_compress.h
	compress/zstd_compress_internal.h
	compress/zstd_compress_literals.h
	compress/zstd_compress_sequences.h
	compress/zstd_compress_superblock.h
	compress/zstd_cwksp.h
	compress/zstd_double_fast.h
	compress/zstd_fast.h
	compress/zstd_lazy.h
	compress/zstd_ldm.h
	compress/zstd_ldm_geartab.h
	compress/zstd_opt.h
	decompress/zstd_ddict.h
	decompress/zstd_decompress_block.h
	decompress/zstd_decompress_internal.h
	dictBuilder/cover.h
	dictBuilder/divsufsort.h)

ADD_LIBRARY(zstd STATIC ${SOURCES} ${HEADERS})
TARGET_INCLUDE_DIRECTORIES(zstd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE common compress decompress dictBuilder)

IF(MSVC)
    TARGET_COMPILE_OPTIONS(zstd PRIVATE /wd4996 /wd4701 /wd4703 /wd4244)
ENDIF()