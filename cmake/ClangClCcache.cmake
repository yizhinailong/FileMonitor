include_guard(GLOBAL)

# ccache treats clang-cl's -clang: pass-through options as unsupported and
# falls back to the real compiler. CMake emits such options for header
# dependency tracking and for the C++23 standard flag, so replace both with
# native clang-cl options to keep compilations cacheable.

foreach(lang IN ITEMS C CXX)
    if(CMAKE_${lang}_COMPILER_ID STREQUAL "Clang" AND
        CMAKE_${lang}_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # Track headers with /showIncludes instead of -clang:-MD/-MT/-MF.
        set(CMAKE_DEPFILE_FLAGS_${lang} "/showIncludes")
        set(CMAKE_${lang}_DEPFILE_FORMAT msvc)
    endif()
endforeach()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
    CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("/std:c++23preview" FILE_MONITOR_CLANG_CL_HAS_STD_CXX23PREVIEW)
    if(FILE_MONITOR_CLANG_CL_HAS_STD_CXX23PREVIEW)
        # Use /std:c++23preview instead of -clang:-std=c++23.
        set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "/std:c++23preview")
        set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "/std:c++23preview")
    endif()
endif()
