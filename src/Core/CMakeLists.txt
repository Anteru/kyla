PROJECT(kyla_core)

SET(HEADERS
	inc/FileIO.h
    inc/Hash.h
	inc/Installer.h
	inc/Kyla.h
	inc/Log.h
	inc/Property.h
    inc/SourcePackage.h
	inc/SourcePackageReader.h
	inc/SourcePackageWriter.h)

SET(SOURCES
	src/FileIO.cpp
    src/Hash.cpp
	src/Installer.cpp
	src/Kyla.cpp
	src/Property.cpp
    src/SourcePackage.cpp
	src/SourcePackageReader.cpp
	src/SourcePackageWriter.cpp)

FIND_PACKAGE(OpenSSL)
FIND_PACKAGE(ZLIB)
FIND_PACKAGE(Boost 1.55.0 REQUIRED QUIET COMPONENTS filesystem system)

ADD_LIBRARY(kyla.Core STATIC ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(kyla.Core ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ZLIB::ZLIB spdlog)
TARGET_INCLUDE_DIRECTORIES(kyla.Core
    PUBLIC inc ${Boost_INCLUDE_DIRS}
    PRIVATE ${OPENSSL_INCLUDE_DIR})
SET_PROPERTY(TARGET kyla.Core PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET kyla.Core PROPERTY CXX_STANDARD_REQUIRED ON)
