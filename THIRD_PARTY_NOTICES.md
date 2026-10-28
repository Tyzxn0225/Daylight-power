# 第三方资源与公开范围

## 仓库内容

MIT 仅用于本仓库中项目自有的代码、剧情文字与文档。公开仓库不包含 Unreal Engine 源码或二进制文件，不包含生产工程的人物、骨骼、动画、音频、原始贴图、模型源文件或模型权重。

UE 项目配置中的资源引用只是标识，不授予对应素材的使用权。获取完整可编辑工程须由权利人另行提供合法资源。

## Unreal Engine 与 Epic 内容

Unreal Engine 及相关示例、角色和内容按各自协议使用；其许可不是 MIT。原始素材与包含素材的成品游戏具有不同分发条件。参考：

- https://www.unrealengine.com/eula/unreal
- https://www.unrealengine.com/eula/content

## 声音与其他制作材料

本地制作记录中使用的第三方声音包括下列非语言反应音。对白使用本项目原创的合成参考声，经 Qwen3-TTS 生成；没有克隆真人演员或名人的录音。本仓库不随附原始语音包、参考音频或模型权重，也不把它们重新许可为 MIT。

| 材料 | 作者与来源 | 使用与修改 |
| --- | --- | --- |
| Super Dialogue Audio Pack | [Dillon Becker](https://dillonbecker.itch.io/sdap)；本项目所用表演者 Ian Lampert、Alex Brodie | CC BY 4.0；截取非语言反应音，转为单声道 PCM16 24 kHz、调整音量与首尾淡化 |
| Free Voice Clips Pack: Mature Female / Bright Female | [Cici Fyre](https://cicifyre.itch.io/free-voice-clips-pack-mature-female)，[Bright Female](https://cicifyre.itch.io/free-voice-clips-pack-bright-female) | 作者发布页标示 CC0 1.0；随包 readme 也允许免版税使用。转为单声道、调整音量与淡化 |
| Qwen3-TTS | [QwenLM/Qwen3-TTS](https://github.com/QwenLM/Qwen3-TTS) | 本地合成流程；模型采用 Apache 2.0，不随程序分发；对白和参考声音为本项目制作 |

配乐为本项目编写的原创器乐合成曲，未采样商业游戏音乐。合成器乐不等同于现场乐团演奏。

## 画面资源

场景表面使用 [Poly Haven 的 CC0 材质](https://polyhaven.com/license)：`brick_wall_001`、`painted_plaster_wall`、`concrete`、`rusty_painted_metal`、`wood_cabinet_worn_long`、`asphalt_02`。项目制作了贴图转换、材质组合与空间尺度适配。

主要人物基础网格由本项目原创人物设计图通过 Hyper3D Rodin 生成，再进行 Blender 编辑、骨骼与引擎适配。其使用依据 [Hyper3D 的 Rodin 条款](https://hyper3d.ai/legal/terms)，不以 ChatAvatar 的条款代替，也不把服务输出的权利状态等同于第三方已经保证其可版权性。

Epic 示例动作、引擎字体和运行时组件保留各自许可。完整程序的分发只包括运行所需的烘焙内容，不提供第三方素材源文件。引擎第三方软件和字体的随附许可证应随完整程序保留。

若另行提供可玩程序，必须同时提供该程序实际使用的第三方素材清单、作者、来源、许可和必要的修改说明；只在核对完成的范围内分发。

## 设计预览

`Docs/Exploration` 中的地图来自本项目虚拟场景的平面投影，人物与地点为虚构。页面中的界面和规则是设计预览，并非商业游戏截图，也不是对其他游戏素材的复用授权。
