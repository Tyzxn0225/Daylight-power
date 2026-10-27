#include "StationPrototype.h"
#include "StationAchievementData.inl"
#include "StationAchievementPolicy.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "CanvasItem.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString AStationHUD::GetAchievementHUDStateJson() const
{
 TSharedRef<FJsonObject> State=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Keys;
 for(const auto& Row:AchievementPhotos)if(Row.Value&&Row.Value->GetResource())Keys.Add(MakeShared<FJsonValueString>(Row.Key));
 int32 PortraitCount=0;for(const auto& Portrait:NavigationPortraits)if(Portrait&&Portrait->GetResource())++PortraitCount;
 State->SetArrayField(TEXT("photos"),Keys);State->SetNumberField(TEXT("portraits"),PortraitCount);State->SetNumberField(TEXT("missing"),MissingAchievementPhotos.Num());
 FString Result;const auto Writer=TJsonWriterFactory<>::Create(&Result);FJsonSerializer::Serialize(State,Writer);return Result;
}

namespace
{
const FLinearColor PageGold(.82f,.66f,.41f,1),PagePaper(.94f,.915f,.845f,1),PageMuted(.57f,.65f,.63f,1),PageRule(.24f,.31f,.30f,1);
}
UTexture2D* AStationHUD::GetAchievementPhoto(AStationDirector* D,int32 Index)
{
 // Never load spoiler images for locked records, even into the cache.
 const FString Key=D->GetAchievementPhotoKey(Index);if(Key.IsEmpty())return nullptr;
 if(const auto* Found=AchievementPhotos.Find(Key))return Found->Get();
 if(MissingAchievementPhotos.Contains(Key))return nullptr;
 const FString File=FPaths::Combine(FPaths::ProjectContentDir(),TEXT("BZ/StoryAchievementsV20/Photos"),TEXT("A20_")+Key+TEXT(".png"));
 UTexture2D* Texture=FPaths::FileExists(File)?FImageUtils::ImportFileAsTexture2D(File):nullptr;
 if(!Texture){MissingAchievementPhotos.Add(Key);return nullptr;}
 // Only the current photograph and a few recently viewed pages stay resident.
 if(AchievementPhotos.Num()>=3)AchievementPhotos.Empty();
 AchievementPhotos.Add(Key,Texture);return Texture;
}
void AStationHUD::DrawAchievementPhoto(AStationDirector* D,int32 Index,float X,float Y,float Width,float Height,float Alpha)
{
 DrawRect(FLinearColor(.008f,.014f,.015f,.98f*Alpha),X*Unit,Y*Unit,Width*Unit,Height*Unit);
 if(UTexture2D* Photo=GetAchievementPhoto(D,Index);Photo&&Photo->GetResource())
 {
  FCanvasTileItem Item(FVector2D(X*Unit,Y*Unit),Photo->GetResource(),FVector2D(Width*Unit,Height*Unit),FLinearColor(1,1,1,Alpha));
  Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
 }
 else if(Width>300)
 {
  const bool Unlocked=D->IsAchievementUnlocked(Index);
  Text(Unlocked?TEXT("照片整理中"):TEXT("尚未显影"),X+30,Y+Height*.43f,25,PageMuted.CopyWithNewOpacity(Alpha));
  Text(Unlocked?TEXT("这段经历已经保留。稍后可再次查看照片。"):TEXT("走过这段经历之后，照片才会出现。"),X+30,Y+Height*.43f+42,16,PageMuted.CopyWithNewOpacity(Alpha));
 }
 DrawRect(PageGold.CopyWithNewOpacity(.65f*Alpha),X*Unit,(Y+Height)*Unit,Width*Unit,Unit);
}
void AStationHUD::DrawAchievementBook(AStationDirector* D,float X,float Y,float Width,float Height)
{
 const int32 Selected=StationAchievements::ClampRow(D->Selection),Page=StationAchievements::Page(Selected);
 const float ListW=Width*.31f,DetailX=X+ListW+30,DetailW=Width-ListW-30,PhotoY=Y+94,PhotoH=DetailW*.4f;
 auto Box=[&](float A,float B,float C,float E,FLinearColor Color){DrawRect(Color,A*Unit,B*Unit,C*Unit,E*Unit);};
 Text(TEXT("每张照片，留下一个无法抹平的瞬间"),X,Y,16,PageMuted);
 Text(FString::Printf(TEXT("已留下 %d / %d"),D->GetAchievementCount(),StationAchievements::Count),DetailX,Y,17,PageGold);
 for(int32 Row=0;Row<StationAchievements::RowsPerPage;++Row)
 {
  const int32 I=Page*StationAchievements::RowsPerPage+Row;if(I>=StationAchievements::Count)break;const auto& E=StoryAchievements20::Entries[I];const bool Earned=D->IsAchievementUnlocked(I),Active=I==Selected;
  const float SY=Y+42+Row*69;
  if(Active){Box(X,SY,ListW,63,FLinearColor(.115f,.125f,.106f,.96f));Box(X,SY,3,63,PageGold);}
  Text(Earned?TEXT("已记"):TEXT("待记"),X+14,SY+12,13,Earned?PageGold:PageMuted);
  Text(!Earned&&E.Hidden?TEXT("未写下的一页"):E.Title,X+64,SY+8,21,Earned?PagePaper:PageMuted);
  Text(StoryAchievements20::Categories[E.Category],X+64,SY+38,13,PageMuted);
 }
 Box(X+ListW+14,Y+40,1,Height-99,PageRule);
 const auto& E=StoryAchievements20::Entries[Selected];const bool Earned=D->IsAchievementUnlocked(Selected);
 Text(!Earned&&E.Hidden?TEXT("未写下的一页"):E.Title,DetailX,Y+46,29,Earned?PagePaper:PageMuted);
 DrawAchievementPhoto(D,Selected,DetailX,PhotoY,DetailW,PhotoH);
 const float CaptionY=PhotoY+PhotoH+13;
 const float End=Paragraph(Earned?D->GetAchievementCaption(Selected):E.Hint,DetailX,CaptionY,DetailW,18,Earned?PagePaper:PageMuted);
 if(Earned)Paragraph(E.Description,DetailX,End+9,DetailW,16,PageMuted);
 Text(FString::Printf(TEXT("%d / %d 页  ·  W / S 选择"),Page+1,(StationAchievements::Count+StationAchievements::RowsPerPage-1)/StationAchievements::RowsPerPage),X,Y+Height-42,16,PageGold);
 Text(TEXT("独立留存于本机 · 开始新游戏仍保留"),DetailX,Y+Height-42,15,PageMuted);
}
void AStationHUD::DrawAchievementToast(AStationDirector* D,float Right,float Bottom)
{
 const int32 I=D->GetAchievementToastIndex();if(I<0||I>=StationAchievements::Count)return;
 const float T=D->AchievementToastTime,Alpha=D->ReducedMotion?1.f:FMath::Min(FMath::Clamp(T/.22f,0.f,1.f),FMath::Clamp((5.f-T)/.35f,0.f,1.f));
 const float W=405,X=Right-W+(D->ReducedMotion?0.f:12.f*(1-Alpha)),Y=Bottom-335;
 DrawRect(FLinearColor(.01f,.023f,.025f,.94f*Alpha),X*Unit,Y*Unit,W*Unit,109*Unit);
 DrawRect(PageGold.CopyWithNewOpacity(Alpha),X*Unit,Y*Unit,2*Unit,109*Unit);
 Text(TEXT("留光簿 · 新的一页"),X+16,Y+10,14,PageGold.CopyWithNewOpacity(Alpha));
 DrawAchievementPhoto(D,I,X+16,Y+40,105,42,Alpha);
 Text(StoryAchievements20::Entries[I].Title,X+136,Y+39,21,PagePaper.CopyWithNewOpacity(Alpha));
 Text(TEXT("H  查看这张照片"),X+136,Y+76,14,PageMuted.CopyWithNewOpacity(Alpha));
}
