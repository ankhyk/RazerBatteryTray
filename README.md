# RazerBattery

[TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 插件，在任务栏显示 Razer 鼠标和耳机电量。

当前针对这两台设备做了适配：

| 设备 | PID | 读电量方式 |
| --- | --- | --- |
| Razer Viper V2 Pro | `00A6` / `00A5` | HID Feature Report（OpenRazer `0x07/0x80`） |
| Razer BlackShark V2 Pro | `0528` | HID Feature Report `0xFF`；失败时回退到 Synapse 3 日志 |

## 安装

1. 确认 TrafficMonitor 版本 **≥ 1.82**。
2. 按主程序位数复制 DLL：
   - 64 位 TrafficMonitor → `bin\x64\Release\RazerBattery.dll`
   - 32 位 TrafficMonitor → `bin\Win32\Release\RazerBattery.dll`
3. 放到 TrafficMonitor 目录下的 `plugins` 文件夹。
4. 重启 TrafficMonitor。
5. 右键任务栏窗口 → **显示设置**，勾选：
   - `Razer 鼠标电量`（鼠标图标 + 百分比）
   - `Razer 耳机电量`（耳机图标 + 百分比）
   - 或 `Razer 电量`（两个图标并排）

充电中会在百分比后面加 `+`，例如 `85%+`。图标颜色跟随任务栏文字颜色。鼠标悬停可看设备名和充电状态。电量低于 20% 时会弹一次通知。

## 编译

需要 Visual Studio 2022（MSVC v143）和 Windows 10 SDK。

```bat
msbuild RazerBattery.sln /p:Configuration=Release /p:Platform=x64
msbuild RazerBattery.sln /p:Configuration=Release /p:Platform=Win32
```

## 配置

插件加载后会在 TrafficMonitor 的插件配置目录写入 `RazerBattery.ini`：

```ini
[config]
poll_interval_sec=10
low_battery_percent=20
notify=1
```

右键插件命令 **立即刷新电量** 会马上再查一次 HID。

## 协议说明

- 鼠标：91 字节 Feature Report，命令类 `0x07`，电量 `0x80`，充电 `0x84`，transaction id `0x1F` / `0x3F`。返回值 `0–255`，按 `raw * 100 / 255` 换成百分比。接口需要共享打开。
- 耳机：64 字节 Feature Report，Report ID `0xFF`，第 14 字节是 `0–100` 的电量。不向耳机写 Output Report，避免和 Synapse 抢占用。
- 回退：`C:\ProgramData\Razer\Synapse3\Service\Bin\Devices\Mw\Log\RzCtl_XXXX.log` 里的 `Power_BatteryLevel`。

## 开发参考

插件接口遵循 [TrafficMonitor 插件开发指南](https://github.com/zhongyang219/TrafficMonitor/wiki/%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97)，导出 `TMPluginGetInstance()`，实现 `ITMPlugin` / `IPluginItem`。
