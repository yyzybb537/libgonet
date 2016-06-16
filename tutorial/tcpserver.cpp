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
