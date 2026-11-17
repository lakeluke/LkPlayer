# LkPlayer

面向本地媒体库的跨平台桌面播放器（Windows / Linux）。指定一个媒体库根目录，程序递归扫描其中的视频与
旁挂资源（海报、nfo、字幕、剧照），以卡片网格提供「全库平铺」和「逐层钻取」两种浏览方式，支持按 nfo
标签过滤，并提供三种播放方式：libmpv 画面内嵌到播放页、ffplay 独立窗口、或直接交给系统注册的播放器。

## 环境依赖

| 依赖 | 版本 | 说明 |
| --- | --- | --- |
| Qt | 6.5 及以上（开发环境 6.11.2） | Core / Gui / Widgets / Qml / Quick / QuickControls2 / QuickDialogs2 / QuickLayouts / QuickEffects / Svg / Xml / Concurrent |
| CMake | 3.21 及以上 | |
| C++ | C++17 | |
| Windows | MinGW-w64 或 clang（MSYS2 UCRT64 已验证） | |
| Linux | GCC（Debian 13 / Ubuntu 25.04 起提供 Qt 6.5+） | 需 `qt6-base-dev`、`qt6-declarative-dev`、`qt6-svg-dev` |
| libmpv | 2.x（开发环境 2.5.0） | 内嵌播放。MSYS2 装 `mingw-w64-ucrt-x86_64-mpv`，Debian 装 `libmpv-dev` |
| ffmpeg / ffplay | 任意近期版本 | 封面抽帧；ffplay 仅「独立窗口」模式使用，路径由用户在设置中指定 |

> 内嵌播放不使用 `Qt6Multimedia`（本环境不提供该模块），而是通过 libmpv 的渲染 API 直接把
> 画面画进 Qt Quick 场景图。没找到 libmpv 时构建仍然成功，「内嵌到界面」会自动回退为 ffplay
> 独立窗口。

## 构建

Windows（MSYS2 UCRT64）：

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Linux：

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev qt6-svg-dev libmpv-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build      # /usr/bin/lkplayer
```

内嵌播放需要 libmpv 的开发文件，Windows 下单独安装：

```bash
pacman -S mingw-w64-ucrt-x86_64-mpv
```

产物：Windows 为 `build/LkPlayer.exe`（GUI 程序，无控制台），Linux 为 `build/lkplayer`。

### 平台差异

| 能力 | Windows | Linux |
| --- | --- | --- |
| 独立窗口播放 | 支持 | 支持 |
| 内嵌到播放页 | 支持（libmpv → Qt Quick） | 支持（同上，需 `libmpv-dev`） |
| 系统播放器 | `QDesktopServices` → 关联程序 | `QDesktopServices` → `xdg-open` |
| 便携模式 | 支持（见下） | 支持（需自行放置 `portable.txt`） |

内嵌播放不再依赖操作系统的窗口重定向：libmpv 在进程内解码，通过渲染 API 把画面作为纹理交给
Qt Quick 的场景图，因此视频和界面共用同一个窗口与垂直同步。只有构建时找不到 libmpv，该模式才会
在设置中置灰并回退为 ffplay 独立窗口。

## 运行与首次配置

1. 运行 `build/LkPlayer.exe`。
2. 打开右上角「设置」：
   - **媒体库目录**：选择媒体库根目录，保存后自动重新扫描。
   - **程序目录**：指向包含 `ffmpeg.exe` / `ffplay.exe` 的目录，界面会实时校验并显示可用状态。
     封面抽帧需要 `ffmpeg`；只用内嵌播放时可以留空。
   - **播放模式**：
     - `内嵌到界面`：libmpv 把画面直接渲染进播放页，硬件解码、与界面同步刷新，不需要 ffplay；
       进度、音量与暂停通过 libmpv 的 API 完成，可以精确定位到任意位置。
     - `独立窗口`：ffplay 打开自己的窗口，需要配置程序目录。
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

## 扫描与增量校验

每次扫描的结果会写成一个索引文件（`%LOCALAPPDATA%\LkPlayer\LkPlayer\cache\scan-<hash>.bin`，
便携模式在 `Data/` 下），按目录记录：目录修改时间、该目录下的视频条目、已解析的 nfo 元数据与文件夹节点。

- **启动**：先把索引直接铺到界面（机械硬盘也能秒开），随后在后台按**目录修改时间**校验；
  时间没变的目录不重新读取，只有变化的目录才重新扫描，并用新条目替换对应卡片。
- **运行中**：应用在前台时每 3 分钟、以及每次切回窗口时做一次同样的轻量校验（每个目录一次
  `stat`），新增/删除/重命名的文件通常几秒到几分钟内就会出现在界面里；没有任何变化时不刷新
  界面，也不改写索引文件。
- **侧栏「重新扫描」**：同上，走增量校验，速度很快。
- **设置页「忽略缓存重扫」**：丢弃索引全量读取，用于目录时间戳不会变化的情况（例如替换了同名
  视频、或直接改了 `.nfo` 内容）。
- 目录时间戳由文件系统异步更新，刚复制完文件时可能有一两秒的延迟，下一次校验会补上。

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

> 快捷键与底部控制条对 libmpv 与 ffplay 后端生效；使用「系统播放器」时由该播放器自己处理。

## 测试

```bash
cd build && ctest --output-on-failure
```

覆盖 nfo 解析（完整 / 字段缺失 / 截断 XML）、扩展名与时长格式化、封面 ID 编解码，
以及扫描时的层级遍历与旁挂资源关联。

## 发布

推送版本 tag 会触发 `.github/workflows/release.yml`，并行产出三个包并创建 GitHub Release：

```bash
git tag v0.1.0
git push origin v0.1.0
```

| 产物 | 平台 | 说明 |
| --- | --- | --- |
| `LkPlayer-<tag>-win64-portable.zip` | Windows x64 | 便携版：解压即用，内置 Qt 运行时与 `qt.conf` |
| `lkplayer_<version>_amd64.deb` | Debian / Ubuntu | 链接发行版 Qt，依赖由 `dpkg-shlibdeps` 自动计算 |
| `LkPlayer-<version>-x86_64.AppImage` | 通用 Linux | 内置 Qt 运行时与 QML 模块 |

也可在 Actions 页面用 `Run workflow` 手动触发（此时只上传构建产物，不创建 Release）。
Windows 便携包与 AppImage 都自带 libmpv（`libmpv-2.dll` / `libmpv.so.2`），内嵌播放开箱可用；
两者仍不含 ffmpeg / ffplay，需要封面抽帧或「独立窗口」模式时按「运行与首次配置」自行设置工具目录。

Linux 构建跑在 `debian:trixie` 容器内：它是带 Qt 6.5+ 的最旧发行版（Qt 6.8.2），
`.deb` 因此能直接复用发行版 Qt 而不必捆绑运行时。

### 便携模式

Windows 包里带有 `portable.txt` 标记，程序会把可写状态全部放在程序目录，方便放进 U 盘：

```
Data/lkplayer.ini     设置
Data/covers/          封面缓存
Data/lkplayer.log     日志
```

删掉 `portable.txt` 即可回到每用户目录（`%LOCALAPPDATA%\LkPlayer\LkPlayer`）。
设置页会显示当前使用的配置文件路径。
