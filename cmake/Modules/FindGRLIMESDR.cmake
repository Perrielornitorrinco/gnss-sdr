# GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
# This file is part of GNSS-SDR.
#
# SPDX-FileCopyrightText: 2011-2020 C. Fernandez-Prades cfernandez(at)cttc.es
# SPDX-License-Identifier: BSD-3-Clause

# Tries to find gr-limesdr.
#
# Usage of this module as follows:
#
# find_package(GRLIMESDR)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
# GrLimeSDR_ROOT_DIR Set this variable to the root installation of
# gr-limesdr if the module has problems finding
# the proper installation path.
#
# Variables defined by this module:
#
# GRLIMESDR_FOUND System has gr-limesdr libs/headers
# GRLIMESDR_LIBRARIES The gr-limesdr libraries (gnuradio-limesdr)
# GRLIMESDR_INCLUDE_DIR The location of gr-limesdr headers
#
# Provides the following imported target:
# Gnuradio::limesdr
#

if(NOT COMMAND feature_summary)
    include(FeatureSummary)
endif()

if(NOT PKG_CONFIG_FOUND)
    include(FindPkgConfig)
endif()

pkg_check_modules(GRLIMESDR_PKG gnuradio-limesdr)

if(NOT GRLIMESDR_ROOT)
    set(GRLIMESDR_ROOT_USER_DEFINED /usr)
else()
    set(GRLIMESDR_ROOT_USER_DEFINED ${GRLIMESDR_ROOT})
endif()
if(DEFINED ENV{GRLIMESDR_ROOT})
    set(GRLIMESDR_ROOT_USER_DEFINED
        ${GRLIMESDR_ROOT_USER_DEFINED}
        $ENV{GRLIMESDR_ROOT}
    )
endif()

find_path(GRLIMESDR_INCLUDE_DIR
    NAMES
        limesdr/source.h
        limesdr/api.h
    HINTS
        ${GRLIMESDR_PKG_INCLUDEDIR}
    PATHS
        ${GRLIMESDR_ROOT_USER_DEFINED}/include
        /usr/include
        /usr/local/include
        /opt/local/include
)

find_library(GRLIMESDR_LIBRARIES
    NAMES
        gnuradio-limesdr
    HINTS
        ${GRLIMESDR_PKG_LIBDIR}
    PATHS
        ${GRLIMESDR_ROOT_USER_DEFINED}/lib
        ${GRLIMESDR_ROOT_USER_DEFINED}/lib64
        /usr/lib
        /usr/lib64
        /usr/lib/x86_64-linux-gnu
        /usr/lib/i386-linux-gnu
        /usr/lib/arm-linux-gnueabihf
        /usr/lib/arm-linux-gnueabi
        /usr/lib/aarch64-linux-gnu
        /usr/lib/mipsel-linux-gnu
        /usr/lib/mips-linux-gnu
        /usr/lib/mips64el-linux-gnuabi64
        /usr/lib/powerpc-linux-gnu
        /usr/lib/powerpc64-linux-gnu
        /usr/lib/powerpc64le-linux-gnu
        /usr/lib/powerpc-linux-gnuspe
        /usr/lib/hppa-linux-gnu
        /usr/lib/s390x-linux-gnu
        /usr/lib/i386-gnu
        /usr/lib/hppa-linux-gnu
        /usr/lib/x86_64-kfreebsd-gnu
        /usr/lib/i386-kfreebsd-gnu
        /usr/lib/m68k-linux-gnu
        /usr/lib/sh4-linux-gnu
        /usr/lib/sparc64-linux-gnu
        /usr/lib/x86_64-linux-gnux32
        /usr/lib/riscv64-linux-gnu
        /usr/lib/alpha-linux-gnu
        /usr/local/lib
        /usr/local/lib64
        /opt/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GRLIMESDR DEFAULT_MSG GRLIMESDR_LIBRARIES GRLIMESDR_INCLUDE_DIR)

if(GRLIMESDR_PKG_VERSION)
    set(GRLIMESDR_VERSION_AUX ${GRLIMESDR_PKG_VERSION})
    string(REGEX REPLACE "^v" "" GRLIMESDR_VERSION ${GRLIMESDR_VERSION_AUX})
endif()

set_package_properties(GRLIMESDR PROPERTIES
    URL "https://github.com/myriadrf/gr-limesdr"
)

if(GRLIMESDR_FOUND AND GRLIMESDR_VERSION)
    set_package_properties(GRLIMESDR PROPERTIES
        DESCRIPTION "limesdr GNU Radio blocks (found: v${GRLIMESDR_VERSION})"
    )
else()
    set_package_properties(GRLIMESDR PROPERTIES
        DESCRIPTION "limesdr GNU Radio blocks"
    )
endif()

if(GRLIMESDR_FOUND AND NOT TARGET Gnuradio::limesdr)
    add_library(Gnuradio::limesdr SHARED IMPORTED)
    set_target_properties(Gnuradio::limesdr PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${GRLIMESDR_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${GRLIMESDR_INCLUDE_DIR};${GRLIMESDR_INCLUDE_DIR}/limesdr"
        INTERFACE_LINK_LIBRARIES "${GRLIMESDR_LIBRARIES}"
    )

	#check for PPS custom version
	file(READ ${GRLIMESDR_INCLUDE_DIR}/limesdr/source.h TMPTXT)

	string(FIND "${TMPTXT}" "enable_PPS_mode" matchres)

	message(STATUS ${matchres})

	if(${matchres} EQUAL -1)
	     message("Using standard gr-limesdr library ")
	else()
	     set(GRLIMESDR_PPS True)
             message("Using custom gr-limesdr library with PPS support ")
	endif ()

endif()

mark_as_advanced(GRLIMESDR_LIBRARIES GRLIMESDR_INCLUDE_DIR)
