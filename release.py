from __future__ import annotations

import argparse
import contextlib
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import zipfile
from pathlib import Path

APP = Path(__file__).resolve().parent
ROOT = APP.parent
APP_JSON5 = APP / 'AppScope' / 'app.json5'
ROOT_PROFILE = APP / 'build-profile.json5'
ROOT_PROFILE_BACKUP = APP / 'build' / 'build-profile.orig.json5'
ENTRY_PROFILE = APP / 'entry' / 'build-profile.json5'
HAP_OUT = APP / 'entry' / 'build' / 'default' / 'outputs' / 'default'
DEVECO_HOME = Path(os.environ.get('DEVECO_HOME')
                   or r'D:\Program Files\Huawei\DevEco Studio')
HVIGORW = DEVECO_HOME / 'tools' / 'hvigor' / 'bin' / 'hvigorw.bat'
HVIGOR_CACHE = Path(os.environ.get('HVIGOR_USER_HOME')
                    or r'D:\WorkBuddy_Work\MyEnv\.hvigor')
BUILD_LOG = ROOT / '文档指南' / '构建记录.md'
OUT_DIR = ROOT / '产出'
BUNDLE_NAME = 'com.develop.skiff'
KEEP_BUILDS = 5

ABI_MAP = {
    'arm64': ['arm64-v8a'],
    'x64': ['x86_64'],
    'both': ['arm64-v8a', 'x86_64'],
}

NAME_RE = re.compile(r'("versionName"\s*:\s*")([^"]*)(")')
CODE_RE = re.compile(r'("versionCode"\s*:\s*)(\d+)')
SIGNING_RE = re.compile(r'"signingConfig"\s*:\s*"[^"]*"')
ABI_RE = re.compile(r'"abiFilters"\s*:\s*\[[^\]]*\]')

HELP_TEXT = """\
============================================================
 Skiff 轻舟（HarmonyOS）出包脚本
============================================================
用法：python release.py [参数]

  --version x.y.z   精确指定本次出包版本（不再自动 +1 patch；versionCode 仍 +1）
  --no-bump         出包后不推进版本号（versionName 与 versionCode 都不动）
  --overwrite       产物同名时允许覆盖（默认拒绝，避免旧包与 sha256 被悄悄替换）
  --debug           出 debug 包（默认 release）
  --abi arm64|x64|both
                    构建架构（默认 arm64，即真机 arm64-v8a；
                    x64 供模拟器 127.0.0.1:5555，both 出双架构包）
  --note 文本       写入构建记录的「阶段」列（默认「release.py 出包」）
  --out 目录        产物输出目录（默认 产出/）
  --mirror 目录     额外复制一份产物到该目录
  --no-verify       跳过 HAP 内容自检
  --no-record       不向 文档指南/构建记录.md 追加行
  --dry-run         只打印计划，不构建不写文件
  --no-pause        结束不等待按键

产物：产出/Skiff-<版本>-<abi>-<mode>-unsigned.hap
      与同名 .sha256；默认保留最近 5 个版本，更早的自动清理。

★ 一切构建都不保留签名：构建前恒临时注释掉 build-profile.json5 的
  products.signingConfig，产物不含任何证书；侧载须自行重签
  （DevEco 自动签名 / 小白调试助手等）。
★ 版本真源：AppScope/app.json5 的 versionName / versionCode。
  推进规则：不传 --version 时 versionName 末位 +1、versionCode 严格 +1；
  传 --version 时 versionName 以该值为准、不再 +1 patch，versionCode 仍严格 +1
  （保证可覆盖安装）。
  推进在**构建之前**完成，产物名直接用推进后的版本号，只构建一次；
  构建失败会自动回滚 app.json5 到原版本。
★ C 盘红线：HVIGOR_USER_HOME 必须指向非 C 盘，否则脚本直接拒绝构建。
============================================================
"""


def log(msg: str) -> None:
    print('[skiff] %s' % msg, flush=True)


def die(msg: str, code: int = 1) -> int:
    print('[skiff][FAIL] %s' % msg, flush=True)
    return code


def has_console() -> bool:
    if os.name != 'nt':
        return sys.stdin.isatty()
    try:
        import ctypes
        return bool(ctypes.windll.kernel32.GetConsoleWindow())
    except Exception:
        return False


def wait_key() -> None:
    if os.name == 'nt':
        try:
            import msvcrt
            while True:
                if msvcrt.kbhit():
                    msvcrt.getch()
                    return
                time.sleep(0.05)
        except Exception:
            pass
        try:
            os.system('pause >nul')
            return
        except Exception:
            pass
    try:
        input()
    except BaseException:
        pass


def pause_before_exit(code: int = 0) -> None:
    if os.environ.get('SKIFF_NO_PAUSE') == '1':
        return
    if '--no-pause' in sys.argv:
        return
    console = has_console()
    tty = False
    for stream in (sys.stdout, sys.stdin):
        try:
            if stream is not None and stream.isatty():
                tty = True
                break
        except BaseException:
            pass
    if not console and not tty:
        return
    try:
        print('\n[skiff] 结束（exit=%d），按任意键关闭窗口…' % code, flush=True)
    except BaseException:
        pass
    if console:
        try:
            wait_key()
        except BaseException:
            pass
        return
    try:
        input()
    except BaseException:
        pass


def _read_text(path: Path) -> str:
    with open(path, 'r', encoding='utf-8', newline='') as f:
        return f.read()


def _write_atomic(path: Path, text: str) -> None:
    tmp = path.with_name(path.name + '.tmp')
    with open(tmp, 'w', encoding='utf-8', newline='') as f:
        f.write(text)
        f.flush()
        os.fsync(f.fileno())
    os.replace(str(tmp), str(path))


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def read_version() -> tuple[str, int]:
    if not APP_JSON5.exists():
        raise SystemExit(die('找不到 %s' % APP_JSON5))
    raw = _read_text(APP_JSON5)
    mn = NAME_RE.search(raw)
    mc = CODE_RE.search(raw)
    if mn is None or mc is None:
        raise SystemExit(die('%s 缺少 versionName / versionCode' % APP_JSON5))
    core = mn.group(2).strip()
    try:
        code = int(mc.group(2))
    except ValueError:
        raise SystemExit(die('%s 的 versionCode 不是整数' % APP_JSON5))
    parts = core.split('.')
    if len(parts) != 3 or not all(p.isdigit() for p in parts) or code <= 0:
        raise SystemExit(die('%s 版本不合法（需 x.y.z 与正整数 code）' % APP_JSON5))
    return core, code


def write_version(core: str, code: int) -> None:
    cur_core, cur_code = read_version()
    if cur_core == core and cur_code == code:
        return
    raw = _read_text(APP_JSON5)
    new = NAME_RE.sub(lambda m: m.group(1) + core + m.group(3), raw, count=1)
    new = CODE_RE.sub(lambda m: m.group(1) + str(code), new, count=1)
    if new == raw:
        raise SystemExit(die('%s 改写未产生变化' % APP_JSON5))
    _write_atomic(APP_JSON5, new)
    back_core, back_code = read_version()
    if back_core != core or back_code != code:
        raise SystemExit(die('改写后回读仍是 %s/%d —— 未生效，已中止'
                             % (back_core, back_code)))


def bump_patch(core: str) -> str:
    parts = core.split('.')
    if len(parts) != 3 or not all(p.isdigit() for p in parts):
        raise SystemExit(die('版本号必须形如 x.y.z，实得 %r' % core))
    return '%s.%s.%d' % (parts[0], parts[1], int(parts[2]) + 1)


def read_abi_filters() -> list[str]:
    if not ENTRY_PROFILE.exists():
        return []
    m = ABI_RE.search(_read_text(ENTRY_PROFILE))
    if m is None:
        return []
    return re.findall(r'"([^"]+)"', m.group(0))[1:]


@contextlib.contextmanager
def abi_pinned(abis: list[str]):
    if not ENTRY_PROFILE.exists():
        raise BuildProfileError('缺少 %s' % ENTRY_PROFILE)
    raw = _read_text(ENTRY_PROFILE)
    if ABI_RE.search(raw) is None:
        raise BuildProfileError('%s 缺少 externalNativeOptions.abiFilters'
                                % ENTRY_PROFILE)
    nl = '\r\n' if '\r\n' in raw else '\n'
    body = '"abiFilters": [%s%s%s      ]' % (
        nl, (',' + nl).join('        "%s"' % a for a in abis), nl)
    stripped = ABI_RE.sub(lambda _m: body, raw, count=1)
    if stripped == raw:
        yield False
        return
    _write_atomic(ENTRY_PROFILE, stripped)
    log('ABI 已临时改写 : %s' % ' + '.join(abis))
    try:
        yield True
    finally:
        _write_atomic(ENTRY_PROFILE, raw)
        log('ABI 已还原     : entry/build-profile.json5 写回原文')


class BuildProfileError(RuntimeError):
    pass


def profile_broken() -> str:
    if not ROOT_PROFILE.exists():
        return '缺少 build-profile.json5'
    if ROOT_PROFILE.stat().st_size == 0:
        return 'build-profile.json5 为 0 字节（上轮摘除签名时被中断）'
    return ''


def heal_profile() -> str:
    reason = profile_broken()
    if not reason:
        return ''
    if not ROOT_PROFILE_BACKUP.exists():
        raise BuildProfileError('%s，且无备份可恢复' % reason)
    text = _read_text(ROOT_PROFILE_BACKUP)
    if not text.strip():
        raise BuildProfileError('备份 %s 为空，无法恢复' % ROOT_PROFILE_BACKUP)
    _write_atomic(ROOT_PROFILE, text)
    return '%s → 已从 %s 恢复' % (reason, ROOT_PROFILE_BACKUP.name)


@contextlib.contextmanager
def signing_off():
    reason = profile_broken()
    if reason:
        raise BuildProfileError('%s' % reason)
    raw = _read_text(ROOT_PROFILE)
    stripped = SIGNING_RE.sub('// "signingConfig": ""', raw)
    if stripped == raw:
        yield False
        return
    ROOT_PROFILE_BACKUP.parent.mkdir(parents=True, exist_ok=True)
    _write_atomic(ROOT_PROFILE_BACKUP, raw)
    _write_atomic(ROOT_PROFILE, stripped)
    log('签名配置摘除   : app.products.signingConfig 已临时注释')
    try:
        yield True
    finally:
        _write_atomic(ROOT_PROFILE, raw)
        log('签名配置还原   : build-profile.json5 已写回原文')


def build_env() -> dict:
    env = dict(os.environ)
    env.pop('NODE_OPTIONS', None)
    cache = str(HVIGOR_CACHE)
    if cache[:2].upper() == 'C:':
        raise SystemExit(die('HVIGOR_USER_HOME 指向 C 盘（%s）—— 违反磁盘边界，'
                             '请改为 D 盘路径后重试' % cache))
    env['HVIGOR_USER_HOME'] = cache
    env['DEVECO_SDK_HOME'] = str(DEVECO_HOME / 'sdk')
    env['PATH'] = os.pathsep.join((
        str(DEVECO_HOME / 'tools' / 'node'),
        env.get('PATH', ''),
    ))
    return env


def run_build(mode: str) -> int:
    if not HVIGORW.exists():
        return die('找不到 hvigor 入口：%s' % HVIGORW)
    if not (DEVECO_HOME / 'sdk').is_dir():
        return die('DEVECO_HOME 无效（缺 sdk 目录）：%s' % DEVECO_HOME)
    env = build_env()
    cmd = ['cmd', '/c', str(HVIGORW), 'assembleHap',
           '--mode', 'module',
           '-p', 'product=default',
           '-p', 'buildMode=%s' % mode,
           '--no-daemon']
    log('构建命令     : hvigorw assembleHap --mode module '
        '-p product=default -p buildMode=%s --no-daemon' % mode)
    log('HVIGOR 缓存  : %s' % HVIGOR_CACHE)
    proc = subprocess.run(cmd, cwd=str(APP), env=env)
    return proc.returncode


def hap_sign_size(path: Path) -> int:
    data = path.read_bytes()
    with zipfile.ZipFile(path) as z:
        cd_off = z.start_dir
        end = 0
        for info in z.infolist():
            off = info.header_offset
            name_len, extra_len = struct.unpack('<HH', data[off + 26:off + 30])
            tail = off + 30 + name_len + extra_len + info.compress_size
            if tail > end:
                end = tail
    gap = cd_off - end
    if gap <= 0:
        return 0
    block = data[end:cd_off]
    return gap if b'<hap sign block>' in block[-32:] else 0


def find_hap() -> Path | None:
    name = 'entry-default-unsigned.hap'
    p = HAP_OUT / name
    if p.exists():
        log('产物时间戳   : %s（增量构建可能未重打包，内容由自检兜底）'
            % time.strftime('%m-%d %H:%M:%S', time.localtime(p.stat().st_mtime)))
        return p
    found = sorted(q.name for q in HAP_OUT.glob('*.hap')) if HAP_OUT.exists() else []
    log('✖ 期望产物 %s 不存在' % name)
    log('   outputs 目录实际含：%s' % (', '.join(found) if found else '无 .hap'))
    return None


def verify_hap(path: Path, abis: list[str], core: str, code: int) -> str:
    if not path.exists():
        return '产物不存在'
    if path.stat().st_size <= 0:
        return '产物为空'
    try:
        sign_bytes = hap_sign_size(path)
    except Exception as e:
        return '签名块检测失败：%s' % e
    if sign_bytes > 0:
        return '产物含 %d 字节签名块，构建未摘除签名' % sign_bytes
    try:
        with zipfile.ZipFile(path) as z:
            names = set(z.namelist())
            meta = json.loads(z.read('module.json').decode('utf-8'))
    except Exception as e:
        return 'HAP 无法解析：%s' % e
    want = ['module.json', 'ets/modules.abc']
    for arch in abis:
        want.append('libs/%s/libentry.so' % arch)
    missing = [n for n in want if n not in names]
    if missing:
        return 'HAP 缺少关键条目：%s' % ', '.join(missing)
    extra = [n for n in names
             if n.startswith('libs/') and n.endswith('libentry.so')
             and n.split('/')[1] not in abis]
    if extra:
        return 'HAP 含未声明的架构：%s' % ', '.join(sorted(extra))
    app = meta.get('app') or {}
    name = str(app.get('versionName') or '').strip()
    vcode = app.get('versionCode')
    if name and name != core:
        return ('HAP 内 versionName=%s 与目标 %s 不一致；hvigor 同步缓存'
                '未刷新，删 Skiff/.hvigor 后重试' % (name, core))
    if vcode is not None and int(vcode) != code:
        return ('HAP 内 versionCode=%s 与目标 %d 不一致；hvigor 同步缓存'
                '未刷新，删 Skiff/.hvigor 后重试' % (vcode, code))
    bundle = str(app.get('bundleName') or '').strip()
    if bundle and bundle != BUNDLE_NAME:
        return 'HAP 内 bundleName=%s 与 %s 不一致' % (bundle, BUNDLE_NAME)
    return ''


def prune(out_dir: Path) -> None:
    items = sorted(out_dir.glob('Skiff-*.hap'),
                   key=lambda p: p.stat().st_mtime, reverse=True)
    for old in items[KEEP_BUILDS:]:
        try:
            side = old.with_name(old.name + '.sha256')
            old.unlink()
            if side.exists():
                side.unlink()
            log('清理旧包     : %s' % old.name)
        except OSError as e:
            log('清理失败     : %s（%s）' % (old.name, e))


def record_build(core: str, code: int, abi_tag: str, target: Path,
                 digest: str, mode: str, elapsed: float,
                 note: str) -> None:
    if not BUILD_LOG.exists():
        log('构建记录     : %s 不存在，跳过追加' % BUILD_LOG)
        return
    ts = time.strftime('%Y-%m-%d %H:%M')
    desc = '%s %.1fs；versionCode %d；未签名；%s' % (mode, elapsed, code, note)
    row = '| %s | %s | %s | `产出\\%s` | %.2f MB | %s |' % (
        ts, note, abi_tag, target.name, target.stat().st_size / 1048576.0, desc)
    text = _read_text(BUILD_LOG)
    idx = text.find('\n## 待补')
    if idx < 0:
        if not text.endswith('\n'):
            text += '\n'
        text += row + '\n'
    else:
        text = text[:idx] + row + '\n' + text[idx:]
    _write_atomic(BUILD_LOG, text)
    log('构建记录     : 已追加一行到 %s' % BUILD_LOG.name)


def main() -> int:
    return _main()


def _main() -> int:
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument('-h', '--help', action='store_true')
    ap.add_argument('--version')
    ap.add_argument('--no-bump', action='store_true')
    ap.add_argument('--overwrite', action='store_true')
    ap.add_argument('--release', action='store_true',
                    help='等同默认（保留兼容）')
    ap.add_argument('--debug', action='store_true', help='出 debug 包')
    ap.add_argument('--abi', default='arm64')
    ap.add_argument('--note', default='release.py 出包')
    ap.add_argument('--out', default=str(OUT_DIR))
    ap.add_argument('--mirror')
    ap.add_argument('--no-verify', action='store_true')
    ap.add_argument('--no-record', action='store_true')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--no-pause', action='store_true')
    args = ap.parse_args(sys.argv[1:])

    if args.help:
        print(HELP_TEXT)
        return 0

    key = args.abi.strip().lower()
    if key not in ABI_MAP:
        return die('--abi 只支持 arm64 / x64 / both，实得 %r' % args.abi)
    abis = ABI_MAP[key]
    abi_tag = '+'.join(abis)

    cur_core, cur_code = read_version()
    base_core = (args.version or cur_core).strip()
    parts = base_core.split('.')
    if len(parts) != 3 or not all(p.isdigit() for p in parts):
        return die('--version 必须形如 x.y.z，实得 %r' % base_core)
    if args.no_bump:
        core, code = base_core, cur_code
    elif args.version:
        core, code = base_core, cur_code + 1
    else:
        core, code = bump_patch(base_core), cur_code + 1
    bumped = (core, code) != (cur_core, cur_code)
    mode = 'debug' if args.debug else 'release'
    out_dir = Path(args.out).resolve()
    target = out_dir / ('Skiff-%s-%s-%s-unsigned.hap' % (core, abi_tag, mode))
    clash = target.exists()
    if clash and not args.overwrite and not args.dry_run:
        return die('产物已存在：%s\n'
                   '  同名覆盖会顶掉旧包与它的 .sha256，已中止。'
                   '确认要覆盖请加 --overwrite' % target.name)

    log('版本         : %s (versionCode %d)%s'
        % (core, code, '  ← 本次推进' if bumped else ''))
    if bumped:
        log('原版本       : %s (versionCode %d)' % (cur_core, cur_code))
    log('构建模式     : %s' % mode)
    log('架构         : %s' % abi_tag)
    log('签名         : 不保留（构建时恒摘除 products.signingConfig）')
    log('当前配置     : abiFilters=%s' % ('/'.join(read_abi_filters()) or '未读到'))
    log('产物         : %s%s'
        % (target, '  （已存在，将覆盖）' if clash and args.overwrite else ''))
    log('版本真源     : %s' % APP_JSON5)

    if args.dry_run:
        log('--dry-run：不写文件、不构建')
        return 0

    try:
        note = heal_profile()
        if note:
            log('配置自愈     : %s' % note)
    except BuildProfileError as e:
        log('✖ %s' % e)
        return 1

    try:
        write_version(core, code)
    except SystemExit:
        raise

    rc = 1
    started = time.time()
    try:
        with abi_pinned(abis):
            with signing_off():
                rc = run_build(mode)
    except BuildProfileError as e:
        log('✖ build-profile 配置异常：%s' % e)
        rc = 1

    elapsed = time.time() - started
    if rc != 0:
        if bumped:
            write_version(cur_core, cur_code)
            log('版本已回滚   : %s (versionCode %d)' % (cur_core, cur_code))
        return die('构建失败（exit=%d，耗时 %.1fs）' % (rc, elapsed), rc)

    hap = find_hap()
    if hap is None:
        return die('构建结束但找不到 HAP 产物（%s）' % HAP_OUT)

    try:
        sign_bytes = hap_sign_size(hap)
        sign_text = ('%d 字节签名块' % sign_bytes) if sign_bytes > 0 else '无签名块'
    except Exception:
        sign_text = '检测失败'
    log('签名实测     : %s' % sign_text)
    log('构建耗时     : %.1fs' % elapsed)

    if not args.no_verify:
        reason = verify_hap(hap, abis, core, code)
        if reason:
            return die('HAP 自检失败：%s（%s）' % (reason, hap.name))

    out_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(hap, target)
    digest = sha256_of(target)
    target.with_name(target.name + '.sha256').write_text(
        '%s  %s\n' % (digest, target.name), encoding='utf-8', newline='\n')
    log('产物已生成   : %s（%.2f MB）'
        % (target.name, target.stat().st_size / 1048576.0))
    log('SHA256       : %s' % digest)

    if args.mirror:
        mirror = Path(args.mirror).resolve()
        mirror.mkdir(parents=True, exist_ok=True)
        shutil.copy2(target, mirror / target.name)
        shutil.copy2(target.with_name(target.name + '.sha256'),
                     mirror / (target.name + '.sha256'))
        log('已镜像到     : %s' % mirror)

    if not args.no_record:
        record_build(core, code, abi_tag, target, digest, mode, elapsed,
                     args.note)
    prune(out_dir)

    if args.no_bump:
        log('--no-bump：版本保持 %s (versionCode %d)' % (core, code))
    else:
        log('版本已推进   : %s (versionCode %d) 已写入 AppScope/app.json5'
            % (core, code))

    log('★ 该包不含任何证书：侧载须自行重签，对外分发可直接提供此件')
    log('完成：%s' % target)
    return 0


if __name__ == '__main__':
    _code = 0
    try:
        try:
            sys.stdout.reconfigure(encoding='utf-8')
        except Exception:
            pass
        try:
            _code = main()
        except SystemExit as e:
            _code = e.code if isinstance(e.code, int) else (0 if e.code is None else 1)
        except KeyboardInterrupt:
            _code = 130
        except BaseException:
            import traceback
            traceback.print_exc()
            print('')
            print('✖ 未捕获异常，已中止（完整堆栈见上）')
            _code = 1
    finally:
        pause_before_exit(_code)
    sys.exit(_code)
