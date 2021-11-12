cmake_minimum_required(VERSION 3.5.1)
project(BluePrintSDK)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_MACOSX_RPATH 0)
if (POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()

option(IMGUI_BP_SDK_STATIC                "Build BluePrint as static library" OFF)

set(IMGUI_BP_SDK_SRC
    src/BluePrint.cpp
    src/Context.cpp
    src/Pin.cpp
    src/Node.cpp
    src/Icon.cpp
    src/Debug.cpp
    src/Utils.cpp
    src/Document.cpp
    src/UI.cpp
)

set(IMGUI_BP_SDK_INC
    include/BluePrint.h
    include/Pin.h
    include/Node.h
    include/Icon.h
    include/Debug.h
    include/Utils.h
    include/Document.h
    include/UI.h
    include/variant.hpp
    include/span.hpp
)

set(IMGUI_BP_SDK_INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/
)

# Generate all buildin node files into one header file
message(STATUS "Generate Build In Nodes")
file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/BuildInNodes.h)
list(APPEND incls "#pragma once\n")
list(APPEND incls "// Genetated By cmake, don't change it\n")
file(GLOB_RECURSE header_files "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes/*.h")
foreach(f ${header_files})
    file(RELATIVE_PATH fr "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInNodes" ${f})
    list(APPEND incls "#include \"${fr}\"\n")
endforeach()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildInNodes.h ${incls})

include_directories(${IMGUI_BP_SDK_INC_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

if(IMGUI_BP_SDK_STATIC)
set(LIBRARY STATIC)
else(IMGUI_BP_SDK_STATIC)
set(LIBRARY SHARED)
endif(IMGUI_BP_SDK_STATIC)

add_library(
    BluePrintSDK
    ${LIBRARY}
    ${IMGUI_BP_SDK_SRC}
    ${IMGUI_BP_NODE_LIST}
    ${IMGUI_BP_SDK_INC}
)
set_property(TARGET BluePrintSDK PROPERTY POSITION_INDEPENDENT_CODE ON)

if(NOT IMGUI_BP_SDK_STATIC)
target_link_libraries(BluePrintSDK imgui)
endif(NOT IMGUI_BP_SDK_STATIC)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(IMGUI_BLUEPRINT_SDK_LIBRARYS BluePrintSDK PARENT_SCOPE )
    set(IMGUI_BLUEPRINT_INCLUDES ${IMGUI_BP_SDK_INC} PARENT_SCOPE )
    set(IMGUI_BLUEPRINT_INCLUDE_DIRS ${IMGUI_BP_SDK_INC_DIRS} PARENT_SCOPE )
endif()