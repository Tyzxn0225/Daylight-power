#pragma once
namespace StoryAchievements20 {
struct FEntry {const TCHAR* Id;const TCHAR* Title;int32 Category;bool Hidden;const TCHAR* Hint;const TCHAR* Description;};
static const FEntry Entries[]={
{TEXT("first_borrowed_light"),TEXT("第一束借来的光"),1,false,TEXT("完成旧站检修，并看到首轮供光落地。"),TEXT("旧站重新亮起。谁先见光、谁继续等，才刚成为问题。")},
{TEXT("names_have_weight"),TEXT("名字不是空格"),1,false,TEXT("在决定公示方式前，核对人和名单。"),TEXT("听过林禾与周驰的说明，也读过原始名单。一个名字背后有一户生活。")},
{TEXT("hear_the_objection"),TEXT("把反对听完"),0,false,TEXT("作出公示选择后，回访三位当事人。"),TEXT("你听完了林禾、叶岑和周驰的回应。被理解，不等于被同意。")},
{TEXT("after_the_light"),TEXT("并非皆大欢喜"),1,true,TEXT("留意一轮配给之后，人们实际经历了什么。"),TEXT("核验首夜三处结果，听完三方回访并读到结算。额度平了，生活没有因此平整。")},
{TEXT("a_place_at_home"),TEXT("家里留的位置"),0,false,TEXT("回到林家，看看没有递出的便条。"),TEXT("你读过那张便条。林禾划掉了代签的一栏，没有划掉哥哥的座位。")},
{TEXT("the_old_signature"),TEXT("当年那一笔"),0,true,TEXT("沿着家人的故事，回看一笔旧承诺。"),TEXT("经历了兄妹关于旧保证书的谈话。你想护住的家，也有不能替别人写的名字。")},
{TEXT("not_a_blank_consent"),TEXT("异议仍在"),0,true,TEXT("面对退件冲突时，把原件与当事人的说明都留下。"),TEXT("随行核验，读过封签，听完双方并完成退件处置。没有一张回执能把异议自动抹掉。")},
{TEXT("measure_before_signing"),TEXT("先测，再签字"),1,false,TEXT("核对供货原单、现场读数与当事人的说明。"),TEXT("三份原单、三处测量、三方说明之后，你完成了供货。签名有事实，也仍有代价。")},
{TEXT("clock_out_not_debt_free"),TEXT("下班不是清账"),0,false,TEXT("完成一次真实验收，并核对本班结算。"),TEXT("本班工时与收入已经核销。下班之后，还有人等你回来。")},
{TEXT("working_but_repossessed"),TEXT("合格，却被收回"),1,true,TEXT("在回收片区检查一盏退回来的灯。"),TEXT("退光井的现场检验证明：灯具合格，停订的是原来的地址。")},
{TEXT("two_claims_one_balance"),TEXT("两份都不是空白"),1,false,TEXT("在回光埠作决定前，核对检验、押金与复售原单。"),TEXT("你看见两边的申请，却只有同一笔清算额。任何选择都不能把另一份申请写成空白。")},
{TEXT("objections_beside_signature"),TEXT("签名旁的异议"),0,true,TEXT("让一轮处置走到实际回执与人物回应。"),TEXT("本批处置已经落地，兄妹的回应也留在结尾。先试亮或先退款，都不等于另一边不难受。")},
{TEXT("one_meter_two_prices"),TEXT("同一台表"),1,false,TEXT("在后巷计量庭查找旧检定原单。"),TEXT("同一型号的仪表，对应两种价格。你记下的是问题，不是一个假装修好了的答案。")},
{TEXT("a_door_for_deliveries"),TEXT("留给送货人的门"),2,false,TEXT("查看厨房后门的插销，让这条近路真正通行。"),TEXT("厨房和院落之间，多了一条真实可走的门道。它没有增加额度，也没有要求别人先欠你一次。")},
{TEXT("clauses_in_the_light"),TEXT("亮处也有条款"),1,false,TEXT("走进恒昼访客廊，读完三份现场条款。"),TEXT("你读过续约、维保与退出。明亮的窗后，也有被分开计算的生活。")},
{TEXT("stay_a_moment"),TEXT("不急着走"),2,false,TEXT("在一个可驻足的地方，安静留一会儿。"),TEXT("你没有立刻赶往下一个任务。停下来，本身也可以是一种观看。")},
{TEXT("the_shadow_marks"),TEXT("影子曾经会走"),2,false,TEXT("留意旧操场墙脚的时间刻度。"),TEXT("你读过那几道影子刻度。过去，人还能抱怨太阳太大。")},
{TEXT("ask_the_sixth_household"),TEXT("别替邻居答应"),2,false,TEXT("到共光露台读一读分摊本。"),TEXT("五个手印，不会自动成为第六户的同意。你留下问题，没有擅自替人接线。")},
{TEXT("heat_is_not_an_address"),TEXT("门还认另一笔账"),1,true,TEXT("核验两端余热分流，再听完等候者的回应。"),TEXT("本班热量已经实际分配。暖房里的温度，没有自动变成门外人的居住资格。")},
{TEXT("a_key_with_a_list"),TEXT("门认得你了"),0,true,TEXT("把一次最终选择走到回家后的回应。"),TEXT("稳定住处和管理门卡已经兑现。林禾没有跟你搬走，下一班的核验表也将交到你手里。")},
{TEXT("leave_both_copies"),TEXT("两份原件"),0,true,TEXT("让现场的记录和合作走到各自的回执。"),TEXT("共同复核开始了，住房和维护仍要轮候。兄妹保留各自的申请，没有用同一个签名结束分歧。")},
{TEXT("tomorrow_is_not_signed"),TEXT("明天再说"),0,true,TEXT("完成一次临时保全，再回到仍在继续的生活。"),TEXT("今晚暂时保住了，林照的临时工牌已经停用。明天的工作，兄妹各自去问。")},
};
static const TCHAR* Categories[]={TEXT("人与家"),TEXT("光与账"),TEXT("街与记忆")};
}
