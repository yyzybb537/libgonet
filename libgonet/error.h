#pragma once
#include "config.h"

namespace network
{

using boost_ec = boost::system::error_code;

enum class eNetworkErrorCode : int
{
    ec_ok                   = 0,
    ec_connecting           = 1,
    ec_estab                = 2,
    ec_shutdown             = 3,
    ec_half                 = 4,
    ec_no_destition         = 5,
    ec_send_timeout         = 6,
    ec_recv_timeout         = 7,
    ec_url_parse_error      = 8,
    ec_data_parse_error     = 9,
    ec_unsupport_protocol   = 10,
    ec_recv_overflow        = 11,
    ec_send_overflow        = 12,
    ec_dns_not_found        = 13,

    // 兼容
    ec_timeout = ec_send_timeout,
};

class network_error_category
    : public boost::system::error_category
{
public:
#ifdef BOOST_SYSTEM_NOEXCEPT
    virtual const char* name() const BOOST_SYSTEM_NOEXCEPT;
#else
    virtual const char* name() const throw();
#endif

    virtual std::string message(int) const;
};

const boost::system::error_category& GetNetworkErrorCategory();

boost_ec MakeNetworkErrorCode(eNetworkErrorCode code);

void ThrowError(eNetworkErrorCode code, const char* what = "");

} //namespace network
