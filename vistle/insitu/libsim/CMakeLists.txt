option(LIBSIM_DEBUG "print debug messages" OFF)
option(LIBSIM_PARALLEL "build parallel instead of serial LibSim Interface" ON)

if(NOT VISTLE_MULTI_PROCESS)
vistle_add_library(vistle_libsim_engine_interface EngineInterface.h EngineInterface.cpp)
vistle_export_library(vistle_libsim_engine_interface EngineInterface.h EngineInterface.cpp)
vistle_target_link_libraries(vistle_libsim_engine_interface
        PRIVATE Boost::boost
        PRIVATE Threads::Threads
)
endif()

vistle_add_library(vistle_libsim_connect EstablishConnection.cpp EstablishConnection.h exportEC.h)
vistle_export_library(vistle_libsim_connect EstablishConnection.cpp EstablishConnection.h exportEC.h)
target_link_libraries(vistle_libsim_connect
        PRIVATE Boost::boost
        PRIVATE Boost::system
        PRIVATE Threads::Threads
)

if(LIBSIM_DEBUG)
   add_definitions(-DLIBSIM_DEBUG)
endif()
if (VISTLE_SHM_PER_RANK)
   add_definitions(-DSHMPERRANK)
endif()
add_definitions(-DLIBSIM_EXPORT)

SET(RUNTIME_SOURCES 
  VertexTypesToVistle.h
  Exeption.cpp
  VisItControlInterfaceRuntime.C 
  VisItDataInterfaceRuntime.C 
  VisItDataInterfaceRuntimeP.C 
  CommandMetaData.C
  CurveData.C
  CurveMetaData.C
  CurvilinearMesh.C
  CSGMesh.C
  #DomainBoundaries.C
  DomainList.C 
  #DomainNesting.C
  Engine.cpp
  ExpressionMetaData.C
  MaterialData.C
  MaterialMetaData.C
  MeshMetaData.C
  MessageMetaData.C
  NameList.C
  OptionList.C
  PointMesh.C
  ProxyModule.cpp
  RectilinearMesh.C
  SimulationMetaData.C
  SpeciesData.C
  SpeciesMetaData.C
  UnstructuredMesh.C
  VariableData.C
  VariableMetaData.C
  #View2D.C
  #View3D.C
)

SET(RUNTIME_HEADERS 
  VertexTypesToVistle.h  
  Exeption.h
  VisItControlInterfaceRuntime.h
  VisItDataInterfaceRuntime.h
  VisItDataInterfaceRuntimeP.h
  VisItDataTypes.h
  VisitDataTypesToVistle.h  
  CommandMetaData.h
  CurveData.h
  CurveMetaData.h
  CurvilinearMesh.h
  CSGMesh.h
  #DomainBoundaries.h
  DomainList.h 
  #DomainNesting.h
  Engine.h
  ExpressionMetaData.h
  MaterialData.h
  MaterialMetaData.h
  MeshMetaData.h
  MessageMetaData.h
  MetaData.h
  NameList.h
  OptionList.h
  PointMesh.h
  ProxyModule.h
  RectilinearMesh.h
  SimulationMetaData.h
  SpeciesData.h
  SpeciesMetaData.h
  UnstructuredMesh.h
  VariableData.h
  VariableMetaData.h
  VectorTypes.h
  #View2D.h
  #View3D.h
  export.h
)

set (LIBCORENAME simV2runtime_par) # use disable LIBSIM_PARALLEL CMake option for serial simulation
if(NOT LIBSIM_PARALLEL)
set (LIBCORENAME simV2runtime_ser) 
endif()
set(LIBNAME lib${LIBCORENAME}) #export.h has to be adopted if this changes
if(NOT MSVC)
	set(LIBNAME ${LIBCORENAME}) #to compensate the additional lib prefix from vistle_add_library
endif()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()

vistle_add_library(${LIBNAME} ${RUNTIME_HEADERS} ${RUNTIME_SOURCES})
vistle_export_library(${LIBNAME} ${RUNTIME_HEADERS} ${RUNTIME_SOURCES})


vistle_target_link_libraries(${LIBNAME}
		PRIVATE vistle_insitu_core
		PRIVATE vistle_module
		PRIVATE vistle_util
		#PRIVATE vistle_control
        PRIVATE vistle_core
        PRIVATE vistle_boost_mpi
		PRIVATE vistle_insitu_message
		PRIVATE Boost::boost
        PRIVATE MPI::MPI_C
        PRIVATE Threads::Threads
		)
		
if(NOT VISTLE_MULTI_PROCESS)
target_link_libraries(${LIBNAME}
		PRIVATE vistle_clustermanager
		PRIVATE vistle_libsim_engine_interface
		)
endif()

vistle_install_docs(README.md)

