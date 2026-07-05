# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
#
# Run at *build* time (cmake -P) to regenerate appversion.cpp from the latest
# git tag, so the dev-build commit-count/hash suffix stays current on every
# commit without re-running CMake configure. Because it delegates the write to
# configure_file(), the file is only rewritten when its contents actually
# change, so appversion.cpp recompiles only when the version string moves.
#
# Expects on the command line (-D...):
#   APPVER_SRC   - source dir holding appversion.cpp.in
#   APPVER_BIN   - binary dir to write appversion.cpp into
#   REPO_DIR     - git work tree to describe
#   GIT_EXECUTABLE - path to git

execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${REPO_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
    WORKING_DIRECTORY ${REPO_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

if(NOT GIT_TAG)
    message(FATAL_ERROR
        "thumbgrid version could not be determined: no git tag found "
        "(need git history with at least one vYYYY.M.N tag).")
endif()

# "2026.7.1" for the clean short form; the numeric parts feed QVersionNumber.
string(REGEX REPLACE "^v" "" APPVER_SHORT "${GIT_TAG}")
# "2026.7.1-22-g3bafbd99" on dev builds, with a "-dirty" marker appended while
# the working tree has uncommitted changes; equals the short form exactly on a
# clean tagged release commit.
string(REGEX REPLACE "^v" "" APPVER_FULL "${GIT_DESCRIBE}")

if(NOT APPVER_SHORT MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "thumbgrid tag '${GIT_TAG}' is not vMAJOR.MINOR.PATCH.")
endif()
set(APPVER_MAJOR "${CMAKE_MATCH_1}")
set(APPVER_MINOR "${CMAKE_MATCH_2}")
set(APPVER_PATCH "${CMAKE_MATCH_3}")

# configure_file only touches the output when the rendered content differs,
# which is what keeps needless recompiles from happening every build.
configure_file(
    ${APPVER_SRC}/appversion.cpp.in
    ${APPVER_BIN}/appversion.cpp
    @ONLY)
