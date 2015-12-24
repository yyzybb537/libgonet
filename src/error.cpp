#include "error.h"

namespace network
{

const char* network_error_category::name() const BOOST_SYSTEM_NOEXCEPT
{
    return "network_error";
}

std::string network_error_category::message(int v) const
{
    switch (v) {
        case (int)eNetworkErrorCode::ec_ok:
            return "(networ)ok";

        case (int)eNetworkErrorCode::ec_connecting:
            return "(networ)client was connecting";

        case (int)eNetworkErrorCode::ec_estab:
            return "(networ)client was ESTAB";

        case (int)eNetworkErrorCode::ec_shutdown:
            return "(networ)user shutdown";

        case (int)eNetworkErrorCode::ec_half:
            return "(networ)send or recv half of package";

        case (int)eNetworkErrorCode::ec_no_destition:
            return "(networ)udp send must be appoint a destition address";

        case (int)eNetworkErrorCode::ec_timeout:
            return "(networ)time out";

        case (int)eNetworkErrorCode::ec_url_parse_error:
            return "(networ)url parse error";

        case (int)eNetworkErrorCode::ec_data_parse_error:
            return "(networ)data parse error";

        case (int)eNetworkErrorCode::ec_unsupport_protocol:
            return "(networ)unsupport protocol";

        case (int)eNetworkErrorCode::ec_recv_overflow:
            return "(networ)recv buf overflow";
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
