#pragma once

#define BENZIN_NON_COPYABLE_IMPL(ClassName) \
    ClassName(const ClassName& other) = delete; \
    ClassName& operator =(const ClassName& other) = delete;

#define BENZIN_NON_MOVEABLE_IMPL(ClassName) \
    ClassName(ClassName&& other) = delete; \
    ClassName& operator =(ClassName&& other) = delete;

#define BENZIN_NON_CONSTRUCTABLE_IMPL(ClassName) \
    ClassName() = delete; \
    BENZIN_NON_COPYABLE_IMPL(ClassName); \
    BENZIN_NON_MOVEABLE_IMPL(ClassName);
