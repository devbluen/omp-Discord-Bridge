/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <boost/asio/ssl/context.hpp>

namespace DiscordTLS
{
void configureCertificateVerification(boost::asio::ssl::context& context);
}
