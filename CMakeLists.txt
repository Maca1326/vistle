IF(WIN32)
cmake_minimum_required(VERSION 3.15.2 FATAL_ERROR)
ELSE(WIN32)
cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)
ENDIF()

if(${CMAKE_VERSION} VERSION_LESS "3.6.0") 
    set (CMAKE_CXX_STANDARD 14)
else()
    set (CMAKE_CXX_STANDARD 17)
endif()
set (CMAKE_CXX_STANDARD_REQUIRED 14)
set(CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_C_STANDARD 11)
set (CMAKE_C_STANDARD_REQUIRED 99)
set(CMAKE_C_EXTENSIONS OFF)

project(Vistle C CXX)

#find_package(Threads)
#message("THREADS: ${CMAKE_THREAD_LIB_INIT}")

message ("CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "CrayLinuxEnvironment")
   message("compiling for Cray")
   set(CRAY ON)
   #set(Boost_DEBUG ON)
   set(Boost_NO_BOOST_CMAKE OFF)
   set(CRAYPE_LINK_TYPE "$ENV{CRAYPE_LINK_TYPE}")
   if (NOT "${CRAYPE_LINK_TYPE}" STREQUAL "dynamic")
      message("dynamic linking required: set CRAYPE_LINK_TYPE to \"dynamic\"")
      return()
   endif()

   set(VISTLE_USE_PYTHON3 ON CACHE INTERNAL "")

   # work around broken FindCUDA
   macro( find_host_program )
      set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
      set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER )
      set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER )
      if( CMAKE_HOST_WIN32 )
         SET( WIN32 1 )
         SET( UNIX )
      elseif( CMAKE_HOST_APPLE )
         SET( APPLE 1 )
         SET( UNIX )
      endif()
      find_program( ${ARGN} )
      SET( WIN32 )
      SET( APPLE )
      SET( UNIX 1 )
      set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
      set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
      set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
   endmacro()
endif()

if(POLICY CMP0042)
   # default to finding shlibs relative to @rpath on MacOS
   cmake_policy(SET CMP0042 NEW)
endif()

# Automatically link Qt executables to qtmain target on Windows
cmake_policy(SET CMP0020 NEW)

if (POLICY CMP0074)
   # make find_include/find_library search in <PackageName>_ROOT prefix
   cmake_policy(SET CMP0074 NEW)
endif()

# vim's YouCompleteMe can make use of this
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten")
    option(VISTLE_GUI_ONLY "Only graphical workflow editor" ON)
    set(Boost_USE_STATIC_RUNTIME TRUE)
    option(VISTLE_BUILD_SHARED "Build all libraries as shared libraries instead of static" OFF)
else()
    option(VISTLE_GUI_ONLY "Only graphical workflow editor" OFF)
    option(VISTLE_BUILD_SHARED "Build all libraries as shared libraries instead of static" ON)
endif()

set(VISTLE_CPU_ARCH "" CACHE STRING "CPU architecture to optimize for (native, corei7-avx, core-avx2, atom, ...)")

option(VISTLE_PEDANTIC_ERRORS "Be pedantic about C++ standard and emit errors" ON)

option(VISTLE_SANITIZE_UNDEFINED "Build with -fsanitize=undefined" OFF)
option(VISTLE_SANITIZE_ADDRESS "Build with -fsanitize=address" OFF)
option(VISTLE_SANITIZE_THREAD "Build with -fsanitize=thread" OFF)

option(VISTLE_USE_CUDA "Build with CUDA support" OFF)

if (NOT "$ENV{VBUILD}" STREQUAL "")
   include("${PROJECT_SOURCE_DIR}/build/$ENV{VBUILD}.cmake")
endif()

if (VISTLE_BUILD_SHARED)
   set(VISTLE_LIB_TYPE SHARED)
else()
   set(VISTLE_LIB_TYPE STATIC)
   add_definitions(-DVISTLE_STATIC)
   set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif()

# allow for optimizations in Boost.MPI
if (NOT WIN32)
   add_definitions(-DBOOST_MPI_HOMOGENEOUS)
endif()

enable_testing()

if (CMAKE_BUILD_TYPE STREQUAL "")
   set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
      "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
      FORCE
   )
   message("Setting CMAKE_BUILD_TYPE to ${CMAKE_BUILD_TYPE}")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release" OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
else()
   add_definitions(-DEIGEN_INITIALIZE_MATRICES_BY_NAN)
endif()

#add_definitions(-DEIGEN_DEFAULT_DENSE_INDEX_TYPE=short)
add_definitions(-DEIGEN_DEFAULT_DENSE_INDEX_TYPE=int)

# use this instead of FIND_PACKAGE to prefer Package in $PACKAGE_HOME and $EXTERNLIBS/package

macro(vistle_find_package package)
   set(SAVED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
   set(SAVED_CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH})

   set(pack "${package}")
   if (pack STREQUAL "PythonLibs")
      set(pack "Python")
   endif()
   if (pack STREQUAL "PythonInterp")
      set(pack "Python")
   endif()
   if (pack STREQUAL "COVISE" OR pack STREQUAL "COVER")
       set(CMAKE_MODULE_PATH "$ENV{COVISEDIR}/cmake" ${CMAKE_MODULE_PATH})
       set(CMAKE_MODULE_PATH "$ENV{COVISEDIR}/share/cmake" ${CMAKE_MODULE_PATH})
   endif()

   string(TOUPPER ${pack} UPPER)
   string(TOLOWER ${pack} LOWER)
   if(MINGW)
      set(CMAKE_PREFIX_PATH ${MINGW_SYSROOT} ${CMAKE_PREFIX_PATH})
   endif()
   if(NOT "$ENV{EXTERNLIBS}" STREQUAL "")
      set(CMAKE_PREFIX_PATH $ENV{EXTERNLIBS}/${LOWER}/bin ${CMAKE_PREFIX_PATH})
      set(CMAKE_PREFIX_PATH $ENV{EXTERNLIBS} ${CMAKE_PREFIX_PATH})
      set(CMAKE_PREFIX_PATH $ENV{EXTERNLIBS}/${LOWER} ${CMAKE_PREFIX_PATH})
   endif()
   if(NOT "$ENV{${UPPER}_HOME}" STREQUAL "")
      set(CMAKE_PREFIX_PATH $ENV{${UPPER}_HOME} ${CMAKE_PREFIX_PATH})
   endif()
   if(NOT "$ENV{${UPPER}_DIR}" STREQUAL "")
      set(CMAKE_PREFIX_PATH $ENV{${UPPER}_DIR} ${CMAKE_PREFIX_PATH})
   endif()
   if(NOT "$ENV{${UPPER}ROOT}" STREQUAL "")
      set(CMAKE_PREFIX_PATH $ENV{${UPPER}ROOT} ${CMAKE_PREFIX_PATH})
   endif()
   if(NOT "$ENV{${UPPER}_ROOT}" STREQUAL "")
      set(CMAKE_PREFIX_PATH $ENV{${UPPER}_ROOT} ${CMAKE_PREFIX_PATH})
   endif()
   #message("looking for package ${ARGV}")
   #message("CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
   find_package(${ARGV})

   set(CMAKE_PREFIX_PATH ${SAVED_CMAKE_PREFIX_PATH})
   set(CMAKE_MODULE_PATH ${SAVED_CMAKE_MODULE_PATH})
endmacro(vistle_find_package PACKAGE)

if(WIN32)
   #set(BOOST_ROOT "$ENV{EXTERNLIBS}/boost")
   #STRING(REGEX REPLACE "\\\\" "/" BOOST_ROOT ${BOOST_ROOT}) 
    #set(MPI_HOME "$ENV{EXTERNLIBS}/OpenMPI")
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/vistle/cmake/Modules/windows;${CMAKE_MODULE_PATH}")
    #add_definitions(-DOMPI_IMPORTS)

    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)

    add_definitions(-DBOOST_ALL_NO_LIB)
    #add_definitions(-DBOOST_ALL_DYN_LINK)
    add_definitions(-DBOOST_MPI_DYN_LINK)
    add_definitions(-DBOOST_PROGRAM_OPTIONS_DYN_LINK)
    add_definitions(-DBOOST_ZLIB_DYN_LINK)
    add_definitions(-DBOOST_IOSTREAMS_DYN_LINK)

    add_definitions(-D_WIN32_WINNT=0x0601) # windows 7
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4244") # possible loss of data
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4250") # 'class1': inheritcs 'class2::member' via dominance
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4251") # 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4267") # 'var' : conversion from 'size_t' to 'type', possible loss of data
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4275") # non - DLL-interface class 'class_1' used as base for DLL-interface class 'class_2'
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4661") # 'identifier' : no suitable definition provided for explicit template instantiation request
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4910") # '<identifier>' : '__declspec(dllexport)' and 'extern' are incompatible on an explicit instantiation
    set(MSVC_WARNINGS "${MSVC_WARNINGS} /wd4996") # The compiler encountered a deprecated declaration.
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSVC_WARNINGS} /EHsc")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MSVC_WARNINGS} /EHsc")
endif(WIN32)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/vistle/cmake/Modules/;${CMAKE_MODULE_PATH}")

if (NOT VISTLE_GUI_ONLY)
    find_package(OpenMP)
    
    option(VISTLE_USE_PYTHON3 "use Python 3" ON)
    if(VISTLE_USE_PYTHON3)
        set(PythonLibs_FIND_VERSION_MAJOR 3)
        message("Searching for Python 3")
    else()
        if (NOT CMAKE_VERSION VERSION_LESS 3.6)
            set(PythonLibs_FIND_VERSION 2.7 2.6)
        endif()
        set(PythonLibs_FIND_VERSION_MAJOR 2)
        message("Searching for Python 2")
    endif()
    vistle_find_package(PythonLibs REQUIRED)
    vistle_find_package(PythonInterp REQUIRED)
    if (UNIX AND NOT APPLE)
        set(PYTHON_LIBRARIES ${PYTHON_LIBRARIES} ${CMAKE_DL_LIBS})
        set(PYTHON_LIBRARIES ${PYTHON_LIBRARIES} -lutil)
    endif()

    #vistle_find_package(IceT REQUIRED)
    set(ICET_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/3rdparty/IceT/src/include;${CMAKE_BINARY_DIR}/3rdparty/IceT/src/include)
    set(ICET_CORE_LIBS "IceTCore")
    set(ICET_MPI_LIBS "IceTMPI")
    set(ICET_FOUND TRUE)

    vistle_find_package(TBB)
endif(NOT VISTLE_GUI_ONLY)

set(ASAN_COMPILE_FLAGS "")
set(ASAN_LINK_FLAGS "")
if (VISTLE_SANITIZE_UNDEFINED)
   set(ASAN_LINK_FLAGS "${ASAN_LINK_FLAGS} -fsanitize=undefined")
   set(ASAN_COMPILE_FLAGS "${ASAN_COMPILE_FLAGS} -fsanitize=undefined")
endif()
if (VISTLE_SANITIZE_ADDRESS)
   set(ASAN_LINK_FLAGS "${ASAN_LINK_FLAGS} -fsanitize=address")
   set(ASAN_COMPILE_FLAGS "${ASAN_COMPILE_FLAGS} -fsanitize=address")
endif()
if (VISTLE_SANITIZE_THREAD)
   set(ASAN_LINK_FLAGS "${ASAN_LINK_FLAGS} -fsanitize=thread")
   set(ASAN_COMPILE_FLAGS "${ASAN_COMPILE_FLAGS} -fsanitize=thread")
endif()
if (VISTLE_SANITIZE_THREAD OR VISTLE_SANITIZE_ADDRESS OR VISTLE_SANITIZE_UNDEFINED)
   set(ASAN_COMPILE_FLAGS "${ASAN_COMPILE_FLAGS} -fno-omit-frame-pointer")
   set(ASAN_COMPILE_FLAGS "${ASAN_COMPILE_FLAGS} -g3")
   set(ASAN_COMPILE_FLAGS "${ASAN_COMPILE_FLAGS} -fPIC")
   set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_COMPILE_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${ASAN_LINK_FLAGS}")
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${ASAN_LINK_FLAGS}")

set (BOOST_INCLUDEDIR /usr/include/boost169)  
set (BOOST_LIBRARYDIR /usr/lib64/boost169)

if (WIN32)
    set(BOOST_OPT zlib)
set (BOOST_LIBRARYDIR $ENV{EXTERNLIBS}/boost/lib)
else()
    set(BOOST_OPT)
endif()
if (NOT VISTLE_GUI_ONLY)
    set(BOOST_OPT ${BOOST_OPT} iostreams locale)
endif()

#set (Boost_DEBUG ON)
IF(WIN32)
#otherwise Boost_INCLUDE_DIRS is not set and headers are not found
set (Boost_NO_BOOST_CMAKE ON)
set (Boost_USE_STATIC_RUNTIME OFF)
ENDIF(WIN32)

vistle_find_package(Boost 1.59 REQUIRED
    COMPONENTS 
        system 
        serialization 
        program_options 
        date_time 
        filesystem
        chrono 
        thread 
        regex
    OPTIONAL_COMPONENTS ${BOOST_OPT})
	

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-command-line-argument")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument")
   if (VISTLE_PEDANTIC_ERRORS)
       add_compile_options("-pedantic-errors")
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
   if (VISTLE_PEDANTIC_ERRORS)
       add_compile_options("-pedantic-errors")
   endif()

   if (NOT ${BUILD_SHARED_LIBS})
      # for linking the libraries into plugins
      #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
      #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
      set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
   endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
endif()

if (NOT CRAY)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(APPLE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-stdlibcxx-not-found")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

    if (DEFINED VISTLE_CPU_ARCH AND NOT VISTLE_CPU_ARCH STREQUAL "")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${VISTLE_CPU_ARCH}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${VISTLE_CPU_ARCH}")
    endif()
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    if (NOT APPLE)
      if (DEFINED VISTLE_CPU_ARCH AND NOT VISTLE_CPU_ARCH STREQUAL "")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${VISTLE_CPU_ARCH}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${VISTLE_CPU_ARCH}")
      endif()
    endif()

    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7)
      set(CUDA_HOST_COMPILER ${CMAKE_SOURCE_DIR}/scripts/cuda-host-compiler CACHE STRING "CUDA nvcc host compiler" FORCE)
    endif()

  endif()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
#elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
endif()

if (NOT CRAY AND NOT VISTLE_GUI_ONLY)
    set(MPI_SKIP_MPICXX TRUE)
    #set(MPI_FIND_COMPONENTS "C CXX")
    add_definitions(-DOMPI_SKIP_MPICXX) # OpenMPI
    add_definitions(-DMPI_NO_CPPBIND) # HPE MPT
    vistle_find_package(MPI REQUIRED COMPONENTS C CXX)

    include_directories(SYSTEM ${MPI_C_INCLUDE_PATH})
    set(CMAKE_CXX_COMPILE_FLAGS "${CMAKE_CXX_COMPILE_FLAGS} ${MPI_C_COMPILE_FLAGS}")
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${MPI_C_LINK_FLAGS}")

    if (Boost_VERSION VERSION_GREATER 106999)
        set(BOOST_MPI_DIR "boost-mpi")
    elseif (Boost_VERSION VERSION_GREATER 106099)
        set(BOOST_MPI_DIR "boost-mpi-1.69")
    elseif (Boost_VERSION VERSION_GREATER 1.69.99)
        set(BOOST_MPI_DIR "boost-mpi")
    elseif (Boost_VERSION VERSION_GREATER 1.60.99)
        set(BOOST_MPI_DIR "boost-mpi-1.69")
    else()
        set(BOOST_MPI_DIR "boost-mpi-1.60")
    endif()
    message("Using Boost.MPI from ${BOOST_MPI_DIR} for Boost ${Boost_VERSION}")
endif()

# for CMake config file version of boost this needs to be changed to version numbers such as 1.60.99 instead of 106099
add_definitions(-DBOOST_LIB_DIAGNOSTIC=1)

get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if (LIB64)
   set(LIBSUFFIX 64)
else()
   set(LIBSUFFIX "")
endif()
if (NOT WIN32 AND NOT APPLE)
    set(INSTALL_LIB_DIR     lib${LIBSUFFIX} CACHE PATH "Installation directory for libraries")
    mark_as_advanced(INSTALL_LIB_DIR)
endif()

if (VISTLE_USE_CUDA AND NOT VISTLE_GUI_ONLY)
   vistle_find_package(CUDA)
   if ("${CUDA_VERSION}" VERSION_LESS 7.0)
      set (CUDA_HOST_COMPILER ${PROJECT_SOURCE_DIR}/scripts/cuda-host-compiler CACHE STRING "CUDA nvcc host compiler" FORCE)
   else()
      set(CUDA_PROPAGATE_HOST_FLAGS OFF)
      set(CUDA_NVCC_FLAGS "--std=c++11 ${CUDA_NVCC_FLAGS}")
   endif()
   if (APPLE)
      set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} \"-DBOOST_NOINLINE=__attribute__ ((noinline))\"")
   endif (APPLE)
endif()


include(GNUInstallDirs)
macro(vistle_add_library targetname)

	add_library(${targetname} ${ARGN})
    set_target_properties(${targetname} PROPERTIES FOLDER "Libraries")
	string(FIND ${targetname} "vistle_" vistle_prefix)
	if(NOT ${vistle_prefix} EQUAL 0)
		message(WARNING " missing prefix for " ${targetname} " vistle libraries must be prefixed with vistle_")
	elseif() #change the vistle_libname to vistle::libname in exported target
		string(REPLACE "vistle_" "" target_suffix ${targetname})
		set_target_properties(${targetname} PROPERTIES 
			EXPORT_NAME ${target_suffix}
		)
		add_library(vistle::${target_suffix} ALIAS ${targetname})
	endif()
	
    if (CRAY)
       set_property(TARGET ${targetname} APPEND_STRING PROPERTY LINK_FLAGS -Wl,-soname=lib${targetname}.so)
    endif()
    if(${CMAKE_VERSION} VERSION_LESS "3.12.4")
        install(TARGETS ${targetname} EXPORT ${targetname}Targets
		    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
				     COMPONENT	${targetname}_Runtime
		    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
				     COMPONENT 	${targetname}_Runtime
		    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
				     COMPONENT 	${targetname}_Development
        )
    else()
            install(TARGETS ${targetname} EXPORT ${targetname}Targets
		    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
				     COMPONENT	${targetname}_Runtime
		    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
					 COMPONENT 	${targetname}_Runtime
                     NAMELINK_COMPONENT ${targetname}_Development
		    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
				     COMPONENT 	${targetname}_Development
        )
    endif()
endmacro(vistle_add_library)

set(buildPackageLocation ${PROJECT_BINARY_DIR}/lib/cmake)


option(VISTLE_INSTALL_3RDPARTY "Add the 3rdParty directories to the install directory" OFF)
option(VISTLE_INSTALL_HEADER "Install the Vistle headers in CMAKE_INSTALL_PREFIX/include/path to lib" ON)

macro(vistle_export_library targetname)
	
	
	string(REPLACE ${PROJECT_BINARY_DIR}/vistle "" directory  ${CMAKE_CURRENT_BINARY_DIR})

	target_include_directories(${targetname}  
        PRIVATE  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}> 
        PRIVATE  $<INSTALL_INTERFACE:include> 
        INTERFACE  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}> 
        INTERFACE  $<INSTALL_INTERFACE:include> 
		)
	
	set(header "${ARGN}")
	list(FILTER	header INCLUDE	REGEX "(\.(h|H|hpp|HPP)$)"	)
        if(VISTLE_INSTALL_HEADER)
	    install(
	      FILES
		    ${header}
	      DESTINATION
		    include/vistle/${directory}
	      COMPONENT
		    Devel
	    ) 
    endif()
	set(config_in_file ${CMAKE_CURRENT_BINARY_DIR}/cmake/${targetname}Config.cmake.in)
	file(WRITE  ${config_in_file} "include(\"\${CMAKE_CURRENT_LIST_DIR}/${targetname}Targets.cmake\")") 

	configure_file(${config_in_file}
		"${buildPackageLocation}/${targetname}Config.cmake"
		COPYONLY)  
	include(CMakePackageConfigHelpers)
	write_basic_package_version_file("${targetname}ConfigVersion.cmake"
	  VERSION "1.0.0"
	  COMPATIBILITY ExactVersion
    )
    configure_file(${CMAKE_CURRENT_BINARY_DIR}/${targetname}ConfigVersion.cmake
        ${buildPackageLocation}/${targetname}ConfigVersion.cmake
        COPYONLY)
	export(EXPORT ${targetname}Targets
		FILE 		"${buildPackageLocation}/${targetname}Targets.cmake"
		NAMESPACE 	Vistle::
	)
	set(Vistle_ConfigPackageLocation 
	    ${CMAKE_INSTALL_LIBDIR}/cmake#/${directory}
	    #CACHE STRING "Path to " ${targetname} " cmake files"
) 	
	install(EXPORT ${targetname}Targets
		FILE		${targetname}Targets.cmake
		NAMESPACE	Vistle::
		DESTINATION	${Vistle_ConfigPackageLocation}
		COMPONENT	${targetname}_Development
	)
#	configure_package_config_file(${config_in_file}
#	"${CMAKE_CURRENT_BINARY_DIR}/${targetname}Config.cmake"
#	INSTALL_DESTINATION "${Vistle_ConfigPackageLocation}/${targetname}Config.cmake")  
	install(
		FILES 
			"${buildPackageLocation}/${targetname}Config.cmake"
			"${buildPackageLocation}/${targetname}ConfigVersion.cmake"
		DESTINATION ${Vistle_ConfigPackageLocation}
		COMPONENT ${targetname}_Development
	)
endmacro(vistle_export_library)

function(prepend_file filename)
    file(READ ${filename} old_content)
    file(WRITE ${filename} ${ARGN}  ${old_content})
endfunction()

function(getPublicBoostComponents PublicBoostComponents)
    set(addNext FALSE)
    foreach(Comp IN LISTS ARGN)
		if(addNext)
			string(REGEX REPLACE "::.*" "" TARGET_NAME ${Comp})
			if(${TARGET_NAME} STREQUAL "Boost")
				string(REGEX REPLACE ".*::" "" COMPONENT_NAME ${Comp})
				LIST(APPEND LocalPublicBoostComponents ${COMPONENT_NAME})
			endif()
			set(addNext FALSE)
		else()
			if( ${Comp} MATCHES "PUBLIC|INTERFACE")
				set(addNext TRUE)
			endif()
		endif()
    endforeach()
    set(${PublicBoostComponents} ${LocalPublicBoostComponents} PARENT_SCOPE)
endfunction()


#additionally adds public and interface dependencies to Config.cmake file
macro(vistle_target_link_libraries targetname)
	target_link_libraries(${targetname}
		${ARGN})
	set(file_content "include(CMakeFindDependencyMacro)\n")
    set(file_content ${file_content} "set(oldCMAKE_PREFIX_PATH \${CMAKE_PREFIX_PATH})\n")
    set(file_content ${file_content} "set(CMAKE_PREFIX_PATH \${CMAKE_CURRENT_LIST_DIR} \${CMAKE_PREFIX_PATH})\n") 
	set(add_next_arg false)
    set(hasPublicDependencies false)
    foreach(arg IN ITEMS ${ARGN})
        if(add_next_arg)
        	#set(file_content ${file_content} "set(" ${arg} "_DIR \"\${CMAKE_CURRENT_LIST_DIR}\")\n")
            if(${arg} MATCHES "vistle_.*")
	            set(file_content ${file_content} "find_dependency(" ${arg} ")\n") 
                set(add_next_arg false)
            endif()
        elseif(${arg} MATCHES "PUBLIC|INTERFACE")
            set(add_next_arg true)
            set(hasPublicDependencies true)
		endif()
	endforeach()

    getPublicBoostComponents(public_boost_componets ${ARGN})
    list(LENGTH public_boost_componets num_PUBLIC_Boost_componets)
    if(num_PUBLIC_Boost_componets GREATER 0)
        set(file_content ${file_content} "find_dependency( Boost " ${BOOST_MINIMUM_REQUIRED_VERSION} " REQUIRED COMPONENTS \n")
        foreach(item IN LISTS public_boost_componets)
            set(file_content ${file_content} "\t" ${item} "\n")
        endforeach()
        set(file_content ${file_content} ")\n") 
    endif()
    set(file_content ${file_content} "set(CMAKE_PREFIX_PATH \${oldCMAKE_PREFIX_PATH})\n")
    if(hasPublicDependencies)
	    prepend_file(${buildPackageLocation}/${targetname}Config.cmake ${file_content})
    endif()
endmacro()

macro(vistle_install_docs)
	string(REPLACE ${PROJECT_BINARY_DIR} "" directory  ${CMAKE_CURRENT_BINARY_DIR})
	install(
	  FILES
		"${ARGN}"
	  DESTINATION
		doc/${directory}
	  COMPONENT
		Devel
	) 
endmacro(vistle_install_docs) 

vistle_install_docs(README.md) 	
macro(vistle_install_headers directory)
    install(FILES ${ARGN} DESTINATION include/vistle/${directory})
endmacro(vistle_install_headers)

if (APPLE)
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

add_subdirectory(3rdparty)
add_subdirectory(vistle)

if(${CMAKE_VERSION} VERSION_GREATER "2.8.2")
   include(FeatureSummary)
   feature_summary(WHAT ALL)
endif()                                                                                                                                                            
