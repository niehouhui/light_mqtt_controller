用来控制地图的背景灯的颜色亮灭和亮度；
第一颗灯为指示，红，led启动，黄，WiFi启动，蓝，连接tcp设置mqtt，绿，mqtt连接成功。
设置流程，上电后，设备等待WiFi配置，如果仍未上一次的WiFi，则自动连接，第一次则通过ESPTouch配置wifi，
WiFi连接后，连接mqtt，同样，如果为上一次mqtt服务器，自动重连，如果是第一次，则通过连接tcp配置mqtt，ip为ESPTouch显示ip，端口为8080，tcp会有提示，tcp配置未完成即退出需要重启设备，

每次重新设置wifi和mqtt配置需要上一次mqtt发送reset_all信息！！

mqtt配置成功后，
mqtt订阅的topic为/fd7764f2-ad70-d04c-9427-d72b837cb935/recv
发布json格式命令
{
"led_total" : 6,
"reset_led" : 0,
"index" :  0,
"brightness" : 1,
"red"  : 0,
"green" :0,
"blue" :  0,
"reset_all" : 0
}
led_total为led总数，要重新设置时，先改total，再设reset_led为 1，
index为下标，0~（total-1），brightness为亮度，1~9，可分为2，4，8，三个明显的改变亮度，
red，green，blue，0~255，

reset_all设为 1 时，即清空所有保存的WiFi，mqtt，led信息，重启后重新设置。