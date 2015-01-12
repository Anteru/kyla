PROJECT(pugixml)

ADD_LIBRARY(pugixml STATIC pugixml.cpp pugiconfig.hpp pugixml.hpp)
TARGET_INCLUDE_DIRECTORIES(pugixml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
