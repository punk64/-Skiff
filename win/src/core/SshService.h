#pragma once

#include <string>

namespace sk {

struct SshParams {
    std::string host;
    int port = 22;
    std::string user;
    std::string password;
    bool useKey = false;
    std::string keyPath;
    std::string expectedHostKey;
    int keepAlive = 60;
};

enum class SshState {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Failed = 3
};

class SshService {
public:
    using NotifyFn = void (*)(void* ctx);

    static SshService& Instance();

    bool Connect(const SshParams& p);
    void Disconnect();
    bool Send(const std::string& bytes);
    bool ResizePty(int cols, int rows);

    SshState State() const;
    bool IsConnected() const;
    std::string LastError() const;
    std::string HostKey() const;
    std::string ActiveHost() const;

    void SetNotify(NotifyFn fn, void* ctx);
    std::string PumpOutput();
    bool HasPending() const;

private:
    SshService();
    ~SshService();
    SshService(const SshService&) = delete;
    SshService& operator=(const SshService&) = delete;

    struct Impl;
    Impl* d_;
};

}
