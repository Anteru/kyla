PROJECT(nim_generator)

SET(HEADERS
	${CMAKE_CURRENT_BINARY_DIR}/build-db-structure.h
	${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h)

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/build-db-structure.h
	COMMAND
		txttoheader build_db_structure ${niven_installation_manager_SOURCE_DIR}/sql/install-db-structure.sql > ${CMAKE_CURRENT_BINARY_DIR}/build-db-structure.h
	)

ADD_CUSTOM_COMMAND(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	COMMAND
		txttoheader install_db_structure ${niven_installation_manager_SOURCE_DIR}/sql/install-db-structure.sql > ${CMAKE_CURRENT_BINARY_DIR}/install-db-structure.h
	)


SET(SOURCES
    src/GeneratorMain.cpp)

MESSAGE("${niven_installation_manager_SOURCE_DIR}")

FIND_PACKAGE(OpenSSL)
FIND_PACKAGE(ZLIB)
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

# Needed for log
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
FIND_PACKAGE(Boost 1.55.0 REQUIRED COMPONENTS program_options filesystem system log)

ADD_EXECUTABLE(nim.Generator ${SOURCES} ${HEADERS})
TARGET_LINK_LIBRARIES(nim.Generator nim.Common pugixml sqlite ${Boost_LIBRARIES} ZLIB::ZLIB)
TARGET_INCLUDE_DIRECTORIES(nim.Generator
	PUBLIC ${Boost_INCLUDE_DIRS}
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
SET_PROPERTY(TARGET nim.Generator PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET nim.Generator PROPERTY CXX_STANDARD_REQUIRED ON)
