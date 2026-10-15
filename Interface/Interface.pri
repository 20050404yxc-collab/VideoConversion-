# Interface：外部工具、媒体解析及原生播放适配，不依赖上层业务或窗口实现。
INCLUDEPATH += $$PWD
HEADERS += $$PWD/media_range.h $$PWD/media_playback_service.h
SOURCES += $$PWD/media_playback_service.cpp
HEADERS += $$PWD/media_types.h $$PWD/ffmpeg_arguments.h $$PWD/progress_parser.h $$PWD/ffmpeg_service.h
SOURCES += $$PWD/media_types.cpp $$PWD/ffmpeg_arguments.cpp $$PWD/progress_parser.cpp $$PWD/ffmpeg_service.cpp
