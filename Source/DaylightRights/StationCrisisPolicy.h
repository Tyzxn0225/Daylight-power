#pragma once
namespace StationCrisis
{
    constexpr int Capacity=36,Minimum=12,Maximum=24;
    constexpr bool Valid(int Poor){return Poor>=Minimum&&Poor<=Maximum&&Poor%2==0;}
    constexpr int Other(int Poor){return Capacity-Poor;}
    constexpr int Clamp(int Value){return Value<Minimum?Minimum:Value>Maximum?Maximum:Value;}
    constexpr int Tier(int Flow){return Flow>=24?2:Flow>=18?1:0;}
    static_assert(Other(12)==24&&Other(24)==12&&Valid(18)&&!Valid(26),"Conserved temporary heat capacity");
}
