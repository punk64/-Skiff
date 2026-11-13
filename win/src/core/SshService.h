#pragma once

#include <functional>
#include <string>
#include <vector>

namespace sk {

struct SshParams {
    std::string host;
    int port = 22;
    std::string user;
    std::string password;
    bool useKey = false;
    std::string keyPath;
    std::wstring keyPathW;
    std::string keyPass;
    std::string expectedHostKey;
    std::string expectedHostKeyType;
    int keepAlive = 60;
    int timeout = 10;
};

struct RemoteEntry {
    std::string name;
    std::string fullPath;
    bool dir = false;
    unsigned long long size = 0;
    unsigned int mode = 0;
    unsigned long long mtime = 0;
};

enum class SshState {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Failed = 3
};

class SshService {
public:
    using NotifyFn = void (*)(void* ctx, int id);
    using ProgressFn = std::function<void(unsigned long long done, unsigned long long total)>;

    static SshService& Instance();

    bool Connect(int id, const SshParams& p);
    void Disconnect(int id);
    void DisconnectAll();
    void CancelAllTransfers();
    void CancelTransfer(int sid);
    void PauseTransfer(int sid, bool on);
    bool Send(int id, const std::string& bytes);
    bool ResizePty(int id, int cols, int rows);

    SshState State(int id) const;
    bool IsConnected(int id) const;
    std::string LastError(int id) const;
    std::string HostKey(int id) const;
    std::string HostKeyType(int id) const;
    std::string ActiveHost(int id) const;
    std::string CipherName(int id) const;
    std::string MacName(int id) const;

    void SetNotify(NotifyFn fn, void* ctx);
    std::string PumpOutput(int id);
    bool HasPending(int id) const;

    bool Exec(int id, const std::string& command, std::string& out);

    bool SftpList(int id, const std::string& path, std::vector<RemoteEntry>& out,
                  std::string* err = nullptr);
    bool SftpUpload(int id, const std::wstring& localPath, const std::string& remotePath,
                    std::string& err, ProgressFn prog = nullptr);
    bool SftpDownload(int id, const std::string& remotePath, const std::wstring& localPath,
                      std::string& err, ProgressFn prog = nullptr);
    bool SftpDelete(int id, const std::string& path, bool dir, std::string* err = nullptr);
    bool SftpMkdir(int id, const std::string& path, std::string* err = nullptr);
    bool SftpHome(int id, std::string& out);
    bool SftpRename(int id, const std::string& from, const std::string& to,
                    std::string* err = nullptr);
    bool SftpStat(int id, const std::string& path, RemoteEntry& out, std::string* err = nullptr);
    bool SftpChmod(int id, const std::string& path, unsigned int mode,
                   std::string* err = nullptr);

    void Traffic(int id, unsigned long long& rx, unsigned long long& tx) const;

    int EnsureTransferSession(int id);
    int TransferSessionId(int id) const;
    void ReapTransferSession();
    void SetUploadLimit(int id, long long bps);

    bool ForwardStartLocal(int id, int localPort, const std::string& targetHost, int targetPort);
    bool ForwardStopLocal(int id, int localPort);
    bool ForwardActive(int id, int localPort);
    int ForwardAccepts(int id);
    int ForwardFailures(int id);
    int ForwardAcceptsOf(int id, int port);
    int ForwardFailuresOf(int id, int port);

private:
    SshService();
    ~SshService();
    SshService(const SshService&) = delete;
    SshService& operator=(const SshService&) = delete;

    struct Impl;
    Impl* d_;
};

}
