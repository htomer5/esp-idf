if(BOOTLOADER_BUILD)
    # Bootloader builds need the platform_include directory (for assert.h), but nothing else
    idf_component_register(INCLUDE_DIRS platform_include)
    return()
endif()

set(srcs
    "abort.c"
    "assert.c"
    "heap.c"
    "locks.c"
    "poll.c"
    "pthread.c"
    "random.c"
    "reent_init.c"
    "newlib_init.c"
    "syscalls.c"
    "termios.c"
    "stdatomic.c"
    "time.c")
set(include_dirs platform_include)

if(CONFIG_SPIRAM_CACHE_WORKAROUND)
    set(ldfragments "esp32-spiram-rom-functions-c.lf")
endif()

list(APPEND ldfragments "newlib.lf" "system_libs.lf")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS priv_include
                    PRIV_REQUIRES soc spi_flash
                    LDFRAGMENTS "${ldfragments}")

# Toolchain libraries require code defined in this component
idf_component_get_property(newlib newlib COMPONENT_LIB)
target_link_libraries(${COMPONENT_LIB} INTERFACE c m gcc "$<TARGET_FILE:${newlib}>")

set_source_files_properties(heap.c PROPERTIES COMPILE_FLAGS -fno-builtin)

# Forces the linker to include heap, syscall, pthread, assert, and retargetable locks from this component,
# instead of the implementations provided by newlib.
list(APPEND EXTRA_LINK_FLAGS "-u newlib_include_heap_impl")
list(APPEND EXTRA_LINK_FLAGS "-u newlib_include_syscalls_impl")
list(APPEND EXTRA_LINK_FLAGS "-u newlib_include_pthread_impl")
list(APPEND EXTRA_LINK_FLAGS "-u newlib_include_assert_impl")
target_link_libraries(${COMPONENT_LIB} INTERFACE "${EXTRA_LINK_FLAGS}")

if(CONFIG_NEWLIB_NANO_FORMAT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "--specs=nano.specs")
endif()

add_subdirectory(port)
