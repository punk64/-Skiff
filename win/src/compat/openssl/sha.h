#pragma once
#include "../Platform.h"

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#define SHA256_DIGEST_LENGTH 32

#ifdef __cplusplus
extern "C" {
#endif

unsigned char* skSHA256(const unsigned char* data, size_t len, unsigned char* out);

#ifdef __cplusplus
}
#endif

#define SHA256(d, n, md) skSHA256((d), (n), (md))
