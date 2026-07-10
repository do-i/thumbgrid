if(NOT DEFINED PLATFORM_HEADER)
    message(FATAL_ERROR "PLATFORM_HEADER is required")
endif()

if(NOT DEFINED PLATFORM_IMPLEMENTATIONS)
    message(FATAL_ERROR "PLATFORM_IMPLEMENTATIONS is required")
endif()

if(NOT EXISTS "${PLATFORM_HEADER}")
    message(FATAL_ERROR "Platform header not found: ${PLATFORM_HEADER}")
endif()

file(READ "${PLATFORM_HEADER}" header_contents)
string(REGEX MATCHALL "\n[A-Za-z_:<>]+[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\\(" declarations "${header_contents}")

set(function_names)
foreach(declaration IN LISTS declarations)
    string(REGEX REPLACE ".*[A-Za-z_:<>]+[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*\\(.*" "\\1" function_name "${declaration}")
    list(APPEND function_names "${function_name}")
endforeach()
list(REMOVE_DUPLICATES function_names)

if(NOT function_names)
    message(FATAL_ERROR "No platform function declarations found in ${PLATFORM_HEADER}")
endif()

foreach(implementation IN LISTS PLATFORM_IMPLEMENTATIONS)
    if(NOT EXISTS "${implementation}")
        message(FATAL_ERROR "Platform implementation not found: ${implementation}")
    endif()

    file(READ "${implementation}" implementation_contents)
    foreach(function_name IN LISTS function_names)
        string(REGEX MATCH "[A-Za-z_:<>]+[ \t\r\n]+PlatformDesktop::${function_name}[ \t\r\n]*\\(" definition "${implementation_contents}")
        if(NOT definition)
            message(FATAL_ERROR "${implementation} does not define PlatformDesktop::${function_name}")
        endif()
    endforeach()
endforeach()
