PROJECT(kylabase_test)

SET(SOURCES
    Hash_test.cpp
	main.cpp)

ADD_EXECUTABLE(kylabase_test ${SOURCES})
TARGET_LINK_LIBRARIES(kylabase_test
	kylabase catch)

SET_PROPERTY(TARGET kylabase_test PROPERTY CXX_STANDARD 17)
SET_PROPERTY(TARGET kylabase_test PROPERTY CXX_STANDARD_REQUIRED ON)

ADD_TEST(NAME kylabase_test COMMAND kylabase_test)