cmake_minimum_required(VERSION 3.14)

project(unpacker_stages_core VERSION 1.0 LANGUAGES CXX)

# Set the project namespace; overrideable by parent scope if desired
if(NOT DEFINED PROJECT_NAMESPACE)
  set(PROJECT_NAMESPACE analysis_pipeline)
endif()

# ----------------------- Compiler Settings ------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Suppress false-positive GCC warnings when this is the top-level project
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
  endif()
endif()

# ----------------------- Includes and Utilities -------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ----------------------- CPM (Dependency Management) --------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

# ----------------------- Required Packages ------------------------
find_package(ROOT REQUIRED COMPONENTS Core RIO)
find_package(analysis_pipeline_core REQUIRED)
find_package(unpacker_data_products_core REQUIRED)
include(${ROOT_USE_FILE})

# ----------------------- Source Files -----------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# ----------------------- Library Target ---------------------------
add_library(${PROJECT_NAME} SHARED ${SOURCES})

add_library(${PROJECT_NAMESPACE}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    analysis_pipeline::spdlog_header_only
    analysis_pipeline::nlohmann_json_header_only
    analysis_pipeline::analysis_pipeline_core
    analysis_pipeline::unpacker_data_products_core
    ROOT::Core
    ROOT::RIO
)

# ----------------------- ROOT Dictionary Helper --------------------
function(append_target_includes_to_root_dict target_name)
  get_target_property(INCLUDE_DIRS ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
  if(INCLUDE_DIRS AND NOT INCLUDE_DIRS STREQUAL "${target_name}_NOTFOUND")
    foreach(dir IN LISTS INCLUDE_DIRS)
      if(dir MATCHES "^\\$<BUILD_INTERFACE:(.+)>$")
        set(actual "${CMAKE_MATCH_1}")
        if(EXISTS "${actual}")
          list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${actual}")
        endif()
      elseif(NOT dir MATCHES "^\\$")
        if(EXISTS "${dir}")
          list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${dir}")
        endif()
      endif()
    endforeach()
  endif()
  set(ROOT_DICTIONARY_INCLUDE_OPTIONS ${ROOT_DICTIONARY_INCLUDE_OPTIONS} PARENT_SCOPE)
endfunction()

# ----------------------- Detect Plugin Root Directory --------------
# Detect plugin root directory name (relative to include/analysis_pipeline)
file(GLOB PLUGIN_ROOT_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline
  ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/*
)

set(PLUGIN_ROOTS "")
foreach(dir IN LISTS PLUGIN_ROOT_DIRS)
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/${dir})
    list(APPEND PLUGIN_ROOTS ${dir})
  endif()
endforeach()

list(LENGTH PLUGIN_ROOTS plugin_root_count)
if(plugin_root_count EQUAL 1)
  list(GET PLUGIN_ROOTS 0 PLUGIN_ROOT_DIR)
  message(STATUS "Detected plugin root directory: ${PLUGIN_ROOT_DIR}")
else()
  message(FATAL_ERROR
    "Expected exactly one plugin root directory under include/analysis_pipeline/, "
    "found ${plugin_root_count}: ${PLUGIN_ROOTS}. Please fix directory structure or specify plugin root explicitly."
  )
endif()

# Define full plugin root path for convenience
set(PLUGIN_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/${PLUGIN_ROOT_DIR}")

# ----------------------- ROOT Dictionary Generation ----------------
set(ROOT_LINKDEF ${PLUGIN_ROOT_PATH}/LinkDef.h)
set(ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/include")

append_target_includes_to_root_dict(analysis_pipeline::nlohmann_json_header_only)
append_target_includes_to_root_dict(analysis_pipeline::analysis_pipeline_core)
append_target_includes_to_root_dict(analysis_pipeline::unpacker_data_products_core)

file(GLOB_RECURSE STAGE_HEADERS CONFIGURE_DEPENDS
  ${PLUGIN_ROOT_PATH}/stages/*.h
)
file(GLOB_RECURSE DATAPRODUCT_HEADERS CONFIGURE_DEPENDS
  ${PLUGIN_ROOT_PATH}/data_products/*.h
)
file(GLOB_RECURSE UTILS_HEADERS CONFIGURE_DEPENDS
  ${PLUGIN_ROOT_PATH}/utils/*.h
)

set(ALL_DICT_HEADERS ${STAGE_HEADERS} ${DATAPRODUCT_HEADERS} ${UTILS_HEADERS})
list(FILTER ALL_DICT_HEADERS EXCLUDE REGEX "LinkDef\\.h$")

ROOT_GENERATE_DICTIONARY(G__${PROJECT_NAME}
  ${ALL_DICT_HEADERS}
  LINKDEF ${ROOT_LINKDEF}
  OPTIONS ${ROOT_DICTIONARY_INCLUDE_OPTIONS} -Wno-inconsistent-missing-override
)

# Add generated dictionary source file to the target sources
target_sources(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/G__${PROJECT_NAME}.cxx
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}  # For generated ROOT headers
)

# ----------------------- Install Rules (Top-level only) -----------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.rootmap
    ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}_rdict.pcm
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    OPTIONAL
  )

  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAMESPACE}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
endif()
