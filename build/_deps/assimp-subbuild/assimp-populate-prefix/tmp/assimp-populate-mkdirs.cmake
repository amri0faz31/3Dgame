# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/amri-fazlul/3D_world/build/_deps/assimp-src"
  "/home/amri-fazlul/3D_world/build/_deps/assimp-build"
  "/home/amri-fazlul/3D_world/build/_deps/assimp-subbuild/assimp-populate-prefix"
  "/home/amri-fazlul/3D_world/build/_deps/assimp-subbuild/assimp-populate-prefix/tmp"
  "/home/amri-fazlul/3D_world/build/_deps/assimp-subbuild/assimp-populate-prefix/src/assimp-populate-stamp"
  "/home/amri-fazlul/3D_world/build/_deps/assimp-subbuild/assimp-populate-prefix/src"
  "/home/amri-fazlul/3D_world/build/_deps/assimp-subbuild/assimp-populate-prefix/src/assimp-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/amri-fazlul/3D_world/build/_deps/assimp-subbuild/assimp-populate-prefix/src/assimp-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/amri-fazlul/3D_world/build/_deps/assimp-subbuild/assimp-populate-prefix/src/assimp-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
