# HIPS-Light
一个简单的用于win7 x64的驱动级HIPS

# 驱动部分
分为进程、文件、注册表三个驱动，其中文件为使用minifilter的过滤驱动，其他两个是普通的NT式驱动


#应用程序部分
	应用程序启动时自动加载驱动

功能：

	控制监视功能的开启与关闭
  
	增/删规则
  
	查看日志
  
	枚举当前进程，及其线程和模块信息
  
	禁止进/线程创建
  
	还有个卸载驱动的按钮


	进程监控在规则匹配不到时弹出确认放行的对话框，文件和注册表都是直接设置黑名单进行拦截(注册表有个白名单，只是让操作记录不出现在日志中而已)
