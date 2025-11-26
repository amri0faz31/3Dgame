# Install script for directory: /home/amri-fazlul/3D_world/build/_deps/assimp-src/code

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "libassimp5.3.0-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/amri-fazlul/3D_world/build/_deps/assimp-build/lib/libassimp.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "assimp-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/assimp" TYPE FILE FILES
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/anim.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/aabb.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/ai_assert.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/camera.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/color4.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/color4.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-build/code/../include/assimp/config.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/ColladaMetaData.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/commonMetaData.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/defs.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/cfileio.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/light.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/material.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/material.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/matrix3x3.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/matrix3x3.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/matrix4x4.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/matrix4x4.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/mesh.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/ObjMaterial.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/pbrmaterial.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/GltfMaterial.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/postprocess.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/quaternion.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/quaternion.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/scene.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/metadata.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/texture.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/types.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/vector2.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/vector2.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/vector3.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/vector3.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/version.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/cimport.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/AssertHandler.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/importerdesc.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Importer.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/DefaultLogger.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/ProgressHandler.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/IOStream.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/IOSystem.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Logger.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/LogStream.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/NullLogger.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/cexport.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Exporter.hpp"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/DefaultIOStream.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/DefaultIOSystem.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/ZipArchiveIOSystem.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/SceneCombiner.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/fast_atof.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/qnan.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/BaseImporter.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Hash.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/MemoryIOWrapper.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/ParsingUtils.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/StreamReader.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/StreamWriter.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/StringComparison.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/StringUtils.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/SGSpatialSort.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/GenericProperty.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/SpatialSort.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/SkeletonMeshBuilder.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/SmallVector.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/SmoothingGroups.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/SmoothingGroups.inl"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/StandardShapes.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/RemoveComments.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Subdivision.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Vertex.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/LineSplitter.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/TinyFormatter.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Profiler.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/LogAux.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Bitmap.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/XMLTools.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/IOStreamBuffer.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/CreateAnimMesh.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/XmlParser.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/BlobIOSystem.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/MathFunctions.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Exceptional.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/ByteSwapper.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Base64.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "assimp-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/assimp/Compiler" TYPE FILE FILES
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Compiler/pushpack1.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Compiler/poppack1.h"
    "/home/amri-fazlul/3D_world/build/_deps/assimp-src/code/../include/assimp/Compiler/pstdint.h"
    )
endif()

