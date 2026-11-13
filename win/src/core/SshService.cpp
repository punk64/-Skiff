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
#include <exception>
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

template <typename Fn>
bool GuardSftp(std::string* err, Fn&& fn) {
    try {
        return fn();
    } catch (const std::exception& e) {
        if (err != nullptr) *err = e.what();
        return false;
    } catch (...) {
        if (err != nullptr) *err = "SFTP_OP_FAILED";
        return false;
    }
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
    if (!p.expectedHostKeyType.empty()) {
        j += ",\"expectedHostKeyType\":\"" + EscapeJson(p.expectedHostKeyType) + "\"";
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
        std::shared_ptr<SshSession> session;
        std::thread worker;
        std::string pending;
        std::string lastError;
        std::string hostKey;
        std::string hostKeyType;
        std::string activeHost;
        std::atomic<int> state{0};
        std::atomic<bool> busy{false};
        bool sftpReady = false;
        SshParams params;
        std::atomic<int> xferSid{-1};
        std::atomic<long long> upLimit{0};
        std::atomic<unsigned long long> rxBytes{0};
        std::atomic<unsigned long long> txBytes{0};
        mutable std::mutex ioMtx;
        mutable std::mutex dataMtx;
    };

    mutable std::mutex mtx;
    std::map<int, std::shared_ptr<Item>> items;
    std::atomic<int> xferSeq{0};
    std::vector<int> xferZombie;
    std::vector<int> deferZombie;
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

    std::shared_ptr<SshSession> Live(int id) const {
        std::lock_guard<std::mutex> lk(mtx);
        auto pos = items.find(id);
        if (pos == items.end()) return nullptr;
        return pos->second->session;
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
        item->rxBytes.fetch_add(data.size());
        {
            std::lock_guard<std::mutex> lk(item->dataMtx);
            item->pending += data;
            if (item->pending.size() > 4194304) {
                item->pending.erase(0, item->pending.size() - 2097152);
            }
        }
        Notify(id);
    }

    void RunConnect(std::shared_ptr<Item> item, SshParams p, int id) {
        int keyFd = -1;
        if (p.useKey && !p.keyPathW.empty()) {
            keyFd = _wopen(p.keyPathW.c_str(), _O_RDONLY | _O_BINARY);
        }

        std::shared_ptr<SshSession> s = std::make_shared<SshSession>();
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
                std::lock_guard<std::mutex> lk(item->dataMtx);
                item->lastError = s->getLastConnectError();
                if (item->lastError.empty()) item->lastError = "连接失败";
            }
            item->state.store(static_cast<int>(SshState::Failed));
            item->busy.store(false);
            Notify(id);
            return;
        }

        {
            std::lock_guard<std::mutex> lk(item->dataMtx);
            item->hostKey = s->getHostKeyFingerprint();
            item->hostKeyType = s->getHostKeyType();
            item->activeHost = p.host;
            item->lastError.clear();
        }
        {
            std::lock_guard<std::mutex> lk(mtx);
            item->session = s;
        }
        item->state.store(static_cast<int>(SshState::Connected));
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

    std::shared_ptr<SshSession> old;
    {
        std::lock_guard<std::mutex> ilk(item->ioMtx);
        {
            std::lock_guard<std::mutex> lk(d_->mtx);
            old = item->session;
            item->session.reset();
            int oldXid = item->xferSid.exchange(-1);
            if (oldXid != -1) d_->xferZombie.push_back(oldXid);
        }
        {
            std::lock_guard<std::mutex> lk(item->dataMtx);
            item->pending.clear();
            item->lastError.clear();
            item->hostKey.clear();
            item->activeHost = p.host;
            item->params = p;
        }
        item->sftpReady = false;
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

    int xid = item->xferSid.exchange(-1);
    if (xid != -1) {
        std::shared_ptr<SshSession> xs = d_->Live(xid);
        if (xs) xs->cancelTransfer();
        std::lock_guard<std::mutex> lk(d_->mtx);
        d_->xferZombie.push_back(xid);
    }

    std::shared_ptr<SshSession> live = d_->Live(id);
    if (live) live->cancelTransfer();

    std::shared_ptr<SshSession> owned;
    {
        std::unique_lock<std::mutex> ilk(item->ioMtx, std::try_to_lock);
        if (!ilk.owns_lock()) {
            std::lock_guard<std::mutex> lk(d_->mtx);
            bool dup = false;
            for (size_t i = 0; i < d_->deferZombie.size(); ++i) {
                if (d_->deferZombie[i] == id) {
                    dup = true;
                    break;
                }
            }
            if (!dup) d_->deferZombie.push_back(id);
            return;
        }
        {
            std::lock_guard<std::mutex> lk(d_->mtx);
            owned = item->session;
            item->session.reset();
        }
        {
            std::lock_guard<std::mutex> lk(item->dataMtx);
            item->pending.clear();
        }
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

void SshService::CancelAllTransfers() {
    std::vector<std::shared_ptr<SshSession>> all;
    {
        std::lock_guard<std::mutex> lk(d_->mtx);
        for (auto& kv : d_->items) {
            if (kv.second && kv.second->session) all.push_back(kv.second->session);
        }
    }
    for (size_t i = 0; i < all.size(); ++i) all[i]->cancelTransfer();
}

void SshService::CancelTransfer(int sid) {
    std::shared_ptr<Impl::Item> item = d_->Find(sid);
    if (!item) return;
    std::shared_ptr<SshSession> s = d_->Live(sid);
    if (s) s->cancelTransfer();
    if (sid >= 0) {
        int tid = item->xferSid.load();
        if (tid != -1) {
            std::shared_ptr<SshSession> xs = d_->Live(tid);
            if (xs) xs->cancelTransfer();
        }
    }
}

void SshService::PauseTransfer(int sid, bool on) {
    std::shared_ptr<Impl::Item> item = d_->Find(sid);
    if (!item) return;
    std::shared_ptr<SshSession> s = d_->Live(sid);
    if (s) s->pauseTransfer(on);
    if (sid >= 0) {
        int tid = item->xferSid.load();
        if (tid != -1) {
            std::shared_ptr<SshSession> xs = d_->Live(tid);
            if (xs) xs->pauseTransfer(on);
        }
    }
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
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::shared_ptr<SshSession> s = d_->Live(id);
    if (!s || !s->isConnected()) return false;
    bool ok = s->sendData(bytes);
    if (ok) item->txBytes.fetch_add(bytes.size());
    return ok;
}

void SshService::Traffic(int id, unsigned long long& rx, unsigned long long& tx) const {
    rx = 0;
    tx = 0;
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return;
    rx = item->rxBytes.load();
    tx = item->txBytes.load();
}

bool SshService::ResizePty(int id, int cols, int rows) {
    std::shared_ptr<SshSession> s = d_->Live(id);
    if (!s) return false;
    return s->resizePty(cols, rows);
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
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(item->dataMtx);
    return item->lastError;
}

std::string SshService::HostKey(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(item->dataMtx);
    return item->hostKey;
}

std::string SshService::HostKeyType(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(item->dataMtx);
    return item->hostKeyType;
}

std::string SshService::ActiveHost(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(item->dataMtx);
    return item->activeHost;
}

std::string SshService::CipherName(int id) const {
    std::shared_ptr<SshSession> s = d_->Live(id);
    if (!s || !s->isConnected()) return std::string();
    return s->getCipherName();
}

std::string SshService::MacName(int id) const {
    std::shared_ptr<SshSession> s = d_->Live(id);
    if (!s || !s->isConnected()) return std::string();
    return s->getMacName();
}

void SshService::SetNotify(NotifyFn fn, void* ctx) {
    d_->notify = fn;
    d_->notifyCtx = ctx;
}

std::string SshService::PumpOutput(int id) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return std::string();
    std::lock_guard<std::mutex> lk(item->dataMtx);
    std::string out;
    out.swap(item->pending);
    return out;
}

bool SshService::HasPending(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->dataMtx);
    return !item->pending.empty();
}

bool SshService::Exec(int id, const std::string& command, std::string& out) {
    out.clear();
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held || !held->isConnected()) return false;
    SshSession* ssh = held.get();
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

bool SshService::SftpList(int id, const std::string& path, std::vector<RemoteEntry>& out,
                          std::string* err) {
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
    return GuardSftp(err, [&]() {
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
    });
}

int SshService::EnsureTransferSession(int id) {
    std::shared_ptr<Impl::Item> host = d_->Find(id);
    if (!host || !IsConnected(id)) return -1;
    SshParams p;
    {
        std::lock_guard<std::mutex> lk(host->dataMtx);
        p = host->params;
    }
    if (p.host.empty()) return -1;
    int cur = host->xferSid.load();
    if (cur != -1) {
        if (IsConnected(cur)) return cur;
        host->xferSid.store(-1);
        std::lock_guard<std::mutex> lk(d_->mtx);
        d_->xferZombie.push_back(cur);
    }
    int nid = -2 - d_->xferSeq.fetch_add(1);
    if (!Connect(nid, p)) return -1;
    for (int i = 0; i < 400; ++i) {
        SshState st = State(nid);
        if (st == SshState::Connected) {
            std::shared_ptr<Impl::Item> ni = d_->Find(nid);
            if (ni) ni->upLimit.store(host->upLimit.load());
            host->xferSid.store(nid);
            return nid;
        }
        if (st == SshState::Failed) break;
        Sleep(25);
    }
    std::lock_guard<std::mutex> lk(d_->mtx);
    d_->xferZombie.push_back(nid);
    return -1;
}

int SshService::TransferSessionId(int id) const {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return -1;
    return item->xferSid.load();
}

void SshService::ReapTransferSession() {
    std::vector<int> kill;
    {
        std::lock_guard<std::mutex> lk(d_->mtx);
        kill.swap(d_->xferZombie);
        for (size_t i = 0; i < d_->deferZombie.size(); ++i) kill.push_back(d_->deferZombie[i]);
        d_->deferZombie.clear();
    }
    if (kill.empty()) return;
    std::vector<int> again;
    for (size_t i = 0; i < kill.size(); ++i) {
        std::shared_ptr<Impl::Item> item = d_->Find(kill[i]);
        if (!item) continue;
        std::unique_lock<std::mutex> lk(item->ioMtx, std::try_to_lock);
        if (!lk.owns_lock()) {
            again.push_back(kill[i]);
            continue;
        }
        lk.unlock();
        Disconnect(kill[i]);
    }
    if (again.empty()) return;
    std::lock_guard<std::mutex> lk(d_->mtx);
    for (size_t i = 0; i < again.size(); ++i) d_->deferZombie.push_back(again[i]);
}

bool SshService::SftpUpload(int id, const std::wstring& localPath, const std::string& remotePath,
                            std::string& err, ProgressFn prog) {    err.clear();
    int tid = EnsureTransferSession(id);
    int useId = tid != -1 ? tid : id;
    std::shared_ptr<Impl::Item> item = d_->Find(useId);
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
    long long fsize = _filelengthi64(fd);
    unsigned long long total = fsize > 0 ? static_cast<unsigned long long>(fsize) : 0ULL;
    ssh->setUploadThrottle(item->upLimit.load());
    long long sent = 0;
    std::function<void(int64_t)> up = [prog, total, item, &sent](int64_t done) {
        long long d = done > 0 ? done : 0;
        if (d > sent) {
            item->txBytes.fetch_add(static_cast<unsigned long long>(d - sent));
            sent = d;
        }
        if (!prog) return;
        unsigned long long v = static_cast<unsigned long long>(d);
        if (total > 0 && v > total) v = total;
        prog(v, total);
    };
    bool ok = item->sftpReady &&
              GuardSftp(&err, [&]() { return ssh->uploadFile(fd, remotePath, up); });
    _close(fd);
    if (!ok && err.empty()) err = "SFTP_UPLOAD_FAILED";
    return ok;
}

bool SshService::SftpDownload(int id, const std::string& remotePath, const std::wstring& localPath,
                              std::string& err, ProgressFn prog) {
    err.clear();
    int tid = EnsureTransferSession(id);
    int useId = tid != -1 ? tid : id;
    std::shared_ptr<Impl::Item> item = d_->Find(useId);
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
    GuardSftp(&err, [&]() { return ssh->getFileInfo(remotePath, fi); });
    if (fi.size > 0) total = fi.size;
    long long got = 0;
    std::function<void(int64_t)> dn = [prog, total, item, &got](int64_t done) {
        long long d = done > 0 ? done : 0;
        if (d > got) {
            item->rxBytes.fetch_add(static_cast<unsigned long long>(d - got));
            got = d;
        }
        if (!prog) return;
        unsigned long long v = static_cast<unsigned long long>(d);
        if (total > 0 && v > total) v = total;
        prog(v, total);
    };
    bool ok = item->sftpReady &&
              GuardSftp(&err, [&]() { return ssh->downloadFile(remotePath, fd, dn); });
    _close(fd);
    if (!ok && err.empty()) err = "SFTP_DOWNLOAD_FAILED";
    return ok;
}

bool SshService::SftpDelete(int id, const std::string& path, bool dir, std::string* err) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return GuardSftp(err, [&]() {
        return dir ? ssh->deleteDirectory(path) : ssh->deleteFile(path);
    });
}

bool SshService::SftpMkdir(int id, const std::string& path, std::string* err) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return GuardSftp(err, [&]() { return ssh->createDirectory(path); });
}

bool SshService::SftpHome(int id, std::string& out) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return GuardSftp(nullptr, [&]() { return ssh->realPath(".", out); });
}

bool SshService::SftpRename(int id, const std::string& from, const std::string& to,
                            std::string* err) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return GuardSftp(err, [&]() { return ssh->rename(from, to); });
}

bool SshService::SftpStat(int id, const std::string& path, RemoteEntry& out,
                          std::string* err) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    FileInfo fi;
    bool ok = GuardSftp(err, [&]() { return ssh->getFileInfo(path, fi); });
    if (!ok) return false;
    out.name = fi.name;
    out.fullPath = fi.fullPath.empty() ? path : fi.fullPath;
    out.dir = fi.isDirectory;
    out.size = fi.size;
    out.mode = fi.permissions;
    out.mtime = fi.mtime;
    return true;
}

bool SshService::SftpChmod(int id, const std::string& path, unsigned int mode,
                           std::string* err) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return false;
    std::lock_guard<std::mutex> lk(item->ioMtx);
    if (!item->session || !item->session->isConnected()) return false;
    SshSession* ssh = item->session.get();
    if (!item->sftpReady) {
        if (!ssh->initSftp()) return false;
        item->sftpReady = true;
    }
    return GuardSftp(err, [&]() { return ssh->setPermissions(path, mode); });
}

bool SshService::ForwardStartLocal(int id, int localPort, const std::string& targetHost,
                                   int targetPort) {
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held || !held->isConnected()) return false;
    SshSession* ssh = held.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->startLocalPortForwarding(localPort, targetHost, targetPort, false);
}

bool SshService::ForwardStopLocal(int id, int localPort) {
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held) return false;
    SshSession* ssh = held.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->stopPortForwarding(localPort, false);
}

bool SshService::ForwardActive(int id, int localPort) {
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held) return false;
    SshSession* ssh = held.get();
    std::lock_guard<std::recursive_mutex> lock(ssh->getLibssh2Mutex());
    return ssh->isPortForwardingActive(localPort, false);
}

int SshService::ForwardAccepts(int id) {
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held) return 0;
    std::lock_guard<std::recursive_mutex> lock(held->getLibssh2Mutex());
    return held->getForwardAccepts();
}

int SshService::ForwardFailures(int id) {
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held) return 0;
    std::lock_guard<std::recursive_mutex> lock(held->getLibssh2Mutex());
    return held->getForwardFailures();
}

int SshService::ForwardAcceptsOf(int id, int port) {
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held) return 0;
    std::lock_guard<std::recursive_mutex> lock(held->getLibssh2Mutex());
    return held->getForwardAcceptsOf(port);
}

int SshService::ForwardFailuresOf(int id, int port) {
    std::shared_ptr<SshSession> held = d_->Live(id);
    if (!held) return 0;
    std::lock_guard<std::recursive_mutex> lock(held->getLibssh2Mutex());
    return held->getForwardFailuresOf(port);
}

void SshService::SetUploadLimit(int id, long long bps) {
    std::shared_ptr<Impl::Item> item = d_->Find(id);
    if (!item) return;
    item->upLimit.store(bps);
    int xid = item->xferSid.load();
    if (xid == -1) return;
    std::shared_ptr<Impl::Item> xi = d_->Find(xid);
    if (xi) xi->upLimit.store(bps);
    std::shared_ptr<SshSession> s = d_->Live(xid);
    if (s) s->setUploadThrottle(bps);
}

}
