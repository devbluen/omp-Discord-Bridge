/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-tls.hpp"

#include <boost/beast/core/error.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#endif

namespace DiscordTLS
{
namespace
{
#if defined(_WIN32)
void loadWindowsRootStore(boost::asio::ssl::context& context, DWORD location)
{
	X509_STORE* store = SSL_CTX_get_cert_store(context.native_handle());
	if (!store)
	{
		return;
	}

	HCERTSTORE windowsStore = CertOpenStore(
		CERT_STORE_PROV_SYSTEM_W,
		0,
		0,
		location | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG,
		L"ROOT");
	if (!windowsStore)
	{
		return;
	}

	PCCERT_CONTEXT certificateContext = nullptr;
	while ((certificateContext = CertEnumCertificatesInStore(windowsStore, certificateContext)) != nullptr)
	{
		const unsigned char* encodedCertificate = certificateContext->pbCertEncoded;
		X509* certificate = d2i_X509(nullptr, &encodedCertificate,
			static_cast<long>(certificateContext->cbCertEncoded));
		if (!certificate)
		{
			continue;
		}

		// The two Windows stores can contain the same root. OpenSSL reports a
		// duplicate as an error, but it should not poison the next handshake.
		if (X509_STORE_add_cert(store, certificate) != 1)
		{
			ERR_clear_error();
		}
		X509_free(certificate);
	}

	CertCloseStore(windowsStore, 0);
}
#endif
}

void configureCertificateVerification(boost::asio::ssl::context& context)
{
	context.set_default_verify_paths();

#if defined(_WIN32)
	// OpenSSL does not automatically consult the Windows certificate stores.
	// Import both stores so this also works for servers running as services or
	// under an account with user-installed enterprise roots.
	loadWindowsRootStore(context, CERT_SYSTEM_STORE_CURRENT_USER);
	loadWindowsRootStore(context, CERT_SYSTEM_STORE_LOCAL_MACHINE);
#elif defined(__linux__)
	// The bundled static OpenSSL build has its own compile-time OPENSSLDIR,
	// which is not a system CA directory. Explicitly load the common Linux CA
	// bundles while keeping peer and hostname verification enabled.
	boost::beast::error_code ec;
	context.load_verify_file("/etc/ssl/certs/ca-certificates.crt", ec);
	if (ec)
	{
		context.load_verify_file("/etc/pki/tls/certs/ca-bundle.crt", ec);
	}
#endif
}
}
