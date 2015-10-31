PROJECT(kyla_core)

SET(HEADERS
	inc/sql/Database.h

	inc/ArrayAdapter.h
	inc/ArrayRef.h

	inc/Compression.h
	inc/Config.h
	inc/Exception.h
	inc/FileIO.h
    inc/Hash.h
	inc/Installer.h
	inc/Kyla.h
	inc/Log.h
	inc/Property.h
    inc/SourcePackage.h
	inc/SourcePackageReader.h
	inc/SourcePackageWriter.h
	inc/StringRef.h
	inc/Types.h
	inc/Uuid.h)

SET(SOURCES
	src/sql/Database.cpp

	src/Compression.cpp
	src/Exception.cpp
	src/FileIO.cpp
    src/Hash.cpp
	src/Installer.cpp
	src/Kyla.cpp
	src/Property.cpp
    src/SourcePackage.cpp
	src/SourcePackageReader.cpp
	src/SourcePackageWriter.cpp
	src/StringRef.cpp
	src/Uuid.cpp)

FIND_PACKAGE(OpenSSL)
FIND_PACKAGE(ZLIB)
FIND_PACKAGE(Boost 1.59.0 REQUIRED QUIET COMPONENTS filesystem system)

ADD_LIBRARY(kyla.Core STATIC ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(kyla.Core ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ZLIB::ZLIB spdlog sqlite)
TARGET_INCLUDE_DIRECTORIES(kyla.Core
    PUBLIC inc ${Boost_INCLUDE_DIRS}
	PRIVATE ${OPENSSL_INCLUDE_DIR})
target_compile_definitions(kyla.Core
	PUBLIC -DBOOST_NO_AUTO_PTR)
SET_PROPERTY(TARGET kyla.Core PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET kyla.Core PROPERTY CXX_STANDARD_REQUIRED ON)
