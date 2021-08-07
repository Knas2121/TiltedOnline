#pragma once

#include "Message.h"
#include <Structs/ObjectData.h>

using TiltedPhoques::Map;

struct RequestObjectInventoryChanges final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestObjectInventoryChanges;

    RequestObjectInventoryChanges() : ClientMessage(Opcode)
    {
    }

    virtual ~RequestObjectInventoryChanges() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestObjectInventoryChanges& acRhs) const noexcept
    {
        return Changes == acRhs.Changes &&
            GetOpcode() == acRhs.GetOpcode();
    }
    
    Map<GameId, ObjectData> Changes;
};
