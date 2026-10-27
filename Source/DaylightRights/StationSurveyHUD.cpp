#include "StationPrototype.h"
#include "StationSurveyData.inl"
#include "Engine/Canvas.h"

bool AStationHUD::DrawSurveyHUD(AStationDirector* D)
{
    if(D->Panel!=EStationPanel::SurveyGame&&D->Panel!=EStationPanel::SurveyBook&&D->Panel!=EStationPanel::SurveyRecord)return false;
    using namespace StationSurveyData;
    const FLinearColor Ink(.012f,.025f,.027f,.98f),Paper(.91f,.89f,.81f),Gold(.83f,.66f,.39f),Teal(.39f,.73f,.68f),Dim(.52f,.60f,.59f),Rule(.16f,.26f,.26f),Warn(.91f,.57f,.39f);
    const float W=Canvas->SizeX/Unit,H=Canvas->SizeY/Unit;
    const float X=(W-1400.f)*.5f,Y=(H-770.f)*.5f;
    auto Box=[&](float A,float B,float C,float E,FLinearColor Color){DrawRect(Color,A*Unit,B*Unit,C*Unit,E*Unit);};
    auto Line=[&](float A,float B,float C,float E,FLinearColor Color,float Thick=1.f){DrawLine(A*Unit,B*Unit,C*Unit,E*Unit,Color,Thick*Unit);};
    auto Cross=[&](FVector2D P,FLinearColor Color){Line(P.X-10,P.Y,P.X+10,P.Y,Color,2);Line(P.X,P.Y-10,P.X,P.Y+10,Color,2);};
    Box(0,0,W,H,FLinearColor(.002f,.008f,.01f,.88f));Box(X,Y,1400,770,Ink);
    Line(X,Y,X+1400,Y,Gold,2);Line(X+36,Y+115,X+1364,Y+115,Rule);
    Text(TEXT("巡光 / FIELD ARCHIVE"),X+36,Y+24,16,Teal);
    Text(D->Panel==EStationPanel::SurveyBook?TEXT("没有登记的白昼"):D->SurveyActive>=0?Sites[D->SurveyActive].Title:TEXT("现场记录"),X+36,Y+57,32,Paper);
    Text(FString::Printf(TEXT("旧光里 %d/7   恒昼 %d/4   回光埠 %d/4"),StationSurvey::Count(D->Progress->SurveySolvedMask,0,7),StationSurvey::Count(D->Progress->SurveySolvedMask,7,11),StationSurvey::Count(D->Progress->SurveySolvedMask,11,15)),X+920,Y+61,17,Gold);
    if(D->Panel==EStationPanel::SurveyBook)
    {
        const float ListX=X+36,ListY=Y+137,RowH=32;
        for(int32 Row=0;Row<=Count;++Row)
        {
            const bool Selected=D->Selection==Row;
            if(Selected){Box(ListX-10,ListY+Row*RowH-3,610,RowH,FLinearColor(.06f,.14f,.14f));Box(ListX-10,ListY+Row*RowH-3,3,RowH,Gold);}
            const int32 I=Row-1;
            const FString Label=Row==0?TEXT("返回主线指引"):FString::Printf(TEXT("%02d  %s  %s"),I+1,D->SurveySolved(I)?TEXT("已核验"):D->SurveyAvailable(I)?TEXT("可调查"):TEXT("待开放"),Sites[I].Title);
            Text(Label,ListX+5,ListY+Row*RowH+2,18,Selected?Paper:Row==0||D->SurveyAvailable(I)?Dim:Rule);
        }
        Line(X+675,Y+137,X+675,Y+673,Rule);
        const int32 I=D->Selection-1;const float RX=X+713;
        if(I>=0)
        {
            Text(Sites[I].Place,RX,Y+148,24,Gold);
            Text(Mechanic(Sites[I].Type),RX,Y+194,17,Teal);
            float Bottom=Paragraph(D->SurveySolved(I)?Sites[I].Record:Sites[I].Question,RX,Y+233,610,21,Paper);
            if(!D->SurveySolved(I)&&!D->SurveyAvailable(I))
            {
                const FString Gate=I==14?TEXT("汇档条件：旧光里、恒昼区、回光埠各至少两份记录。余下现场可以在归档后继续补看。"):I>=11?TEXT("在主线完成本班验收后开放跨街片区。"):I==2||I==3||I==4?TEXT("收到退件通知后，现场原件开放调查。"):TEXT("随本地供光主线推进开放。");
                Paragraph(Gate,RX,FMath::Max(Bottom+28,Y+470),610,19,Dim);
            }
            else Paragraph(D->SurveySolved(I)?TEXT("E 重读现场记录。记录不会替任何人授权，也不增加供光额度。"):TEXT("E 追踪此现场。小地图与方向提示会切换到巡光调查；K 可随时恢复主线。"),RX,FMath::Max(Bottom+28,Y+545),610,19,Teal);
        }
        else
        {
            Text(TEXT("把灯具的去向，接回人的生活"),RX,Y+151,26,Gold);
            Paragraph(TEXT("从旧巷到恒昼区，再到回光埠。追查一盏技术合格、却总被收回的灯。\n\n先看懂现场，再决定相信哪一种说法。各区可自由选择调查顺序，手记会按片区给出下一站。\n\n这是一条探索调查线。主线结局不要求收集齐全部记录。"),RX,Y+219,610,23,Paper);
            Text(TEXT("E 恢复主线方向，保留全部调查进度"),RX,Y+600,20,Teal);
        }
        Line(X+36,Y+695,X+1364,Y+695,Rule);Text(TEXT("W/S 选择现场     E 追踪 / 重读     Backspace / K 合上手记"),X+36,Y+724,18,Dim);return true;
    }
    const int32 I=D->SurveyActive;if(I<0||I>=Count)return true;
    const auto& Site=Sites[I];const float RX=X+960;
    if(D->Panel==EStationPanel::SurveyRecord)
    {
        Text(TEXT("L—042"),X+55,Y+182,66,Gold);Text(TEXT("技术合格 / 流转档案"),X+59,Y+277,19,Teal);
        Line(X+65,Y+350,X+338,Y+350,Rule);Line(X+65,Y+397,X+338,Y+397,Rule);Line(X+65,Y+444,X+338,Y+444,Rule);
        for(int J=0;J<3;++J){Cross(FVector2D(X+70+J*130,Y+520),J==0?Gold:Teal);if(J<2)Line(X+84+J*130,Y+520,X+186+J*130,Y+520,Rule,2);}
        Text(Site.Place,X+430,Y+153,23,Gold);
        const float Bottom=Paragraph(Site.Record,X+430,Y+205,900,24,Paper);
        Paragraph(Site.Lead,X+430,FMath::Max(Bottom+27,Y+560),900,20,Teal);
        Line(X+36,Y+695,X+1364,Y+695,Rule);Text(TEXT("E / Backspace 关闭记录     K 手记中可重读、追踪下一站"),X+36,Y+724,18,Dim);return true;
    }
    const auto& S=D->SurveyState;const auto A=StationSurvey::Answer(S);
    Text(Site.Place,X+36,Y+139,18,Dim);Text(Mechanic(S.Type),RX,Y+145,24,Gold);
    const float QuestionBottom=Paragraph(Site.Question,RX,Y+196,390,22,Paper);
    Line(X+920,Y+140,X+920,Y+670,Rule);
    const float AX=X+56,AY=Y+204,AW=810,AH=345;
    Box(AX,AY,AW,AH,FLinearColor(.02f,.045f,.047f));
    if(S.Type==StationSurvey::Kind::Align)
    {
        for(int N=0;N<=14;++N)Line(AX+N*58,AY,AX+N*58,AY+AH,Rule);
        for(int N=0;N<=6;++N)Line(AX,AY+N*57,AX+AW,AY+N*57,Rule);
        const FVector2D Center(AX+AW*.5f,AY+AH*.5f);const float R=FMath::DegreesToRadians(float(S.Value[2])*3.f);
        const FVector2D Points[]={FVector2D(-235,-105),FVector2D(218,-90),FVector2D(36,118)};
        for(const FVector2D& P:Points)
        {
            Cross(Center+P,Gold);
            const FVector2D Q(P.X*FMath::Cos(R)-P.Y*FMath::Sin(R),P.X*FMath::Sin(R)+P.Y*FMath::Cos(R));
            Cross(Center+Q+FVector2D(S.Value[0]*9,S.Value[1]*9),Teal);
        }
        Text(TEXT("原件定位 +"),AX+20,AY+17,16,Gold);Text(TEXT("透光底片 +"),AX+180,AY+17,16,Teal);
        Text(TEXT("领料时间  /  住户栏  /  流转编号"),AX+160,AY+154,23,Paper);
        Text(TEXT("横移 / 纵移 / 转角"),RX,FMath::Max(QuestionBottom+35,Y+358),18,Teal);
    }
    else if(S.Type==StationSurvey::Kind::Pulse)
    {
        for(int N=0;N<=6;++N)Line(AX,AY+N*55,AX+AW,AY+N*55,Rule);
        Text(TEXT("铭牌参考"),AX+20,AY+15,17,Gold);Text(TEXT("探针实测"),AX+180,AY+15,17,Teal);
        const float Clock=D->ReducedMotion?0.f:GetWorld()->GetTimeSeconds()*.7f;
        for(int Channel=0;Channel<2;++Channel)
        {
            const auto& V=Channel==0?A:S.Value;FVector2D Previous;
            for(int N=0;N<380;++N)
            {
                const float T=float(N)/379.f;const float Wave=FMath::Sin((T*(2.f+V[0]*.3f)+V[1]/8.f+Clock)*2.f*PI);
                const FVector2D P(AX+20+T*(AW-40),AY+AH*.53f-Wave*95);
                if(N)Line(Previous.X,Previous.Y,P.X,P.Y,Channel==0?Gold:Teal,Channel==0?1.2f:2.f);Previous=P;
            }
        }
        Text(FString::Printf(TEXT("参考频率 %d    相位 %d/8"),A[0],A[1]),AX+20,AY+AH-34,17,Gold);
        Paragraph(TEXT("先听参考，再听探针。让两条曲线的周期与峰值重合。静音时也可以完整完成。"),RX,FMath::Max(QuestionBottom+35,Y+385),390,19,Teal);
    }
    else if(S.Type==StationSurvey::Kind::Flow)
    {
        const TCHAR* Names[]={TEXT("诊疗等候位"),TEXT("夜班值守位"),TEXT("安全保护支路")};
        for(int J=0;J<3;++J)
        {
            const float B=AY+40+J*95;const int Loss=J==0?S.Seed%2:J==1?(S.Seed/2)%2:0;
            Text(Names[J],AX+20,B-5,19,Paper);Line(AX+184,B+20,AX+740,B+20,Rule,9);
            Line(AX+184,B+20,AX+184+S.Value[J]*76,B+20,J==S.Axis?Gold:Teal,7);
            Text(FString::Printf(TEXT("输入 %d − 管损 %d ≥ 需求 %d"),S.Value[J],Loss,A[J]-Loss),AX+195,B-17,18,J==S.Axis?Gold:Dim);
        }
        const int Used=S.Value[0]+S.Value[1]+S.Value[2];Text(FString::Printf(TEXT("已分配 %d / 8 格"),Used),RX,FMath::Max(QuestionBottom+35,Y+380),28,Used>8?Warn:Teal);
        Paragraph(TEXT("这是离线检修复算。允许多种可行分配，不改动现有供光额度和章节选择。"),RX,Y+487,390,19,Dim);
    }
    else
    {
        for(int J=0;J<3;++J)
        {
            const auto& Sheet=SealForSite(I,J);
            const float CX=AX+12+J*268;Text(Sheet.Heading,CX+10,AY+15,23,Gold);
            Paragraph(Sheet.Quote,CX+10,AY+58,236,18,Dim);
            for(int C=0;C<3;++C)
            {
                const int Rank=StationSurvey::Wrap(C-A[J],3);const TCHAR* Claim=Rank==0?Sheet.Correct:Rank==1?Sheet.WrongA:Sheet.WrongB;
                const float CY=AY+125+C*69;
                if(S.Value[J]==C){Box(CX,CY-5,250,62,J==S.Axis?FLinearColor(.13f,.23f,.21f):FLinearColor(.06f,.13f,.13f));Line(CX,CY-5,CX,CY+57,J==S.Axis?Gold:Teal,3);}
                Paragraph(Claim,CX+11,CY,228,19,S.Value[J]==C?Paper:Dim);
            }
        }
        Paragraph(TEXT("原文没有说过的话，不能替原文作证。三列各选一个结论。"),RX,FMath::Max(QuestionBottom+35,Y+406),390,20,Teal);
    }
    if(S.Type!=StationSurvey::Kind::Seal)
    {
        const TCHAR* AlignLabels[]={TEXT("横移"),TEXT("纵移"),TEXT("转角")};
        const TCHAR* PulseLabels[]={TEXT("频率"),TEXT("相位"),TEXT("")};
        const TCHAR* FlowLabels[]={TEXT("诊疗"),TEXT("夜班"),TEXT("保护")};
        const int Channels=StationSurvey::Channels(S.Type);
        for(int J=0;J<Channels;++J)
        {
            const float CX=AX+J*270;const bool Selected=J==S.Axis;
            Box(CX,Y+573,250,53,Selected?FLinearColor(.12f,.20f,.18f):FLinearColor(.025f,.06f,.06f));
            const TCHAR* Label=S.Type==StationSurvey::Kind::Align?AlignLabels[J]:S.Type==StationSurvey::Kind::Pulse?PulseLabels[J]:FlowLabels[J];
            Text(FString::Printf(TEXT("%s   <  %d  >"),Label,S.Value[J]),CX+15,Y+588,22,Selected?Gold:Dim);
        }
    }
    if(D->SurveyShowHint)
    {
        const FString Hint=S.Type==StationSurvey::Kind::Align?TEXT("提示：金色十字属于原件，青色十字属于底片；三个位置都需重合。"):S.Type==StationSurvey::Kind::Pulse?TEXT("提示：频率决定周期，相位决定峰值位置；参考值已标在图下。"):S.Type==StationSurvey::Kind::Flow?TEXT("提示：每支输入至少等于需求加管损；三支输入总和不能超过 8。"):TEXT("提示：技术合格不能替代供给资格；签收不能替代同意。");
        Paragraph(Hint,RX,Y+564,390,18,Gold);
    }
    if(!D->SurveyFeedback.IsEmpty())Paragraph(D->SurveyFeedback,AX,Y+642,810,18,Warn);
    Line(X+36,Y+695,X+1364,Y+695,Rule);Text(TEXT("W/S 切换调节项   A/D 调整   E 核验   F 提示 / 重听   Backspace 退出（不扣资源）"),X+36,Y+724,18,Dim);
    return true;
}
