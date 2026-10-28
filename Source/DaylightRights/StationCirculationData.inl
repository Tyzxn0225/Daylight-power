#pragma once
namespace StationCirculation30
{
// Existing V16 door centres and approaches in world centimetres. Every
// connection must still pass body, head-clearance and floor checks.
static const FVector InteriorDoorNodes[]={
 // Workshop.
 FVector(-3800,3546,0),FVector(-3800,3416,19),FVector(-3800,3286,19),
 FVector(-3800,2444,0),FVector(-3800,2574,19),FVector(-3800,2704,19),
 // Kitchen and home.
 FVector(-4750,2268,0),FVector(-4750,2138,19),FVector(-4750,2008,19),
 FVector(-4750,1169,0),FVector(-4750,1299,19),FVector(-4750,1429,19),
 FVector(-3800,2362,0),FVector(-3800,2232,19),FVector(-3800,2102,19),
 // Clinic.
 FVector(-5720,-163,0),FVector(-5720,-293,19),FVector(-5720,-423,19),
 FVector(-5720,-1254,0),FVector(-5720,-1124,19),FVector(-5720,-994,19),
 // Cinema west/east doors and side aisles.
 FVector(-4319,-3160,0),FVector(-4189,-3160,19),FVector(-4059,-3160,19),
 FVector(-3270,-3160,0),FVector(-3400,-3160,19),FVector(-3530,-3160,19),
 FVector(-4080,-3460,19),FVector(-4080,-2900,19),
 FVector(-3520,-3460,19),FVector(-3520,-2900,19)
};
// Floor heights in centimetres. Existing stair and bridge, not new geometry.
static const FVector TerraceNodes[]={
 FVector(-3005,-2680,0),
 FVector(-3005,-2800,15),FVector(-3005,-2830,30),
 FVector(-3005,-2860,45),FVector(-3005,-2890,60),
 FVector(-3005,-2920,75),FVector(-3005,-2950,90),
 FVector(-3005,-2980,105),FVector(-3005,-3010,120),
 FVector(-3005,-3040,135),FVector(-3005,-3070,150),
 FVector(-3005,-3135,150),FVector(-2875,-3135,150),
 FVector(-2875,-3070,165),FVector(-2875,-3040,180),
 FVector(-2875,-3010,195),FVector(-2875,-2980,210),
 FVector(-2875,-2950,225),FVector(-2875,-2920,240),
 FVector(-2875,-2890,255),FVector(-2875,-2860,270),
 FVector(-2875,-2830,285),FVector(-2875,-2800,300),
 FVector(-2875,-2750,300),FVector(-2680,-2750,300),
 FVector(-2680,-2940,300),FVector(-2680,-3180,300),
 FVector(-2680,-3360,300)
};
}
