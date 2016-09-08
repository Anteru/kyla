CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT(kyla)
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
SET(CMAKE_CXX_STANDARD 11)
ENABLE_TESTING()

SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY 	${PROJECT_BINARY_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY 	${PROJECT_BINARY_DIR}/bin)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY 	${PROJECT_BINARY_DIR}/bin)

ADD_SUBDIRECTORY(base)
ADD_SUBDIRECTORY(kyla)
ADD_SUBDIRECTORY(cli)

ADD_SUBDIRECTORY(extern)
ADD_SUBDIRECTORY(util)

option(KYLA_BUILD_UI "Build the sample UI as well" ON)

IF(KYLA_BUILD_UI)
    ADD_SUBDIRECTORY(ui)
ENDIF()

find_package(PythonInterp)

ADD_CUSTOM_TARGET (docs
    ${PYTHON_EXECUTABLE} -m venv env
    COMMAND env/scripts/activate.bat
    COMMAND pip -q install -r requirements.txt
    COMMAND make.bat html

    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../doc)

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

ADD_TEST(NAME SanityTests
	COMMAND	${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../test/testrunner.py
		$<TARGET_FILE:kcl>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../test)
