vistle_find_package(SeExpr2)
if (NOT SeExpr2_FOUND)
    return()
endif()

include_directories(${SeExpr2_INCLUDE_DIRS})

add_module(Calc Calc.cpp Calc.h)

#link_directories(${SeExpr2_LIBRARY_DIRS})
target_link_libraries(Calc ${SeExpr2_LIBRARIES})
