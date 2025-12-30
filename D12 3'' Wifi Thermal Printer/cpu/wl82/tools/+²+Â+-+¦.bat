@echo off

@echo *********************************************************************
@echo 			    AC791N SDK SD/U盘升级文件生成
@echo *********************************************************************
@echo %date%

::先执行download.bat生成文件
call .\download.bat

if exist upgrade\*.ufw del upgrade\*.ufw

if not exist upgrade md upgrade

if exist upgrade\readme(升级前请阅读).txt del upgrade\readme(升级前请阅读).txt

echo.                               重要说明                                                      >> upgrade\readme(升级前请阅读).txt
echo. 注意事项一：                                                                                >> upgrade\readme(升级前请阅读).txt
echo. BR22_TWS_VERSION为固件版本号，即每次需要对设备进行升级时，需要将该值加1。                   >> upgrade\readme(升级前请阅读).txt
echo. 初次烧录时该值为BR22_TWS_VERSION=0，需要对设备进行升级时，需要将该值加1，                   >> upgrade\readme(升级前请阅读).txt
echo. 即此时BR22_TWS_VERSION=1，然后编译生成升级固件对设备进行升级，以此类推，                    >> upgrade\readme(升级前请阅读).txt
echo. 需要再次对设备升级的时，该值在原来的基础上进行递增1，因此多次升级后该值                     >> upgrade\readme(升级前请阅读).txt
echo. 应该依次递增，0，1，2，3，…，否则会遇到这样一个情况，设备进行OTA之后，                      >> upgrade\readme(升级前请阅读).txt
echo. 再使用SM01-DFU.exe工具对设备烧录bin文件时，会出现设备运行的还是旧程序的情况，               >> upgrade\readme(升级前请阅读).txt
echo. 因为一些工具会利用到这个设置参数。                                                          >> upgrade\readme(升级前请阅读).txt
echo. 设置时请在对应 ``cpu/(wl80或wl82)/tools/isd_config_rule.c``文件下修改BR22_TWS_VERSION参数； >> upgrade\readme(升级前请阅读).txt
echo.                                                                                             >> upgrade\readme(升级前请阅读).txt
echo.                                                                                             >> upgrade\readme(升级前请阅读).txt
echo. 请阅读文档：https://doc.zh-jieli.com/AC79/zh-cn/master/module_example/system/update.html    >> upgrade\readme(升级前请阅读).txt


::再复制文件
copy jl_isd.ufw upgrade\update.ufw

echo.
echo.
echo 升级文件已经在当前upgrade目录下生成，升级文件名称：update.ufw，将update.ufw拷贝到SD卡/U盘的根目录，插卡上电即可升级（2秒钟后自动关闭窗口）
echo.
choice /t 2 /d y /n >nul 

pause
::exit
