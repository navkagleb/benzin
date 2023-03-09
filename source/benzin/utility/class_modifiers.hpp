#pragma once

#define BENZIN_NON_COPYABLE(ClassName)                              \
    ClassName(const ClassName& other) = delete;                     \
    ClassName& operator =(const ClassName& other) = delete;

#define BENZIN_NON_MOVEABLE(ClassName)                              \
    ClassName(ClassName&& other) = delete;                          \
    ClassName& operator =(ClassName&& other) = delete;

#define BENZIN_NON_CONSTRUCTABLE(ClassName)                         \
    ClassName() = delete;                                           \
    BENZIN_NON_COPYABLE(ClassName);                                 \
    BENZIN_NON_MOVEABLE(ClassName);
