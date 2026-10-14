# Interface：外部工具进程、参数与媒体解析，不依赖业务控制或窗口。
INCLUDEPATH += $$PWD
HEADERS += $$PWD/media_types.h $$PWD/ffmpeg_arguments.h $$PWD/progress_parser.h $$PWD/ffmpeg_service.h
SOURCES += $$PWD/media_types.cpp $$PWD/ffmpeg_arguments.cpp $$PWD/progress_parser.cpp $$PWD/ffmpeg_service.cpp
