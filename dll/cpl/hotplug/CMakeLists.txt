
spec2def(hotplug.dll hotplug.spec)

list(APPEND SOURCE
    hotplug.c
    eject.c
    enum.c)

file(GLOB hotplug_rc_deps resources/*.*)
add_rc_deps(hotplug.rc ${hotplug_rc_deps})

add_library(hotplug MODULE
    ${SOURCE}
    hotplug.rc
    ${CMAKE_CURRENT_BINARY_DIR}/hotplug.def)

set_module_type(hotplug win32dll UNICODE)
add_importlibs(hotplug user32 gdi32 advapi32 setupapi comctl32 msvcrt kernel32 ntdll)
add_cd_file(TARGET hotplug DESTINATION reactos/system32 FOR all)
