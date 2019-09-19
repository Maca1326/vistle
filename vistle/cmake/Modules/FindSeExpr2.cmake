include(FindPackageHandleStandardArgs)

option(SeExpr2_USE_LLVM "Use LLVM backend for SeExpr" ON)

set(LLVM_LIB "")
if (SeExpr2_USE_LLVM)
    #set(LLVM_DIR /usr/share/llvm/cmake CACHE PATH "Where to search for LLVM i.e. ")

    find_package(LLVM CONFIG NAMES LLVM
        CONFIGS LLVM-Config.cmake LLVMConfig.cmake
        HINTS /usr/local/opt/llvm)
    if (LLVM_FOUND)
        message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}")
        find_program(LLVM_CONFIG_EXECUTABLE NAMES ${LLVM_TOOLS_BINARY_DIR}/llvm-config)

        # Uncomment to use clang++
        #set(CMAKE_CXX_COMPILER clang++)
        #set(CMAKE_C_COMPILER clang)

        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
        include(LLVM-Config)
        include(LLVMConfig OPTIONAL)
        #cmake_policy(SET CMP0056 NEW)
        #include(HandleLLVMOptions)

        message(STATUS "LLVM_DEFINITIONS =" ${LLVM_DEFINITIONS})
        #add_definitions(${LLVM_DEFINITIONS})

        if (NOT LLVM_CONFIG_EXECUTABLE STREQUAL "LLVM_CONFIG_EXECUTABLE-NOTFOUND")
            execute_process(
                COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
                OUTPUT_VARIABLE LLVM_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        else ()
            set(LLVM_INCLUDE_DIR ${LLVM_INCLUDE_DIRS})
        endif ()
        message(STATUS "LLVM_INCLUDE_DIR =" ${LLVM_INCLUDE_DIR})
        #include_directories(${LLVM_INCLUDE_DIR})

        if (NOT LLVM_CONFIG_EXECUTABLE STREQUAL "LLVM_CONFIG_EXECUTABLE-NOTFOUND")
            execute_process(
                COMMAND ${LLVM_CONFIG_EXECUTABLE} --libdir
                OUTPUT_VARIABLE LLVM_LIBRARY_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
        else ()
            set(LLVM_LIBRARY_DIR ${LLVM_LIBRARY_DIRS})
        endif ()
        message(STATUS "LLVM_LIBRARY_DIR =" ${LLVM_LIBRARY_DIR})
        #link_directories(${LLVM_LIBRARY_DIR})

        #todo infinite loop in this?
        #llvm_map_components_to_libraries(REQ_LLVM_LIBRARIES jit native)

        # construct library name
        if (NOT LLVM_CONFIG_EXECUTABLE STREQUAL "LLVM_CONFIG_EXECUTABLE-NOTFOUND")
            execute_process(
                COMMAND ${LLVM_CONFIG_EXECUTABLE} --version
                OUTPUT_VARIABLE LLVM_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
        else ()
            set(LLVM_VERSION ${LLVM_PACKAGE_VERSION})
        endif ()
        message(STATUS "LLVM_VERSION = ${LLVM_VERSION}")

        set(LLVM_LIB LLVM)
        message(STATUS "LLVM_LIB = ${LLVM_LIB}")
    else (LLVM_FOUND)
        message(STATUS "Not building with SeExpr2, LLVM not found but requested")
    endif (LLVM_FOUND)
endif()

find_path(SeExpr2_INCLUDE_DIR NAMES SeExpr2/SeContext.h)
if (WIN32)
    find_library(SeExpr2_LIBRARY_DEBUG NAMES seexpr2d)
    find_library(SeExpr2_LIBRARY_RELEASE NAMES seexpr2)
    mark_as_advanced(SeExpr2_LIBRARY_DEBUG SeExpr2_LIBRARY_RELEASE)
    find_package_handle_standard_args(SeExpr2 DEFAULT_MSG SeExpr2_LIBRARY_RELEASE SeExpr2_LIBRARY_DEBUG SeExpr2_INCLUDE_DIR)
    if (SeExpr2_FOUND)
        set(SeExpr2_LIBRARY optimized ${SeExpr2_LIBRARY_RELEASE} debug ${SeExpr2_LIBRARY_DEBUG})
    else()
        message(STATUS "Could not find the debug AND release version of SeExpr2")
        set(SeExpr2_LIBRARY NOTFOUND)
    endif()
else (WIN32)
    find_library (SeExpr2_LIBRARY NAMES SeExpr2 seexpr2)
    find_package_handle_standard_args(SeExpr2 DEFAULT_MSG SeExpr2_LIBRARY SeExpr2_INCLUDE_DIR)
endif (WIN32)

if (SeExpr2_FOUND)
    if (SeExpr2_USE_LLVM)
        if (LLVM_FOUND)
            set (SeExpr2_INCLUDE_DIRS ${SeExpr2_INCLUDE_DIR} ${LLVM_INCLUDE_DIRS})
            set (SeExpr2_LIBRARIES ${SeExpr2_LIBRARY} -L${LLVM_LIBRARY_DIR} ${LLVM_LIB})
        else()
            set (SeExpr2_FOUND FALSE)
        endif()
    else()
        set (SeExpr2_INCLUDE_DIRS ${SeExpr2_INCLUDE_DIR})
        set (SeExpr2_LIBRARIES ${SeExpr2_LIBRARY})
    endif()
endif()

message("SeExpr2: ${SeExpr2_LIBRARIES}")
