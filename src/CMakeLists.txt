IF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    # On Windows, we don't want to ship boost DLLs
    # On Linux, system boost is usually not compiled with -fPIC, so we can't
    # statically link into shared libraries, so we don't want it on Linux
    SET(Boost_USE_STATIC_LIBS ON)
ENDIF()

ADD_SUBDIRECTORY(base)
ADD_SUBDIRECTORY(build)
ADD_SUBDIRECTORY(kyla)
ADD_SUBDIRECTORY(cli)

ADD_SUBDIRECTORY(extern)
ADD_SUBDIRECTORY(util)

option(KYLA_BUILD_UI "Build the sample UI as well" ON)

IF(KYLA_BUILD_UI)
    ADD_SUBDIRECTORY(ui)
ENDIF()
