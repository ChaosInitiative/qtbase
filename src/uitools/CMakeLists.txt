# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## UiTools Module:
#####################################################################

qt_internal_add_module(UiTools
    SOURCES
        ../tools/designer/src/lib/uilib/abstractformbuilder.cpp ../tools/designer/src/lib/uilib/abstractformbuilder.h
        ../tools/designer/src/lib/uilib/formbuilder.cpp ../tools/designer/src/lib/uilib/formbuilder.h
        ../tools/designer/src/lib/uilib/formbuilderextra.cpp ../tools/designer/src/lib/uilib/formbuilderextra_p.h
        ../tools/designer/src/lib/uilib/properties.cpp ../tools/designer/src/lib/uilib/properties_p.h
        ../tools/designer/src/lib/uilib/resourcebuilder.cpp ../tools/designer/src/lib/uilib/resourcebuilder_p.h
        ../tools/designer/src/lib/uilib/textbuilder.cpp ../tools/designer/src/lib/uilib/textbuilder_p.h
        ../tools/designer/src/lib/uilib/ui4.cpp ../tools/designer/src/lib/uilib/ui4_p.h
        quiloader.cpp quiloader.h quiloader_p.h
        qtuitoolsglobal.h
    DEFINES
        QFORMINTERNAL_NAMESPACE
        QT_DESIGNER
        QT_DESIGNER_STATIC
        QT_USE_QSTRINGBUILDER
    INCLUDE_DIRECTORIES
        ../tools/designer/src/lib/uilib
    LIBRARIES
        Qt::UiPlugin
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Widgets
    NO_GENERATE_CPP_EXPORTS
)

## Scopes:
#####################################################################

qt_internal_extend_target(UiTools CONDITION TARGET Qt::OpenGLWidgets
    PUBLIC_LIBRARIES
        Qt::OpenGLWidgets
)

qt_internal_extend_target(UiTools CONDITION TARGET Qt::OpenGL
    LIBRARIES
        Qt::OpenGL
)
qt_internal_add_docs(UiTools
    doc/qtuitools.qdocconf
)
