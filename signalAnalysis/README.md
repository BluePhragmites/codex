# MATLAB 信号分析工具

这个目录用于存放常用的 MATLAB 信号分析脚本与函数，便于快速完成时域、频域和基础质量评估工作。

## 文件说明

- `fftSpectrumAnalysis.m`：计算单边幅度谱，返回频率轴、幅值谱以及主频。
- `movingRMS.m`：计算滑动均方根（RMS），用于观察信号能量随时间的变化。
- `estimateSNR.m`：根据信号段与噪声段估算信噪比（SNR）。
- `demoSignalAnalysis.m`：生成测试信号并演示以上工具的基础用法。

## 使用方式

1. 在 MATLAB 中进入当前目录，或将本目录加入路径：
   ```matlab
   addpath('signalAnalysis')
   ```
2. 调用单个函数，例如：
   ```matlab
   fs = 1000;
   t = 0:1/fs:1-1/fs;
   x = sin(2*pi*50*t) + 0.3*sin(2*pi*120*t);
   [f, amp, dominantFreq] = fftSpectrumAnalysis(x, fs);
   ```
3. 运行演示脚本：
   ```matlab
   demoSignalAnalysis
   ```

## 适用场景

- 振动信号初步分析
- 音频或传感器数据的频谱查看
- 滑动能量趋势监测
- 基础信噪比评估
