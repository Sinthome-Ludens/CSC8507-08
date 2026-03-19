# CSC8503 游戏技术项目

基于 C++20 的 3D 游戏技术课程项目，支持 OpenGL / Vulkan 渲染后端，并包含 ECS、物理、AI、导航、网络等模块

## 环境要求

- 操作系统：Windows 10/11
- 编译器：Visual Studio 2019/2022（x64，支持 C++20）
- CMake：>= 3.16.0
- 可选：Vulkan SDK（仅在启用 Vulkan 渲染时需要）

## External 中间件与部署方式

仓库默认不提交 `External/` 目录（用于减小仓库体积），首次拉取后需要先部署以下中间件：

- `External/assimp`
- `External/enet`
- `External/imgui`
- `External/JoltPhysics`
- `External/nlohmann_json`
- `External/fmod`

### 1) Assimp（模型导入）

- 目录：`External/assimp`
- 建议版本：`v5.3.1`（或与课程环境一致的稳定版本）
- 项目集成方式：根 `CMakeLists.txt` 使用 `add_subdirectory(External/assimp)` 直接参与工程编译
- 本项目默认配置：关闭 tests/tools/samples，启用常用导入器（OBJ/FBX/GLTF/COLLADA/BLEND）

### 2) Jolt Physics（物理引擎）

- 目录：`External/JoltPhysics`
- 建议版本：最新稳定版（建议固定到课程统一 tag/commit）
- 项目集成方式：根 `CMakeLists.txt` 使用 `add_subdirectory(External/JoltPhysics/Build)` 编译并链接 `Jolt` 目标
- 本项目默认配置：关闭 UnitTests / Samples / Viewer / Install，保持轻量集成

### 3) nlohmann_json（JSON 解析）

- 目录：`External/nlohmann_json`
- 建议版本：`v3.11.3` 或更高稳定版
- 项目集成方式：header-only，不单独编译；通过 include 目录 `External/nlohmann_json/single_include`

### 4) ImGui（调试 UI）

- 目录：`External/imgui`
- 建议版本：与当前 `imgui_impl_win32.cpp` / `imgui_impl_opengl3.cpp` 兼容的稳定版
- 项目集成方式：根 `CMakeLists.txt` 将 `imgui*.cpp + backends` 组装为静态库 `ImGui`
- 开关选项：`USE_IMGUI=ON/OFF`（默认 ON）

### 5) ENet（网络库）

- 目录：`External/enet`
- 建议版本：最新主分支版
- 项目集成方式：根 `CMakeLists.txt` 使用 `add_subdirectory(External/enet)` 编译，并暴露 `include` 目录。

### 6) FMOD Core API（音频引擎）

- 目录：`External/fmod`
- 建议版本：FMOD Engine 2.02.x（Core API 即可，不需要 Studio API）
- **获取方式**：FMOD 不能通过 git clone 获取，需要从官网下载安装后复制
  1. 访问 https://www.fmod.com/download#fmodengine 下载 **FMOD Engine → Windows**
  2. 安装到默认路径 `C:\Program Files (x86)\FMOD SoundSystem\`
  3. 运行 `setup_dependencies.ps1`，脚本会自动从系统安装路径复制到 `External/fmod/`
  4. 或手动复制：
     ```
     从: C:\Program Files (x86)\FMOD SoundSystem\FMOD Studio API Windows\api\core\
     到: External/fmod/

     External/fmod/
     ├── inc/          ← api/core/inc/ 中的所有 .h 文件
     └── lib/x64/      ← api/core/lib/x64/ 中的 fmod.dll, fmod_vc.lib, fmodL.dll, fmodL_vc.lib
     ```
- 项目集成方式：`CSC8503/CMakeLists.txt` 直接 include + link，Debug 用 `fmodL`，Release 用 `fmod`
- DLL 自动拷贝：CMake POST_BUILD 命令将对应 DLL 复制到可执行文件输出目录
- 许可证：FMOD 免费用于教育/非商业项目（需注册账号下载）

## 首次部署 External（必做）

在项目根目录执行：

```bash
New-Item -ItemType Directory -Force -Path External | Out-Null

$ASSIMP_VERSION = "v6.0.4"
$ENET_VERSION   = "v1.3.18"
$JOLT_VERSION   = "v5.5.0"
$JSON_VERSION   = "v3.12.0"
$IMGUI_VERSION  = "v1.92.6-docking"

git clone --depth 1 --branch $ASSIMP_VERSION https://github.com/assimp/assimp.git External/assimp
git clone --depth 1 --branch $ENET_VERSION   https://github.com/lsalzman/enet.git External/enet
git clone --depth 1 --branch $JOLT_VERSION   https://github.com/jrouwe/JoltPhysics.git External/JoltPhysics
git clone --depth 1 --branch $JSON_VERSION   https://github.com/nlohmann/json.git External/nlohmann_json
git clone --depth 1 --branch $IMGUI_VERSION  https://github.com/ocornut/imgui.git External/imgui

# FMOD: 需先从 https://www.fmod.com/download#fmodengine 安装，然后运行 setup_dependencies.ps1 自动复制
# 或手动复制 api/core/inc/*.h -> External/fmod/inc/ 和 api/core/lib/x64/* -> External/fmod/lib/x64/
```

## 使用 CMake 编译本项目

推荐使用 out-of-source 构建，不要在源码根目录直接生成产物。

### 方案 A：Visual Studio 2022（推荐）

#### OpenGL（默认）

```bash
cmake -S . -B build/vs2022-opengl -G "Visual Studio 17 2022" -A x64 -DUSE_OPENGL=ON -DUSE_VULKAN=OFF -DUSE_IMGUI=ON
cmake --build build/vs2022-opengl --config Debug
```

#### Vulkan

```bash
cmake -S . -B build/vs2022-vulkan -G "Visual Studio 17 2022" -A x64 -DUSE_OPENGL=OFF -DUSE_VULKAN=ON -DUSE_IMGUI=ON
cmake --build build/vs2022-vulkan --config Debug
```

### 方案 B：Ninja

#### OpenGL

```bash
cmake -S . -B build/ninja-opengl -G Ninja -DUSE_OPENGL=ON -DUSE_VULKAN=OFF -DUSE_IMGUI=ON
cmake --build build/ninja-opengl --config Debug
```

#### Vulkan

```bash
cmake -S . -B build/ninja-vulkan -G Ninja -DUSE_OPENGL=OFF -DUSE_VULKAN=ON -DUSE_IMGUI=ON
cmake --build build/ninja-vulkan --config Debug
```

## 运行

- Visual Studio 多配置生成器：可执行文件通常在 `build/<preset>/<Config>/CSC8503.exe`
- 传统 in-source 历史路径（若你仍使用）：`x64/Debug/CSC8503.exe` 或 `x64/Release/CSC8503.exe`

## 常用 CMake 选项

- `USE_OPENGL=ON/OFF`：启用 OpenGL 渲染后端
- `USE_VULKAN=ON/OFF`：启用 Vulkan 渲染后端
- `USE_IMGUI=ON/OFF`：启用 ImGui 调试 UI

## Unity工具与地图
Unity工具包位置：Asset\Meshes\UnityPackage
- 使用方法：配置Unity编辑器版本2023.2.12f1或更高，双击文件自动导入到当前激活的Unity项目资产根目录中
### 包含内容
- 场景文件包含数个关卡，内容会随时间更新；
- 一份C#导出脚本，自动整合所有子模型并打包为单个OBJ文件并导出，包含OBJ MTL和navmesh
### 脚本使用方法
- 将你需要导出的关卡组成对象全部挂载到一个父对象上
- 在父对象上使用NavMesh Surface烘焙地图
- 将脚本挂在到父对象上，选择脚本组件右上角【更多】中找到Export Map选项
- 选择导出位置
- 导出

## 常见问题

### Q1: 配置时提示找不到 `External/*`？

A1: 检查以下目录是否存在：

- `External/assimp`
- `External/enet`
- `External/JoltPhysics`
- `External/nlohmann_json`
- `External/imgui`
- `External/fmod`（需手动安装 FMOD Engine 后运行 `setup_dependencies.ps1`）

如果缺失，按上面的”首次部署 External（必做）”拉取后重新执行 CMake 配置。

### Q2: 如何在 OpenGL 和 Vulkan 之间切换？

A2: 使用不同构建目录分别配置（推荐），或清理缓存后重新配置：

- OpenGL: `-DUSE_OPENGL=ON -DUSE_VULKAN=OFF`
- Vulkan: `-DUSE_OPENGL=OFF -DUSE_VULKAN=ON`

## 许可证

本项目为教学用途。第三方库许可证如下：

- Assimp：BSD-3-Clause
- Jolt Physics：MIT
- nlohmann_json：MIT
- ImGui：MIT
- FMOD：免费用于教育/非商业项目（https://www.fmod.com/legal）
