vistle_find_package(COVER)
if (NOT COVER_FOUND)
   message("COVER not found - not building COVER wrapper")
   return()
endif()

#include_directories(SYSTEM ${MPI_C_INCLUDE_PATH})
#set(CMAKE_CXX_COMPILE_FLAGS "${CMAKE_CXX_COMPILE_FLAGS} ${MPI_C_COMPILE_FLAGS}")
#set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${MPI_C_LINK_FLAGS}")

#SET(EXTRA_LIBS ${EXTRA_LIBS} MPI::MPI_C)
ADD_DEFINITIONS(-DHAS_MPI)
ADD_DEFINITIONS(-DCOVER_PLUGIN)



vistle_add_library(vistle_cover SHARED 
   COVER.cpp COVER.h
   VistleGeometryGenerator.h VistleGeometryGenerator.cpp
)
target_include_directories(vistle_cover SYSTEM
        PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS}
)
target_include_directories(vistle_cover
        PRIVATE ${COVER_INCLUDE_DIRS}
        PRIVATE ../../cover
        PRIVATE ..
)
target_link_libraries(vistle_cover 
    PRIVATE ${EXTRA_LIBS} 
    PRIVATE vistle_renderer 
    PRIVATE vistle_util
    PRIVATE ${CMAKE_DL_LIBS} 
    PRIVATE VistlePluginUtil 
    PRIVATE ${COVER_PLUGINUTIL_LIBRARY})

if(NOT VISTLE_MULTI_PROCESS)
	target_link_libraries(vistle_cover 
        PRIVATE vistle_clustermanager)
endif()

add_module(COVER coverwrapper.cpp)

target_include_directories(COVER SYSTEM
        PRIVATE ${OPENSCENEGRAPH_INCLUDE_DIRS}
)
target_include_directories(COVER
        PRIVATE ${COVER_INCLUDE_DIRS}
        PRIVATE ../../cover
        PRIVATE ..
)
target_link_libraries(COVER 
    ${EXTRA_LIBS} 
    vistle_util 
    ${CMAKE_DL_LIBS} 
    vistle_cover)

if(NOT VISTLE_MULTI_PROCESS)
if(APPLE)
    set (CMAKE_SHARED_LINKER_FLAGS "${VISTLE_SHARED_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")
elseif(WIN32)
else()
    set (CMAKE_SHARED_LINKER_FLAGS "${VISTLE_SHARED_LINKER_FLAGS} -Wl,--allow-shlib-undefined")
endif()
endif()
