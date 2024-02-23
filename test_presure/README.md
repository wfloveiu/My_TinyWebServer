服务器压力测试
===============
适应Webbench网站压力测试工具进行压测


``` sh
wget http://home.tiscali.cz/~cz210552/distfiles/webbench-1.5.tar.gz
tar -zxvf webbench-1.5.tar.gz
cd ./webbench-1.5
make
```
如果make报错：没有<rpc/types.h>,则
```sh
sudo apt-get install -y libtirpc-dev
sudo ln -s /usr/include/tirpc/rpc/types.h /usr/include/rpc
```
再make，若报错:fatal error: netconfig.h: No such file or directory，则
```sh
sudo ln -s /usr/include/tirpc/netconfig.h /usr/include
```
再make，报错/bin/sh: 1: ctags: not found,则
```sh
sudo apt-get install universal-ctags
```
make && sudo make install


测试规则：
```sh
webbench -c 100 -t 30 http://www.baidu.com
```
> * `-c` 表示客户端数
> * `-t` 表示时间

> * Speed=16008 pages/min, (每分钟输出的页面数)
> * 117151064 bytes/sec.   (每秒传输的比特数)
> * Requests: 2668 susceed
> * 0 failed.

webbench -c 500 -t 30  http://127.0.0.1:9006/