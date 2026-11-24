find_package(OpenSSL REQUIRED)
find_package(LibLZMA REQUIRED)

add_library(xdelta3_memory STATIC ../third_party/xdelta3/xdelta3.c)
target_include_directories(xdelta3_memory PUBLIC ../third_party/xdelta3)
target_compile_definitions(xdelta3_memory
    PUBLIC HAVE_CONFIG_H=1
    PRIVATE XD3_MAIN=0
)
target_link_libraries(xdelta3_memory PUBLIC LibLZMA::LibLZMA)

add_library(finesse_core
    common/logger.cpp
    common/timer.cpp
    io/mapped_input.cpp
    dedup/exact_chunk_index.cpp
    rolling_hash/rabin_hash.cpp
    chunking/rabin_chunker.cpp
    feature_extraction/ntransform_extractor.cpp
    feature_extraction/finesse_extractor.cpp
    super_feature/finesse_sf_generator.cpp
    super_feature/ntransform_sf_generator.cpp
    similarity/sf_index.cpp
    delta/xdelta_encoder.cpp
    delta/delta_io.cpp
    delta/xdelta3_memory_adapter.c
    cache/lru_base_chunk_cache.cpp
    storage/base_chunk_store.cpp
    metrics/metric_collector.cpp
    pipeline/resemblance_pipeline.cpp
)
target_include_directories(finesse_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(finesse_core PUBLIC OpenSSL::Crypto xdelta3_memory)
target_compile_options(finesse_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(run_chunk_only ../experiments/run_chunk_only.cpp)
target_link_libraries(run_chunk_only PRIVATE finesse_core)
target_compile_options(run_chunk_only PRIVATE -Wall -Wextra -Wpedantic)

add_executable(run_feature_check ../experiments/run_feature_check.cpp)
target_link_libraries(run_feature_check PRIVATE finesse_core)
target_compile_options(run_feature_check PRIVATE -Wall -Wextra -Wpedantic)

add_executable(run_sf_check ../experiments/run_sf_check.cpp)
target_link_libraries(run_sf_check PRIVATE finesse_core)
target_compile_options(run_sf_check PRIVATE -Wall -Wextra -Wpedantic)

add_executable(run_finesse ../experiments/run_pipeline.cpp)
target_link_libraries(run_finesse PRIVATE finesse_core)
target_compile_options(run_finesse PRIVATE -Wall -Wextra -Wpedantic)

add_executable(run_ntransform ../experiments/run_pipeline.cpp)
target_link_libraries(run_ntransform PRIVATE finesse_core)
target_compile_options(run_ntransform PRIVATE -Wall -Wextra -Wpedantic)

add_executable(stage1_tests ../tests/stage1_tests.cpp)
target_link_libraries(stage1_tests PRIVATE finesse_core)
target_compile_options(stage1_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME stage1_tests COMMAND stage1_tests)

add_executable(xdelta3_memory_tests ../tests/xdelta3_memory_tests.cpp)
target_link_libraries(xdelta3_memory_tests PRIVATE finesse_core)
target_compile_options(xdelta3_memory_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME xdelta3_memory_tests COMMAND xdelta3_memory_tests)

add_executable(delta_io_tests ../tests/delta_io_tests.cpp)
target_link_libraries(delta_io_tests PRIVATE finesse_core)
target_compile_options(delta_io_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME delta_io_tests COMMAND delta_io_tests)

add_library(finesse_destor_bridge ../integration/destor_bridge.cpp)
target_include_directories(finesse_destor_bridge
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../integration
)
target_link_libraries(finesse_destor_bridge PUBLIC finesse_core)
target_compile_options(finesse_destor_bridge PRIVATE -Wall -Wextra -Wpedantic)

add_executable(destor_bridge_smoke ../integration/destor_bridge_smoke.c)
target_link_libraries(destor_bridge_smoke PRIVATE finesse_destor_bridge)
set_target_properties(destor_bridge_smoke PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME destor_bridge_smoke COMMAND destor_bridge_smoke)
