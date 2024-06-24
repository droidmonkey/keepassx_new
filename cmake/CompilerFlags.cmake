#  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 or (at your option)
#  version 3 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

macro(add_gcc_compiler_cxxflags FLAGS)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS}")
    endif()
endmacro()

macro(add_gcc_compiler_cflags FLAGS)
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS}")
    endif()
endmacro()

macro(add_gcc_compiler_flags FLAGS)
    add_gcc_compiler_cxxflags("${FLAGS}")
    add_gcc_compiler_cflags("${FLAGS}")
endmacro()

# Copies of above macros that first ensure the compiler understands a given flag
# Because check_*_compiler_flag() sets -D with name, need to provide "safe" FLAGNAME
macro(check_add_gcc_compiler_cxxflag FLAG FLAGNAME)
    check_cxx_compiler_flag("${FLAG}" CXX_HAS${FLAGNAME})
    if(CXX_HAS${FLAGNAME})
        add_gcc_compiler_cxxflags("${FLAG}")
    endif()
endmacro()

macro(check_add_gcc_compiler_cflag FLAG FLAGNAME)
    check_c_compiler_flag("${FLAG}" CC_HAS${FLAGNAME})
    if(CC_HAS${FLAGNAME})
        add_gcc_compiler_cflags("${FLAG}")
    endif()
endmacro()

# This is the "front-end" for the above macros
# Optionally takes additional parameter(s) with language to check (currently "C" or "CXX")
macro(check_add_gcc_compiler_flag FLAG)
    string(REGEX REPLACE "[-=]" "_" FLAGNAME "${FLAG}")
    set(check_lang_spec ${ARGN})
    list(LENGTH check_lang_spec num_extra_args)
    set(langs C CXX)
    if(num_extra_args GREATER 0)
        set(langs "${check_lang_spec}")
    endif()
    if("C" IN_LIST langs)
        check_add_gcc_compiler_cflag("${FLAG}" "${FLAGNAME}")
    endif()
    if("CXX" IN_LIST langs)
        check_add_gcc_compiler_cxxflag("${FLAG}" "${FLAGNAME}")
    endif()
endmacro()
