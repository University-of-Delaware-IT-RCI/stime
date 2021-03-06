CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
PROJECT (stime C)

#
# For installation:
#
INCLUDE(GNUInstallDirs)

#
# For finding packages:
#
INCLUDE(CheckIncludeFiles)
INCLUDE(FindPackageHandleStandardArgs)

IF (NOT SLURM_PREFIX)
  SET (SLURM_PREFIX "/usr/local" CACHE PATH "Directory in which SLURM is installed.")
ENDIF (NOT SLURM_PREFIX)
IF (SLURM_PREFIX)
  LIST(APPEND CMAKE_PREFIX_PATH ${SLURM_PREFIX})
ENDIF (SLURM_PREFIX)

IF (NOT SLURM_SOURCE_DIR)
  SET (SLURM_SOURCE_DIR "${SLURM_PREFIX}/src" CACHE PATH "Directory containing the SLURM source package.")
ENDIF (NOT SLURM_SOURCE_DIR)

IF (NOT SLURM_BUILD_DIR)
  SET (SLURM_BUILD_DIR "" CACHE PATH "Directory containing the SLURM build (e.g. config.h).")
ENDIF (NOT SLURM_BUILD_DIR)

FIND_PATH(SLURM_INCLUDE_DIR NAMES slurm/slurm.h)
FIND_LIBRARY(SLURM_LIBRARY NAMES libslurm.so)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SLURM DEFAULT_MSG SLURM_LIBRARY SLURM_INCLUDE_DIR)
IF (NOT SLURM_FOUND)
  MESSAGE(FATAL_ERROR "SLURM library could not be found")
ELSE (NOT SLURM_FOUND)
  SET (SLURM_LIBRARIES ${SLURM_LIBRARY})
  SET (SLURM_INCLUDE_DIRS ${SLURM_INCLUDE_DIR})
ENDIF (NOT SLURM_FOUND)
MARK_AS_ADVANCED (SLURM_LIBRARIES SLURM_INCLUDE_DIRS)

#
# Validate the SLURM_SOURCE_DIR path
#
FIND_PATH(SLURM_XLATOR_H NAMES src/common/slurm_xlator.h PATHS ${SLURM_SOURCE_DIR})
IF (NOT SLURM_XLATOR_H)
  MESSAGE(FATAL_ERROR "${SLURM_SOURCE_DIR} does not appear to be a SLURM source tree")
ENDIF (NOT SLURM_XLATOR_H)
UNSET (SLURM_XLATOR_H CACHE)
IF (NOT SLURM_BUILD_DIR)
  FIND_PATH(SLURM_BUILD_DIR NAMES config.h PATHS ${SLURM_SOURCE_DIR})
  IF (NOT SLURM_BUILD_DIR)
    MESSAGE (FATAL_ERROR "SLURM build directory not found")
  ENDIF (NOT SLURM_BUILD_DIR)
ENDIF (NOT SLURM_BUILD_DIR)

INCLUDE_DIRECTORIES (BEFORE ${SLURM_INCLUDE_DIRS} ${SLURM_SOURCE_DIR} ${SLURM_BUILD_DIR})
LINK_LIBRARIES (${SLURM_LIBRARIES})

#
# The executable:
#
ADD_EXECUTABLE(stime stime.c)
INSTALL (TARGETS stime DESTINATION ${CMAKE_INSTALL_BINDIR})

