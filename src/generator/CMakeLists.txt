PROJECT(nim_generator)

SET(SOURCES
    src/GeneratorMain.cpp)

FIND_PACKAGE(Boost 1.55.0 REQUIRED COMPONENTS program_options filesystem system)

ADD_EXECUTABLE(nim.Generator ${SOURCES})
TARGET_LINK_LIBRARIES(nim.Generator nim.Common pugixml sqlite ${Boost_LIBRARIES})
TARGET_INCLUDE_DIRECTORIES(nim.Generator PUBLIC ${Boost_INCLUDE_DIRS})
SET_PROPERTY(TARGET nim.Generator PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET nim.Generator PROPERTY CXX_STANDARD_REQUIRED ON)
