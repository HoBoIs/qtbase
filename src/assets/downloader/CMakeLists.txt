# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

qt_internal_add_module(ExamplesAssetDownloaderPrivate
    CONFIG_MODULE_NAME examples_asset_downloader
    STATIC
    INTERNAL_MODULE
    SOURCES
        assetdownloader.cpp assetdownloader.h
        tasking/barrier.cpp tasking/barrier.h
        tasking/concurrentcall.h
        tasking/networkquery.cpp tasking/networkquery.h
        tasking/qprocesstask.cpp tasking/qprocesstask.h
        tasking/tasking_global.h
        tasking/tasktree.cpp tasking/tasktree.h
        tasking/tasktreerunner.cpp tasking/tasktreerunner.h
    DEFINES
        QT_NO_CAST_FROM_ASCII
    PUBLIC_LIBRARIES
        Qt6::Concurrent
        Qt6::Core
        Qt6::CorePrivate
        Qt6::Network
    NO_GENERATE_CPP_EXPORTS
)

