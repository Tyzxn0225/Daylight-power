#pragma once
#include <cstdint>
namespace StationAchievements
{
constexpr int Count=22,RowsPerPage=6;
static_assert(Count<32,"Achievement bitset must fit uint32_t");
struct Facts
{
 bool started=false,firstLightComplete=false,heardAllocation=false,metBroker=false,readLedger=false;
 int allocationPolicy=-1,allocationReviews=0,rationMask=-1,rationVisits=0,rationReviews=0;
 bool readRecall=false,seenFamily=false,conflictComplete=false,conflictEvidence=false;
 int conflictWitness=0,conflictHeard=0;
 bool supplyComplete=false;int supplyEvidence=0,supplyMeasured=0,supplyHeard=0;
 bool dutySettled=false;int dutyWork=0;
 bool districtStarted=false,districtSettled=false,seenDistrictEnding=false;int districtEvidence=0,districtChoice=-1;
 int exploration=0,urban=0,quiet=0;bool shortcutOpen=false,shortcutReady=false;
 bool heatShiftComplete=false,storyFinished=false,familyResolved=false;int endingRoute=-1;
};
constexpr bool All(int mask,int needed){return mask>=0&&(mask&needed)==needed;}
constexpr bool Choice(int v){return v==0||v==1;}
constexpr const char* FirstPhotoVariant(int index,int publication,int supply,int district)
{
 if(index==2&&Choice(publication))return publication==0?"public":"rules";
 if(index==7&&Choice(supply))return supply==0?"reserve":"labor";
 if(index==11&&Choice(district))return district==0?"trial":"refund";
 return "default";
}
constexpr bool Eligible(const Facts& s,int i)
{
 switch(i)
 {
 case 0:return s.started&&s.firstLightComplete;
 case 1:return s.started&&s.heardAllocation&&s.metBroker&&s.readLedger;
 case 2:return s.started&&Choice(s.allocationPolicy)&&All(s.allocationReviews,7);
 case 3:return s.rationMask>=0&&s.rationMask<=7&&All(s.rationVisits,7)&&All(s.rationReviews,7)&&s.readRecall;
 case 4:return All(s.exploration,1);
 case 5:return s.seenFamily;
 case 6:return s.conflictComplete&&s.conflictEvidence&&s.conflictWitness==3&&All(s.conflictHeard,3);
 case 7:return s.supplyComplete&&All(s.supplyEvidence,7)&&All(s.supplyMeasured,7)&&All(s.supplyHeard,7);
 case 8:return s.dutySettled&&s.dutyWork>0;
 case 9:return s.districtStarted&&All(s.districtEvidence,1);
 case 10:return s.districtStarted&&All(s.districtEvidence,7);
 case 11:return s.districtSettled&&Choice(s.districtChoice)&&s.seenDistrictEnding;
 case 12:return All(s.exploration,4);
 case 13:return s.shortcutOpen&&s.shortcutReady;
 case 14:return All(s.urban,7);
 case 15:return s.quiet>0&&(s.quiet&7)!=0;
 case 16:return All(s.exploration,2);
 case 17:return All(s.exploration,16);
 case 18:return s.heatShiftComplete;
 case 19:return s.storyFinished&&s.familyResolved&&s.endingRoute==0;
 case 20:return s.storyFinished&&s.familyResolved&&s.endingRoute==1;
 case 21:return s.storyFinished&&s.familyResolved&&s.endingRoute==2;
 default:return false;
 }
}
inline uint32_t Earned(const Facts& s){uint32_t bits=0;for(int i=0;i<Count;++i)if(Eligible(s,i))bits|=uint32_t(1)<<i;return bits;}
constexpr uint32_t NewAwards(uint32_t earned,uint32_t known){return earned&~known&((uint32_t(1)<<Count)-1);}
constexpr int ClampRow(int v){return v<0?0:v>=Count?Count-1:v;}
constexpr int Page(int v){return ClampRow(v)/RowsPerPage;}
constexpr bool CanShow(bool menu,bool cinematic,bool quiet,bool action,bool voice,bool ambient)
{return !menu&&!cinematic&&!quiet&&!action&&!voice&&!ambient;}
}
