#include <Structs/ObjectData.h>
#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

bool ObjectData::operator==(const ObjectData& acRhs) const noexcept
{
    return Id == acRhs.Id &&
           CellId == acRhs.CellId &&
           WorldSpaceId == acRhs.WorldSpaceId &&
           CurrentCoords == acRhs.CurrentCoords &&
           CurrentLockData == acRhs.CurrentLockData &&
           CurrentInventory == acRhs.CurrentInventory;
}

bool ObjectData::operator!=(const ObjectData& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

void ObjectData::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Id.Serialize(aWriter);
    CellId.Serialize(aWriter);
    WorldSpaceId.Serialize(aWriter);
    CurrentCoords.Serialize(aWriter);
    CurrentLockData.Serialize(aWriter);
    CurrentInventory.Serialize(aWriter);
}

void ObjectData::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    Id.Deserialize(aReader);
    CellId.Deserialize(aReader);
    WorldSpaceId.Deserialize(aReader);
    CurrentCoords.Deserialize(aReader);
    CurrentLockData.Deserialize(aReader);
    CurrentInventory.Deserialize(aReader);
}
