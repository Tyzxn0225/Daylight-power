#pragma once
namespace Crisis26
{
struct FLine {const TCHAR* Scene;const TCHAR* Text;const TCHAR* Audio;};
static const FLine Lines[]={
{TEXT("intake"),TEXT("叶岑：回水阀卡住了。富区那边还在给花廊加温，院里的等候室却要限热。"),TEXT("EV_Crisis_001")},
{TEXT("intake"),TEXT("林照：不是还有灯吗？"),TEXT("EV_Crisis_002")},
{TEXT("intake"),TEXT("叶岑：灯走应急电。热走另一条管子。先来测余热，别拿电表替人报平安。"),TEXT("EV_Crisis_003")},
{TEXT("intake"),TEXT("林照：知道了。我先看设备，这次不替谁答应。"),TEXT("EV_Crisis_004")},
{TEXT("brief"),TEXT("叶岑：旁路只能接回三十六千瓦热量。两处等候室都想要二十四，凑不出来。"),TEXT("EV_Crisis_005")},
{TEXT("brief"),TEXT("林照：先停花廊的加温。住宅和通风不能跟着断。"),TEXT("EV_Crisis_006")},
{TEXT("brief"),TEXT("叶岑：联锁会保住那两条线。两头各留十二是底线，再多给谁，就得少给另一边。"),TEXT("EV_Crisis_007")},
{TEXT("brief"),TEXT("林照：先把人留在底线里。剩下的，我到现场去看。"),TEXT("EV_Crisis_008")},
{TEXT("broker"),TEXT("周驰：你以为恒昼这头坐的都是业主？擦玻璃的、值夜的，也在廊里等交班。"),TEXT("EV_Crisis_009")},
{TEXT("broker"),TEXT("林照：所以你收他们一笔插队钱？"),TEXT("EV_Crisis_010")},
{TEXT("broker"),TEXT("周驰：我替他们找位置，也收钱。可我挣了钱，不等于他们就不冷。"),TEXT("EV_Crisis_011")},
{TEXT("broker"),TEXT("林照：你收的那笔另算。阀门上，不写谁先付过钱。"),TEXT("EV_Crisis_012")},
{TEXT("high"),TEXT("林禾：院里能全开了。可另一边的人，就得挤在主厅，对吗？"),TEXT("EV_Crisis_013")},
{TEXT("high"),TEXT("林照：对。我想让你少等一次，也想让这里的人进去。这个心思，我不装没有。"),TEXT("EV_Crisis_014")},
{TEXT("high"),TEXT("林禾：我也想。可别把那边等下班的人，都写成住在亮处的人。"),TEXT("EV_Crisis_015")},
{TEXT("high"),TEXT("林照：不会。今晚谁多拿了，谁少拿了，都留在同一张图上。"),TEXT("EV_Crisis_016")},
{TEXT("equal"),TEXT("林禾：两边都能轮换坐。哥，你这次没有把我排到最前面。"),TEXT("EV_Crisis_017")},
{TEXT("equal"),TEXT("林照：我调到一半的时候，手停了。我还是想让你先暖和。"),TEXT("EV_Crisis_018")},
{TEXT("equal"),TEXT("林禾：我知道。可一人一半，也不是谁都够了。院里那排侧座还开不了。"),TEXT("EV_Crisis_019")},
{TEXT("equal"),TEXT("林照：明天要谈的，就是为什么每次都让等的人互相分。"),TEXT("EV_Crisis_020")},
{TEXT("low"),TEXT("林禾：你看过恒昼那边的人，所以把多的留给他们了？"),TEXT("EV_Crisis_021")},
{TEXT("low"),TEXT("林照：他们也有一身工服。我不能看见那栋楼，就当里面没人挨冷。"),TEXT("EV_Crisis_022")},
{TEXT("low"),TEXT("林禾：我能理解。可我还是冷，也还是会怨你。别替我把这句删了。"),TEXT("EV_Crisis_023")},
{TEXT("low"),TEXT("林照：不删。你不用先原谅我，才有资格坐到暖处。"),TEXT("EV_Crisis_024")},
};
}
