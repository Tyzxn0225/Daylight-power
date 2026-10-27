#pragma once
namespace StationAftermath
{
inline bool Available(bool Settled,bool FilmSeen){return Settled&&FilmSeen;}
inline int Mask(int Heard){return Heard&7;}
inline int Next(int Heard){for(int I=0;I<3;++I)if(!(Mask(Heard)&(1<<I)))return I;return 3;}
inline int Record(int Heard,int Person){return Person>=0&&Person<3?Mask(Heard)|(1<<Person):Mask(Heard);}
inline int Count(int Heard){const int M=Mask(Heard);return (M&1)+((M>>1)&1)+((M>>2)&1);}
}
