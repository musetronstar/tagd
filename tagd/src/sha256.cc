#include "tagd/sha256.h"
#include <openssl/evp.h>

namespace tagd {

class sha256::impl {
public:
	EVP_MD_CTX* context = EVP_MD_CTX_new();
	bool finished = false;
	~impl() { EVP_MD_CTX_free(context); }
};

sha256::sha256() : _impl(std::make_unique<impl>()) {
	if (!_impl->context || EVP_DigestInit_ex(_impl->context, EVP_sha256(), nullptr) != 1) {
		error(TS_INTERNAL_ERR, "SHA-256 initialization failed");
	}
}

sha256::~sha256() = default;

code sha256::update(std::string_view bytes) {
	if (!ok()) {
		return code();
	}
	if (_impl->finished) {
		return error(TS_MISUSE, "SHA-256 is already finalized");
	}
	if (EVP_DigestUpdate(_impl->context, bytes.data(), bytes.size()) != 1) {
		return error(TS_INTERNAL_ERR, "SHA-256 update failed");
	}
	return TAGD_OK;
}

code sha256::finish(std::string& output) {
	output.clear();
	if (!ok()) {
		return code();
	}
	if (_impl->finished) {
		return error(TS_MISUSE, "SHA-256 is already finalized");
	}
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned size = 0;
	if (EVP_DigestFinal_ex(_impl->context, digest, &size) != 1 || size != 32) {
		return error(TS_INTERNAL_ERR, "SHA-256 finalization failed");
	}
	_impl->finished = true;
	constexpr char hex[] = "0123456789abcdef";
	for (unsigned i = 0; i < size; ++i) {
		output += hex[digest[i] >> 4];
		output += hex[digest[i] & 15];
	}
	return TAGD_OK;
}

}
