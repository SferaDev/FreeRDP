set(MODULE_NAME "TestSDL3Client")
set(MODULE_PREFIX "TEST_SDL3_CLIENT")

set(${MODULE_PREFIX}_DRIVER ${MODULE_NAME}.cpp)

set(${MODULE_PREFIX}_TESTS TestSDLClipMacos.mm)

disable_warnings_for_directory(${CMAKE_CURRENT_BINARY_DIR})

create_test_sourcelist(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_DRIVER} ${${MODULE_PREFIX}_TESTS})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS} ../sdl_clip_macos.mm)
target_include_directories(${MODULE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_source_files_properties(TestSDLClipMacos.mm ../sdl_clip_macos.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_link_libraries(
  ${MODULE_NAME} PRIVATE ${APPKIT_LIBRARY} ${COREGRAPHICS_LIBRARY} ${IMAGEIO_LIBRARY} ${UTI_LIBRARY}
)

set_target_properties(${MODULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TESTING_OUTPUT_DIRECTORY}")

foreach(test ${${MODULE_PREFIX}_TESTS})
  get_filename_component(TestName ${test} NAME_WE)
  add_test(${TestName} ${TESTING_OUTPUT_DIRECTORY}/${MODULE_NAME} ${TestName})
endforeach()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "FreeRDP/Client/Test")
