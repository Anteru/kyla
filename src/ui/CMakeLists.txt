PROJECT(KYLA_UI)

SET(SOURCES
	src/main.cpp

	src/OpenSourceRepositoryThread.cpp
	src/SetupContext.cpp
	src/SetupLogic.cpp)

SET(HEADERS
	inc/OpenSourceRepositoryThread.h
	inc/SetupContext.h
	inc/SetupLogic.h
	)

SET(MOC_SOURCES
	inc/OpenSourceRepositoryThread.h
	inc/SetupContext.h
	inc/SetupLogic.h)

SET(RCC_FILES
	ui/res.qrc)

FIND_PACKAGE(Qt5Core)
FIND_PACKAGE(Qt5Qml)

QT5_WRAP_CPP(OUT_MOC_FILES ${MOC_SOURCES})
QT5_ADD_RESOURCES(OUT_RCC_FILES ${RCC_FILES})

ADD_EXECUTABLE(kui WIN32
	${SOURCES}
	${HEADERS}
	${OUT_MOC_FILES}
	${OUT_RCC_FILES})

TARGET_LINK_LIBRARIES(kui
	kyla Qt5::Core Qt5::Qml)

IF(WIN32)
	FIND_PACKAGE(Qt5WinExtras)
	TARGET_LINK_LIBRARIES(kui Qt5::WinExtras)
ENDIF()

TARGET_INCLUDE_DIRECTORIES(kui
	PRIVATE inc ${CMAKE_CURRENT_BINARY_DIR})

SET_PROPERTY(TARGET kui PROPERTY CXX_STANDARD 14)
SET_PROPERTY(TARGET kui PROPERTY CXX_STANDARD_REQUIRED ON)