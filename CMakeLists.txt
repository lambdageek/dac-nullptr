cmake_minimum_required(VERSION 3.20)

# Set the project name
project(MiniLegacyDac CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_FLAGS -g)

set(SOURCES
  daccess.h
  miniSample.cpp
)


add_executable(miniSample ${SOURCES})
