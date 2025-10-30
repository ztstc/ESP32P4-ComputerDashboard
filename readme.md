# ESP32P4-ComputerDashboard
基于微雪Waveshare ESP32-P4 智能86盒开发板的多功能桌面小闹钟

# 基于微雪Waveshare ESP32-P4 开发板智能86盒的多功能桌面小闹钟



## 功能展示：



### 1.主页时间 天气信息显示



[<img src="https://github.com/ztstc/ESP32P4-ComputerDashboard/raw/main/assets/IMG_20251030_151016.jpg" alt="IMG_20251030_151016" style="zoom:15%;" />](https://github.com/ztstc/ESP32P4-ComputerDashboard/blob/main/assets/IMG_20251030_151016.jpg)

### 2.电脑性能参数显示



[<img src="https://github.com/ztstc/ESP32P4-ComputerDashboard/raw/main/assets/IMG_20251030_151010.jpg" alt="IMG_20251030_151010" style="zoom:15%;" />](https://github.com/ztstc/ESP32P4-ComputerDashboard/blob/main/assets/IMG_20251030_151010.jpg)

### 3.天气预报显示



[<img src="https://github.com/ztstc/ESP32P4-ComputerDashboard/raw/main/assets/IMG_20251030_151022.jpg" alt="IMG_20251030_151022" style="zoom:15%;" />](https://github.com/ztstc/ESP32P4-ComputerDashboard/blob/main/assets/IMG_20251030_151022.jpg)

### 4.类MIUI控制面板



[<img src="https://github.com/ztstc/ESP32P4-ComputerDashboard/raw/main/assets/IMG_20251030_151031.jpg" alt="IMG_20251030_151031" style="zoom:15%;" />](https://github.com/ztstc/ESP32P4-ComputerDashboard/blob/main/assets/IMG_20251030_151031.jpg)

[<img src="https://github.com/ztstc/ESP32P4-ComputerDashboard/raw/main/assets/IMG_20251030_151042.jpg" alt="IMG_20251030_151042" style="zoom:15%;" />](https://github.com/ztstc/ESP32P4-ComputerDashboard/blob/main/assets/IMG_20251030_151042.jpg)

## 功能实现



LVGL通过SquareLine Studio设计 但是主包的试用过期了 暂时停更

通过`接口盒子`API实现天气自动更新 基于IP获取的天气地址 含气象预警特别提醒

通过NTP服务器获取本地时间

通过Info_Server的python程序获取电脑参数 含GPU CPU型号占用率 RAM ROM剩余容量（支持热插拔更新 磁盘显示最多5个） 上传下载速度显示 电脑关机时会自动返回主页

## 部署教程



购买微雪的这个开发板约219（好贵）

esp_idf vscode环境配置

修改 微雪ESP32P4桌面闹钟\lvgl_clock\components\tasks\tasks.c内对天气API的定义

```
// ===== 天气更新：配置/状态 =====
#define WEATHER_USER_ID   "enter your user ID" // 需替换为自己的用户 ID
#define WEATHER_USER_KEY  "enter your API key" // 需替换为自己的 API Key
#define WEATHER_URL_OPTIMAL "https://api.apihz.cn/getapi.php" // 天气 API 地址 百度搜索 API 盒子 免费注册一个
```



编译 上传

电脑端 将 微雪ESP32P4桌面闹钟\InfoServer\开机自启.bat 添加到开机启动

如果不想开机启动 直接双击也可以运行 此时右键点击托盘内仪表盘图标

记录电脑IP（端口保持默认） 电脑网络需要和ESP32在同一网段

[![image-20251030153609476](https://github.com/ztstc/ESP32P4-ComputerDashboard/raw/main/assets/image-20251030153609476.png)](https://github.com/ztstc/ESP32P4-ComputerDashboard/blob/main/assets/image-20251030153609476.png)

在主页（时钟显示页面）从上往下滑动进入控制面板 长按WIFI按钮 搜索配对WIFI

连接成功后 点击边缘处 返回到控制面板页面 点击左下角关于按钮 点击ComputerIP下的文本框

输入电脑IP 后点Save

现在回到主页 右滑到电脑性能显示器页面 即可查看电脑信息
