QT += core gui widgets testlib
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = ui_tests
win32-g++:QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
include(../Control/Control.pri)
include(../Interface/Interface.pri)
INCLUDEPATH += $$PWD/../Application
HEADERS += $$PWD/../Application/main_window.h
SOURCES += $$PWD/../Application/main_window.cpp $$PWD/ui_tests.cpp
FORMS += $$PWD/../Application/main_window.ui
