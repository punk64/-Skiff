import { common } from '@kit.AbilityKit'
import { fileIo as fs, picker } from '@kit.CoreFileKit'
import { util } from '@kit.ArkTS'
import { HostItem, TunnelItem } from './Models'
import { UiPrefs, loadUiPrefsSync, saveHosts } from './Store'
import { keyDir } from './KeyStore'
import { APP_VERSION } from './AppInfo'
import { sealBackup, openBackup } from 'libentry.so'

export interface BackupMergeResult {
  added: number
  skipped: number
  keys: number
}

export interface BackupKey {
  name: string
  pem: string
}

interface BackupTunnel {
  type: string
  port: string
  dest: string
  on: boolean
}

interface BackupHost {
  name: string
  user: string
  host: string
  port: number
  auth: string
  secret: string
  keyFile: string
  hostKey: string
  hostKeyType: string
  registeredAt: string
  expireAt: string
  lastConnected: string
  autoConnect: boolean
  tunnels: BackupTunnel[]
}

interface BackupSource {
  platform: string
  version: string
}

interface BackupUi {
  theme: string
  lang: string
  termFontSize: number
  autoReconnect: boolean
  autoConnectAll: boolean
  fpCheck: boolean
  keepAlive: boolean
}

export interface BackupDoc {
  format: string
  schema: number
  createdAt: string
  source: BackupSource
  keys: BackupKey[]
  hosts: BackupHost[]
  ui: BackupUi
}

function stampText(ms: number): string {
  const d: Date = new Date(ms)
  const p = (n: number): string => n < 10 ? '0' + n.toString() : n.toString()
  return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate()) + ' ' +
    p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds())
}

function baseName(p: string): string {
  const i: number = Math.max(p.lastIndexOf('/'), p.lastIndexOf('\\'))
  return i >= 0 ? p.substring(i + 1) : p
}

function isKeyAuth(h: HostItem): boolean {
  return h.auth === '密钥' || h.auth.startsWith('私钥')
}

function readText(path: string): string {
  let f: fs.File | null = null
  try {
    f = fs.openSync(path, fs.OpenMode.READ_ONLY)
    const buf: ArrayBuffer = new ArrayBuffer(1048576)
    const n: number = fs.readSync(f.fd, buf)
    if (n <= 0) {
      return ''
    }
    const dec: util.TextDecoder = util.TextDecoder.create('utf-8')
    return dec.decodeToString(new Uint8Array(buf, 0, n))
  } catch (e) {
    return ''
  } finally {
    if (f !== null) {
      try {
        fs.closeSync(f)
      } catch (e) {
      }
    }
  }
}

function writeText(path: string, text: string): boolean {
  let f: fs.File | null = null
  try {
    f = fs.openSync(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC)
    fs.writeSync(f.fd, text)
    return true
  } catch (e) {
    return false
  } finally {
    if (f !== null) {
      try {
        fs.closeSync(f)
      } catch (e) {
      }
    }
  }
}

export function backupFileName(): string {
  const d: Date = new Date()
  const p = (n: number): string => n < 10 ? '0' + n.toString() : n.toString()
  return 'Skiff-backup-' + d.getFullYear() + p(d.getMonth() + 1) + p(d.getDate()) + '-' +
    p(d.getHours()) + p(d.getMinutes()) + '.skbk'
}

export function buildBackupDoc(ctx: common.UIAbilityContext, hosts: HostItem[]): BackupDoc {
  const keys: BackupKey[] = []
  const names: string[] = []
  const list: BackupHost[] = []
  const prefs: UiPrefs = loadUiPrefsSync(ctx)

  hosts.forEach((h: HostItem) => {
    const keyAuth: boolean = isKeyAuth(h)
    let keyFile: string = ''
    if (keyAuth && h.keyPath.length > 0) {
      const pem: string = readText(h.keyPath)
      if (pem.length > 0) {
        keyFile = baseName(h.keyPath)
        if (names.indexOf(keyFile) < 0) {
          names.push(keyFile)
          keys.push({ name: keyFile, pem: pem })
        }
      }
    }
    const tunnels: BackupTunnel[] = h.tunnels.map((t: TunnelItem) => {
      const item: BackupTunnel = { type: t.type, port: t.port, dest: t.dest, on: t.on }
      return item
    })
    list.push({
      name: h.name, user: h.user, host: h.host, port: h.port,
      auth: keyAuth ? 'key' : 'password',
      secret: h.password,
      keyFile: keyFile,
      hostKey: h.hostKey, hostKeyType: h.hostKeyType,
      registeredAt: h.registeredAt, expireAt: h.expireAt, lastConnected: '',
      autoConnect: h.autoConnect,
      tunnels: tunnels
    })
  })

  return {
    format: 'skiff-backup',
    schema: 1,
    createdAt: stampText(Date.now()),
    source: { platform: 'mobile', version: APP_VERSION },
    keys: keys,
    hosts: list,
    ui: {
      theme: prefs.themeDark ? 'dark' : 'light',
      lang: prefs.lang,
      termFontSize: prefs.termFont,
      autoReconnect: prefs.autoReconnect,
      autoConnectAll: false,
      fpCheck: true,
      keepAlive: prefs.keepAlive
    }
  }
}

export function mergeBackupDoc(ctx: common.UIAbilityContext, text: string,
  hosts: HostItem[]): BackupMergeResult {
  const doc: BackupDoc = JSON.parse(text) as BackupDoc
  const stat: BackupMergeResult = { added: 0, skipped: 0, keys: 0 }
  if (doc.format !== 'skiff-backup' || !Array.isArray(doc.hosts)) {
    throw new Error('BAD_BACKUP')
  }

  const dir: string = keyDir(ctx)
  const mapped: Map<string, string> = new Map<string, string>()
  const keys: BackupKey[] = Array.isArray(doc.keys) ? doc.keys : []
  keys.forEach((k: BackupKey) => {
    if (k.name.length === 0 || k.pem.length === 0) {
      return
    }
    let target: string = k.name
    let round: number = 1
    while (fs.accessSync(dir + '/' + target)) {
      if (readText(dir + '/' + target) === k.pem) {
        break
      }
      round++
      if (round > 40) {
        return
      }
      target = k.name + '-' + round
    }
    if (!fs.accessSync(dir + '/' + target)) {
      if (!writeText(dir + '/' + target, k.pem)) {
        return
      }
    }
    mapped.set(k.name, target)
    stat.keys++
  })

  const seen: string[] = hosts.map((h: HostItem) => `${h.user}@${h.host}:${h.port}`.toLowerCase())
  let nextId: number = 1
  let nextTun: number = 1
  hosts.forEach((h: HostItem) => {
    if (h.id >= nextId) {
      nextId = h.id + 1
    }
    h.tunnels.forEach((t: TunnelItem) => {
      if (t.id >= nextTun) {
        nextTun = t.id + 1
      }
    })
  })

  doc.hosts.forEach((s: BackupHost) => {
    const user: string = s.user.length === 0 ? 'root' : s.user
    const port: number = s.port > 0 && s.port < 65536 ? s.port : 22
    const id: string = `${user}@${s.host}:${port}`.toLowerCase()
    if (seen.indexOf(id) >= 0) {
      stat.skipped++
      return
    }
    const keyAuth: boolean = s.auth === 'key'
    let keyPath: string = ''
    if (keyAuth && s.keyFile.length > 0) {
      const target: string | undefined = mapped.get(s.keyFile)
      if (target !== undefined) {
        keyPath = dir + '/' + target
      }
    }
    const tunnels: TunnelItem[] = (Array.isArray(s.tunnels) ? s.tunnels : []).map((t: BackupTunnel) => {
      const item: TunnelItem = {
        id: nextTun++, type: t.type.length === 0 ? 'L' : t.type,
        port: t.port, dest: t.dest === undefined ? '' : t.dest,
        on: t.on === undefined ? true : t.on
      }
      return item
    })
    const item: HostItem = {
      id: nextId++, name: s.name.length === 0 ? (s.host.length === 0 ? '导入主机' : s.host) : s.name,
      user: user, host: s.host, port: port,
      auth: keyAuth ? '密钥' : '密码',
      password: s.secret === undefined ? '' : s.secret,
      keyFd: -1, keyPath: keyPath,
      hostKey: s.hostKey === undefined ? '' : s.hostKey,
      hostKeyType: s.hostKeyType === undefined ? '' : s.hostKeyType,
      registeredAt: s.registeredAt === undefined ? '' : s.registeredAt,
      expireAt: s.expireAt === undefined ? '' : s.expireAt,
      autoConnect: s.autoConnect === undefined ? false : s.autoConnect,
      latency: '', online: false, state: 'idle',
      stats: { cpu: 0, ram: 0, net: '', netp: 0 },
      tunnels: tunnels
    }
    seen.push(id)
    hosts.push(item)
    stat.added++
  })

  return stat
}

export function sealPayload(payload: string, password: string, encrypt: boolean): string {
  return sealBackup(payload, password, encrypt)
}

export function openPayload(text: string, password: string): string {
  return openBackup(text, password)
}

export async function exportToFile(ctx: common.UIAbilityContext, text: string): Promise<string> {
  const dp: picker.DocumentViewPicker = new picker.DocumentViewPicker(ctx)
  const opt: picker.DocumentSaveOptions = new picker.DocumentSaveOptions()
  opt.newFileNames = [backupFileName()]
  const uris: string[] = await dp.save(opt)
  if (uris.length === 0) {
    return ''
  }
  let f: fs.File | null = null
  try {
    f = fs.openSync(uris[0], fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC)
    fs.writeSync(f.fd, text)
    return uris[0]
  } finally {
    if (f !== null) {
      try {
        fs.closeSync(f)
      } catch (e) {
      }
    }
  }
}

export async function importFromFile(ctx: common.UIAbilityContext): Promise<string> {
  const dp: picker.DocumentViewPicker = new picker.DocumentViewPicker(ctx)
  const opt: picker.DocumentSelectOptions = new picker.DocumentSelectOptions()
  opt.maxSelectNumber = 1
  const uris: string[] = await dp.select(opt)
  if (uris.length === 0) {
    return ''
  }
  return readText(uris[0])
}

export async function persistHosts(ctx: common.UIAbilityContext, hosts: HostItem[]): Promise<void> {
  await saveHosts(ctx, hosts)
}
