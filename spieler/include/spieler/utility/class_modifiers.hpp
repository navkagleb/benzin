#pragma once

#define SPIELER_NON_COPYABLE(ClassName) \
    ClassName(const ClassName& other) = delete; \
    ClassName& operator =(const ClassName& other) = delete

#define SPIELER_NON_MOVEABLE(ClassName) \
    ClassName(ClassName&& other) = delete; \
    ClassName& operator =(ClassName&& other) = delete

#define SPIELER_NON_CONSTRUCTABLE(ClassName) \
    ClassName() = delete; \
    SPIELER_NON_COPYABLE(ClassName); \
    SPIELER_NON_MOVEABLE(ClassName)
