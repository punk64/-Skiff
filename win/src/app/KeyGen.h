#pragma once
#include <string>

namespace sk {

enum class KeyKind { Rsa2048, EcdsaP256, Ed25519 };

struct KeyPair {
    bool ok = false;
    std::string privatePem;
    std::string publicLine;
    std::string error;
};

KeyPair Generate(KeyKind kind, const std::string& comment);

bool SaveTo(const KeyPair& kp, const std::wstring& dir, const std::string& name, std::string& err);

}
