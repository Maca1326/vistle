vistle_find_package(TBB)
use_openmp()

set(PREFER_TBB TRUE)

set(SOURCES
    ../IsoSurface/IsoSurface.cpp
    ../IsoSurface/IsoSurface.h
    ../IsoSurface/IsoDataFunctor.cpp
    ../IsoSurface/IsoDataFunctor.h
    )
set(CUDA_OBJ "")
#vistle_find_package(CUDA)
if(NOT APPLE AND CUDA_FOUND AND FALSE)
   include_directories(SYSTEM
        ${CUDA_INCLUDE_DIRS}
   )
   add_definitions(-DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_CUDA)
   cuda_compile(CUDA_OBJ "../IsoSurface/Leveller.cu")
else()
   set(SOURCES ${SOURCES} "../IsoSurface/Leveller.cpp")
   if(TBB_FOUND AND PREFER_TBB)
      add_definitions(-DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_TBB)
   elseif(OPENMP_FOUND)
      add_definitions(-DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_OMP)
   elseif(TBB_FOUND)
      add_definitions(-DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_TBB)
   else()
      add_definitions(-DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_CPP)
   endif()
endif()

if(TBB_FOUND AND PREFER_TBB)
   add_definitions(-DUSE_TBB)
   add_definitions(-DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_TBB)
elseif(OPENMP_FOUND)
   add_definitions(-DUSE_OMP)
   add_definitions(-DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_OMP)
elseif(TBB_FOUND)
   add_definitions(-DUSE_TBB)
   add_definitions(-DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_TBB)
else()
   add_definitions(-DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_CPP)
   add_definitions(-DUSE_CPP)
endif()

include_directories(SYSTEM
        ${THRUST_INCLUDE_DIR}
        ${TBB_INCLUDE_DIRS}
)

add_module(${NAME} ${SOURCES} ${CUDA_OBJ})

target_link_libraries(${NAME}
        ${TBB_LIBRARIES}
)

if (CUDA_FOUND)
   target_link_libraries(${NAME}
        ${CUDA_LIBRARIES}
   )
endif()
