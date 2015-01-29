PROJECT(kyla_generator)

SET(HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/build-db-structure.h
	${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	${CMAKE_CURRENT_BINARY_DIR}/install-db-indices.h)

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

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/install-db-indices.h
	COMMAND
		txttoheader install_db_indices ${niven_installation_manager_SOURCE_DIR}/sql/install-db-indices.sql > ${CMAKE_CURRENT_BINARY_DIR}/install-db-indices.h
	DEPENDS
		${niven_installation_manager_SOURCE_DIR}/sql/install-db-indices.sql
	)

SET(SOURCES
    src/GeneratorMain.cpp)

FIND_PACKAGE(OpenSSL)
FIND_PACKAGE(ZLIB)

FIND_PACKAGE(Boost 1.55.0 REQUIRED QUIET COMPONENTS program_options filesystem system)

ADD_EXECUTABLE(kyla.Generator ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(kyla.Generator
	kyla.Core pugixml sqlite
	${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ZLIB::ZLIB spdlog)
TARGET_INCLUDE_DIRECTORIES(kyla.Generator
	PRIVATE ${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${OPENSSL_INCLUDE_DIR})
SET_PROPERTY(TARGET kyla.Generator PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET kyla.Generator PROPERTY CXX_STANDARD_REQUIRED ON)
