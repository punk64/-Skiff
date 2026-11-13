#include "core/SshService.h"
#include <windows.h>
#include <cstdio>
#include <string>

extern "C" void skPlatformInit(void);

struct Slot {
    int id = -1;
    int port = 0;
    const wchar_t* file = nullptr;
    const char* remote = nullptr;
    std::string err;
    bool ok = false;
    unsigned long long done = 0;
    unsigned long long total = 0;
};

static Slot g_slot[2];
static volatile bool g_stop = false;

static DWORD WINAPI RunUpload(LPVOID p) {
    Slot* s = static_cast<Slot*>(p);
    sk::SshService& svc = sk::SshService::Instance();
    s->ok = svc.SftpUpload(s->id, s->file, s->remote, s->err,
                           [s](unsigned long long d, unsigned long long t) {
                               s->done = d;
                               s->total = t;
                           });
    return 0;
}

static DWORD WINAPI Sample(LPVOID) {
    unsigned long long pa = 0;
    unsigned long long pb = 0;
    int bothGrowing = 0;
    while (!g_stop) {
        Sleep(250);
        unsigned long long a = g_slot[0].done;
        unsigned long long b = g_slot[1].done;
        bool ga = a > pa;
        bool gb = b > pb;
        if (ga && gb) ++bothGrowing;
        printf("PAR a=%llu/%llu b=%llu/%llu grow=%d%d both=%d\n",
               a, g_slot[0].total, b, g_slot[1].total, ga ? 1 : 0, gb ? 1 : 0, bothGrowing);
        fflush(stdout);
        pa = a;
        pb = b;
    }
    printf("BOTH_GROWING_TICKS %d\n", bothGrowing);
    fflush(stdout);
    return 0;
}

static bool ConnectOne(Slot& s, int port, const wchar_t* file, const char* remote) {
    sk::SshParams p;
    p.host = "127.0.0.1";
    p.port = port;
    p.user = "test";
    p.password = "test";
    p.timeout = 15;
    p.keepAlive = 60;
    s.port = port;
    s.file = file;
    s.remote = remote;
    sk::SshService& svc = sk::SshService::Instance();
    if (!svc.Connect(s.id, p)) {
        printf("CONNECT_START_FAILED port=%d\n", port);
        return false;
    }
    for (int i = 0; i < 400; ++i) {
        sk::SshState st = svc.State(s.id);
        if (st == sk::SshState::Connected) return true;
        if (st == sk::SshState::Failed) {
            printf("CONNECT_FAILED port=%d err=%s\n", port, svc.LastError(s.id).c_str());
            return false;
        }
        Sleep(50);
    }
    printf("NOT_CONNECTED port=%d\n", port);
    return false;
}

int wmain(int argc, wchar_t** argv) {
    skPlatformInit();
    if (argc < 3) {
        printf("usage: test_resume <fileA> <fileB> [limitKBps]\n");
        return 2;
    }
    long long limit = 512 * 1024;
    if (argc >= 4) limit = _wtoi64(argv[3]) * 1024LL;

    g_slot[0].id = 900;
    g_slot[1].id = 901;
    if (!ConnectOne(g_slot[0], 2222, argv[1], "/a.bin")) return 3;
    if (!ConnectOne(g_slot[1], 2223, argv[2], "/b.bin")) return 4;
    printf("CONNECTED both\n");
    fflush(stdout);

    sk::SshService& svc = sk::SshService::Instance();
    svc.SetUploadLimit(g_slot[0].id, limit);
    svc.SetUploadLimit(g_slot[1].id, limit);

    HANDLE hs = CreateThread(nullptr, 0, Sample, nullptr, 0, nullptr);
    ULONGLONG t0 = GetTickCount64();
    HANDLE h1 = CreateThread(nullptr, 0, RunUpload, &g_slot[0], 0, nullptr);
    HANDLE h2 = CreateThread(nullptr, 0, RunUpload, &g_slot[1], 0, nullptr);
    WaitForSingleObject(h1, 600000);
    WaitForSingleObject(h2, 600000);
    ULONGLONG spent = GetTickCount64() - t0;
    g_stop = true;
    if (hs != nullptr) {
        WaitForSingleObject(hs, 3000);
        CloseHandle(hs);
    }
    CloseHandle(h1);
    CloseHandle(h2);
    printf("RESULT a_ok=%d a_err=%s b_ok=%d b_err=%s ms=%llu xferA=%d xferB=%d\n",
           g_slot[0].ok ? 1 : 0, g_slot[0].err.c_str(),
           g_slot[1].ok ? 1 : 0, g_slot[1].err.c_str(), spent,
           svc.TransferSessionId(g_slot[0].id), svc.TransferSessionId(g_slot[1].id));
    fflush(stdout);
    svc.Disconnect(g_slot[0].id);
    svc.Disconnect(g_slot[1].id);
    return (g_slot[0].ok && g_slot[1].ok) ? 0 : 1;
}
