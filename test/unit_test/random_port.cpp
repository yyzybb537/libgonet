#include <iostream>
#include <unistd.h>
#include <gtest/gtest.h>
#include <libgo/coroutine.h>
#include <boost/thread.hpp>
#include <atomic>
#include <libgonet/network.h>
using namespace std;
using namespace co;
using namespace network;

void foo()
{
    boost_ec ignore_ec;

    cout << "test 1" << endl;
    {
        Server s;
        boost_ec ec = s.goStart("tcp://127.0.0.1:0");
        ASSERT_FALSE(!!ec);
        cout << s.LocalAddr().to_string(ignore_ec) << endl;
        EXPECT_TRUE(!!s.LocalAddr().port());
    }
    
    cout << "test 2" << endl;
    {
        Server s;
        boost_ec ec = s.goStart("tcp://127.0.0.1");
        ASSERT_FALSE(!!ec);
        cout << s.LocalAddr().to_string(ignore_ec) << endl;
        EXPECT_TRUE(!!s.LocalAddr().port());
    }

    cout << "test 3" << endl;
    {
        Server s;
        boost_ec ec = s.goStart("udp://127.0.0.1:0");
        ASSERT_FALSE(!!ec);
        cout << s.LocalAddr().to_string(ignore_ec) << endl;
        EXPECT_TRUE(!!s.LocalAddr().port());
    }

    cout << "test 4" << endl;
    {
        Server s;
        boost_ec ec = s.goStart("udp://127.0.0.1");
        ASSERT_FALSE(!!ec);
        cout << s.LocalAddr().to_string(ignore_ec) << endl;
        EXPECT_TRUE(!!s.LocalAddr().port());
    }
    cout << "test ok" << endl;
}

TEST(testRandomPort, testRandomPort)
{
    go foo;
    co_sched.RunUntilNoTask();
}
