#pragma once

#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Stl.hpp>
#include <Structs/GameId.h>
#include <Structs/LockData.h>
#include <Structs/Inventory.h>
#include <Structs/GridCellCoords.h>

using TiltedPhoques::Map;

struct ObjectData
{
    ObjectData() = default;
    ~ObjectData() = default;

    bool operator==(const ObjectData& acRhs) const noexcept;
    bool operator!=(const ObjectData& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    GameId Id{};
    GameId CellId{};
    GameId WorldSpaceId{};
    GridCellCoords CurrentCoords{};
    LockData CurrentLockData{};
    Inventory CurrentInventory{};
};
