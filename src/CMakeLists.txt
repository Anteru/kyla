SET(Boost_USE_STATIC_LIBS ON)

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

IF(KYLA_BUILD_UI)
    # This builds an installation package for kyla itself. It requires the UI,
    # as well that the Qt binaries have been copied manually into the bin/
    # folder
    ADD_CUSTOM_TARGET (deploy
        ${PYTHON_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../samples/gen-sdk.py
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        $<TARGET_FILE:kcl>
        ${PROJECT_BINARY_DIR}/.cache
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

    ADD_DEPENDENCIES(deploy docs kui)
ENDIF()
