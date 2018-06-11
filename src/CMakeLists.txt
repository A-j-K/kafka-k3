

SET(k3_SRCS
	s3.cpp
	utils.cpp
	consume.cpp
	awsguard.cpp
	kafkaconf.cpp
	memchecker.cpp
	messagewrapper.cpp
)

IF(BUILD_DEBUG)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
ENDIF(BUILD_DEBUG)

ADD_LIBRARY(k3_lib
	${k3_SRCS}
)

SET_TARGET_PROPERTIES(k3_lib PROPERTIES LINK_FLAGS "-static" )

TARGET_LINK_LIBRARIES(k3_lib PUBLIC "-static"
	${ALL_LIBS}
)

