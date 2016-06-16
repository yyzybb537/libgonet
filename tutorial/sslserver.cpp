/**************************************************
* 一个简单的ssl echo server.
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

    // Step2: 设置ssl参数, 使用ssl之前, 需要先安装openssl, 并在编译libgonet时使用如下CMake参数:
    //   $ cmake .. -DENABLE_SSL=ON
    //
    // 编译程序时, 请链接openssl: -lssl -lcrypto
#if ENABLE_SSL
    OptionSSL ssl_opt;
    ssl_opt.certificate_chain_file = "../sslopt/server.crt";
    ssl_opt.private_key_file = "../sslopt/server.key";
    ssl_opt.tmp_dh_file = "../sslopt/dh2048.pem";
    server.SetSSLOption(ssl_opt);
#endif

    // Step3: 设置收到数据的处理函数
    server.SetReceiveCb(&OnMessage);

    // Step3: 启动server
    boost_ec ec = server.goStart("ssl://127.0.0.1:3030");

    // Step4: 处理goStart返回值, 检测是否启动成功
    if (ec) {
        printf("server start error %d:%s\n", ec.value(), ec.message().c_str());
        return 1;
    } else {
        boost_ec ignore_ec;
        printf("server start at %s\n", server.LocalAddr().to_string(ignore_ec).c_str());
    }

    // Step5: 启动协程调度器
    co_sched.RunUntilNoTask();
    return 0;
}
