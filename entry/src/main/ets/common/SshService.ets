import {
  openProtocol, closeProtocol, sendTerminalCommand, resizeTerminal, registerDataCallback,
  startLocalPortForwarding, startRemotePortForwarding, startDynamicPortForwarding,
  stopPortForwarding, listActivePortForwardings,
  initSftp, listDirectory, deleteFile, createDirectory, deleteDirectory, rename,
  uploadFile, downloadFile, cancelTransfer, setLogging, LoggingConfig,
  getHostKeyFingerprint,
  execCommand,
  getForwardStats,
  SftpFileInfo, UploadProgressItem, DownloadProgressItem
} from 'libentry.so'
import { fileIo as fs } from '@kit.CoreFileKit'
import { common } from '@kit.AbilityKit'
import { util } from '@kit.ArkTS'
import { HostItem, TunnelItem, SftpEntry } from './Models'
import { AppLog } from './AppLog'

export interface UploadFileItem {
  fd: number
  name: string
}

export interface LogFileInfo {
  name: string
  path: string
  size: number
  mtime: number
}

function isSessionLog(name: string): boolean {
  return name.startsWith('skiff-') && name.endsWith('.log')
}

function fmtSize(b: number): string {
  if (b >= 1073741824) {
    return (b / 1073741824).toFixed(1) + ' GB'
  }
  if (b >= 1048576) {
    return (b / 1048576).toFixed(1) + ' MB'
  }
  if (b >= 1024) {
    return (b / 1024).toFixed(1) + ' KB'
  }
  return b.toString() + ' B'
}

function pad2(n: number): string {
  return n < 10 ? '0' + n.toString() : n.toString()
}

function fmtTime(raw: string): string {
  if (raw.length === 0) {
    return ''
  }
  const n: number = Number(raw)
  if (isNaN(n) || n <= 0) {
    return raw
  }
  const d: Date = new Date(n * 1000)
  return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate()) + ' ' +
    pad2(d.getHours()) + ':' + pad2(d.getMinutes())
}

function joinPath(dir: string, name: string): string {
  if (dir === '/' || dir.length === 0) {
    return '/' + name
  }
  return dir + '/' + name
}

export type SshDataHandler = (hostId: number, data: string, closed: boolean) => void

export type SessionLostHandler = (hostId: number, reason: string) => void

interface CfgSession {
  connectionType: string
  hostName: string
  port: string
  userName: string
  password: string
  authType: string
  privateKeyFd: number
  expectedHostKey: string
}

interface HostKeyInfo {
  fp: string
  type: string
}

interface CfgConnection {
  keepalive: number
  tcpOptions: string[]
  version: string
  SHHKeyHost: string
}

interface CfgTelnet {
  userName: string
  hostName: string
  port: string
  ambiguity: string
  mode: string
  keyboard: boolean
  telnetNewLine: boolean
}

interface CfgProxy {
  type: string
  host: string
  port: string
  username: string
  password: string
  enabled: boolean
}

interface ForwardInfo {
  port: number
  targetHost: string
  targetPort: number
  isRemote: boolean
  isDynamic: boolean
}

interface CfgRoot {
  Session: CfgSession
  Connection: CfgConnection
  Telnet: CfgTelnet
  Proxy: CfgProxy
}

function isKeyAuth(h: HostItem): boolean {
  return h.auth === '密钥' || h.auth.startsWith('私钥')
}

function uiBool(key: string, def: boolean): boolean {
  const v: boolean | undefined = AppStorage.get<boolean>(key)
  return v === undefined ? def : v
}

function uiStr(key: string, def: string): string {
  const v: string | undefined = AppStorage.get<string>(key)
  return v === undefined ? def : v
}

function buildConfig(h: HostItem, keyFd: number): string {
  const ka: boolean = uiBool('sk_keepAlive', true)
  const pon: boolean = uiBool('sk_proxyOn', false)
  const ptype: string = uiStr('sk_proxyType', 'socks5')
  const cfg: CfgRoot = {
    Session: {
      connectionType: 'SSH',
      hostName: h.host,
      port: h.port.toString(),
      userName: h.user,
      password: h.password,
      authType: isKeyAuth(h) ? 'publickey' : 'password',
      privateKeyFd: keyFd,
      expectedHostKey: h.hostKey
    },
    Connection: {
      keepalive: ka ? 30 : 0,
      tcpOptions: ['1', '2'],
      version: 'Auto',
      SHHKeyHost: ''
    },
    Telnet: {
      userName: '', hostName: '', port: '',
      ambiguity: 'BSD', mode: 'Active', keyboard: false, telnetNewLine: true
    },
    Proxy: {
      type: pon ? (ptype === 'http' ? 'HTTP' : 'SOCKS5') : 'None',
      host: pon ? uiStr('sk_proxyHost', '') : '',
      port: pon ? uiStr('sk_proxyPort', '') : '',
      username: '',
      password: '',
      enabled: pon
    }
  }
  return JSON.stringify(cfg)
}

export class SshService {
  private static inst: SshService | null = null
  private byHost: Map<number, number> = new Map<number, number>()
  private bySess: Map<number, number> = new Map<number, number>()
  private dataHandlers: SshDataHandler[] = []
  private lostHandlers: SessionLostHandler[] = []
  private chain: Promise<void> = Promise.resolve()
  private bound: boolean = false
  private started: Map<number, TunnelItem[]> = new Map<number, TunnelItem[]>()
  private sftpSids: Set<number> = new Set<number>()
  private logFds: Map<number, number> = new Map<number, number>()
  private rxLogged: Set<number> = new Set<number>()

  static get(): SshService {
    let s: SshService | null = SshService.inst
    if (s === null) {
      s = new SshService()
      SshService.inst = s
    }
    return s
  }

  private bind(): void {
    if (this.bound) {
      return
    }
    this.bound = true
    registerDataCallback((sid: number, data: string, closed: boolean) => {
      const hid: number | undefined = this.bySess.get(sid)
      if (hid === undefined) {
        return
      }
      if (closed) {
        AppLog.w('term', `rx closed sid=${sid} host=${hid}`)
      } else if (!this.rxLogged.has(sid)) {
        this.rxLogged.add(sid)
        AppLog.i('term', `rx first sid=${sid} host=${hid} len=${data.length}`)
      }
      this.dataHandlers.forEach((fn: SshDataHandler) => {
        fn(hid, data, closed)
      })
    })
  }

  onData(fn: SshDataHandler): void {
    this.bind()
    this.dataHandlers.push(fn)
  }

  offData(fn: SshDataHandler): void {
    const i: number = this.dataHandlers.indexOf(fn)
    if (i >= 0) {
      this.dataHandlers.splice(i, 1)
    }
  }

  onSessionLost(fn: SessionLostHandler): void {
    this.lostHandlers.push(fn)
  }

  offSessionLost(fn: SessionLostHandler): void {
    const i: number = this.lostHandlers.indexOf(fn)
    if (i >= 0) {
      this.lostHandlers.splice(i, 1)
    }
  }

  private reportLost(hostId: number, reason: string): void {
    const hs: SessionLostHandler[] = this.lostHandlers.slice()
    for (let i = 0; i < hs.length; i++) {
      try {
        hs[i](hostId, reason)
      } catch (e) {
      }
    }
  }

  sessionOf(hostId: number): number {
    const sid: number | undefined = this.byHost.get(hostId)
    return sid === undefined ? -1 : sid
  }

  exec(hostId: number, command: string): Promise<string> {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      this.reportLost(hostId, 'no session')
      return Promise.reject(new Error('NOT_CONNECTED'))
    }
    return execCommand(sid, command)
  }

  isAlive(hostId: number): boolean {
    return this.sessionOf(hostId) >= 0
  }

  forwardStats(hostId: number): string {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      return '0,0'
    }
    try {
      return getForwardStats(sid)
    } catch (e) {
      return '0,0'
    }
  }

  aliveCount(): number {
    return this.byHost.size
  }

  connect(h: HostItem): Promise<string> {
    this.bind()
    const job: () => Promise<string> = async (): Promise<string> => {
      const old: number = this.sessionOf(h.id)
      AppLog.i('ssh', `connect host=${h.host}:${h.port} user=${h.user} auth=${h.auth} prev=${old}`)
      if (old >= 0) {
        this.started.delete(h.id)
        try {
          const list: ForwardInfo[] = await listActivePortForwardings(old) as ForwardInfo[]
          for (let i = 0; i < list.length; i++) {
            try {
              await stopPortForwarding(old, list[i].port, list[i].isRemote)
            } catch (e) {
            }
          }
        } catch (e) {
        }
        this.byHost.delete(h.id)
        this.bySess.delete(old)
        this.sftpSids.delete(old)
        this.stopLog(h.id)
        try {
          await closeProtocol(old)
        } catch (e) {
        }
      }
      let keyFd: number = -1
      if (isKeyAuth(h)) {
        if (h.keyPath.length === 0) {
          throw new Error('MISSING_KEY')
        }
        try {
          keyFd = fs.openSync(h.keyPath, fs.OpenMode.READ_ONLY).fd
        } catch (e) {
          throw new Error('KEY_UNREADABLE')
        }
      }
      try {
        const sid: number = await openProtocol(buildConfig(h, keyFd))
        this.byHost.set(h.id, sid)
        this.bySess.set(sid, h.id)
        AppLog.i('ssh', `connected host=${h.id} sid=${sid}`)
        let fp: string = ''
        try {
          const raw: string = getHostKeyFingerprint(sid)
          if (raw.length > 0) {
            const info: HostKeyInfo = JSON.parse(raw) as HostKeyInfo
            fp = info.fp === undefined ? '' : info.fp
            if (fp.length > 0) {
              h.hostKeyType = info.type === undefined ? '' : info.type
            }
          }
        } catch (e) {
        }
        return fp
      } catch (e) {
        const msg: string = e instanceof Error ? e.message : String(e)
        AppLog.e('ssh', `connect failed host=${h.id}: ${msg}`)
        throw new Error(msg)
      } finally {
        if (keyFd >= 0) {
          try {
            fs.closeSync(keyFd)
          } catch (e) {
          }
        }
      }
    }
    const p: Promise<string> = this.chain.then(job, job)
    this.chain = p.then((): void => {}, (): void => {})
    return p
  }

  disconnect(hostId: number): Promise<void> {
    const sid: number = this.sessionOf(hostId)
    this.started.delete(hostId)
    AppLog.i('ssh', `disconnect host=${hostId} sid=${sid}`)
    if (sid < 0) {
      return Promise.resolve()
    }
    this.stopLog(hostId)
    const hs: SshDataHandler[] = this.dataHandlers.slice()
    for (let i = 0; i < hs.length; i++) {
      try {
        hs[i](hostId, '', true)
      } catch (e) {
      }
    }
    this.byHost.delete(hostId)
    this.bySess.delete(sid)
    this.sftpSids.delete(sid)
    const job: () => Promise<void> = async (): Promise<void> => {
      try {
        const list: ForwardInfo[] = await listActivePortForwardings(sid) as ForwardInfo[]
        for (let i = 0; i < list.length; i++) {
          try {
            await stopPortForwarding(sid, list[i].port, list[i].isRemote)
          } catch (e) {
          }
        }
      } catch (e) {
      }
      try {
        await new Promise<void>((resolve: () => void) => {
          setTimeout(() => {
            resolve()
          }, 400)
        })
      } catch (e) {
      }
      try {
        await closeProtocol(sid)
      } catch (e) {
      }
    }
    const p: Promise<void> = this.chain.then(job, job)
    this.chain = p.then((): void => {}, (): void => {})
    return p
  }

  send(hostId: number, cmd: string): void {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      AppLog.w('term', `send dropped: no session host=${hostId} len=${cmd.length}`)
      this.reportLost(hostId, 'no session')
      return
    }
    AppLog.i('term', `send host=${hostId} sid=${sid} len=${cmd.length}`)
    sendTerminalCommand(sid, cmd).catch((e: Error) => {
      AppLog.e('term', `send failed host=${hostId}: ${e.message}`)
      this.reportLost(hostId, 'send failed')
    })
  }

  resize(hostId: number, cols: number, rows: number): void {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      return
    }
    resizeTerminal(sid, cols, rows).catch((): void => {})
  }

  private sftpOf(hostId: number): number {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      throw new Error('NOT_CONNECTED')
    }
    if (!this.sftpSids.has(sid)) {
      const ok: boolean = initSftp(sid)
      if (!ok) {
        throw new Error('SFTP_INIT_FAILED')
      }
      this.sftpSids.add(sid)
    }
    return sid
  }

  sftpReady(hostId: number): boolean {
    try {
      this.sftpOf(hostId)
      return true
    } catch (e) {
      return false
    }
  }

  listDir(hostId: number, path: string): Promise<SftpEntry[]> {
    const sid: number = this.sftpOf(hostId)
    return new Promise<SftpEntry[]>((resolve: (v: SftpEntry[]) => void, reject: (e: Error) => void) => {
      listDirectory(sid, path, (err: Error | null, result: SftpFileInfo[]) => {
        if (err !== null) {
          reject(err)
          return
        }
        const out: SftpEntry[] = []
        for (let i = 0; i < result.length; i++) {
          const f: SftpFileInfo = result[i]
          out.push({
            name: f.name,
            isDir: f.isDirectory,
            size: f.isDirectory ? '' : fmtSize(f.size),
            time: fmtTime(f.mtime),
            path: joinPath(path, f.name),
            bytes: f.size
          })
        }
        resolve(out)
      })
    })
  }

  remove(hostId: number, path: string, isDir: boolean): Promise<void> {
    const sid: number = this.sftpOf(hostId)
    return new Promise<void>((resolve: () => void, reject: (e: Error) => void) => {
      const cb: (err: Error | null, ok: boolean) => void = (err: Error | null, ok: boolean) => {
        if (err !== null) {
          reject(err)
          return
        }
        if (!ok) {
          reject(new Error('SFTP_OP_FAILED'))
          return
        }
        resolve()
      }
      if (isDir) {
        deleteDirectory(sid, path, cb)
      } else {
        deleteFile(sid, path, cb)
      }
    })
  }

  makeDir(hostId: number, path: string): Promise<void> {
    const sid: number = this.sftpOf(hostId)
    return new Promise<void>((resolve: () => void, reject: (e: Error) => void) => {
      createDirectory(sid, path, (err: Error | null, ok: boolean) => {
        if (err !== null) {
          reject(err)
          return
        }
        if (!ok) {
          reject(new Error('SFTP_OP_FAILED'))
          return
        }
        resolve()
      })
    })
  }

  renamePath(hostId: number, oldPath: string, newPath: string): Promise<void> {
    const sid: number = this.sftpOf(hostId)
    return new Promise<void>((resolve: () => void, reject: (e: Error) => void) => {
      rename(sid, oldPath, newPath, (err: Error | null, ok: boolean) => {
        if (err !== null) {
          reject(err)
          return
        }
        if (!ok) {
          reject(new Error('SFTP_OP_FAILED'))
          return
        }
        resolve()
      })
    })
  }

  upload(hostId: number, files: UploadFileItem[], targetDir: string,
    onProgress: (pct: number, speed: string) => void): Promise<void> {
    const sid: number = this.sftpOf(hostId)
    return new Promise<void>((resolve: () => void, reject: (e: Error) => void) => {
      uploadFile(sid, files, targetDir,
        (err: Error | null, p: UploadProgressItem) => {
          if (err === null) {
            onProgress(p.progress, p.speed > 0 ? p.speed.toFixed(2) + ' MB/s' : '')
          }
        },
        (err: Error | null, r: UploadProgressItem) => {
          if (err !== null) {
            reject(err)
            return
          }
          resolve()
        })
    })
  }

  download(hostId: number, remotePath: string, fd: number,
    onProgress: (pct: number, speed: string) => void): Promise<void> {
    const sid: number = this.sftpOf(hostId)
    return new Promise<void>((resolve: () => void, reject: (e: Error) => void) => {
      downloadFile(sid, remotePath, fd,
        (err: Error | null, p: DownloadProgressItem) => {
          if (err === null) {
            onProgress(p.progress, p.speed > 0 ? p.speed.toFixed(2) + ' MB/s' : '')
          }
        },
        (err: Error | null, r: DownloadProgressItem | null) => {
          if (err !== null) {
            reject(err)
            return
          }
          resolve()
        })
    })
  }

  cancel(hostId: number): void {
    const sid: number = this.sessionOf(hostId)
    if (sid >= 0) {
      cancelTransfer(sid)
    }
  }

  logDirOf(ctx: common.Context): string {
    return ctx.filesDir + '/logs'
  }

  startLog(hostId: number, ctx: common.Context): void {
    if (!uiBool('sk_logOn', false)) {
      this.disableLog(hostId)
      return
    }
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      return
    }
    this.stopLog(hostId)
    const dir: string = this.logDirOf(ctx)
    try {
      fs.mkdirSync(dir)
    } catch (e) {
    }
    this.pruneLogs(dir)
    const path: string = dir + '/skiff-h' + hostId.toString() + '-' + Date.now().toString() + '.log'
    try {
      const f = fs.openSync(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.APPEND)
      const fd: number = f.fd
      this.logFds.set(hostId, fd)
      this.applyLogging(sid, fd)
    } catch (e) {
      AppLog.e('log', `start log failed host=${hostId}: ${e instanceof Error ? e.message : String(e)}`)
    }
  }

  private applyLogging(sid: number, fd: number): void {
    const cfg: LoggingConfig = {
      fd: fd,
      logType: AppStorage.get<number>('sk_logType') === 1 ? 1 : 2,
      existOperation: 1,
      includeHeader: true,
      omitKnownPassword: true,
      omitSessionData: false
    }
    setLogging(sid, cfg, (ok: boolean, err: string) => {
      if (!ok) {
        AppLog.e('log', `setLogging failed sid=${sid}: ${err}`)
      }
    })
  }

  disableLog(hostId: number): void {
    const sid: number = this.sessionOf(hostId)
    if (sid >= 0) {
      const off: LoggingConfig = {
        fd: -1, logType: 0, existOperation: 1,
        includeHeader: false, omitKnownPassword: true, omitSessionData: false
      }
      try {
        setLogging(sid, off, (ok: boolean, err: string) => {
          if (!ok) {
            AppLog.e('log', `disable log failed sid=${sid}: ${err}`)
          }
        })
      } catch (e) {
      }
    }
    this.stopLog(hostId)
  }

  refreshLogging(ctx: common.Context): void {
    const ids: number[] = Array.from(this.byHost.keys())
    for (let i = 0; i < ids.length; i++) {
      const id: number = ids[i]
      if (uiBool('sk_logOn', false)) {
        const sid: number = this.sessionOf(id)
        const fd: number | undefined = this.logFds.get(id)
        if (sid >= 0 && fd !== undefined) {
          this.applyLogging(sid, fd)
        } else {
          this.startLog(id, ctx)
        }
      } else {
        this.disableLog(id)
      }
    }
  }

  stopLog(hostId: number): void {
    const fd: number | undefined = this.logFds.get(hostId)
    if (fd === undefined) {
      return
    }
    this.logFds.delete(hostId)
    try {
      fs.closeSync(fd)
    } catch (e) {
    }
  }

  stopAllLogs(): void {
    const ids: number[] = Array.from(this.logFds.keys())
    for (let i = 0; i < ids.length; i++) {
      this.stopLog(ids[i])
    }
  }

  logFiles(ctx: common.Context): LogFileInfo[] {
    const dir: string = this.logDirOf(ctx)
    const out: LogFileInfo[] = []
    try {
      const names: string[] = fs.listFileSync(dir)
      for (let i = 0; i < names.length; i++) {
        if (!isSessionLog(names[i])) {
          continue
        }
        const p: string = dir + '/' + names[i]
        let size: number = 0
        let mt: number = 0
        try {
          const st: fs.Stat = fs.statSync(p)
          size = st.size
          mt = st.mtime * 1000
        } catch (e) {
        }
        out.push({ name: names[i], path: p, size: size, mtime: mt })
      }
    } catch (e) {
    }
    out.sort((a: LogFileInfo, b: LogFileInfo) => b.mtime - a.mtime)
    return out
  }

  pruneLogs(dir: string): void {
    const all: LogFileInfo[] = []
    try {
      const names: string[] = fs.listFileSync(dir)
      for (let i = 0; i < names.length; i++) {
        if (!isSessionLog(names[i])) {
          continue
        }
        const p: string = dir + '/' + names[i]
        let mt: number = 0
        try {
          mt = fs.statSync(p).mtime * 1000
        } catch (e) {
        }
        all.push({ name: names[i], path: p, size: 0, mtime: mt })
      }
    } catch (e) {
      return
    }
    if (all.length < 20) {
      return
    }
    all.sort((a: LogFileInfo, b: LogFileInfo) => a.mtime - b.mtime)
    const over: number = all.length - 19
    for (let i = 0; i < over; i++) {
      try {
        fs.unlinkSync(all[i].path)
      } catch (e) {
      }
    }
  }

  restartLogs(ctx: common.Context): void {
    const ids: number[] = Array.from(this.byHost.keys())
    for (let i = 0; i < ids.length; i++) {
      this.startLog(ids[i], ctx)
    }
  }

  purgeLogs(ctx: common.Context): number {
    this.stopAllLogs()
    const dir: string = this.logDirOf(ctx)
    let n: number = 0
    try {
      const names: string[] = fs.listFileSync(dir)
      for (let i = 0; i < names.length; i++) {
        if (!isSessionLog(names[i])) {
          continue
        }
        try {
          fs.unlinkSync(dir + '/' + names[i])
          n = n + 1
        } catch (e) {
        }
      }
    } catch (e) {
    }
    return n
  }

  readLog(path: string, maxBytes: number): string {
    try {
      const st: fs.Stat = fs.statSync(path)
      if (st.size <= 0) {
        return ''
      }
      let start: number = st.size > maxBytes ? st.size - maxBytes : 0
      if (start > 0) {
        start = start - 1
      }
      const len: number = st.size - start
      const f = fs.openSync(path, fs.OpenMode.READ_ONLY)
      const buf: ArrayBuffer = new ArrayBuffer(len)
      const n: number = fs.readSync(f.fd, buf, { offset: start, length: len })
      fs.closeSync(f.fd)
      const dec: util.TextDecoder = util.TextDecoder.create('utf-8', { fatal: false, ignoreBOM: true })
      let text: string = dec.decodeToString(new Uint8Array(buf, 0, n > 0 ? n : len))
      if (start > 0) {
        const nl: number = text.indexOf('\n')
        text = nl >= 0 ? text.substring(nl + 1) : ''
      }
      return text
    } catch (e) {
      return ''
    }
  }

  readLogHead(path: string, maxBytes: number): string {
    try {
      const st: fs.Stat = fs.statSync(path)
      const size: number = Math.min(st.size, maxBytes)
      if (size <= 0) {
        return ''
      }
      const f = fs.openSync(path, fs.OpenMode.READ_ONLY)
      const buf: ArrayBuffer = new ArrayBuffer(size)
      const n: number = fs.readSync(f.fd, buf)
      fs.closeSync(f.fd)
      const dec: util.TextDecoder = util.TextDecoder.create('utf-8', { fatal: false, ignoreBOM: true })
      return dec.decodeToString(new Uint8Array(buf, 0, n > 0 ? n : size))
    } catch (e) {
      return ''
    }
  }

  readLogRange(path: string, offset: number, length: number): string {
    try {
      const st: fs.Stat = fs.statSync(path)
      if (st.size <= 0 || offset >= st.size) {
        return ''
      }
      const len: number = Math.min(length, st.size - offset)
      if (len <= 0) {
        return ''
      }
      const f = fs.openSync(path, fs.OpenMode.READ_ONLY)
      const buf: ArrayBuffer = new ArrayBuffer(len)
      const n: number = fs.readSync(f.fd, buf, { offset: offset, length: len })
      fs.closeSync(f.fd)
      const dec: util.TextDecoder = util.TextDecoder.create('utf-8', { fatal: false, ignoreBOM: true })
      return dec.decodeToString(new Uint8Array(buf, 0, n > 0 ? n : len))
    } catch (e) {
      return ''
    }
  }

  async startForward(hostId: number, t: TunnelItem): Promise<void> {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      throw new Error('NOT_CONNECTED')
    }
    const port: number = Number(t.port)
    let ok: boolean = false
    if (t.type === 'D') {
      ok = await startDynamicPortForwarding(sid, port, false)
    } else {
      const parts: string[] = t.dest.split(':')
      const dhost: string = parts.length > 0 ? parts[0] : ''
      const dport: number = parts.length > 1 ? Number(parts[1]) : 0
      if (t.type === 'R') {
        ok = await startRemotePortForwarding(sid, port, dhost, dport)
      } else {
        ok = await startLocalPortForwarding(sid, port, dhost, dport, false)
      }
    }
    if (!ok) {
      AppLog.e('fwd', `start ${t.type}${t.port} -> ${t.dest} failed: listen rejected`)
      throw new Error('FORWARD_FAILED')
    }
    AppLog.i('fwd', `start ${t.type}${t.port} -> ${t.dest} ok`)
    const key: string = t.type + ':' + t.port
    const list: TunnelItem[] = this.started.get(hostId) ?? []
    let exists: boolean = false
    for (let i = 0; i < list.length; i++) {
      if (list[i].type + ':' + list[i].port === key) {
        exists = true
      }
    }
    if (!exists) {
      this.started.set(hostId, list.concat([t]))
    }
  }

  async stopForward(hostId: number, t: TunnelItem): Promise<void> {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      return
    }
    const key: string = t.type + ':' + t.port
    const list: TunnelItem[] = this.started.get(hostId) ?? []
    this.started.set(hostId, list.filter((x: TunnelItem) => x.type + ':' + x.port !== key))
    AppLog.i('fwd', `stop ${t.type}${t.port}`)
    await stopPortForwarding(sid, Number(t.port), t.type === 'R')
  }

  async syncForwards(hostId: number, tunnels: TunnelItem[]): Promise<number[]> {
    const failed: number[] = []
    for (let i = 0; i < tunnels.length; i++) {
      const t: TunnelItem = tunnels[i]
      if (!t.on) {
        continue
      }
      try {
        await this.startForward(hostId, t)
      } catch (e) {
        const msg: string = e instanceof Error ? e.message : String(e)
        AppLog.e('fwd', `restore ${t.type}${t.port} failed: ${msg}`)
        failed.push(t.id)
      }
    }
    return failed
  }

  async stopAllForwards(hostId: number): Promise<void> {
    const sid: number = this.sessionOf(hostId)
    if (sid < 0) {
      return
    }
    const list: TunnelItem[] = this.started.get(hostId) ?? []
    this.started.delete(hostId)
    for (let i = 0; i < list.length; i++) {
      try {
        await stopPortForwarding(sid, Number(list[i].port), list[i].type === 'R')
      } catch (e) {
      }
    }
  }

  disconnectAll(): void {
    const ids: number[] = Array.from(this.byHost.keys())
    ids.forEach((hid: number) => {
      this.disconnect(hid)
    })
  }
}
