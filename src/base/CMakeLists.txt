PROJECT(libkyla)

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	COMMAND
		txttoheader install_db_structure ${kyla_SOURCE_DIR}/sql/install-db-structure.sql > ${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	DEPENDS
		${kyla_SOURCE_DIR}/sql/install-db-structure.sql
	)

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/temp-db-structure.h
	COMMAND
		txttoheader temp_db_structure ${kyla_SOURCE_DIR}/sql/temp-db-structure.sql > ${CMAKE_CURRENT_BINARY_DIR}/temp-db-structure.h
	DEPENDS
		${kyla_SOURCE_DIR}/sql/temp-db-structure.sql
	)

SET(HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	${CMAKE_CURRENT_BINARY_DIR}/temp-db-structure.h

	inc/sql/Database.h
	inc/ArrayAdapter.h
	inc/ArrayRef.h

	inc/Compression.h
	inc/Config.h
	inc/Exception.h
	inc/FileIO.h
	inc/Hash.h
	inc/Log.h
	inc/Repository.h
	inc/RepositoryBuilder.h
	inc/StringRef.h
	inc/Types.h
	inc/Uuid.h)

SET(SOURCES
	src/sql/Database.cpp

	src/Compression.cpp
	src/Exception.cpp
	src/FileIO.cpp
	src/Hash.cpp
	src/Log.cpp
	src/Repository.cpp
	src/RepositoryBuilder.cpp
	src/StringRef.cpp
	src/Uuid.cpp)

IF(WIN32)
	ADD_DEFINITIONS(-DKYLA_PLATFORM_WINDOWS=1)
ELSE()
	ADD_DEFINITIONS(-DKYLA_PLATFORM_LINUX=1)
ENDIF()

FIND_PACKAGE(OpenSSL)
FIND_PACKAGE(Boost 1.59.0 REQUIRED QUIET COMPONENTS filesystem system)

ADD_LIBRARY(kylabase STATIC ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(kylabase ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES}
	zlib spdlog sqlite pugixml)
TARGET_INCLUDE_DIRECTORIES(kylabase
	PUBLIC inc ${Boost_INCLUDE_DIRS}
	PRIVATE ${OPENSSL_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(kylabase
	PUBLIC -DBOOST_NO_AUTO_PTR)
SET_PROPERTY(TARGET kylabase PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET kylabase PROPERTY CXX_STANDARD_REQUIRED ON)
