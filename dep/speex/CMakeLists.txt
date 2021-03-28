cmake_minimum_required(VERSION 3.3)

if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()
cmake_policy(SET CMP0063 NEW)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(Speex C)

add_library(${PROJECT_NAME} STATIC
	src/bits.c
	src/cb_search.c
	src/exc_10_16_table.c
	src/exc_10_32_table.c
	src/exc_20_32_table.c
	src/exc_5_256_table.c
	src/exc_5_64_table.c
	src/exc_8_128_table.c
	src/filters.c
	src/gain_table.c
	src/gain_table_lbr.c
	src/hexc_10_32_table.c
	src/hexc_table.c
	src/high_lsp_tables.c
	src/lpc.c
	src/lsp.c
	src/lsp_tables_nb.c
	src/ltp.c
	src/math_approx.c
	src/misc.c
	src/modes.c
	src/nb_celp.c
	src/quant_lsp.c
	src/sb_celp.c
	src/speex_callbacks.c
	src/speex_header.c
	src/stereo.c
	src/vbr.c
	src/vq.c
)

target_include_directories(${PROJECT_NAME} PUBLIC include)