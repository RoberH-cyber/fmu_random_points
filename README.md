# RandomPoints500 FMU (FMI 2.0 Co-Simulation)

该 FMU 在初始化时生成 500 个 $[0,100]$ 的随机点，随后根据仿真时间输出对应的点值。

## 变量

- `y` (Real, output): 当前输出值
- `index` (Integer, output): 当前索引 (0–499)
- `seed` (Integer, parameter): 随机种子
- `samplePeriod` (Real, parameter): 采样周期（秒）

## 行为

在 $t=\text{startTime}$ 时输出第 0 个点，之后每经过 `samplePeriod` 秒索引加 1，直到第 499 个点。

## 打包步骤（简述）

1. 用 FMI 2.0 头文件（`fmi2Functions.h` 等）编译 [fmu_random_points.c](sources/fmu_random_points.c) 生成 DLL。
2. 组织 FMU 结构：
   - `modelDescription.xml`
   - `binaries/win64/RandomPoints500.dll`
   - `sources/fmu_random_points.c`
3. 将上述内容 zip 成 `RandomPoints500.fmu`。

## Windows 一键构建脚本

使用 [build_fmu.ps1](build_fmu.ps1) 在 Windows 下生成 DLL 并自动打包 FMU：

- MSVC（推荐）：
   - 在 **Developer PowerShell for VS** 中运行
   - `-FmiHeadersPath` 指向 FMI 2.0 头文件目录

- MinGW：
   - 追加 `-UseMingw`

输出目录默认为 `build`，FMU 将生成在 `build/RandomPoints500.fmu`。

> Simulink 导入 FMU 时，确保仿真步长与 `samplePeriod` 对齐，或使用可变步长并在模型中处理离散更新。
