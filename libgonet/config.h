#pragma once

// cmake config
#include "cmake_config.h"

// std
#include <limits>
#include <string>
#include <stdint.h>
#include <atomic>
#include <exception>

// boost
#include <boost/asio.hpp>
#include <boost/any.hpp>
#include <boost/function.hpp>
#include <boost/regex.hpp>

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>

#if ENABLE_SSL
#include <boost/asio/ssl.hpp>
#endif

// linux
#include <signal.h>

// libgo
#include <libgo/coroutine.h>
