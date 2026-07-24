## M2-D4 LAN setup

### 使用方案

采用方案 B：
WSL2 NAT + Windows portproxy。

原因：
Windows mirrored networking 在当前win11环境下不稳定，因此使用端口转发方案。

### 配置

Windows:

0.0.0.0:8888
    ↓
WSL IP:172.20.224.123:8888


### 验证结果

[x] WSL curl 127.0.0.1:8888
成功，服务端打印 HTTP GET 请求。

**client connected: 127.0.0.1:36044 fd=5**
**GET / HTTP/1.1**
**Host: 127.0.0.1:8888**
**User-Agent: curl/7.81.0**
**Accept: */***

**client fd=5 disconnected**



[x] Windows localhost:8888
成功，服务端打印 Chrome 请求。

**client connected: 172.20.224.1:6801 fd=5**
**client connected: 172.20.224.1:6802 fd=6**
**GET / HTTP/1.1**
**Host: localhost:8888**
**Connection: keep-alive**
**sec-ch-ua: "Not;A=Brand";v="8", "Chromium";v="150", "Google Chrome";v="150"**
**sec-ch-ua-mobile: ?0**
**sec-ch-ua-platform: "Windows"**
**Upgrade-Insecure-Requests: 1**
**User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36**
**Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7**
**Sec-Fetch-Site: none**
**Sec-Fetch-Mode: navigate**
**Sec-Fetch-User: ?1**
**Sec-Fetch-Dest: document**
**Accept-Encoding: gzip, deflate, br, zstd**
**Accept-Language: zh-CN,zh;q=0.9**

**client fd=5 disconnected**
**client connected: 172.20.224.1:10746 fd=5**



[x] Windows WLAN IP:8888
成功，服务端打印浏览器请求。

**client connected: 172.20.224.1:11954 fd=5**
**GET / HTTP/1.1**
**Host: 192.168.110.197:8888**
**Connection: keep-alive**
**Upgrade-Insecure-Requests: 1**
**User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36**
**Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7**
**Accept-Encoding: gzip, deflate**
**Accept-Language: zh-CN,zh;q=0.9**

**client fd=5 disconnected**
**client connected: 172.20.224.1:7073 fd=5**
**GET / HTTP/1.1**
**Host: 192.168.110.197:8888**
**Connection: keep-alive**
**Cache-Control: max-age=0**
**Upgrade-Insecure-Requests: 1**
**User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36**
**Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7**
**Accept-Encoding: gzip, deflate**
**Accept-Language: zh-CN,zh;q=0.9**

**client fd=5 disconnected**
**client connected: 172.20.224.1:10598 fd=5**
**GET / HTTP/1.1**
**Host: 192.168.110.197:8888**
**Connection: keep-alive**
**Cache-Control: max-age=0**
**Upgrade-Insecure-Requests: 1**
**User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36**
**Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7**
**Accept-Encoding: gzip, deflate**
**Accept-Language: zh-CN,zh;q=0.9**

**client connected: 172.20.224.1:11865 fd=6**
**client fd=5 disconnected**





[x] 手机 WLAN IP:8888
成功，服务端打印手机 User-Agent。

**client connected: 172.20.224.1:7093 fd=5**
**GET / HTTP/1.1**
**Host: 192.168.110.197:8888**
**Connection: keep-alive**
**Upgrade-Insecure-Requests: 1**
**User-Agent: Mozilla/5.0 (Linux; U; Android 16; zh-cn; 23127PN0CC Build/BP2A.250605.031.A3) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.7049.79 Mobile Safari/537.36 XiaoMi/MiuiBrowser/20.23.1020715**
**Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7**
**Accept-Encoding: gzip, deflate**
**Accept-Language: zh-CN,zh;q=0.9,en-US;q=0.8,en;q=0.7**

**client fd=5 disconnected**



### 遇到的问题

第一次 portproxy 配置：

0.0.0.0:8888 -> 127.0.0.1:8888

无法进入 WSL。

修改为：

0.0.0.0:8888 -> 172.20.224.123:8888

后恢复正常。


### 原理

WSL2 默认使用 NAT 网络。

Windows 和 WSL 拥有不同 IP：

Windows:
192.168.x.x

WSL:
172.x.x.x

外部设备只能访问 Windows IP，
需要 portproxy 将 Windows 端口转发到 WSL 服务。



**最后补充scripts里的这个setup_lan.ps1脚本的使用原因，以及如何使用：**

**WSL因为是虚拟网络，所以每次分配的IP可能是随机不一致的，采用的方案B又是NAT + portproxy，那么就要求windows每次都需要转发到WSL本次使用的IP，因为手动设置太麻烦，就有了这个脚本，用hostname -I命令获取WSL里的IP。**

**又因为项目结构问题，我想让相关的东西尽量存在同一文件夹里，就放在项目下属的scripts文件夹里，但毕竟是修改windows里的东西，所以需要(powershell管理员)：**

```bash
cd \\wsl$\Ubuntu-22.04\home\pluto\lantalk\scripts
.\setup_lan.ps1
```

**简单转一下**