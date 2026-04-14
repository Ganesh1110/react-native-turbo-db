#pragma once
#include <jsi/jsi.h>
#include <string>
#include <vector>
#include "ArenaAllocator.h"

namespace secure_db {

enum class BinaryType : uint8_t {
    Null = 1,
    Boolean = 2,
    Number = 3,
    String = 4,
    Object = 5,
    Array = 6
};

class BinarySerializer {
public:
    // Packs an entire deeply-nested JSI value into the ArenaAllocator
    static void serialize(facebook::jsi::Runtime& rt, const facebook::jsi::Value& val, ArenaAllocator& arena);
    
    // Reconstructs a deeply-nested JSI value from raw memory
    // Returns the fully constructed Value and the exact number of bytes it consumed.
    static std::pair<facebook::jsi::Value, size_t> deserialize(facebook::jsi::Runtime& rt, const uint8_t* ptr, size_t remaining_size);
};

}
