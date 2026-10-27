#include "StationPrototype.h"
#include "StationBackgroundFilm.h"
#include "StationNeighborhoodData.inl"
#include "StationExperienceData.inl"
#include "StationExperiencePolicy.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

namespace StationUI
{
    const FLinearColor Ink(.012f,.016f,.017f,.96f),Paper(.94f,.915f,.845f,1),Muted(.64f,.67f,.65f,1);
    const FLinearColor Brass(.81f,.63f,.36f,1),Rule(.27f,.29f,.27f,1),Document(.76f,.73f,.65f,1),Print(.075f,.079f,.074f,1);
    struct FLine { FString Text; int32 Start=0; };
    FSlateFontInfo FontInfo(UFont* Font,float Size,float Scale)
    {
        // The UFont object retains the engine's Chinese fallback face.
        return FSlateFontInfo(Font,FMath::Max(10.f,Size*Scale),TEXT("Regular"));
    }
    float Width(const FString& Text,UFont* Font,float Size,float Scale)
    {
        return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text,FontInfo(Font,Size,Scale)).X/Scale;
    }
    TArray<FLine> Wrap(const FString& Text,UFont* Font,float Size,float Scale,float MaxWidth)
    {
        TArray<FLine> Lines;FString Line;int32 Start=0;
        const FString Closing=TEXT("，。！？；：、）》】」』…,.!?;:)]}");
        const FString Opening=TEXT("（《【「『([{ ");
        for(int32 I=0;I<Text.Len();++I)
        {
            const TCHAR Ch=Text[I];
            if(Ch=='\r')continue;
            if(Ch=='\n'){Lines.Add({Line,Start});Line.Empty();Start=I+1;continue;}
            FString Candidate=Line;Candidate.AppendChar(Ch);
            if(!Line.IsEmpty()&&Width(Candidate,Font,Size,Scale)>MaxWidth)
            {
                // Keep paired Chinese punctuation on the appropriate side of a line break.
                if(Line.Len()>1&&(Closing.Contains(FString::Chr(Ch))||Opening.Contains(Line.Right(1))))
                {
                    const FString Carry=Line.Right(1);Line.LeftChopInline(1);Lines.Add({Line,Start});Line=Carry;Start=I-1;
                }
                else{Lines.Add({Line,Start});Line.Empty();Start=I;}
            }
            Line.AppendChar(Ch);
        }
        if(!Line.IsEmpty()||Lines.IsEmpty())Lines.Add({Line,Start});return Lines;
    }
    FLinearColor SpeakerColor(const FString& Name)
    {
        if(Name==TEXT("林照"))return FLinearColor(.64f,.77f,.81f,1);
        if(Name==TEXT("叶岑"))return FLinearColor(.73f,.79f,.61f,1);
        if(Name==TEXT("林禾"))return FLinearColor(.92f,.66f,.52f,1);
        if(Name==TEXT("周驰"))return FLinearColor(.83f,.72f,.49f,1);
        return Brass;
    }
    struct FPresentationState
    {
        TWeakObjectPtr<AStationHUD> Owner;
        EStationPanel Panel=EStationPanel::None;
        int32 Selection=-1;
        float SelectionChanged=0;
        float PanelChanged=0,ObjectiveChanged=0;
        FString Objective;
    };
}
AStationHUD::AStationHUD()
{
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    if(Font.Succeeded())InterfaceFont=Font.Object;
}
void AStationHUD::BeginPlay()
{
    Super::BeginPlay();LoadNavigationPortraits();
}
void AStationHUD::Text(const FString& String,float X,float Y,float Size,FLinearColor Color)
{
    FCanvasTextItem Item(FVector2D(X*Unit,Y*Unit),FText::FromString(String),StationUI::FontInfo(InterfaceFont,Size,Unit),Color);
    if(Color.GetLuminance()>.25f)Item.EnableShadow(FLinearColor(0,0,0,.88f));
    Canvas->DrawItem(Item);
}
float AStationHUD::Paragraph(const FString& String,float X,float Y,float Width,float Size,FLinearColor Color)
{
    const auto Lines=StationUI::Wrap(String,InterfaceFont,Size,Unit,Width);
    for(const auto& Line:Lines){Text(Line.Text,X,Y,Size,Color);Y+=Size*1.62f;}return Y;
}
void AStationHUD::DrawHUD()
{
    Super::DrawHUD();if(AStationBackgroundFilm::DrawFilm(this,Canvas))return;if(!Canvas||!PlayerOwner)return;
    AStationDirector* D=nullptr;for(TActorIterator<AStationDirector> It(GetWorld());It;++It){D=*It;break;}
    if(!D||!D->Progress)return;
    using namespace StationUI;
    Unit=FMath::Max(.1f,FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f));
    const float W=Canvas->SizeX/Unit,H=Canvas->SizeY/Unit;
    if(DrawSurveyHUD(D))return;
    // Centre content in a 16:9 safe area, including on ultrawide monitors.
    const float SafeW=FMath::Min(W-96.f,1504.f),Left=(W-SafeW)*.5f,Right=Left+SafeW;
    // Anchor the compact map to the actual viewport, including ultrawide screens.
    const float MiniMapWidth=292.f,MiniMapHeight=258.f,MiniMapTop=86.f,MiniMapLeft=W-48.f-MiniMapWidth;
    const float Now=GetWorld()->GetRealTimeSeconds();
    static FPresentationState State;
    if(State.Owner.Get()!=this){State=FPresentationState();State.Owner=this;State.PanelChanged=Now;State.ObjectiveChanged=Now;}
    if(State.Panel!=D->Panel){State.Panel=D->Panel;State.PanelChanged=Now;}
    if(State.Selection!=D->Selection){State.Selection=D->Selection;State.SelectionChanged=Now;}
    if(State.Objective!=D->GetObjective()){State.Objective=D->GetObjective();State.ObjectiveChanged=Now;}
    const float Entry=D->ReducedMotion?1.f:FMath::Clamp((Now-State.PanelChanged)/.24f,0.f,1.f);
    auto Box=[&](float X,float Y,float Width,float Height,FLinearColor Color){DrawRect(Color,X*Unit,Y*Unit,Width*Unit,Height*Unit);};
    auto Line=[&](float X,float Y,float Width,FLinearColor Color){Box(X,Y,Width,1.2f,Color);};
    auto Measure=[&](const FString& S,float Size){return Width(S,InterfaceFont,Size,Unit);};
    auto Center=[&](const FString& S,float X,float Y,float Size,FLinearColor Color){Text(S,X-Measure(S,Size)*.5f,Y,Size,Color);};
    auto RightText=[&](const FString& S,float X,float Y,float Size,FLinearColor Color){Text(S,X-Measure(S,Size),Y,Size,Color);};
    auto Key=[&](const FString& Label,float X,float Y,FLinearColor Color)
    {
        const float KW=FMath::Max(28.f,Measure(Label,17)+15.f);
        Box(X,Y,KW,28,FLinearColor(.025f,.03f,.03f,.82f));
        Line(X,Y,KW,FLinearColor(Color.R,Color.G,Color.B,.62f));
        Line(X,Y+28,KW,FLinearColor(Color.R,Color.G,Color.B,.34f));Center(Label,X+KW*.5f,Y+3,17,Color);
        return KW;
    };
    auto Shade=[&](float Top,float Height,float Alpha)
    {
        const int32 First=FMath::FloorToInt(Top*Unit),Last=FMath::CeilToInt((Top+Height)*Unit);
        for(int32 Py=First;Py<Last;++Py)
        {
            const float T=FMath::Clamp((Py+.5f-Top*Unit)/FMath::Max(1.f,Height*Unit),0.f,1.f);
            DrawRect(FLinearColor(.006f,.009f,.011f,Alpha*T*T),0,float(Py),W*Unit,1.f);
        }
    };
    auto TypedBody=[&](const FString& Full,float X,float Y,float MaxWidth,float Size,FLinearColor Color,float Leading=1.62f)
    {
        const int32 Visible=D->GetDisplayedText().Len();const auto Lines=Wrap(Full,InterfaceFont,Size,Unit,MaxWidth);
        for(const auto& L:Lines){const int32 Count=FMath::Clamp(Visible-L.Start,0,L.Text.Len());if(Count>0)Text(L.Text.Left(Count),X,Y,Size,Color);Y+=Size*Leading;}return Y;
    };
    auto Caption=[&](bool Cinematic)
    {
        FString Full=Cinematic?D->GetSubtitle():D->GetPanelBody();
        const FString Prefix=D->GetSpeaker()+TEXT("：");if(Full.StartsWith(Prefix))Full=Full.Mid(Prefix.Len());
        const float MaxWidth=FMath::Min(1060.f,SafeW*.79f);
        const float SubtitleFactor=1.f+D->SubtitleScale*.18f;float Size=31.f*SubtitleFactor;auto Lines=Wrap(Full,InterfaceFont,Size,Unit,MaxWidth);
        while(Lines.Num()>3&&Size>25.f*SubtitleFactor){Size-=1.f;Lines=Wrap(Full,InterfaceFont,Size,Unit,MaxWidth);}
        const float Leading=Size*1.48f,Bottom=H-(Cinematic?96.f:122.f),BodyTop=Bottom-Lines.Num()*Leading+10*(1-Entry);
        Shade(FMath::Min(BodyTop-115,H-280.f),H-FMath::Min(BodyTop-115,H-280.f),D->SubtitleBackdrop?.88f:.45f);
        const FLinearColor Accent=SpeakerColor(D->GetSpeaker());const float NameWidth=Measure(D->GetSpeaker(),20);
        Line(W*.5f-NameWidth*.5f-50,BodyTop-28,30,FLinearColor(Accent.R,Accent.G,Accent.B,.65f));
        Center(D->GetSpeaker(),W*.5f,BodyTop-40,20,Accent);
        Line(W*.5f+NameWidth*.5f+20,BodyTop-28,30,FLinearColor(Accent.R,Accent.G,Accent.B,.65f));
        const int32 Visible=D->GetDisplayedText().Len();float Y=BodyTop;
        for(const auto& L:Lines)
        {
            const float LW=Measure(L.Text,Size),X=(W-LW)*.5f;const int32 Count=FMath::Clamp(Visible-L.Start,0,L.Text.Len());
            if(Count>0){Box(X-13,Y-3,LW+26,Leading-2,FLinearColor(.006f,.008f,.009f,D->SubtitleBackdrop?.66f:.24f));Text(L.Text.Left(Count),X,Y,Size,Paper);}Y+=Leading;
        }
        if(!Cinematic)
        {
            const FString Hint=D->IsTyping()?TEXT("显示整句"):D->GetConversationIndex()+1==D->GetConversationCount()?TEXT("结束交谈"):TEXT("继续聆听");
            const float HintWidth=Measure(Hint,18)+43;
            Key(TEXT("E"),(W-HintWidth)*.5f,H-80,Paper);Text(Hint,(W-HintWidth)*.5f+41,H-77,18,Paper);
            RightText(TEXT("退格  离开"),Right,H-76,17,Muted);Text(D->IsVoiceEnabled()?TEXT("V  人声开启"):TEXT("V  人声关闭"),Left,H-76,17,Muted);
        }
    };
    if(D->Panel==EStationPanel::ArcScene)
    {
        Box(0,0,W,58,FLinearColor::Black);Box(0,H-49,W,49,FLinearColor::Black);
        Text(D->GetArcTitle(),Left,18,18,Muted);RightText(TEXT("长按 E  跳过片段"),Right,18,17,Paper);
        if(D->GetArcSkipProgress()>0)Box(Right-174,49,174*D->GetArcSkipProgress(),3,Brass);
        Caption(true);return;
    }
    if(D->IsArcMenu()||D->Panel==EStationPanel::ArcCredits||D->Panel==EStationPanel::Journal&&D->IsArcChapter())
    {
        const float X=W*.5f-650,Y=H*.5f-362,PW=1300;
        Box(0,0,W,H,FLinearColor(0,0,0,.35f));Box(X,Y,PW,724,Ink);Box(X,Y,3,724,Brass);
        Text(D->Panel==EStationPanel::ArcCredits?TEXT("白昼产权 / 留在生活里的结果"):D->IsArcMenu()?TEXT("眼前的决定 / 得到与承担"):TEXT("林照的记录 / 已经发生的事"),X+44,Y+30,17,Muted);
        Text(D->GetArcTitle(),X+44,Y+70,34,Paper);Line(X+44,Y+126,PW-88,Rule);
        if(D->Panel==EStationPanel::ArcCredits)
        {
            Paragraph(D->GetArcBody(),X+44,Y+161,720,23,Paper);
            Text(TEXT("制作与留存"),X+845,Y+166,24,Brass);
            Paragraph(TEXT("建筑与社区叙事毕业设计\n《白昼产权》\n\n创作：本项目作者\n技术与制作协作：Codex\n引擎：Unreal Engine 5\n人物基础与动作来源见工程资产清单\n对白使用合成配音，未冒用真人身份\n\n这份回执没有代替每个人的明天。"),X+845,Y+213,408,18,Muted);
            Text(D->ArcSceneElapsed<5?TEXT("片尾留存中…"):TEXT("E  返回社区 · 保留本次结局"),X+44,Y+664,20,Brass);
        }
        else if(D->IsArcMenu())
        {
            Paragraph(D->GetArcBody(),X+44,Y+157,1170,23,Paper);
            const int Count=D->GetArcChoiceCount();
            for(int I=0;I<Count;++I)
            {
                const float CY=Y+255+I*74;const bool Selected=I==D->Selection;
                const bool Enabled=D->Panel==EStationPanel::ArcConfirm||D->IsArcChoiceEnabled(I);
                if(Selected){const float A=D->ReducedMotion?1.f:FMath::Clamp((Now-State.SelectionChanged)/.16f,0.f,1.f);Box(X+44+7*(1-A),CY,1212,62,FLinearColor(.15f,.13f,.10f,.96f));Box(X+44,CY,3,62,Brass);}
                Text(Selected?TEXT("›"):TEXT("·"),X+62,CY+16,26,Selected?Brass:Muted);
                Text(D->GetArcChoiceLabel(I),X+102,CY+17,23,Enabled?Paper:Muted.CopyWithNewOpacity(.55f));
            }
            Line(X+44,Y+498,1212,Rule);Paragraph(D->GetArcChoicePreview(),X+44,Y+518,1200,19,Muted);
            Text(TEXT("E  确认       W / S  选择       退格  返回"),X+44,Y+664,18,Brass);
        }
        else
        {
            Paragraph(D->GetArcRecap(),X+44,Y+166,440,21,Paper);
            const auto Steps=D->GetArcSteps();const int Stage=D->GetArcStage();
            for(int I=0;I<Steps.Num();++I)
            {
                const float SY=Y+163+I*76;const bool Active=I==Stage;
                if(Active)Box(X+536,SY-6,718,62,FLinearColor(.12f,.105f,.074f,.94f));
                Text(FString::Printf(TEXT("%02d"),I+1),X+555,SY+7,22,Active?Brass:Muted);Text(Steps[I],X+615,SY+7,23,Active?Paper:Muted);
            }
            Text(TEXT("J / E / 退格  收起记录"),X+44,Y+664,18,Muted);
        }
        return;
    }
    if(D->QuietIndex>=0)
    {
        Box(0,0,W,38,FLinearColor(0,0,0,.45f));Shade(H-210,210,.72f);
        Text(Experience17::Quiet[D->QuietIndex].Name,Left,12,17,Muted);
        Paragraph(D->GetQuietMomentText(),Left,H-156,FMath::Min(1000.f,SafeW),25,Paper);
        Text(TEXT("E / 移动 / Esc  离开此刻"),Left,H-52,17,Muted);return;
    }
    if(D->IsStoryFilmActive())
    {
        Box(0,0,W,58,FLinearColor::Black);Box(0,H-49,W,49,FLinearColor::Black);
        Text(D->GetStoryTitle(),Left,18,18,Muted);if(!D->IsStoryCapture())RightText(TEXT("长按 E  跳过片段"),Right,18,17,Paper);
        if(D->GetStorySkipProgress()>0)Box(Right-174,49,174*D->GetStorySkipProgress(),3,Brass);
        const FString ArchiveTitle=D->GetStoryArchiveTitle();
        if(!ArchiveTitle.IsEmpty())
        {
            const float T=D->GetStoryShotProgress(),A=FMath::Min(FMath::Clamp(T*9.f,0.f,1.f),FMath::Clamp((1-T)*9.f,0.f,1.f));
            const bool Identity=ArchiveTitle.StartsWith(TEXT("林照 /"))||ArchiveTitle.StartsWith(TEXT("叶岑 /"))||ArchiveTitle.StartsWith(TEXT("林禾 /"))||ArchiveTitle.StartsWith(TEXT("周驰 /"));
            const float X=Left+12*(1-A),Y=H*.17f,PW=Identity?440.f:600.f,PH=Identity?156.f:273.f;
            if(Identity)
            {
                Box(X,Y,PW,PH,FLinearColor(.012f,.019f,.021f,.88f*A));Box(X,Y,2,PH,FLinearColor(.81f,.63f,.36f,A));
                Text(ArchiveTitle,X+20,Y+18,25,Paper.CopyWithNewOpacity(A));
                Paragraph(D->GetStoryArchiveBody(),X+20,Y+64,PW-40,18,Muted.CopyWithNewOpacity(A));
            }
            else
            {
            Box(X+8,Y+10,PW,PH,FLinearColor(0,0,0,.30f*A));Box(X,Y,PW,PH,FLinearColor(.80f,.775f,.685f,.96f*A));
            Box(X,Y,4,PH,FLinearColor(.43f,.31f,.16f,A));
            Text(TEXT("留存副本 / 私人与公共"),X+27,Y+19,16,FLinearColor(.31f,.31f,.27f,A));
            Text(ArchiveTitle,X+27,Y+57,28,FLinearColor(.105f,.12f,.115f,A));Line(X+27,Y+103,PW-54,FLinearColor(.39f,.38f,.33f,A));
            Paragraph(D->GetStoryArchiveBody(),X+27,Y+122,PW-54,24,FLinearColor(.16f,.18f,.17f,A));
            if(ArchiveTitle==TEXT("申请人声明")&&T>.45f)
                Text(TEXT("自愿"),X+PW-118,Y+PH-57,29,FLinearColor(.47f,.17f,.10f,A*FMath::Clamp((T-.45f)*5,0.f,1.f)));
            }
        }
        if(!D->GetDisplayedText().IsEmpty())Caption(true);
        if(!D->IsStoryCapture())Text(D->IsVoiceEnabled()?TEXT("V  人声开启"):TEXT("V  人声关闭"),Left,H-33,15,Muted);
        if(D->GetStoryFade()>0)Box(0,0,W,H,FLinearColor(0,0,0,D->GetStoryFade()));
        return;
    }
    if(D->Starting)
    {
        Box(0,0,W,56,FLinearColor::Black);Box(0,H-48,W,48,FLinearColor::Black);
        Text(TEXT("旧站复燃"),Left,19,17,Muted);RightText(TEXT("E  跳过镜头"),Right,19,17,Muted);
        if(!D->GetDisplayedText().IsEmpty())Caption(true);return;
    }
    // Dialogue clears the exploration panels so the actors and community remain visible.
    if(D->Panel==EStationPanel::Conversation){Caption(false);return;}
    if(D->Panel==EStationPanel::CrisisBalance)
    {
        Box(0,0,W,H,FLinearColor(.003f,.005f,.006f,.64f));
        const float PW=1080,PH=640,X=(W-PW)*.5f,Y=(H-PH)*.5f,Pad=44,GW=PW-2*Pad;
        const int A=D->GetCrisisDraft(),B=36-A;
        Box(X,Y,PW,PH,Ink);Line(X+Pad,Y+32,GW,Rule);
        Text(TEXT("M13 / 余热旁路 · 本班安排"),X+Pad,Y+49,18,Muted);
        Text(TEXT("热量只有这么多，阀门由你调"),X+Pad,Y+88,30,Paper);
        Paragraph(TEXT("每端完整开放需 24 kW，主厅保底需 12 kW。两端合计只能用 36 kW。"),X+Pad,Y+151,GW,22,Paper);
        const float Split=GW*A/36.f,GY=Y+264;
        Box(X+Pad,GY,Split,38,Brass);Box(X+Pad+Split,GY,GW-Split,38,FLinearColor(.24f,.52f,.57f));
        Box(X+Pad+Split-2,GY-13,4,64,Paper);
        Text(FString::Printf(TEXT("院内等候室  %d kW"),A),X+Pad,Y+210,27,Brass);
        RightText(FString::Printf(TEXT("恒昼夜班廊  %d kW"),B),X+PW-Pad,Y+210,27,Paper);
        auto Mode=[](int F){return F>=24?TEXT("完整开放"):F>=18?TEXT("轮换使用"):TEXT("主厅保底");};
        Text(Mode(A),X+Pad,Y+324,23,Paper);RightText(Mode(B),X+PW-Pad,Y+324,23,Paper);
        Paragraph(TEXT("W 向院内多分 2 kW / S 向恒昼多分 2 kW。每次调节都从另一端扣回相同热量。"),X+Pad,Y+382,GW,21,Paper);
        Paragraph(TEXT("草稿尚未生效。住宅、通风与应急照明不在本阀控制范围；热量不计入公共供光额度。"),X+Pad,Y+454,GW,19,Muted);
        Line(X+Pad,Y+555,GW,Rule);Text(TEXT("E 复核并操作阀门"),X+Pad,Y+579,21,Paper);RightText(TEXT("退格 取消草稿"),X+PW-Pad,Y+579,19,Muted);return;
    }
    if(D->Panel==EStationPanel::DutyCalibrate)
    {
        Box(0,0,W,H,FLinearColor(.003f,.005f,.006f,.60f));
        const float PW=1050.f,PH=560.f,X=(W-PW)*.5f,Y=(H-PH)*.5f,Pad=45.f,GW=PW-2*Pad;
        Box(X,Y,PW,PH,Ink);Line(X+Pad,Y+32,GW,Rule);
        Text(TEXT("现场仪表 / 两阶段稳压验收"),X+Pad,Y+48,18,Muted);
        Text(D->GetDutyCalibrationTitle(),X+Pad,Y+88,31,Paper);
        RightText(FString::Printf(TEXT("%d / 2"),D->GetDutyCalibrationPhase()+1),X+PW-Pad,Y+89,27,Brass);
        Paragraph(TEXT("按住 E 加压，松开 E 泄压。把读数留在标线范围，稳定条满后进入下一阶段。"),X+Pad,Y+152,GW,23,Paper);
        const float GX=X+Pad,GY=Y+280,GH=66,Value=D->GetDutyPressure(),Low=D->GetDutyBandMin(),High=D->GetDutyBandMax();
        Box(GX,GY,GW,GH,FLinearColor(.025f,.035f,.035f,1));Box(GX+GW*Low/100,GY,GW*(High-Low)/100,GH,FLinearColor(.12f,.32f,.25f,1));
        Box(GX+GW*Low/100,GY-5,2,GH+10,Paper);Box(GX+GW*High/100,GY-5,2,GH+10,Paper);
        Box(GX+GW*Value/100-3,GY-12,6,GH+24,Brass);
        Text(TEXT("0"),GX,GY+80,18,Muted);RightText(TEXT("100"),GX+GW,GY+80,18,Muted);
        Text(FString::Printf(TEXT("读数 %.0f / 稳定区 %.0f–%.0f"),Value,Low,High),GX,GY-44,22,Value>=Low&&Value<=High?Paper:Muted);
        Box(GX,Y+407,GW,7,Rule);Box(GX,Y+407,GW*D->GetDutyStableProgress(),7,Brass);
        Text(TEXT("稳定保持"),GX,Y+427,18,Muted);RightText(D->IsDutyAssisted()?TEXT("S 慢速辅助：开 · 同额记账"):TEXT("S 慢速辅助：关"),GX+GW,Y+427,18,Muted);
        Line(GX,Y+485,GW,Rule);Text(TEXT("E 按住 / 松开调压"),GX,Y+506,20,Paper);RightText(TEXT("退格 退出 · 未完成验收不记账"),GX+GW,Y+506,18,Muted);return;
    }
    if(D->IsMenuOpen())
    {
        Box(0,0,W,H,FLinearColor(.003f,.005f,.006f,.57f));
        const bool IsDocument=D->Panel==EStationPanel::Document,IsJournal=D->Panel==EStationPanel::Journal,IsRelationships=D->Panel==EStationPanel::Relationships;
        const float PW=D->Panel==EStationPanel::Achievements?1360.f:IsDocument?930.f:D->Panel==EStationPanel::NeighborhoodMap?1340.f:1120.f,PH=D->Panel==EStationPanel::Achievements?820.f:D->Panel==EStationPanel::NeighborhoodMap?772.f:IsJournal?704.f:690.f;
        const float X=(W-PW)*.5f,Y=(H-PH)*.5f+10*(1-Entry),Pad=42,FooterY=Y+PH-61;
        const FLinearColor Surface=IsDocument?Document:Ink,BodyColor=IsDocument?Print:Paper;
        const FLinearColor SubColor=IsDocument?FLinearColor(.26f,.27f,.24f,1):Muted,Edge=IsDocument?FLinearColor(.38f,.37f,.32f,1):Rule;
        Box(X+12,Y+15,PW,PH,FLinearColor(0,0,0,.37f));Box(X,Y,PW,PH,Surface);Line(X+Pad,Y+31,PW-2*Pad,Edge);
        const FString Folio=D->Panel==EStationPanel::Achievements?TEXT("留光簿 / 已经发生的经历"):D->Panel==EStationPanel::DialogueLog?TEXT("实际触发的对白 / 最近 64 句"):D->Panel==EStationPanel::WorldRecord?TEXT("恒昼访客廊 / 合同、维保与退出"):D->Panel==EStationPanel::Exploration?TEXT("在场的旧物 / 林照的所见"):D->Panel==EStationPanel::NeighborhoodMap?TEXT("旧光里 / 恒昼区 / 回光埠"):IsRelationships?TEXT("人与回访 / 事实与未决关系"):D->IsMandateChapter()?TEXT("下一期申请 / 本人意见与授权边界"):D->IsAftermathChapter()?TEXT("回执之后 / 三位当事人的回访"):D->IsDistrictChapter()?TEXT("回光埠 / 检验、押金与复售"):D->IsDutyChapter()?TEXT("本班验收 / 工时、备用与等待"):D->IsSupplyChapter()?TEXT("新批供货 / 工时与公共备用"):D->IsConflictChapter()?TEXT("社区耗材 / 退件与共同担保"):IsJournal?TEXT("林照的记录  /  旧公共光站"):IsDocument?TEXT("社区档案  /  旧公共光站"):D->IsRationChapter()?TEXT("首夜配给  /  派单复核"):D->IsAllocationChapter()?TEXT("第一轮配给  /  公示复核"):TEXT("旧公共光站  /  启动复核");
        Text(Folio,X+Pad,Y+45,17,SubColor);RightText(D->Panel==EStationPanel::Achievements?TEXT("照片 / 经历 / 留存"):D->Panel==EStationPanel::DialogueLog?FString::Printf(TEXT("%d / %d"),D->LogPage+1,StationExperience::LogPages(D->Progress->DialogueHistory.Num())):D->Panel==EStationPanel::WorldRecord?TEXT("阅览副本"):D->Panel==EStationPanel::Exploration?TEXT("原文与心声"):D->Panel==EStationPanel::NeighborhoodMap?TEXT("人的尺度"):IsRelationships?TEXT("已发生的事"):IsJournal?(D->IsCrisisChapter()?TEXT("13"):D->IsMandateChapter()?TEXT("12"):D->IsAftermathChapter()?TEXT("11"):D->IsDistrictChapter()?TEXT("10"):D->IsDutyChapter()?TEXT("09"):D->IsSupplyChapter()?TEXT("08"):D->IsConflictChapter()?TEXT("07"):D->IsAllocationChapter()?TEXT("06"):TEXT("05")):IsDocument?TEXT("存档副本"):TEXT("待确认"),X+PW-Pad,Y+45,17,SubColor);
        Text(IsJournal?D->GetMissionTitle():D->GetPanelTitle(),X+Pad,Y+87,34,BodyColor);Line(X+Pad,Y+145,PW-2*Pad,Edge);
        if(D->Panel==EStationPanel::Achievements)
        {
            DrawAchievementBook(D,X+Pad,Y+174,PW-2*Pad,PH-235);
        }
        else if(D->Panel==EStationPanel::Relationships)
        {
            TArray<FString> Sections;D->GetSocialMemoryBody().ParseIntoArray(Sections,TEXT("\n\n"),true);
            const float Sidebar=246,CX=X+Pad+Sidebar+26,CW=PW-2*Pad-Sidebar-26;
            const int Selected=FMath::Clamp(D->Selection,0,FMath::Max(0,Sections.Num()-1));
            for(int32 I=0;I<Sections.Num();++I)
            {
                int32 Split=INDEX_NONE;Sections[I].FindChar('\n',Split);const FString Heading=Split>=0?Sections[I].Left(Split):Sections[I];
                FString Name,Relationship;Heading.Split(TEXT(" / "),&Name,&Relationship);
                const float CY=Y+173+I*58.f;const bool Active=I==Selected;
                Box(X+Pad,CY,Sidebar,53,Active?FLinearColor(.095f,.105f,.10f,.98f):FLinearColor(.035f,.047f,.048f,.7f));
                if(Active)Box(X+Pad,CY,3,53,Brass);
                Text(Name,X+Pad+16,CY+5,20,Active?Brass:Paper);Text(Relationship,X+Pad+16,CY+31,13,Muted);
            }
            if(Sections.IsValidIndex(Selected))
            {
                int32 Split=INDEX_NONE;Sections[Selected].FindChar('\n',Split);const FString Heading=Split>=0?Sections[Selected].Left(Split):Sections[Selected],Body=Split>=0?Sections[Selected].Mid(Split+1):TEXT("");
                int Id=Heading.StartsWith(TEXT("林照"))?0:Heading.StartsWith(TEXT("叶岑"))?1:Heading.StartsWith(TEXT("林禾"))?2:Heading.StartsWith(TEXT("周驰"))?3:Heading.StartsWith(TEXT("沈砚"))?4:Heading.StartsWith(TEXT("罗婶"))?5:6;
                Box(CX,Y+173,CW,420,FLinearColor(.035f,.047f,.048f,.98f));
                if(NavigationPortraits.IsValidIndex(Id)&&NavigationPortraits[Id]&&NavigationPortraits[Id]->GetResource())
                {
                    FCanvasTileItem Tile(FVector2D((CX+18)*Unit,(Y+190)*Unit),NavigationPortraits[Id]->GetResource(),FVector2D(82*Unit,82*Unit),FLinearColor::White);Tile.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Tile);
                }
                Text(Heading,CX+116,Y+195,22,Brass);Text(TEXT("只记录你已经经历的事"),CX+116,Y+234,15,Muted);
                Paragraph(Body,CX+20,Y+291,CW-40,18,Paper);
            }
        }
        else if(IsJournal)
        {
            Text(D->IsCrisisChapter()?TEXT("阀门两边，坐着谁？"):D->IsMandateChapter()?TEXT("地址相同，谁同意了？"):D->IsAftermathChapter()?TEXT("同一回执，三份余账"):D->IsDistrictChapter()?TEXT("灯没坏，谁的生活停订了？"):D->IsDutyChapter()?TEXT("谁在替你等下班？"):D->IsSupplyChapter()?TEXT("签自己的字，谁在付代价？"):D->IsConflictChapter()?TEXT("设备合格，为什么退回？"):D->IsRationChapter()?TEXT("三份申请，一笔余账"):D->IsAllocationChapter()?TEXT("谁在替谁排队"):TEXT("第一束光之后"),X+Pad,Y+179,24,Brass);
            if(D->IsCrisisChapter())Paragraph(TEXT("不是多出一份能源。\n先测，再隔离。\n到两头看实际结果。"),X+Pad,Y+224,280,23,Paper);
            else if(D->IsMandateChapter())Paragraph(TEXT("先把名字还给本人。\n核对原件和经手人，\n再读清撤回代价。"),X+Pad,Y+224,280,23,Paper);
            else if(D->IsAftermathChapter())Paragraph(TEXT("押金、工作与家。\n三个人各有余账。\n回访不等于认可。"),X+Pad,Y+224,280,23,Paper);
            else Paragraph(D->IsDistrictChapter()?TEXT("清算额只有 6。\n\n一次公共试亮，\n或退回夜班押金。\n\n先看见两边的人。"):D->IsDutyChapter()?TEXT("工时要核销，备用要补回。\n\n先稳住读数，\n再决定谁等下一轮。\n\n下班不是所有账都清了。"):D->IsSupplyChapter()?TEXT("三份原单，两种担保。\n\n公共备用少六个额度，\n或将自己的工时押出去。\n\n先测现场，再听当事人。"):D->IsConflictChapter()?TEXT("一枚稳压器。\n三方担保，一份名册。\n\n先随行核验，\n再亲手完成处置。"):D->IsRationChapter()?TEXT("诊疗 32 / 厨房 24\n居民共用灯 24\n\n总计 80，无法全保。"):D->IsAllocationChapter()?TEXT("名单先排好，\n规则后公示。\n\n听完三人，再决定。"):TEXT("应急额度有限。\n\n把设备重新接通，只是分配问题的开始。"),X+Pad,Y+224,280,23,Paper);
            Text(TEXT("当前目标"),X+Pad,Y+394,17,Muted);Paragraph(D->GetObjective(),X+Pad,Y+428,280,23,Paper);Box(X+369,Y+177,1,400,Rule);
            const TCHAR* Steps[]={TEXT("与叶岑会合"),TEXT("阅读抵押告示"),TEXT("取得备用接口"),TEXT("检修三处连接"),TEXT("了解启动风险"),TEXT("确认供光方案"),TEXT("查看第一束公共光")};
            const TCHAR* AllocationSteps[]={TEXT("听林禾说明配给"),TEXT("向周驰核对名单"),TEXT("阅读原始配给名单"),TEXT("决定公示方式"),TEXT("回访三位当事人"),TEXT("本轮公示完成")};
            const TCHAR* RationSteps[]={TEXT("核验三处服务申请"),TEXT("请叶岑核对余账"),TEXT("勾选服务并确认配给"),TEXT("到三处查看实际结果"),TEXT("回访三位当事人"),TEXT("阅读结算与外部通知"),TEXT("首夜配给已结算")};
            const TCHAR* ConflictSteps[]={TEXT("收到退件通知"),TEXT("随行核验并读取封签"),TEXT("核对担保与家属登记"),TEXT("选择并复核处置"),TEXT("取件并安装或退回"),TEXT("签收回执与代价"),TEXT("退件风波已记录")};
            const TCHAR* SupplySteps[]={TEXT("核验三份供货与抵押原单"),TEXT("完成端点与旁路检测"),TEXT("听完三位当事人的说明"),TEXT("选择并复核本批担保"),TEXT("取件并接入新批耗材"),TEXT("核对供货回执与余账"),TEXT("本批供货已接入")};
            const TCHAR* DutySteps[]={TEXT("接下本班检修与履约"),TEXT("完成前两张调压验收"),TEXT("回应欠压与验收冲突"),TEXT("落实选择并完成余单"),TEXT("补测或等候到场抢修"),TEXT("核对本班收入与工时"),TEXT("本班已结算，回家之前")};
            const TCHAR* DistrictSteps[]={TEXT("接取夜班回收委托"),TEXT("核验回收灯与停订原因"),TEXT("核对夜班押金申领"),TEXT("核对厂房复售原单"),TEXT("复核并亲手执行处置"),TEXT("读取回执与兄妹尾声"),TEXT("本轮异议与处置已留档")};
            const TCHAR* AftermathSteps[]={TEXT("听完叶岑的工位与工资回访"),TEXT("听完周驰的垫款与折价回访"),TEXT("听完林禾本人的家庭回访"),TEXT("分别留下三人的意见")};
            const TCHAR* MandateSteps[]={TEXT("接下院落事务台的关联通知"),TEXT("听林禾本人的回应"),TEXT("抽取底联，找出授权范围扩大"),TEXT("问叶岑或周驰形成经过"),TEXT("读完恒昼访客廊撤回条款"),TEXT("记录林禾独立意见"),TEXT("复核代价并亲手提交"),TEXT("兄妹分别留下回执")};
            const TCHAR* CrisisSteps[]={TEXT("接听检修电台的回水告警"),TEXT("在连接 B 实测旁路容量"),TEXT("向叶岑核对联锁与保底负荷"),TEXT("隔离景观加温支路"),TEXT("调节余热并亲手锁定阀位"),TEXT("核验院内等候室末端"),TEXT("核验恒昼夜班廊末端"),TEXT("听林禾本人的回应")};
            const int32 Stage=D->GetQuestStage();
            const bool ChapterComplete=D->IsCrisisChapter()?D->Progress->CrisisEndingHeard:D->IsMandateChapter()?D->Progress->MandateEndingHeard:D->IsAftermathChapter()?(D->Progress->AftermathHeardMask&7)==7:D->IsDistrictChapter()?D->Progress->SeenDistrictFilm:D->IsDutyChapter()?D->Progress->DutySettled:D->IsSupplyChapter()?D->Progress->SupplyComplete:D->IsConflictChapter()?D->Progress->ConflictComplete:D->IsRationChapter()?D->IsRationComplete():D->IsAllocationChapter()&&D->Progress->AllocationPolicy>=0&&D->Progress->AllocationReviewMask==7;
            for(int32 I=0;I<D->GetQuestStepCount();++I)
            {
                const float SY=Y+176+I*(D->IsMandateChapter()?51:57);const bool Active=I==Stage&&!ChapterComplete;
                const bool MFlags[]={D->Progress->MandateStarted,D->Progress->MandateOwnerHeard,D->Progress->MandateCompared,(D->Progress->MandateWitnessMask&3)!=0,D->Progress->MandateTermsRead,D->Progress->MandateConsentRecorded,D->Progress->MandateCommitted,D->Progress->MandateEndingHeard};
                const bool CFlags[]={D->Progress->CrisisStarted,D->Progress->CrisisMeasured,D->Progress->CrisisBriefed,D->Progress->CrisisIsolated,D->Progress->CrisisAllocation>=0,(D->Progress->CrisisVerifiedMask&1)!=0,(D->Progress->CrisisVerifiedMask&2)!=0,D->Progress->CrisisEndingHeard};
                const bool Done=D->IsCrisisChapter()?CFlags[I]:D->IsMandateChapter()?MFlags[I]:D->IsAftermathChapter()?(I<3?(D->Progress->AftermathHeardMask&(1<<I))!=0:ChapterComplete):I<Stage||ChapterComplete;
                if(Active){Box(X+395,SY-5,PW-437,50,FLinearColor(.12f,.105f,.074f,.94f));Box(X+395,SY-5,3,50,Brass);}
                const FLinearColor C=Active?Brass:Done?Paper:Muted;
                Text(FString::Printf(TEXT("%02d"),I+1),X+415,SY+4,22,C);Text(D->IsCrisisChapter()?CrisisSteps[I]:D->IsMandateChapter()?MandateSteps[I]:D->IsAftermathChapter()?AftermathSteps[I]:D->IsDistrictChapter()?DistrictSteps[I]:D->IsDutyChapter()?DutySteps[I]:D->IsSupplyChapter()?SupplySteps[I]:D->IsConflictChapter()?ConflictSteps[I]:D->IsRationChapter()?RationSteps[I]:D->IsAllocationChapter()?AllocationSteps[I]:Steps[I],X+472,SY+4,23,C);
                RightText(Done?TEXT("完成"):Active?TEXT("当前"):TEXT("待办"),X+PW-Pad-4,SY+7,17,C);
            }
        }
        else if(D->Panel==EStationPanel::NeighborhoodMap)
        {
            DrawNavigation(D,X+Pad,Y+177,784,500,true);
            const float TX=X+Pad+820;AActor* Guide=D->GetGuidanceTarget();
            Text(TEXT("当前剧情"),TX,Y+179,18,Brass);
            const float TextBottom=Paragraph(D->GetObjective(),TX,Y+215,400,22,Paper);
            const float DetailY=FMath::Max(Y+345,TextBottom+20);
            Paragraph(D->GetNavigationPlace(Guide),TX,DetailY,400,18,Brass);
            Paragraph(TEXT("金框肖像 / 金色菱形：当前目标\n浅蓝肖像：林照与朝向\n灰框肖像：人物的实际位置\n青色菱形：附近发现与已读记录"),TX,DetailY+52,400,17,Paper);
            Paragraph(TEXT("人物会移动；地图固定朝北。\n青色任务点可自行选择先后。\n金线仅显示已核验的可走路线。"),TX,DetailY+197,400,16,Muted);
        }
        else if(D->Panel==EStationPanel::DialogueLog)
        {
            Paragraph(D->GetDialogueLogBody(),X+Pad,Y+176,PW-2*Pad,18,Paper);
            Text(TEXT("W / S  最近与更早的记录"),X+Pad,Y+565,18,Brass);
        }
        else if(D->Panel==EStationPanel::WorldRecord||D->Panel==EStationPanel::Exploration)
        {
            Text(TEXT("留存原文"),X+Pad,Y+174,18,Muted);Paragraph(D->Panel==EStationPanel::WorldRecord?D->GetUrbanBody():D->GetExplorationBody(),X+Pad,Y+210,PW-2*Pad,D->Panel==EStationPanel::WorldRecord?19.f:23.f,Paper);
            Line(X+Pad,Y+478,PW-2*Pad,Rule);Text(TEXT("林照 / 此刻所想"),X+Pad,Y+496,18,Brass);
            Paragraph(D->GetDisplayedText(),X+Pad,Y+531,PW-2*Pad,22,Paper);
        }
        else if(D->Panel==EStationPanel::StoryArchive)
        {
            Paragraph(D->GetPanelBody(),X+Pad,Y+174,PW-2*Pad,23,Paper);
            for(int32 I=0;I<10;++I)
            {
                const float CY=Y+264+I*33;const bool Selected=D->Selection==I;
                if(Selected){Box(X+Pad,CY,PW-2*Pad,34,FLinearColor(.17f,.145f,.105f,.97f));Box(X+Pad,CY,3,34,Brass);}
                Text(Selected?TEXT("›"):TEXT("·"),X+Pad+17,CY+9,24,Selected?Brass:Muted);
                Text(D->GetStoryMenuChoice(I),X+Pad+51,CY+6,19,Selected?Paper:Muted);
            }
        }
        else if(D->Panel==EStationPanel::RationPlan)
        {
            Paragraph(D->GetPanelBody(),X+Pad,Y+168,PW-2*Pad,22,Paper);
            const TCHAR* Names[]={TEXT("诊疗点 · 稳定照明与药柜"),TEXT("互助厨房 · 备餐与保温"),TEXT("居民共用灯 · 按户轮候"),TEXT("核对并提交")};
            const int32 Costs[]={32,24,24};
            for(int32 I=0;I<4;++I)
            {
                const float CY=Y+306+I*65;const bool Selected=D->Selection==I,Checked=I<3&&(D->GetRationDraftMask()&(1<<I));
                const bool Affordable=I==3||Checked||D->GetRationDraftCost()+Costs[I]<=D->Progress->Reserve;
                if(Selected){Box(X+Pad,CY,PW-2*Pad,57,FLinearColor(.17f,.145f,.105f,.97f));Box(X+Pad,CY,3,57,Brass);}
                Text(I==3?TEXT("→"):Checked?TEXT("■"):TEXT("□"),X+Pad+18,CY+14,24,Selected?Brass:Muted);
                Text(Names[I],X+Pad+60,CY+14,24,Affordable?Paper:Muted);
                RightText(I==3?FString::Printf(TEXT("余 %d"),D->Progress->Reserve-D->GetRationDraftCost()):FString::Printf(TEXT("%d 额度 · %s"),Costs[I],I==0?TEXT("4 小时"):TEXT("3 小时")),X+PW-Pad-20,CY+16,21,Checked?Brass:Muted);
            }
        }
        else if(D->Panel==EStationPanel::Plans)
        {
            Paragraph(TEXT("这轮供光，谁先等到天亮？"),X+Pad,Y+171,PW-2*Pad,25,Paper);
            Text(TEXT("设备尚未启动。选择方案后，还需要再次确认。"),X+Pad,Y+218,19,Muted);
            for(int32 I=0;I<2;++I)
            {
                const float CY=Y+274+I*142;const bool Selected=D->Selection==I;const FLinearColor C=Selected?Paper:Muted;
                Box(X+Pad,CY,PW-2*Pad,126,Selected?FLinearColor(.17f,.145f,.105f,.97f):FLinearColor(.045f,.053f,.052f,.92f));Box(X+Pad,CY,Selected?4:1,126,Selected?Brass:Rule);
                Text(Selected?TEXT("›"):TEXT("·"),X+Pad+18,CY+19,28,Selected?Brass:Muted);Text(I==0?TEXT("保守启动"):TEXT("大功率启动"),X+Pad+53,CY+20,28,C);
                Text(I==0?TEXT("近街 3 个单元"):TEXT("全部 6 个单元"),X+Pad+53,CY+64,21,C);
                Text(I==0?TEXT("远街继续等待 · 负荷较稳"):TEXT("覆盖更广 · 临时过载风险"),X+Pad+53,CY+96,18,Muted);
                RightText(I==0?TEXT("72"):TEXT("38"),X+PW-Pad-28,CY+17,42,Selected?Brass:Paper);RightText(TEXT("启动后剩余额度 / 100"),X+PW-Pad-28,CY+76,17,Muted);
            }
        }
        else
        {
            const bool ConflictMenu=D->Panel==EStationPanel::ConflictStart||D->Panel==EStationPanel::ConflictChoice||D->Panel==EStationPanel::ConflictConfirm;
            const bool SupplyMenu=D->Panel==EStationPanel::SupplyStart||D->Panel==EStationPanel::SupplyChoice||D->Panel==EStationPanel::SupplyConfirm;
            const bool DutyMenu=D->Panel==EStationPanel::DutyStart||D->Panel==EStationPanel::DutyChoice||D->Panel==EStationPanel::DutyConfirm;
            const bool DistrictMenu=D->Panel==EStationPanel::DistrictChoice||D->Panel==EStationPanel::DistrictConfirm;
            const bool CrisisMenu=D->IsCrisisMenu(),MandateMenu=D->IsMandateMenu();
            const bool HasChoices=CrisisMenu||MandateMenu||DistrictMenu||DutyMenu||SupplyMenu||ConflictMenu||D->Panel==EStationPanel::Confirmation||D->Panel==EStationPanel::Restart||D->Panel==EStationPanel::ReturnHome||D->Panel==EStationPanel::Allocation||D->Panel==EStationPanel::AllocationConfirm||D->Panel==EStationPanel::RationConfirm;const FString Full=D->GetPanelBody();
            const FString Consequence=D->GetDecisionPreview();const float AvailableHeight=HasChoices?(Consequence.IsEmpty()?220.f:167.f):410.f;float Size=24.f;
            while(Wrap(Full,InterfaceFont,Size,Unit,PW-2*Pad).Num()*Size*1.62f>AvailableHeight&&Size>18.f)Size-=1;
            const float Leading=FMath::Clamp(AvailableHeight/FMath::Max(1,Wrap(Full,InterfaceFont,Size,Unit,PW-2*Pad).Num())/Size,1.35f,1.62f);
            TypedBody(Full,X+Pad,Y+173,PW-2*Pad,Size,BodyColor,Leading);
            if(!Consequence.IsEmpty())
            {
                Line(X+Pad,Y+354,PW-2*Pad,Rule);Text(TEXT("签字前 / 得到与承担"),X+Pad,Y+365,16,Brass);
                Paragraph(Consequence,X+Pad,Y+392,PW-2*Pad,18,Muted);
            }
            if(HasChoices)
            {
                const FString Choices[]={D->Panel==EStationPanel::Restart?TEXT("保留当前进度"):D->Panel==EStationPanel::Allocation?TEXT("公开原始名单与担保关系"):D->Panel==EStationPanel::AllocationConfirm?TEXT("确认公示"):D->Panel==EStationPanel::RationConfirm?TEXT("确认扣费并供光"):TEXT("确认启动"),D->Panel==EStationPanel::Restart?TEXT("清除本段进度并重来"):D->Panel==EStationPanel::Allocation?TEXT("仅公示规则，封存个人名单"):TEXT("返回调整")};
                const int32 ChoiceCount=D->Panel==EStationPanel::MandateCompare?3:2;
                for(int32 I=0;I<ChoiceCount;++I)
                {
                    const float CY=Y+(ChoiceCount==3?420:447)+I*54;const bool Selected=D->Selection==I;
                    if(Selected){const float Settle=D->ReducedMotion?1.f:FMath::Clamp((Now-State.SelectionChanged)/.16f,0.f,1.f);Box(X+Pad+6*(1-Settle),CY,PW-2*Pad-6*(1-Settle),50,FLinearColor(.17f+.045f*(1-Settle),.145f,.105f,.97f));Box(X+Pad,CY,3+2*(1-Settle),50,Brass);}
                    Text(Selected?TEXT("›"):TEXT("·"),X+Pad+17,CY+10,25,Selected?Brass:Muted);Text(CrisisMenu?D->GetCrisisChoiceLabel(I):MandateMenu?D->GetMandateChoiceLabel(I):D->Panel==EStationPanel::ReturnHome?(I==0?TEXT("继续留在社区"):TEXT("保存进度，返回主页")):DistrictMenu?D->GetDistrictChoiceLabel(I):DutyMenu?D->GetDutyMenuChoice(I):SupplyMenu?D->GetSupplyMenuChoice(I):ConflictMenu?D->GetConflictMenuChoice(I):Choices[I],X+Pad+53,CY+10,(CrisisMenu||MandateMenu||DistrictMenu||DutyMenu||SupplyMenu||ConflictMenu)?22:24,Selected?Paper:Muted);
                }
            }
        }
        Line(X+Pad,FooterY-14,PW-2*Pad,Edge);Key(TEXT("E"),X+Pad,FooterY,Paper);
        const FString Action=D->IsTyping()?((D->Panel==EStationPanel::WorldRecord||D->Panel==EStationPanel::Exploration)?TEXT("显示整句"):TEXT("显示全文")):D->Panel==EStationPanel::DialogueLog||D->Panel==EStationPanel::WorldRecord||IsDocument||IsJournal||IsRelationships||D->Panel==EStationPanel::Exploration||D->Panel==EStationPanel::NeighborhoodMap||D->Panel==EStationPanel::Achievements?TEXT("收起"):D->Panel==EStationPanel::RationPlan?TEXT("勾选 / 核对"):D->Panel==EStationPanel::Plans?TEXT("查看并复核"):TEXT("确认");
        Text(Action,X+Pad+42,FooterY+3,18,BodyColor);RightText(D->Panel==EStationPanel::Achievements?TEXT("H / 退格  收起"):D->Panel==EStationPanel::DialogueLog?TEXT("L / 退格  收起"):D->Panel==EStationPanel::WorldRecord?TEXT("退格  返回"):D->Panel==EStationPanel::NeighborhoodMap?TEXT("M / 退格  收起"):D->Panel==EStationPanel::Exploration?TEXT("退格  返回"):IsRelationships?TEXT("W / S  切换人物       Tab / 退格  收起"):IsJournal?TEXT("J / 退格  收起"):IsDocument?TEXT("退格  返回"):TEXT("W / S  选择       退格  返回"),X+PW-Pad,FooterY+3,18,SubColor);return;
    }
    // The objective dims after eight seconds rather than retaining a dominant plaque.
    const float ObjectiveAlpha=Now-State.ObjectiveChanged<8.f?1.f:.62f;
    const bool SurveyRoute=D->GetSurveyGuidance()!=nullptr;
    Box(Left-12,34,548,134,FLinearColor(.006f,.01f,.011f,.41f*ObjectiveAlpha));Text(SurveyRoute?TEXT("巡光调查 / 没有登记的白昼"):D->GetMissionTitle(),Left,43,20,FLinearColor(Brass.R,Brass.G,Brass.B,ObjectiveAlpha));
    Line(Left,75,42,Brass);RightText(SurveyRoute?FString::Printf(TEXT("%02d / 15"),StationSurvey::Count(D->Progress->SurveySolvedMask,0,15)):FString::Printf(TEXT("%02d / %02d"),D->GetQuestStage()+1,D->GetQuestStepCount()),Left+510,45,15,Muted);Paragraph(D->GetTypedObjective(),Left,89,510,SurveyRoute?20:22,FLinearColor(Paper.R,Paper.G,Paper.B,ObjectiveAlpha));
    if(D->GuidanceDetail>0)
    {
        if(D->SceneGuides)DrawNavigationInstruction(D,Left-12,222,548);
        DrawNavigation(D,MiniMapLeft,MiniMapTop,MiniMapWidth,MiniMapHeight,false);
    }
    if(D->Progress->Started)
    {
        const float BalanceRight=D->GuidanceDetail>0?MiniMapLeft+MiniMapWidth:Right,BalanceTop=D->GuidanceDetail>0?MiniMapTop+MiniMapHeight+18:43;
        RightText(TEXT("公共供光额度"),BalanceRight,BalanceTop,17,Muted);RightText(FString::Printf(TEXT("%02d / 100"),D->Progress->Reserve),BalanceRight,BalanceTop+28,29,Brass);
    }
    Box(W*.5f-1.5f,H*.5f-1.5f,3,3,FLinearColor(.90f,.89f,.82f,.65f));
    if(AActor* Guide=D->GetGuidanceTarget();D->ShouldShowWorldGuide(Guide)&&Guide!=D->Focus&&PlayerOwner->GetPawn())
    {
        FVector2D Projected;const float Meters=FVector::Dist2D(Guide->GetActorLocation(),PlayerOwner->GetPawn()->GetActorLocation())/100;
        const FString Name=Cast<AStationObject>(Guide)?Cast<AStationObject>(Guide)->DisplayName:Cast<AStationEngineer>(Guide)?Cast<AStationEngineer>(Guide)->PersonName:TEXT("交互目标"),Label=FString::Printf(TEXT("◇  %s  ·  %.1f m"),*Name,Meters);const float LW=Measure(Label,18);
        if(PlayerOwner->ProjectWorldLocationToScreen(D->TargetPoint(Guide),Projected,true))
        {float X=FMath::Clamp(float(Projected.X/Unit)-LW*.5f,Left,Right-LW);const float Y=FMath::Clamp(float(Projected.Y/Unit)-83,183.f,H-178.f);if(D->GuidanceDetail>0&&Y<MiniMapTop+MiniMapHeight+10&&X+LW>MiniMapLeft-12)X=FMath::Max(Left,MiniMapLeft-LW-18);Text(Label,X,Y,18,Brass);}
        else Text(FString::Printf(TEXT("转身寻找 %s  ·  %.1f m"),*Name,Meters),Left,H-152,18,Brass);
    }
    if(GetWorld()->GetTimeSeconds()<D->ResumeRecapUntil&&!D->IsAmbientStoryActive())
    {
        const float RX=Left-12,RY=D->GuidanceDetail>0&&D->SceneGuides?360.f:188.f;Box(RX,RY,548,164,FLinearColor(.01f,.016f,.018f,.91f));
        Text(TEXT("上一次留下的决定"),RX+20,RY+17,18,Brass);Paragraph(D->GetResumeRecap(),RX+20,RY+50,508,18,Paper);
    }
    if(D->IsPhysicalActionActive())
    {
        const float Progress=FMath::Clamp(D->GetPhysicalActionProgress(),0.f,1.f),PW=460.f,X=(W-PW)*.5f,Y=H-163;
        Box(X-22,Y-17,PW+44,83,FLinearColor(.007f,.01f,.011f,.9f));
        Text(D->GetPhysicalActionLabel(),X,Y,23,Paper);RightText(TEXT("退格  取消"),X+PW,Y+4,17,Muted);
        Box(X,Y+43,PW,2,Rule);Box(X,Y+43,PW*Progress,2,Brass);
    }
    else if(D->Focus)
    {
        FString Prompt=D->GetPrompt();Prompt.RemoveFromStart(TEXT("E  "));Prompt.RemoveFromStart(TEXT("E "));
        const float PromptWidth=Measure(Prompt,23)+65,X=(W-PromptWidth)*.5f,Y=H-151;
        Box(X-17,Y-12,PromptWidth+34,57,FLinearColor(.007f,.01f,.011f,.84f));Key(TEXT("E"),X,Y,Paper);Text(Prompt,X+44,Y,23,Paper);
    }
    if(D->IsAmbientStoryActive()&&!D->GetDisplayedText().IsEmpty())Caption(true);
    if(GetWorld()->GetTimeSeconds()<D->ToastUntil)
    {
        const float TW=FMath::Min(930.f,SafeW-80),TX=(W-TW)*.5f,TY=H-241;Box(TX-22,TY-16,TW+44,77,FLinearColor(.007f,.01f,.011f,.91f));Line(TX,TY-16,52,Brass);Paragraph(D->Toast,TX,TY,TW,21,Paper);
    }
    DrawAchievementToast(D,Right,H);
    Text(TEXT("Shift 疾跑   J 任务   Tab 关系   M 地图   K 巡光手记   L 对白   H 留光簿"),Left,H-55,16,Muted);RightText(D->IsVoiceEnabled()?TEXT("V  人声开启       Esc  主页"):TEXT("V  人声关闭       Esc  主页"),Right,H-55,17,Muted);
}
