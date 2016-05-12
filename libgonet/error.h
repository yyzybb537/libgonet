#pragma once
#include "config.h"

namespace network
{

using boost_ec = boost::system::error_code;

enum class eNetworkErrorCode : int
{
    ec_ok           = 0,
    ec_connecting   = 1,
    ec_estab        = 2,
    ec_shutdown     = 3,
    ec_half         = 4,
    ec_no_destition = 5,
    ec_timeout      = 6,
    ec_url_parse_error  = 7,
    ec_data_parse_error  = 8,
    ec_unsupport_protocol  = 9,
    ec_recv_overflow = 10,
    ec_send_overflow = 11,
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
