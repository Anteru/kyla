PROJECT(kylabase)

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	COMMAND
		txttoheader install_db_structure ${kyla_SOURCE_DIR}/src/sql/install-db-structure.sql > ${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	DEPENDS
		${kyla_SOURCE_DIR}/src/sql/install-db-structure.sql
	)

SET(HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h

	inc/sql/Database.h
	inc/ArrayAdapter.h
	inc/ArrayRef.h

	inc/BaseRepository.h
	inc/Compression.h
	inc/DeployedRepository.h
	inc/Exception.h
	inc/FileIO.h
	inc/Hash.h
	inc/Log.h
	inc/PackedRepository.h
	inc/PackedRepositoryBase.h
	inc/Repository.h
	inc/StringRef.h
	inc/Types.h
	inc/Uuid.h
	inc/WebRepository.h
)

SET(SOURCES
	src/sql/Database.cpp

	src/BaseRepository.cpp
	src/Compression.cpp
	src/DeployedRepository.cpp
	src/Exception.cpp
	src/FileIO.cpp
	src/Hash.cpp
	src/Log.cpp
	src/PackedRepository.cpp
	src/PackedRepositoryBase.cpp
	src/Repository.cpp
	src/StringRef.cpp
	src/Uuid.cpp
	src/WebRepository.cpp
)

FIND_PACKAGE(OpenSSL)

ADD_LIBRARY(kylabase STATIC
	${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(kylabase
	OpenSSL::Crypto brotli fmt::fmt zlib sqlite pugixml zstd)
TARGET_INCLUDE_DIRECTORIES(kylabase
	PUBLIC
		inc
		${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_CURRENT_BINARY_DIR})

IF(WIN32)
	TARGET_COMPILE_DEFINITIONS(kylabase
		PUBLIC KYLA_PLATFORM_WINDOWS=1)
	TARGET_LINK_LIBRARIES(kylabase crypt32 ws2_32)
ELSE()
	FIND_PACKAGE(CURL REQUIRED QUIET)
	TARGET_LINK_LIBRARIES(kylabase CURL::libcurl)
	TARGET_COMPILE_DEFINITIONS(kylabase
		PUBLIC KYLA_PLATFORM_LINUX=1)
ENDIF()

SET_PROPERTY(TARGET kylabase PROPERTY CXX_STANDARD 17)
SET_PROPERTY(TARGET kylabase PROPERTY CXX_STANDARD_REQUIRED ON)

ADD_SUBDIRECTORY(test)