#include "SshService.h"

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include "protocol/ssh/SshSession.h"

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
    j += "\"keepalive\":" + std::to_string(p.keepAlive) + "\",";
    j += "\"version\":\"1.0.0\"";
    j += "}}";
    return j;
}

}

struct SshService::Impl {
    std::unique_ptr<SshSession> session;
    mutable std::mutex mtx;
    std::string pending;
    std::string lastError;
    std::string hostKey;
    std::string activeHost;
    std::atomic<int> state{0};
    std::thread worker;
    std::atomic<bool> busy{false};
    NotifyFn notify = nullptr;
    void* notifyCtx = nullptr;

    void Notify() {
        NotifyFn fn = notify;
        void* ctx = notifyCtx;
        if (fn != nullptr) fn(ctx);
    }

    void OnData(const std::string& data) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            pending += data;
            if (pending.size() > 4194304) {
                pending.erase(0, pending.size() - 2097152);
            }
        }
        Notify();
    }

    void RunConnect(SshParams p) {
        int keyFd = -1;
        if (p.useKey && !p.keyPath.empty()) {
            keyFd = _open(p.keyPath.c_str(), _O_RDONLY | _O_BINARY);
        }

        std::unique_ptr<SshSession> s = std::make_unique<SshSession>();
        s->setDataCallbacks([this](int, std::string data, bool isStatus) {
            if (isStatus) return;
            OnData(data);
        });
        s->setSessionCallbacks([this](SessionStatus st) {
            if (st == SessionStatus::DISCONNECTED) {
                state.store(static_cast<int>(SshState::Disconnected));
                Notify();
            }
        });

        std::string json = BuildJson(p, keyFd);
        bool ok = s->openConnect(json);

        if (keyFd >= 0) {
            _close(keyFd);
        }

        if (!ok) {
            std::lock_guard<std::mutex> lk(mtx);
            lastError = s->getLastConnectError();
            if (lastError.empty()) lastError = "连接失败";
            state.store(static_cast<int>(SshState::Failed));
            busy.store(false);
            Notify();
            return;
        }

        {
            std::lock_guard<std::mutex> lk(mtx);
            hostKey = s->getHostKeyFingerprint();
            activeHost = p.host;
            lastError.clear();
            session = std::move(s);
            state.store(static_cast<int>(SshState::Connected));
        }
        busy.store(false);
        Notify();
    }
};

SshService& SshService::Instance() {
    static SshService inst;
    return inst;
}

SshService::SshService() : d_(new Impl()) {
}

SshService::~SshService() {
    Disconnect();
    if (d_ != nullptr) {
        delete d_;
        d_ = nullptr;
    }
}

bool SshService::Connect(const SshParams& p) {
    if (d_->busy.exchange(true)) return false;
    Disconnect();
    d_->busy.store(true);
    {
        std::lock_guard<std::mutex> lk(d_->mtx);
        d_->lastError.clear();
        d_->hostKey.clear();
        d_->activeHost = p.host;
    }
    d_->state.store(static_cast<int>(SshState::Connecting));
    d_->Notify();
    if (d_->worker.joinable()) d_->worker.join();
    d_->worker = std::thread(&Impl::RunConnect, d_, p);
    return true;
}

void SshService::Disconnect() {
    std::unique_ptr<SshSession> owned;
    {
        std::lock_guard<std::mutex> lk(d_->mtx);
        owned = std::move(d_->session);
    }
    if (owned) {
        owned->disconnect();
        owned.reset();
    }
    if (d_->worker.joinable()) {
        d_->worker.join();
    }
    d_->state.store(static_cast<int>(SshState::Disconnected));
}

bool SshService::Send(const std::string& bytes) {
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!d_->session || !d_->session->isConnected()) return false;
    return d_->session->sendData(bytes);
}

bool SshService::ResizePty(int cols, int rows) {
    std::lock_guard<std::mutex> lk(d_->mtx);
    if (!d_->session) return false;
    return d_->session->resizePty(cols, rows);
}

SshState SshService::State() const {
    return static_cast<SshState>(d_->state.load());
}

bool SshService::IsConnected() const {
    return d_->state.load() == static_cast<int>(SshState::Connected);
}

std::string SshService::LastError() const {
    std::lock_guard<std::mutex> lk(d_->mtx);
    return d_->lastError;
}

std::string SshService::HostKey() const {
    std::lock_guard<std::mutex> lk(d_->mtx);
    return d_->hostKey;
}

std::string SshService::ActiveHost() const {
    std::lock_guard<std::mutex> lk(d_->mtx);
    return d_->activeHost;
}

void SshService::SetNotify(NotifyFn fn, void* ctx) {
    d_->notify = fn;
    d_->notifyCtx = ctx;
}

std::string SshService::PumpOutput() {
    std::lock_guard<std::mutex> lk(d_->mtx);
    std::string out;
    out.swap(d_->pending);
    return out;
}

bool SshService::HasPending() const {
    std::lock_guard<std::mutex> lk(d_->mtx);
    return !d_->pending.empty();
}

}
