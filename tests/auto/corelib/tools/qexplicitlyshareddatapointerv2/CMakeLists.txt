# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## tst_qexplicitlyshareddatapointerv2 Test:
#####################################################################

if(NOT QT_BUILD_STANDALONE_TESTS AND NOT QT_BUILDING_QT)
    cmake_minimum_required(VERSION 3.16)
    project(tst_qexplicitlyshareddatapointerv2 LANGUAGES CXX)
    find_package(Qt6BuildInternals REQUIRED COMPONENTS STANDALONE_TEST)
endif()

qt_internal_add_test(tst_qexplicitlyshareddatapointerv2
    SOURCES
        tst_qexplicitlyshareddatapointerv2.cpp
    LIBRARIES
        Qt::TestPrivate
)
