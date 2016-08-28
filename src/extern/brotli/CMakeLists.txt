PROJECT(brotli)

# version 0.5.2

SET(BROTLI_ENC
  enc/backward_references.c
  enc/bit_cost.c
  enc/block_splitter.c
  enc/brotli_bit_stream.c
  enc/cluster.c
  enc/compress_fragment.c
  enc/compress_fragment_two_pass.c
  enc/encode.c
  enc/entropy_encode.c
  enc/histogram.c
  enc/literal_cost.c
  enc/memory.c
  enc/metablock.c
  enc/static_dict.c
  enc/utf8_util.c
)

SET(BROTLI_DEC
    dec/bit_reader.c
	dec/decode.c
	dec/huffman.c
	dec/state.c
)

IF(MSVC)
	ADD_DEFINITIONS(/wd4334 /wd4146)
ENDIF()

ADD_LIBRARY(brotli STATIC ${BROTLI_ENC} ${BROTLI_DEC} common/dictionary.c)
TARGET_INCLUDE_DIRECTORIES(brotli
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/enc
        ${CMAKE_CURRENT_SOURCE_DIR}/dec)
