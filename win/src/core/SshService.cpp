#include "SshService.h"

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

#include "protocol/ssh/SshSession.h"
#include "protocol/ssh/SftpSession.h"
#include "libssh2.h"

namespace sk {

namespace {

std::string EscapeJson(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') {
            o.push_back('\\');
            o.push_back(c);
        } else if (c == '\n') {
            o += "\\n";
        } else if (c == '\r') {
            o += "\\r";
        } else if (c == '\t') {
            o += "\\t";
        } else {
            o.push_back(c);
        }
    }
    return o;
}

std::string BuildJson(const SshParams& p, int keyFd) {
    std::string j = "{\"Session\":{";
    j += "\"connectionType\":\"SSH\",";
    j += "\"hostName\":\"" + EscapeJson(p.host) + "\",";
    j += "\"port\":\"" + std::to_string(p.port) + "\",";
    j += "\"userName\":\"" + EscapeJson(p.user) + "\",";
    j += "\"password\":\"" + EscapeJson(p.password) + "\",";
    j += "\"authType\":\"";
    j += p.useKey ? "publickey" : "password";
    j += "\"";
    if (p.useKey) {
        j += ",\"privateKeyFd\":" + std::to_string(keyFd);
    }
    if (!p.expectedHostKey.empty()) {
        j += ",\"expectedHostKey\":\"" + EscapeJson(p.expectedHostKey) + "\"";
    }
    j += "},\"Connection\":{";
    j += "\"keepalive\":" + std::to_string(p.keepAlive) + ",";
    j += "\"timeout\":" + std::to_string(p.timeout) + ",";
    j += "\"version\":\"1.0.0\"";
    j += "}}";
    return j;
}

}

struct SshService::Impl {
    struct Item {
        std::unique_ptr<SshSession> session;
        std::thread worker;
        std::string pending;
        std::string lastError;
        std::string hostKey;
        std::string activeHost;
        std::atomic<int> state{0};
        std::atomic<bool> busy{false};
        bool sftpReady = false;
        mutable std::mutex ioMtx;
    };

    mutable std::mutex mtx;
    std::map<int, std::shared_ptr<Item>> items;
    NotifyFn notify = nullptr;
    void* notifyCtx = nullptr;

    void Notify(int id) {
        NotifyFn fn = notify;
        void* ctx = notifyCtx;
        if (fn != nullptr) fn(ctx, id);
    }

    std::shared_ptr<Item> Find(int id) const {
        std::lock_guard<std::mutex> lk(mtx);
        auto pos = items.find(id);
        if (pos == items.end()) return nullptr;
        return pos->second;
    }

    std::shared_ptr<Item> ItemOf(int id) {
        std::lock_guard<std::mutex> lk(mtx);
        auto pos = items.find(id);
        if (pos != items.end()) return pos->second;
        std::shared_ptr<Item> item = std::make_shared<Item>();
        items[id] = item;
        return item;
    }

    void OnData(int id, const std::string& data) {
        std::shared_ptr<Item> item = Find(id);
        if (!item) return;
        {
            std::lock_guard<std::mutex> lk(mtx);
            item->pending += data;
            if (item->pending.size() > 4194304) {
                item->pending.erase(0, item->pending.size() - 2097152);
            }
        }
        Notify(id);
    }

    void RunConnect(std::shared_ptr<Item> item, SshParams p, int id) {
        int keyFd = -1;
        if (p.useKey && !p.keyPath.empty()) {
            keyFd = _open(p.keyPath.c_str(), _O_RDONLY | _O_BINARY);
        }

        std::unique_ptr<SshSession> s = std::make_unique<SshSession>();
        s->setDataCallbacks([this, id](int, std::string data, bool isStatus) {
            if (isStatus) return;
            OnData(id, data);
        });
        s->setSessionCallbacks([this, id](SessionStatus st) {
            if (st != SessionStatus::DISCONNECTED) return;
            std::shared_ptr<Item> target = Find(id);
            if (!target) return;
            target->state.store(static_cast<int>(SshState::Disconnected));
            Notify(id);
        });

        std::string json = BuildJson(p, keyFd);
        bool ok = s->openConnect(json);

        if (keyFd >= 0) {
            _close(keyFd);
        }

        if (!ok) {
            {
                std::lock_guard<std::mutex> lk(mtx);
                item->lastError = s->getLastConnectError();
                if (item->lastError.empty()) item->lastError = "连接失败";
            }
            item->state.store(static_cast<int>(SshState::Failed));
            item->busy.store(false);
            Notify(id);
            return;
        }

        {
            std::lock_guard<std::mutex> lk(mtx);
            item->hostKey = s->getHostKeyFingerprint();
            item->activeHost = p.host;
            item->lastError.clear();
            item->session = std::move(s);
            item->state.store(static_cast<int>(SshState::Connected));
        }
        item->busy.store(false);
        Notify(id);
    }
};

SshService& SshService::Instance() {
    static SshService inst;
    return inst;
}

SshService::SshService() : d_(new Impl()) {
}

SshService::~SshService() {
    DisconnectAll();
    if (d_ != nullptr) {
        delete d_;
        d_ = nullptr;
    }
}

bool SshService::Connect(int id, const SshParams& p) {
    std::shared_ptr<Impl::Item> item = d_->ItemOf(id);
    if (!item) return false;

    std::unique_ptr<SshSession> old;
    {
        std::lock_guard<std::mutex> ilk(item->ioMtx);
        std::lock_guard<std::mutex> lk(d_->mtx);
        old = std::move(item->session);
        item->pending.clear();
        item->sftpReady = false;
        item->lastError.clear();
        item->hostKey.clear();
        item->activeHost = p.host;
    }
    if (old) {
        old->disconnect();
        old.reset();
    }
    if (item->worker.joinable()) item->worker.join();

    item->busy.store(true);
    item->state.store(static_cast<int>(SshState::Connecting));
    d_->Notify(id);
    item->worker = std::thread(&Impl::RunConnect, d_, item, p, id);
    return true;
}

void SshService::Disconnect(int id) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return;

    std::unique_ptr<SshSession> owned;
    {
        std::lock_guard<std::mutex> ilk(item->ioMtx);
        std::lock_guard<std::mutex> lk(d_->mtx);
        owned = std::move(item->session);
        item->pending.clear();
        item->sftpReady = false;
    }
    if (owned) {
        owned->disconnect();
        owned.reset();
    }
    if (item->worker.joinable()) item->worker.join();
    item->busy.store(false);
    item->state.store(static_cast<int>(SshState::Disconnected));
    d_->Notify(id);
}

void SshService::DisconnectAll() {
    std::vector<int> ids;
    {
        std::lock_guard<std::mutex> lk(d_->mtx);
        for (auto& kv : d_->items) ids.push_back(kv.first);
    }
    for (size_t i = 0; i < ids.size(); ++i) Disconnect(ids[i]);
}

bool SshService::Send(int id, const std::string& bytes) {
    std::lock_guard<std::mutex> lk(d_->mtx);
    auto pos = d_->items.find(id);
    if (pos == d_->items.end()) return false;
    std::shared_ptr<Impl::Item> item = pos->second;
    if (!item || !item->session || !item->session->isConnected()) return false;
    return item->session->sendData(bytes);
}

bool SshService::ResizePty(int id, int cols, int rows) {
    std::lock_guard<std::mutex> lk(d_->mtx);
    auto pos = d_->items.find(id);
    if (pos == d_->items.end()) return false;
    std::shared_ptr<Impl::Item> item = pos->second;
    if (!item || !item->session) return false;
    return item->session->resizePty(cols, rows);
}

SshState SshService::State(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return SshState::Disconnected;
    return static_cast<SshState>(item->state.load());
}

bool SshService::IsConnected(int id) const {
    return State(id) == SshState::Connected;
}

std::string SshService::LastError(int id) const {
    std::lock_guard<std::mutex> lk(d_->mtx);
    auto pos = d_->items.find(id);
    if (pos == d_->items.end()) return std::string();
    return pos->second->lastError;
}

std::string SshService::HostKey(int id) const {
    std::lock_guard<std::mutex> lk(d_->mtx);
    auto pos = d_->items.find(id);
    if (pos == d_->items.end()) return std::string();
    return pos->second->hostKey;
}

std::string SshService::ActiveHost(int id) const {
    std::lock_guard<std::mutex> lk(d_->mtx);
    auto pos = d_->items.find(id);
    if (pos == d_->items.end()) return std::string();
    return pos->second->activeHost;
}

std::string SshService::CipherName(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session || !item->session->isConnected()) return std::string();
    return item->session->getCipherName();
}

std::string SshService::MacName(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session || !item->session->isConnected()) return std::string();
    return item->session->getMacName();
}

void SshService::SetNotify(NotifyFn fn, void* ctx) {
    d_->notify = fn;
    d_->notifyCtx = ctx;
}

std::string SshService::PumpOutput(int id) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(d_->mtx);
    std::string out;
    out.swap(item->pending);
    return out;
}

bool SshService::HasPending(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(d_->mtx);
    return !item->pending.empty();
}

bool SshService::Exec(int id, const std::string& command, std::string& out) {
    out.clear();
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    LIBSSH2_SESSION* sess = ssh->getInternalSession();
    if (sess == nullptr) return false;

    libssh2_session_set_timeout(sess, 2000);
    LIBSSH2_CHANNEL* ch = libssh2_channel_open_session(sess);
    if (ch == nullptr) {
        libssh2_session_set_timeout(sess, 0);
        return false;
    }
    if (libssh2_channel_exec(ch, command.c_str()) != 0) {
        libssh2_channel_send_eof(ch);
        libssh2_channel_wait_eof(ch);
        libssh2_channel_wait_closed(ch);
        libssh2_channel_free(ch);
        libssh2_session_set_timeout(sess, 0);
        return false;
    }

    char buf[4096];
    int idle = 0;
    for (int guard = 0; guard < 512 && idle < 20; ++guard) {
        ssize_t n = libssh2_channel_read(ch, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, (size_t)n);
            idle = 0;
            if (out.size() > 262144) break;
            continue;
        }
        if (n == LIBSSH2_ERROR_EAGAIN) {
            if (ssh->waitReadable() < 0) break;
            ++idle;
            continue;
        }
        break;
    }
    char errBuf[256];
    for (int guard = 0; guard < 32; ++guard) {
        ssize_t n = libssh2_channel_read_stderr(ch, errBuf, sizeof(errBuf));
        if (n > 0) continue;
        if (n == LIBSSH2_ERROR_EAGAIN && ssh->waitReadable() >= 0) continue;
        break;
    }

    libssh2_channel_send_eof(ch);
    libssh2_channel_wait_eof(ch);
    libssh2_channel_wait_closed(ch);
    libssh2_channel_free(ch);
    libssh2_session_set_timeout(sess, 0);
    return true;
}

bool SshService::SftpList(int id, const std::string& path, std::vector<RemoteEntry>& out) {
    out.clear();
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return ssh->listDirectory(path, [&out](const FileInfo& fi) {
        RemoteEntry e;
        e.name = fi.name;
        e.fullPath = fi.fullPath;
        e.dir = fi.isDirectory;
        e.size = fi.size;
        e.mode = fi.permissions;
        e.mtime = fi.mtime;
        out.push_back(e);
    });
}

bool SshService::SftpUpload(int id, const std::wstring& localPath, const std::string& remotePath,
                            std::string& err, ProgressFn prog) {
    err.clear();
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) {
        err = "SESSION_NOT_FOUND";
        return false;
    }
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) {
        err = "SESSION_NOT_CONNECTED";
        return false;
    }
    int fd = _wopen(localPath.c_str(), _O_RDONLY | _O_BINARY);
    if (fd < 0) {
        err = "OPEN_LOCAL_FAILED";
        return false;
    }
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (ssh->initSftp()) item->sftpReady = true;
    }
    std::function<void(int)> up;
    if (prog) {
        up = [prog](int pct) {
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            prog(static_cast<unsigned long long>(pct), 100ULL);
        };
    }
    bool ok = item->sftpReady && ssh->uploadFile(fd, remotePath, up);
    _close(fd);
    if (!ok) err = "SFTP_UPLOAD_FAILED";
    return ok;
}

bool SshService::SftpDownload(int id, const std::string& remotePath, const std::wstring& localPath,
                              std::string& err, ProgressFn prog) {
    err.clear();
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) {
        err = "SESSION_NOT_FOUND";
        return false;
    }
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) {
        err = "SESSION_NOT_CONNECTED";
        return false;
    }
    int fd = _wopen(localPath.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                    _S_IREAD | _S_IWRITE);
    if (fd < 0) {
        err = "OPEN_LOCAL_FAILED";
        return false;
    }
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (ssh->initSftp()) item->sftpReady = true;
    }
    unsigned long long total = 0;
    FileInfo fi;
    if (ssh->getFileInfo(remotePath, fi)) total = fi.size;
    std::function<void(off_t)> dn;
    if (prog) {
        dn = [prog, total](off_t done) {
            prog(done < 0 ? 0ULL : static_cast<unsigned long long>(done), total);
        };
    }
    bool ok = item->sftpReady && ssh->downloadFile(remotePath, fd, dn);
    _close(fd);
    if (!ok) err = "SFTP_DOWNLOAD_FAILED";
    return ok;
}

bool SshService::SftpDelete(int id, const std::string& path, bool dir) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return dir ? ssh->deleteDirectory(path) : ssh->deleteFile(path);
}

bool SshService::SftpMkdir(int id, const std::string& path) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return ssh->createDirectory(path);
}

bool SshService::SftpRename(int id, const std::string& from, const std::string& to) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return ssh->rename(from, to);
}

bool SshService::ForwardStartLocal(int id, int localPort, const std::string& targetHost,
                                   int targetPort) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->startLocalPortForwarding(localPort, targetHost, targetPort, false);
}

bool SshService::ForwardStopLocal(int id, int localPort) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session) return false;
    SshSession* ssh = item->session.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->stopPortForwarding(localPort, false);
}

bool SshService::ForwardActive(int id, int localPort) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session) return false;
    SshSession* ssh = item->session.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->isPortForwardingActive(localPort, false);
}

int SshService::ForwardAccepts(int id) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return 0;
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session) return 0;
    SshSession* ssh = item->session.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->getForwardAccepts();
}

int SshService::ForwardFailures(int id) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return 0;
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!item->session) return 0;
    SshSession* ssh = item->session.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->getForwardFailures();
}

}
