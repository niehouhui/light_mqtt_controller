# 地图灯带控制

控制地图标记灯的状态（亮灭，颜色）

## 第一次启动时
第一颗灯为状态指示灯

**红色**，表示led启动成功，等待WiFi连接。
（第一次需要esp_touch智能配网，记住ip地址，连接成功后设备会保存wifi信息，下次重启自动连接。）

**黄色**，表示WiFi启动成功，等待mqtt连接（第一次连接需要连接tcp配置mqtt信息）

**蓝色**，表示需要通过连接tcp设置mqtt（ip为ESPTouch显示ip，端口为8080，按tcp提示输入，tcp配置未完成即退出需要重启设备，）

**绿色**，mqtt连接成功。

每次重新设置wifi和mqtt配置需要上一次mqtt发送reset_all信息！！

## mqtt配置成功后
mqtt订阅的topic为
**/fd7764f2-ad70-d04c-9427-d72b837cb935/recv**

## 发布json格式命令
```
{
"led_length" : 32,
"reset_led" : 0,
"index" :  25,
"brightness" : 1,
"red"  : 255,
"green" : 0,
"blue" :  255,
"reset_all" : 0
}
```
"led_length"为led总数，要重新设置时，先改led_length，再设reset_led为 1 。

"index"为下标，0到（total-1）。

"brightness"为亮度，1到9，可使用2，4，8，为三个明显的亮度分档.

red，green，blue范围0到255。

reset_all设为 1 时，即清空所有保存的WiFi，mqtt，led信息，重启后重新设置。



