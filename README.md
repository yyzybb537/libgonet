# libgonet
C++ network library based on `libgo`.

## Benchmark

性能测试步骤(先安装boost1.59+)：

    $ mkdir build -p && cd build
    $ cmake .. -DENABLE_BOOST_CONTEXT=ON
    $ make
    $ sudo make install
    $ cd ../test/test
    $ make -j4
  
启动服务端：

    $ ./bmserver.t
  
启动客户端：

    $ ./bmclient.t
    
四核测试：

    $ ./bmserver.t 64 0 160 4
    $ ./bmclient.t 64 10 1000 0 160 4
    
性能指标：

    CPU: Intel(R) Core(TM) i7-4770 CPU @ 3.40GHz
    64Bytes数据包、Pipeline=1000、10连接做pingpong测试：
        单核：160w QPS
        四核：270w QPS

## Demo

~~~~~~~~~~cpp
/**************************************************
* 一个简单的echo server.
**************************************************/
#include <stdio.h>
#include <libgonet/network.h>
using namespace std;
using namespace network;

// 数据处理函数
// @sess session标识
// @data 收到的数据起始指针
// @bytes 收到的数据长度
// @returns: 返回一个size_t, 表示已处理的数据长度. 当返回-1时表示数据错误, 链接即会被关闭.
size_t OnMessage(SessionEntry sess, const char* data, size_t bytes)
{
    printf("receive: %.*s\n", (int)bytes, data);
    sess->Send(data, bytes);    // Echo. 将收到的数据原样回传
    return bytes;
}

int main()
{
    // Step1: 创建一个Server对象
    Server server;

    // Step2: 设置收到数据的处理函数
    server.SetReceiveCb(&OnMessage);

    // Step3: 启动server
    // * 启动接口goStart接受一个url, 由以下格式构成：
    //    协议://(IP|Domain)[:Port]
    // 目前支持的协议有：tcp  udp  ssl
    // 其中ssl包含所有的ssl版本(v2  v3  tls等), 使用ssl需要一些额外设置,
    //   在后面的教程中会介绍.
    // Url例子：
    //   tcp://127.0.0.1        # 不设置端口号时采用随机端口, 端口号可以通过LocalAddr接口获取(如下面的例子).
    //   tcp://localhost:8888   # 此处可以用域名
    //   udp://192.168.1.1      # udp协议
    //   ssl://127.0.0.1:6666   # ssl协议
    boost_ec ec = server.goStart("tcp://127.0.0.1:3030");

    // Step4: 处理goStart返回值, 检测是否启动成功
    if (ec) {
        printf("server start error %d:%s\n", ec.value(), ec.message().c_str());
        return 1;
    } else {
        printf("server start at %s:%d\n", server.LocalAddr().address().to_string().c_str(),
                server.LocalAddr().port());
    }

    // Step5: 启动协程调度器
    co_sched.RunUntilNoTask();
    return 0;
}
~~~~~~~~~~

~~~~~~~~~~cpp
/**************************************************
* 一个简单的client.
**************************************************/
#include <stdio.h>
#include <libgonet/network.h>
using namespace std;
using namespace network;

// 数据处理函数
// @sess session标识
// @data 收到的数据起始指针
// @bytes 收到的数据长度
// @returns: 返回一个size_t, 表示已处理的数据长度. 当返回-1时表示数据错误, 链接即会被关闭.
size_t OnMessage(SessionEntry sess, const char* data, size_t bytes)
{
    printf("receive: %.*s\n", (int)bytes, data);
    sess->Shutdown();   // 收到回复后关闭连接
    return bytes;
}

int main()
{
    // Step1: 创建一个Client对象
    Client client;

    // Step2: 设置收到数据的处理函数
    client.SetReceiveCb(&OnMessage);

    // Step3: 连接
    // * 连接接口goStart接受一个url, 规则与Server的goStart接口相同,
    //   参见tcpserver.cpp教程
    boost_ec ec = client.Connect("tcp://127.0.0.1:3030");

    // Step4: 处理Connect返回值, 检测是否连接成功
    if (ec) {
        printf("client connect error %d:%s\n", ec.value(), ec.message().c_str());
        return 1;
    } else {
        printf("connected to %s:%d\n", client.LocalAddr().address().to_string().c_str(),
                client.LocalAddr().port());

        std::string s = "Hello libgonet!";
        client.Send(s.c_str(), s.size(), [](boost_ec ec) {
                    printf("send ec:%s\n", ec.message().c_str());
                });
    }

    // Step5: 启动协程调度器
    co_sched.RunUntilNoTask();
    return 0;
}
~~~~~~~~~~
