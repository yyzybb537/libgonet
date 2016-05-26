#pragma once
#include "config.h"
#include "option.h"

namespace network {

    using namespace ::boost::asio;
    using namespace ::boost::asio::ip;

    enum class tcp_socket_type_t
    {
        tcp,
        ssl,
    };

    enum class handshake_type_t
    {
        client,
        server,
    };

#if ENABLE_SSL
    typedef std::unique_ptr<ssl::context> tcp_context;
#else
    struct tcp_context {};
#endif

    class tcp_socket
    {
    public:
        tcp_socket(io_service& ios, tcp_socket_type_t type, tcp_context & ctx)
            : type_(type)
        {
            if (type_ == tcp_socket_type_t::tcp)
                tcp_socket_.reset(new tcp::socket(ios));
            else {
#if ENABLE_SSL
                tcp_ssl_socket_.reset(new ssl::stream<tcp::socket>(ios, *ctx));
#else
                throw std::invalid_argument("Not support ssl, please rebuild libgonet with cmake option: -DENABLE_SSL=ON");
#endif
            }
        }

#if ENABLE_SSL
        static tcp_context create_tcp_context(OptionSSL const& ssl_opt)
        {
            std::unique_ptr<ssl::context> ctx(new ssl::context((ssl::context::method)ssl_opt.ssl_version));

            // options
            ssl::context::options opt = ssl::context::default_workarounds
                | ssl::context::single_dh_use;
            if (ssl_opt.disable_compression)
                opt |= ssl::context::no_compression;
            ctx->set_options(opt);

            // password callback
            if (ssl_opt.pwd_callback)
                ctx->set_password_callback(ssl_opt.pwd_callback);

            if (ssl_opt.verify_mode == OptionSSL::verify_mode_t::none) {
                ctx->set_verify_mode(ssl::context::verify_none);
            } else if (ssl_opt.verify_mode == OptionSSL::verify_mode_t::optional) {
                ctx->set_verify_mode(ssl::context::verify_peer);
                ctx->load_verify_file(ssl_opt.verify_file);
            } else { // required
                ctx->set_verify_mode(ssl::context::verify_peer 
                        | ssl::context::verify_fail_if_no_peer_cert);
                ctx->load_verify_file(ssl_opt.verify_file);
            }

            if (ssl_opt.certificate_chain_file.size())
                ctx->use_certificate_chain_file(ssl_opt.certificate_chain_file);
            if (ssl_opt.private_key_file.size())
                ctx->use_private_key_file(ssl_opt.private_key_file, ssl::context::pem);
            if (ssl_opt.tmp_dh_file.size())
                ctx->use_tmp_dh_file(ssl_opt.tmp_dh_file);
            return std::move(ctx);
        }
#else
        static tcp_context create_tcp_context(OptionSSL const&)
        {
            return tcp_context();
        }
#endif

        tcp_socket_type_t type() const
        {
            return type_;
        }

        tcp::socket& native_socket()
        {
#if ENABLE_SSL
            if (type_ == tcp_socket_type_t::ssl)
                return tcp_ssl_socket_->next_layer();
#endif
            return *tcp_socket_;
        }

        tcp::socket::native_handle_type native_handle()
        {
            return native_socket().native_handle();
        }

        boost::system::error_code handshake(handshake_type_t type)
        {
            boost::system::error_code ec;
#if ENABLE_SSL
            if (type_ == tcp_socket_type_t::ssl) {
                return tcp_ssl_socket_->handshake(
                        type == handshake_type_t::client ? ssl::stream_base::client : ssl::stream_base::server,
                        ec);
            }
#endif
            return ec;
        }

        boost::system::error_code shutdown(socket_base::shutdown_type type)
        {
            boost::system::error_code ec;
#if ENABLE_SSL
            if (type_ == tcp_socket_type_t::ssl)
                return tcp_ssl_socket_->shutdown(ec);
#endif
            return tcp_socket_->shutdown(type, ec);
        }

        boost::system::error_code close()
        {
            boost::system::error_code ec;
#if ENABLE_SSL
            if (type_ == tcp_socket_type_t::ssl)
                tcp_ssl_socket_->shutdown(ec);
#endif

            return native_socket().close(ec);
        }

        template <typename MutableBufferSequence>
            std::size_t read_some(const MutableBufferSequence& buffers,
                    boost::system::error_code& ec)
            {
#if ENABLE_SSL
                if (type_ == tcp_socket_type_t::ssl)
                    return tcp_ssl_socket_->read_some(buffers, ec);
#endif

                return tcp_socket_->read_some(buffers, ec);
            }

        template <typename ConstBufferSequence>
            std::size_t write_some(const ConstBufferSequence& buffers,
                    boost::system::error_code& ec)
            {
#if ENABLE_SSL
                if (type_ == tcp_socket_type_t::ssl)
                    return tcp_ssl_socket_->write_some(buffers, ec);
#endif

                return tcp_socket_->write_some(buffers, ec);
            }

    private:
        tcp_socket_type_t type_;
        std::unique_ptr<tcp::socket> tcp_socket_;
#if ENABLE_SSL
        std::unique_ptr<ssl::stream<tcp::socket>> tcp_ssl_socket_;
#endif
    };

} //namespace network
