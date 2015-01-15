PROJECT(nim_generator)

SET(HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/build-db-structure.h
	${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h)

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/build-db-structure.h
	COMMAND
		txttoheader build_db_structure ${niven_installation_manager_SOURCE_DIR}/sql/build-db-structure.sql > ${CMAKE_CURRENT_BINARY_DIR}/build-db-structure.h
	DEPENDS
		${niven_installation_manager_SOURCE_DIR}/sql/build-db-structure.sql
	)

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	COMMAND
		txttoheader install_db_structure ${niven_installation_manager_SOURCE_DIR}/sql/install-db-structure.sql > ${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	DEPENDS
		${niven_installation_manager_SOURCE_DIR}/sql/install-db-structure.sql
	)

SET(SOURCES
    src/GeneratorMain.cpp)

FIND_PACKAGE(OpenSSL)
FIND_PACKAGE(ZLIB)
INCLUDE_DIRECTORIES()

# Needed for log
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
FIND_PACKAGE(Boost 1.55.0 REQUIRED QUIET COMPONENTS program_options filesystem system log)

ADD_EXECUTABLE(nim.Generator ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(nim.Generator
	nim.Common pugixml sqlite
	${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ZLIB::ZLIB)
TARGET_INCLUDE_DIRECTORIES(nim.Generator
	PRIVATE ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${OPENSSL_INCLUDE_DIR})
SET_PROPERTY(TARGET nim.Generator PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET nim.Generator PROPERTY CXX_STANDARD_REQUIRED ON)
