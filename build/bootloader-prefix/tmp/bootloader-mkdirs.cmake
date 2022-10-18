# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "F:/ESP/esp-idfv4.4.2/esp-idf/components/bootloader/subproject"
  "C:/Users/Administrator/Desktop/github/car-ppm-mqtt/build/bootloader"
  "C:/Users/Administrator/Desktop/github/car-ppm-mqtt/build/bootloader-prefix"
  "C:/Users/Administrator/Desktop/github/car-ppm-mqtt/build/bootloader-prefix/tmp"
  "C:/Users/Administrator/Desktop/github/car-ppm-mqtt/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/Administrator/Desktop/github/car-ppm-mqtt/build/bootloader-prefix/src"
  "C:/Users/Administrator/Desktop/github/car-ppm-mqtt/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Administrator/Desktop/github/car-ppm-mqtt/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
