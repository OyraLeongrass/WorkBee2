# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/DaraProgram/prog/_deps/bee2-src"
  "D:/DaraProgram/prog/_deps/bee2-build"
  "D:/DaraProgram/prog/_deps/bee2-subbuild/bee2-populate-prefix"
  "D:/DaraProgram/prog/_deps/bee2-subbuild/bee2-populate-prefix/tmp"
  "D:/DaraProgram/prog/_deps/bee2-subbuild/bee2-populate-prefix/src/bee2-populate-stamp"
  "D:/DaraProgram/prog/_deps/bee2-subbuild/bee2-populate-prefix/src"
  "D:/DaraProgram/prog/_deps/bee2-subbuild/bee2-populate-prefix/src/bee2-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/DaraProgram/prog/_deps/bee2-subbuild/bee2-populate-prefix/src/bee2-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/DaraProgram/prog/_deps/bee2-subbuild/bee2-populate-prefix/src/bee2-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
