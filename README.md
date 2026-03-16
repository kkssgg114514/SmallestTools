# SmallestTools

`SmallestTools` 是一个面向上层 C++ 项目的基础工具仓库，目标是为模型查看器、渲染模拟器以及其他需要底层能力的应用提供可复用的工具模块。

当前仓库已经开始按功能拆分为 `FileTools`、`ImageTools`、`LogTools` 三个方向，但整体仍处在原型阶段。中期目标应当是把这些模块整理成可以独立复用、可安装、可链接的库，并同时支持静态库和动态库两种产物。

## 目标定位

- 面向其他 C++ 项目复用，而不是仅供当前仓库内的测试程序使用。
- 每个模块应当拥有清晰的公开接口与私有实现边界。
- 构建系统应当能够稳定产出静态库和动态库。
- 后续应支持示例程序、测试程序、安装导出和打包，而不是把库代码直接做成可执行文件。

## 当前目录概览

- `CMakeLists.txt`
  - 顶层 CMake 入口，当前只负责添加三个子目录。
- `FileTools/`
  - 当前最完整的模块，包含 `FileOperation` 原型实现。
- `ImageTools/PngTool/`
  - 基于 `FileTools` 的 PNG 读取原型，目前只有很早期的解析代码。
- `LogTools/KLogger/`
  - 日志模块占位目录，尚未形成实际 API 或实现。
- `cmake-build-debug/`
  - 现有 CLion 本地调试构建目录，属于生成产物，不应视为源码的一部分。
- `.idea/`
  - IDE 元数据目录。

## 当前构建目录检查结果

已检查到的主要构建目录只有：

- `cmake-build-debug/`

这个目录中包含：

- `CMakeCache.txt`、`build.ninja` 等 CMake/Ninja 生成文件。
- 历史构建产物，例如 `cmake-build-debug/FileTools/FileTools.exe`。
- CLion 环境记录，显示当前使用的是 CLion 自带 MinGW 与 Ninja。

我在仓库根目录执行了：

```powershell
cmake --build cmake-build-debug
```

当前结果是三个子模块的构建都在编译阶段停止，现有 `cmake-build-debug` 只能说明这个工程曾被 IDE 配置过，暂时不能作为“可稳定复现”的标准构建输出目录来对待。

## 当前代码状态

- `FileTools`
  - 已有公开头文件与私有实现，具备文件读取、写入、追加、刷新等基础能力雏形。
- `ImageTools/PngTool`
  - 已有 PNG 文件签名检查和部分块结构尝试，但还没有完成可靠的 PNG 解析流程。
- `LogTools/KLogger`
  - 目前只有目录和空的入口文件，尚未形成可用模块。

## 已发现的问题

- `FileTools/CMakeLists.txt:9`、`ImageTools/PngTool/CMakeLists.txt:10`、`LogTools/KLogger/CMakeLists.txt:10` 当前都使用 `add_executable(...)`，这与“为其他项目提供工具模块并产出静态库/动态库”的目标不一致。
- 顶层和子模块 CMake 目前没有安装、导出、版本、命名空间、`find_package` 支持，也没有为动态库准备导出宏，离可复用库工程还有明显距离。
- `FileTools/CMakeLists.txt:6-7`、`ImageTools/PngTool/CMakeLists.txt:6-8`、`LogTools/KLogger/CMakeLists.txt:6-8` 使用了 `include_directories(...)` 和 `aux_source_directory(...)` 这类全局、隐式写法，不利于模块边界、IDE 可见性和后续库化。
- `FileTools/main.cpp:10` 把测试路径硬编码为 `D:\private\test.txt`，这使当前示例只能在特定机器目录结构下工作，不具备可移植性。
- `FileTools/main.cpp:17` 直接写入中文字符串却手工给出长度 `10`，在不同源码编码下有截断和乱码风险。
- `FileTools/Sources/private/FileOperation.cpp:22` 使用 `strdup(...)` 分配路径内存，但 `FileTools/Sources/private/FileOperation.cpp:45` 使用 `delete` 释放，分配与释放方式不匹配，属于严重资源管理问题。
- `FileTools/Sources/private/FileOperation.cpp:83-97` 的移动构造和移动赋值只处理了 `p_Impl`，没有正确转移 `pSz_FilePath`，会导致未初始化指针、内存泄漏或重复释放风险。
- `FileTools/Sources/private/FileOperation.cpp:24-28` 在文件不存在时直接抛异常，但示例代码又把它当作可以创建新文件并写入的对象来使用，接口语义与使用方式不一致。
- `FileTools/Sources/private/FileOperation.cpp:137-147` 的 `flush()` 没有检查打开文件和写入是否成功，却始终返回 `Success`，错误传播不完整。
- `FileTools/Sources/public/FileOperation.h:53-58` 虽然尝试用不透明实现稳定 ABI，但公开类仍直接持有原始指针并自行做资源管理，和“稳定、可复用工具库”的方向相比还不够安全。
- `ImageTools/PngTool/Sources/public/ST_png.h:28-44` 声明了多个原始指针成员，但当前构造、析构逻辑并没有初始化和释放这些对象，后续扩展时很容易引入未定义行为。
- `ImageTools/PngTool/Sources/private/ST_png.cpp:26-38` 与 `ImageTools/PngTool/Sources/private/ST_png.cpp:60-66` 中的多字节整数拼装逻辑不正确，没有按 PNG 需要的高位在前方式完成累积移位。
- `ImageTools/PngTool/Sources/private/ST_png.cpp:93-99` 读取文件签名后没有检查 `read(...)` 的返回值，当前错误处理依赖异常和假设，接口使用不完整。
- `ImageTools/PngTool/main.cpp:1-3` 和 `LogTools/KLogger/main.cpp:1-3` 目前都只有注释，没有示例逻辑，也没有形成实际可执行入口。
- `LogTools/KLogger/Sources/public` 与 `LogTools/KLogger/Sources/private` 当前为空，说明日志模块还没有进入真正实现阶段。
- 仓库当前没有 `README.md`、模块级文档、测试目录、基准、示例目录和 `.gitignore`，因此构建产物与 IDE 目录容易混在源码仓库视图里。
- `cmake-build-debug/` 是本地 IDE 生成目录，当前状态下不应纳入正式源码管理，也不适合作为后续发布构建的标准输出目录。

## 更适合的后续演进方向

- 先把每个模块从“可执行程序”改成“库目标 + 示例程序/测试程序”。
- 为每个模块明确 `Sources/public` 与 `Sources/private` 的边界，公开头文件只暴露稳定 API。
- 使用 target-based CMake 写法，例如 `target_sources`、`target_include_directories`、`target_link_libraries`。
- 为静态库和动态库设计统一的源码列表，避免两套实现分叉。
- 在 Windows 上引入符号导出宏，为动态库做准备。
- 增加示例、单元测试和最小可复现构建命令，让这个仓库能够被上层项目稳定依赖。

## 现阶段建议理解

如果把当前仓库定义为“工具模块的原型孵化区”是合理的；如果把它定义为“已经可以被其他项目直接稳定依赖的基础库仓库”，目前还不够成熟。最关键的差距在于：

- 目标产物仍是可执行文件，不是库。
- 模块边界还不稳定。
- 资源管理与错误处理还不够可靠。
- 构建和发布形态还没有成型。
