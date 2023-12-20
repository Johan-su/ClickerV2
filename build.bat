@echo off
set CLANG=clang++
set WARNINGS=-Wall -Wpedantic -Wextra -Wconversion -Wshadow -Wno-c++20-designator -Wno-c++17-extensions -Wno-gnu-anonymous-struct -Wno-nested-anon-types
set LIBS=-l User32.lib
set FLAGS= -D _DEBUG -g -gcodeview %WARNINGS%
@REM set FLAGS=-O3 -g -gcodeview %WARNINGS%


if not exist build mkdir build



cd ./build
%CLANG% ../src/main.cpp %FLAGS% %LIBS% -o Clickerv2.exe
cd ..