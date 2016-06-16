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
