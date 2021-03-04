# platform independent code
set (PANOPTES_LIBRARY_INCLUDES
    "${PANOPTES_INCLUDE_DIR}/pfw/internal/definitions.h"
    "${PANOPTES_INCLUDE_DIR}/pfw/Event.h"
    "${PANOPTES_INCLUDE_DIR}/pfw/FileSystemWatcher.h"
    "${PANOPTES_INCLUDE_DIR}/pfw/Filter.h"
    "${PANOPTES_INCLUDE_DIR}/pfw/Listener.h"
    "${PANOPTES_INCLUDE_DIR}/pfw/NativeInterface.h"
    "${PANOPTES_INCLUDE_DIR}/pfw/SingleshotSemaphore.h"
)

set (PANOPTES_LIBRARY_SOURCES
    Filter.cpp
    NativeInterface.cpp
    FileSystemWatcher.cpp
)

if (WIN32)
    message (STATUS "compiling windows specific file system service")
    set (PANOPTES_LIBRARY_INCLUDES ${PANOPTES_LIBRARY_INCLUDES}
        "${PANOPTES_INCLUDE_DIR}/pfw/win32/Collector.h"
        "${PANOPTES_INCLUDE_DIR}/pfw/win32/Controller.h"
        "${PANOPTES_INCLUDE_DIR}/pfw/win32/Watcher.h"
        "${PANOPTES_INCLUDE_DIR}/pfw/win32/WindowsHeader.h"
    )
    set (PANOPTES_LIBRARY_SOURCES ${PANOPTES_LIBRARY_SOURCES}
        win32/Collector.cpp
        win32/Controller.cpp
        win32/Watcher.cpp
    )
endif(WIN32)

if (UNIX)
    if (APPLE)
        message (STATUS "compiling macOS specific file system service")
        set (PANOPTES_LIBRARY_INCLUDES ${PANOPTES_LIBRARY_INCLUDES}
            "${PANOPTES_INCLUDE_DIR}/pfw/osx/FSEventsService.h"
            "${PANOPTES_INCLUDE_DIR}/pfw/osx/OSXHeader.h"
            "${PANOPTES_INCLUDE_DIR}/pfw/osx/RunLoop.h"
        )
        set (PANOPTES_LIBRARY_SOURCES ${PANOPTES_LIBRARY_SOURCES}
            osx/RunLoop.cpp
            osx/FSEventsService.cpp
        )
    else (APPLE)
        message (STATUS "compiling linux specific file system service")
        set (PANOPTES_LIBRARY_INCLUDES ${PANOPTES_LIBRARY_INCLUDES}
            "${PANOPTES_INCLUDE_DIR}/pfw/linux/Collector.h"
            "${PANOPTES_INCLUDE_DIR}/pfw/linux/InotifyEventLoop.h"
            "${PANOPTES_INCLUDE_DIR}/pfw/linux/InotifyNode.h"
            "${PANOPTES_INCLUDE_DIR}/pfw/linux/InotifyService.h"
            "${PANOPTES_INCLUDE_DIR}/pfw/linux/InotifyTree.h"
        )
        set (PANOPTES_LIBRARY_SOURCES ${PANOPTES_LIBRARY_SOURCES}
            linux/Collector.cpp
            linux/InotifyEventLoop.cpp
            linux/InotifyNode.cpp
            linux/InotifyService.cpp
            linux/InotifyTree.cpp
        )
    endif(APPLE)
endif (UNIX)

add_library(${PANOPTES_LIBRARY_NAME} STATIC ${PANOPTES_LIBRARY_INCLUDES} ${PANOPTES_LIBRARY_SOURCES})
target_include_directories(${PANOPTES_LIBRARY_NAME} PUBLIC ${PANOPTES_INCLUDE_DIR})
set_target_properties(${PANOPTES_LIBRARY_NAME} PROPERTIES CXX_STANDARD 17)

if (UNIX)
  if (APPLE)
    target_link_libraries(${PANOPTES_LIBRARY_NAME} PUBLIC ${CORE_SERVICES})
  else (APPLE)
    target_link_libraries(${PANOPTES_LIBRARY_NAME} PUBLIC "-pthread")
  endif (APPLE)
endif (UNIX)

install(TARGETS ${PANOPTES_LIBRARY_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
