# Application：窗口展示、用户交互与入口装配；业务只通过 Control 调用。
INCLUDEPATH += $$PWD
HEADERS += $$PWD/main_window.h $$PWD/preview_dialog.h
SOURCES += $$PWD/main.cpp $$PWD/main_window.cpp $$PWD/preview_dialog.cpp
FORMS += $$PWD/main_window.ui $$PWD/preview_dialog.ui
