#pragma once
namespace Mandate25 {
struct FLine {const TCHAR* Scene;const TCHAR* Text;const TCHAR* Audio;};
static const FLine Lines[]={
{TEXT("intro"),TEXT("林禾：哥，这张续供申请，把我填进关联担保人了。"),TEXT("EV_Mandate_001")},
{TEXT("intro"),TEXT("林照：我只留了地址，没替你签过这一项。"),TEXT("EV_Mandate_002")},
{TEXT("intro"),TEXT("林禾：我知道。可表上写着，户主确认就算全家同意。"),TEXT("EV_Mandate_003")},
{TEXT("intro"),TEXT("林禾：旧账我会记。这次，我的名字得由我自己撤。"),TEXT("EV_Mandate_004")},
{TEXT("crew"),TEXT("叶岑：我让他们自动带入家属，是想让组员少跑两趟。"),TEXT("EV_Mandate_005")},
{TEXT("crew"),TEXT("林照：少跑两趟，就能把没答应的人算进去？"),TEXT("EV_Mandate_006")},
{TEXT("crew"),TEXT("叶岑：我没说能。你把岗位编号交上去，新申请就得全组复核。"),TEXT("EV_Mandate_007")},
{TEXT("crew"),TEXT("叶岑：这事有我的责任。可等工资的人，不止你一个。"),TEXT("EV_Mandate_008")},
{TEXT("broker"),TEXT("周驰：你撤整张单，快线名额就没了。拆成个人申请，值守流程得一起查。"),TEXT("EV_Mandate_009")},
{TEXT("broker"),TEXT("林照：你怎么比递表的人还清楚？"),TEXT("EV_Mandate_010")},
{TEXT("broker"),TEXT("周驰：每多一户走完快线，我都有代办费。你妹妹没签，我也没拦。"),TEXT("EV_Mandate_011")},
{TEXT("broker"),TEXT("周驰：我能帮你撤名字。别让我替谁证明，这事从来没发生。"),TEXT("EV_Mandate_012")},
{TEXT("owner"),TEXT("林禾：我的撤回意见写好了。剩下的是你的申请，不是你替我选人生。"),TEXT("EV_Mandate_013")},
{TEXT("owner"),TEXT("林照：整单撤掉，咱们重新排。拆开送审，叶岑那边得解释。"),TEXT("EV_Mandate_014")},
{TEXT("owner"),TEXT("林禾：别为了证明你是个好哥哥，再替别人答应一遍。"),TEXT("EV_Mandate_015")},
{TEXT("owner"),TEXT("林照：好。你的不愿意，我先原样留下。"),TEXT("EV_Mandate_016")},
{TEXT("ending0"),TEXT("林禾：他们收下整单撤回了。下一次还要从队尾开始。"),TEXT("EV_Mandate_017")},
{TEXT("ending0"),TEXT("林照：我以为只要肯多干，总能把家里的灯留下。"),TEXT("EV_Mandate_018")},
{TEXT("ending0"),TEXT("林禾：我也想亮着。可我不想，每一次亮都先欠一个我没答应的人。"),TEXT("EV_Mandate_019")},
{TEXT("ending0"),TEXT("林照：回去吧。这张回执，我们一人留一份。"),TEXT("EV_Mandate_020")},
{TEXT("ending1"),TEXT("林禾：我的名字撤下来了。你的申请，转人工复核。"),TEXT("EV_Mandate_021")},
{TEXT("ending1"),TEXT("林照：岗位编号也交了。明天叶岑会来找我。"),TEXT("EV_Mandate_022")},
{TEXT("ending1"),TEXT("林禾：那就听她说。今天轮到我说不，明天也别省掉她的那句。"),TEXT("EV_Mandate_023")},
{TEXT("ending1"),TEXT("林照：我带原件去。不替她答，也不让他们把今天抹掉。"),TEXT("EV_Mandate_024")},
};
}