// Copyright (C) Karim Agha - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

#include "error.h"

namespace sentio::rpc
{
not_authorized::not_authorized()
    : not_authorized("not authorized")
{
}
not_authorized::not_authorized(const char *msg)
    : runtime_error(msg)
{
}

bad_request::bad_request()
    : bad_request("bad request")
{
}
bad_request::bad_request(const char *msg)
    : runtime_error(msg)
{
}

server_error::server_error()
    : server_error("server error")
{
}
server_error::server_error(const char *msg)
    : runtime_error(msg)
{
}

bad_method::bad_method()
    : bad_method("bad method")
{
}
bad_method::bad_method(const char *msg)
    : runtime_error(msg)
{
}

not_implemented::not_implemented()
    : not_implemented("not implemented")
{
}
not_implemented::not_implemented(const char *msg)
    : runtime_error(msg)
{
}

}  // namespace sentio::kurier