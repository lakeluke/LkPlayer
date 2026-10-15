# LkPlayer

面向本地媒体库的 Windows 桌面播放器。指定一个媒体库根目录，程序递归扫描其中的视频与旁挂资源
（海报、nfo、字幕、剧照），以卡片网格提供「全库平铺」和「逐层钻取」两种浏览方式，支持按 nfo 标签过滤，
并提供三种播放方式：ffplay 独立窗口、ffplay 画面内嵌到播放页、或直接交给 Windows 注册的视频播放器。

## 环境依赖

| 依赖 | 版本 | 说明 |
| --- | --- | --- |
| Qt | 6.5 及以上（开发环境 6.11.2） | Core / Gui / Widgets / Qml / Quick / QuickControls2 / QuickDialogs2 / QuickLayouts / QuickEffects / Svg / Xml / Concurrent |
| CMake | 3.21 及以上 | |
| C++ | C++17 | |
| 编译器 | MinGW-w64 或 clang（MSYS2 UCRT64 已验证） | |
| ffmpeg / ffplay | 任意近期版本 | 播放与封面抽帧，路径由用户在设置中指定 |

> 播放完全依赖外部 `ffplay` 进程，未使用 `Qt6Multimedia`（本环境不提供该模块）。

## 构建

以 MSYS2 UCRT64 为例：

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

产物：`build/LkPlayer.exe`（GUI 程序，无控制台）与 `build/tst_core.exe`（单元测试）。

## 运行与首次配置

1. 运行 `build/LkPlayer.exe`。
2. 打开右上角「设置」：
   - **媒体库目录**：选择媒体库根目录，保存后自动重新扫描。
   - **程序目录**：指向包含 `ffmpeg.exe` / `ffplay.exe` 的目录，界面会实时校验并显示可用状态。
   - **播放模式**：
     - `独立窗口`：ffplay 打开自己的窗口（默认，最稳定）。
     - `内嵌到界面`：把 ffplay 的画面嵌入播放页。
     - `系统播放器`：调用 Windows 注册的视频播放器打开，不需要 ffmpeg / ffplay；
       此时播放、进度与音量由该播放器接管，LkPlayer 的控制条会置灰。
3. 回到媒体库，点击任意卡片开始播放。播放页右上角与底部控制条的胶囊按钮可在三种模式间循环切换。

配置写入 `%LOCALAPPDATA%\LkPlayer\LkPlayer\lkplayer.ini`；
封面缓存位于 `%LOCALAPPDATA%\LkPlayer\LkPlayer\cache\covers`（可在设置中清理）；
日志写入 `%LOCALAPPDATA%\LkPlayer\LkPlayer\lkplayer.log`（目录可用环境变量 `LKPLAYER_LOG_DIR` 覆盖）。

## 媒体库约定

目录层级与旁挂资源没有强制结构，识别规则如下（`<video>` 为去掉扩展名的视频文件名）：

```
Root/
└── Scene/
    └── Act1/
        └── PPC-001/
            ├── PPC-001-title.mp4        # 视频
            ├── PPC-001-title.nfo        # 或 movie.nfo / <目录名>.nfo
            ├── PPC-001-title-poster.jpg # 或 PPC-001-title.jpg / -cover / -thumb
            ├── PPC-001-title-fanart.jpg # 或 -backdrop / -background / fanart.jpg
            ├── PPC-001-title.zh.srt     # 同名前缀字幕，自动关联
            └── extrafanart/             # 或 stills / screenshots / thumbs
                └── 01.jpg
```

- **海报**：`<video>`、`<video>-poster`、`<video>.poster`、`<video>-cover`、`<video>-thumb`、`<video>-folder`；
  无视频级海报时回退到目录级 `poster / folder / cover / default / thumb`。
- **nfo**：`<video>.nfo`、`<目录名>.nfo`、`movie.nfo`、`tvshow.nfo`、`episode.nfo`、`album.nfo`（Kodi 风格，容错解析）。
- **字幕**：`.srt .ass .ssa .sub .vtt .smi .idx .sup`，按视频名同前缀匹配。
- **剧照**：同前缀图片，以及 `extrafanart / stills / screenshots / thumbs / backdrops` 子目录内的图片。
- 扫描会跳过 `.lkcache`、`@eaDir`、`$RECYCLE.BIN`、`.git`、`node_modules` 等目录。

无海报时由 `ffmpeg` 抽取一帧（统一裁剪为 480×720）作为封面，结果按「路径 + 修改时间」做磁盘缓存。

## 浏览与筛选

- **全库平铺**：把 Root 下所有视频汇总成一个网格。
- **逐层钻取**：以面包屑逐层进入文件夹，进入某层后平铺该层内容，可继续下钻。
- **标签过滤**：侧栏按 nfo 标签多选筛选，即时生效；顶部搜索框按标题 / 文件名 / 标签检索。

## 播放快捷键

| 按键 | 行为 |
| --- | --- |
| `Space` | 播放 / 暂停 |
| `←` / `→` | 后退 / 前进 10 秒 |
| `↑` / `↓` | 音量 ±5 |
| `M` | 静音切换 |
| `Esc` | 返回媒体库 |

> 快捷键与底部控制条只对 ffplay 后端生效；使用「系统播放器」时由该播放器自己处理。

## 测试

```bash
cd build && ctest --output-on-failure
```

覆盖 nfo 解析（完整 / 字段缺失 / 截断 XML）、扩展名与时长格式化、封面 ID 编解码，
以及扫描时的层级遍历与旁挂资源关联。
