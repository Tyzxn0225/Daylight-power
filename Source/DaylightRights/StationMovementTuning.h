#pragma once

// cm/s. Default travel deliberately matches the previous Shift speed.
namespace StationMovement
{
    constexpr float Stroll = 115.f;
    constexpr float Travel = 340.f;
    constexpr float Sprint = 560.f;
    // CharacterMovement keeps the collision capsule 1.9--2.4 cm above the floor.
    // Counter that clearance on the visual mesh so planted soles reach the road.
    constexpr float FloorClearance = 2.15f;
}
