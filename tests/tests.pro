QT += core gui testlib
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = conversion_tests
win32-g++:QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
include(../Control/Control.pri)
include(../Interface/Interface.pri)
SOURCES += $$PWD/conversion_tests.cpp
