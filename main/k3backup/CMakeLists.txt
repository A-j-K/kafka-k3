
INCLUDE_DIRECTORIES(
	"${PROJECT_SOURCE_DIR}/src"
	"${LibRDKafka_INCLUDE_DIR}"
)

SET(k3backup_SRCS
        main.cpp
)

IF(BUILD_DEBUG)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
ENDIF(BUILD_DEBUG)

ADD_EXECUTABLE("k3backup"
        ${k3backup_SRCS}
)

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
SET(BUILD_SHARED_LIBRARIES OFF)
SET(CMAKE_EXE_LINKER_FLAGS "-static")

LINK_DIRECTORIES(
	"/usr/lib"
	"/usr/lib/sasl2"
	"/usr/lib64"
	"/usr/local/lib"
	"/usr/local/lib64"
	"${AWSSDK_LIB_DIR}"
)

TARGET_LINK_LIBRARIES("k3backup" PUBLIC "-static"
	k3_lib
	${ALL_LIBS}
)

INSTALL(PROGRAMS ${PROJECT_BINARY_DIR}/k3backup
	DESTINATION /usr/bin
)
