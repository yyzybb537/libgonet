#include "error.h"
#include "option.h"

namespace network
{

#ifdef BOOST_SYSTEM_NOEXCEPT
const char* network_error_category::name() const BOOST_SYSTEM_NOEXCEPT
#else
const char* network_error_category::name() const throw()
#endif
{
    return "network_error";
}

std::string network_error_category::message(int v) const
{
    switch (v) {
        case (int)eNetworkErrorCode::ec_ok:
            return "(network)ok";

        case (int)eNetworkErrorCode::ec_connecting:
            return "(network)client was connecting";

        case (int)eNetworkErrorCode::ec_estab:
            return "(network)client was ESTAB";

        case (int)eNetworkErrorCode::ec_shutdown:
            return "(network)user shutdown";

        case (int)eNetworkErrorCode::ec_half:
            return "(network)send or recv half of package";

        case (int)eNetworkErrorCode::ec_no_destition:
            return "(network)udp send must be appoint a destition address";

        case (int)eNetworkErrorCode::ec_send_timeout:
            return "(network)send time out";

        case (int)eNetworkErrorCode::ec_recv_timeout:
            return "(network)recv time out";

        case (int)eNetworkErrorCode::ec_url_parse_error:
            return "(network)url parse error";

        case (int)eNetworkErrorCode::ec_data_parse_error:
            return "(network)data parse error";

        case (int)eNetworkErrorCode::ec_unsupport_protocol:
            return "(network)unsupport protocol";

        case (int)eNetworkErrorCode::ec_recv_overflow:
            return "(network)recv buf overflow";

        case (int)eNetworkErrorCode::ec_dns_not_found:
            return "(network)dns not found";
    }

    return "";
}

const boost::system::error_category& GetNetworkErrorCategory()
{
    static network_error_category obj;
    return obj;
}
boost_ec MakeNetworkErrorCode(eNetworkErrorCode code)
{
    return boost_ec((int)code, GetNetworkErrorCategory());
}
void ThrowError(eNetworkErrorCode code, const char* what)
{
    if (std::uncaught_exception()) return ;
    throw boost::system::system_error(MakeNetworkErrorCode(code), what);
}

} //namespace network
