# Control：队列、偏好与文件事务，只调用 Interface，不依赖窗口或解析字节流。
INCLUDEPATH += $$PWD
HEADERS += $$PWD/conversion_types.h $$PWD/conversion_controller.h $$PWD/output_file_manager.h $$PWD/settings_store.h
SOURCES += $$PWD/conversion_controller.cpp $$PWD/output_file_manager.cpp $$PWD/settings_store.cpp
