#pragma once
namespace Experience17 {
struct FMesh {const TCHAR* Id;const TCHAR* Replace;};
static const FMesh Meshes[]={
{TEXT("SM_N17_0403"),TEXT("SM_BZ_0403")},
{TEXT("SM_N17_0405"),TEXT("SM_BZ_0405")},
{TEXT("SM_N17_VisitorGalleryDetails"),TEXT("")},
};
struct FRecord {const TCHAR* Name;FVector Position;const TCHAR* Body;const TCHAR* Thought;const TCHAR* Voice;};
static const FRecord Records[]={
{TEXT("保供细则与值守批注"),FVector(4400.000f,3400.000f,15.000f),TEXT("恒昼居住合作社 / 保供与维修摘录\n\n基础维持费按地址结算；既有会员拥有优先续约权。访客步道照明由合作社维护，不使用旧公共光站的应急额度。\n\n值守员的批注：我在这里擦一晚的玻璃，宿舍的灯却要另外续费。\n\n这份阅览副本不包含居民个人姓名。它说明谁能优先续约，不增加玩家账户的额度。"),TEXT("林照：我以前以为，他们付的钱多，所以灯亮得久。原来有些人，先买到的是不用排队。"),TEXT("EV_Urban_001")},
{TEXT("设备与地址：两份合同"),FVector(3800.000f,2720.000f,15.000f),TEXT("光源设备维保单 / 地址订阅条款\n\n设备检验合格，只说明光源还能使用。地址订阅到期，光源可被拆下，转入另一份仍然生效的合同。\n\n维修员的工时由合作社结算；被停订的家庭仍须另行承担解约费用。把灯修好，不等于获得继续使用它的权利。\n\n副本保留双方条款，不改变已经签署的押金和回收处置。"),TEXT("林照：灯能接着用，地址却不能。原来我们修好的东西，还是轮不到我们决定。"),TEXT("EV_Urban_002")},
{TEXT("可以退出的合作社"),FVector(5300.000f,2400.000f,15.000f),TEXT("合作社成员退出须知 / 访客阅览本\n\n成员可以提出退出。住处保供、工位资格和家属补贴分别复核；退出一项，并不自动免除其他合同中的责任。\n\n签名旁写着：已理解，自愿加入。背面另有一行：请先确认，退出之后住在哪里。\n\n条款没有写谁是好人。便利、依赖与退出的代价，可以出现在同一份协议里。"),TEXT("林照：纸上写着可以退出。可工作、住处和家里的灯，都压在同一个签名上。"),TEXT("EV_Urban_003")},
};
struct FQuiet {const TCHAR* Name;FVector Position;FVector Camera;FVector Aim;const TCHAR* Body;};
static const FQuiet Quiet[]={
{TEXT("林家 / 留一会儿"),FVector(-3650.000f,2040.000f,19.000f),FVector(-3680.000f,2070.000f,165.000f),FVector(-3560.000f,1680.000f,100.000f),TEXT("先不用证明自己有用。饭桌、空座和未写完的便条都在这里。")},
{TEXT("院落 / 听见生活"),FVector(-4630.000f,430.000f,3.000f),FVector(-4530.000f,620.000f,172.000f),FVector(-4900.000f,450.000f,130.000f),TEXT("晾晒与分摊，比一张任务表更早占据这座院落。")},
{TEXT("露台 / 看向恒昼"),FVector(-2820.000f,-3420.000f,300.000f),FVector(-2860.000f,-3350.000f,465.000f),FVector(3500.000f,-1000.000f,900.000f),TEXT("没有新的委托。只是从共同分摊的一盏灯，看向不必排队的白昼。")},
};
}
