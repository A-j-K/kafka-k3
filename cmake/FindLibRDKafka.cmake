# - Try to find LibRDKafka headers and libraries.
#
# Usage of this module as follows:
#
#     find_package(LibRDKafka)
#
# A minimum required version can also be specified, for example:
#
#     find_package(LibRDKafka 0.11.1)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LibRDKafka_ROOT_DIR  Set this variable to the root installation of
#                    LibRDKafka if the module has problems finding
#                    the proper installation path.
#
# Variables defined by this module:
#
#  LIBRDKAFKA_FOUND              System has LibRDKafka libs/headers
#  LibRDKafka_LIBRARIES          The LibRDKafka libraries
#  LibRDKafka_INCLUDE_DIR        The location of LibRDKafka headers

MACRO(HEXCHAR2DEC VAR VAL)
    IF (${VAL} MATCHES "[0-9]")
        SET(${VAR} ${VAL})
    ELSEIF(${VAL} MATCHES "[aA]")
        SET(${VAR} 10)
    ELSEIF(${VAL} MATCHES "[bB]")
        SET(${VAR} 11)
    ELSEIF(${VAL} MATCHES "[cC]")
        SET(${VAR} 12)
    ELSEIF(${VAL} MATCHES "[dD]")
        SET(${VAR} 13)
    ELSEIF(${VAL} MATCHES "[eE]")
        SET(${VAR} 14)
    ELSEIF(${VAL} MATCHES "[fF]")
        SET(${VAR} 15)
    ELSE()
        MESSAGE(FATAL_ERROR "Invalid format for hexidecimal character")
    ENDIF()
ENDMACRO(HEXCHAR2DEC)

MACRO(HEX2DEC VAR VAL)

    SET(CURINDEX 0)
    STRING(LENGTH "${VAL}" CURLENGTH)

    SET(${VAR} 0)

    WHILE(CURINDEX LESS CURLENGTH)
        STRING(SUBSTRING "${VAL}" ${CURINDEX} 1 CHAR)
        HEXCHAR2DEC(CHAR ${CHAR})
        MATH(EXPR POWAH "(1<<((${CURLENGTH}-${CURINDEX}-1)*4))")
        MATH(EXPR CHAR "(${CHAR}*${POWAH})")
        MATH(EXPR ${VAR} "${${VAR}}+${CHAR}")
        MATH(EXPR CURINDEX "${CURINDEX}+1")
    ENDWHILE()
ENDMACRO(HEX2DEC)

MACRO(HEX2DECVERSION DECVERSION HEXVERSION)
    STRING(SUBSTRING "${HEXVERSION}" 0 2 MAJOR_HEX)
    HEX2DEC(MAJOR_DEC ${MAJOR_HEX})
    STRING(SUBSTRING "${HEXVERSION}" 2 2 MINOR_HEX)
    HEX2DEC(MINOR_DEC ${MINOR_HEX})
    STRING(SUBSTRING "${HEXVERSION}" 4 2 REVISION_HEX)
    HEX2DEC(REVISION_DEC ${REVISION_HEX})
    SET(${DECVERSION} ${MAJOR_DEC}.${MINOR_DEC}.${REVISION_DEC})
ENDMACRO(HEX2DECVERSION)

IF(NOT LibRDKafka_FIND_VERSION)
    IF(NOT LibRDKafka_FIND_VERSION_MAJOR)
        SET(LibRDKafka_FIND_VERSION_MAJOR 0)
    ENDIF(NOT LibRDKafka_FIND_VERSION_MAJOR)
    IF(NOT LibRDKafka_FIND_VERSION_MINOR)
        SET(LibRDKafka_FIND_VERSION_MINOR 11)
    ENDIF(NOT LibRDKafka_FIND_VERSION_MINOR)
    IF(NOT LibRDKafka_FIND_VERSION_PATCH)
        SET(LibRDKafka_FIND_VERSION_PATCH 0)
    ENDIF(NOT LibRDKafka_FIND_VERSION_PATCH)
    SET(LibRDKafka_FIND_VERSION "${LibRDKafka_FIND_VERSION_MAJOR}.${LibRDKafka_FIND_VERSION_MINOR}.${LibRDKafka_FIND_VERSION_PATCH}")
ENDIF(NOT LibRDKafka_FIND_VERSION)

MACRO(_LibRDKafka_check_version)
    FILE(READ "${LibRDKafka_INCLUDE_DIR}/librdkafka/rdkafka.h" _LibRDKafka_version_header)
    STRING(REGEX MATCH "define[ \t]+RD_KAFKA_VERSION[ \t]+0x+([a-fA-F0-9]+)" _LibRDKafka_version_match "${_LibRDKafka_version_header}")
    SET(LIBRDKAKFA_VERSION_HEX "${CMAKE_MATCH_1}")
    HEX2DECVERSION(LibRDKafka_VERSION ${LIBRDKAKFA_VERSION_HEX})
    IF(${LibRDKafka_VERSION} VERSION_LESS ${LibRDKafka_FIND_VERSION})
        SET(LibRDKafka_VERSION_OK FALSE)
    ELSE(${LibRDKafka_VERSION} VERSION_LESS ${LibRDKafka_FIND_VERSION})
        SET(LibRDKafka_VERSION_OK TRUE)
    ENDIF(${LibRDKafka_VERSION} VERSION_LESS ${LibRDKafka_FIND_VERSION})
    IF(NOT LibRDKafka_VERSION_OK)
        MESSAGE(SEND_ERROR "LibRDKafka version ${LibRDKafka_VERSION} found in ${LibRDKafka_INCLUDE_DIR}, "
                "but at least version ${LibRDKafka_FIND_VERSION} is required")
    ENDIF(NOT LibRDKafka_VERSION_OK)
ENDMACRO(_LibRDKafka_check_version)

FIND_PATH(LibRDKafka_ROOT_DIR
        NAMES include/librdkafka/rdkafkacpp.h
        PATHS /usr/local
        )   
FIND_PATH(LibRDKafka_INCLUDE_DIR
        NAMES librdkafka/rdkafkacpp.h
        HINTS ${LibRDKafka_ROOT_DIR}/include
        )
FIND_LIBRARY(LibRDKafka
        NAMES rdkafka++ librdkafkacpp
        HINTS ${LibRDKafka_ROOT_DIR}/lib
        )
FIND_LIBRARY(LibRDKafka_DEBUG
        NAMES librdkafkacpp_D
        HINTS ${LibRDKafka_ROOT_DIR}/lib
        )
FIND_LIBRARY(LibRDKafka_C
        NAMES rdkafka librdkafka
        HINTS ${LibRDKafka_ROOT_DIR}/lib
        )
FIND_LIBRARY(LibRDKafka_C_DEBUG
        NAMES librdkafka_D
        HINTS ${LIBRDKafka_ROOT_DIR}/lib
        )

IF(LibRDKafka_INCLUDE_DIR)
    _LibRDKafka_check_version()
ENDIF()
        
IF(LibRDKafka_DEBUG)
SET(LibRDKafka_LIBRARIES optimized ${LibRDKafka}
                          debug ${LibRDKafka_DEBUG}
)
ELSE ()
SET(LibRDKafka_LIBRARIES ${LibRDKafka})
ENDIF()

IF(LibRDKafka_C_DEBUG)
SET(LibRDKafka_C_LIBRARIES optimized ${LibRDKafka_C}
                            debug ${LibRDKafka_C_DEBUG}
)
ELSE()
SET(LibRDKafka_C_LIBRARIES ${LibRDKafka_C}) 
ENDIF()

INCLUDE(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibRDKafka DEFAULT_MSG
        LibRDKafka_LIBRARIES
        LibRDKafka_C_LIBRARIES
        LibRDKafka_INCLUDE_DIR
        )

MARK_AS_ADVANCED(
        LibRDKafka_ROOT_DIR
        LibRDKafka_LIBRARIES
        LibRDKafka_C_LIBRARIES
        LibRDKafka_INCLUDE_DIR
)
