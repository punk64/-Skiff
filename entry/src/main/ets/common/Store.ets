import { preferences } from '@kit.ArkData'
import { common } from '@kit.AbilityKit'
import { HostItem, TunnelItem } from './Models'

const FILE: string = 'skiff_store'
const KEY_HOSTS: string = 'hosts'
const KEY_UI: string = 'ui'

export interface UiPrefs {
  themeDark: boolean
  lang: string
  termFont: number
  keepAlive: boolean
  bgTunnel: boolean
  autoReconnect: boolean
  logOn: boolean
  logType: number
  proxyOn: boolean
  proxyType: string
  proxyHost: string
  proxyPort: string
}

interface PersistHost {
  id: number
  name: string
  user: string
  host: string
  port: number
  auth: string
  password: string
  keyPath: string
  hostKey: string
  hostKeyType: string
  tunnels: TunnelItem[]
}

let store: preferences.Preferences | null = null

async function get(ctx: common.Context): Promise<preferences.Preferences> {
  if (store !== null) {
    return store
  }
  const p: preferences.Preferences = await preferences.getPreferences(ctx, FILE)
  store = p
  return p
}

function toHost(s: PersistHost): HostItem {
  return {
    id: s.id, name: s.name, user: s.user, host: s.host, port: s.port,
    auth: s.auth, password: s.password, keyFd: -1, keyPath: s.keyPath,
    hostKey: s.hostKey === undefined ? '' : s.hostKey,
    hostKeyType: s.hostKeyType === undefined ? '' : s.hostKeyType,
    latency: '', online: false, state: 'idle',
    stats: { cpu: 0, ram: 0, net: '', netp: 0 },
    tunnels: s.tunnels
  }
}

function toPersist(h: HostItem): PersistHost {
  return {
    id: h.id, name: h.name, user: h.user, host: h.host, port: h.port,
    auth: h.auth, password: h.password, keyPath: h.keyPath,
    hostKey: h.hostKey, hostKeyType: h.hostKeyType,
    tunnels: h.tunnels
  }
}

export async function loadHosts(ctx: common.Context): Promise<HostItem[]> {
  const p: preferences.Preferences = await get(ctx)
  const raw: preferences.ValueType = await p.get(KEY_HOSTS, '')
  const text: string = raw as string
  if (text.length === 0) {
    return []
  }
  const saved: PersistHost[] = JSON.parse(text) as PersistHost[]
  return saved.map((s: PersistHost) => toHost(s))
}

export async function saveHosts(ctx: common.Context, list: HostItem[]): Promise<void> {
  const p: preferences.Preferences = await get(ctx)
  const slim: PersistHost[] = list.map((h: HostItem) => toPersist(h))
  await p.put(KEY_HOSTS, JSON.stringify(slim))
  await p.flush()
}

export function loadUiPrefsSync(ctx: common.Context): UiPrefs {
  const out: UiPrefs = {
    themeDark: true, lang: 'zh', termFont: 14,
    keepAlive: true, bgTunnel: true, autoReconnect: false,
    logOn: false, logType: 2,
    proxyOn: false, proxyType: 'socks5', proxyHost: '127.0.0.1', proxyPort: '1080'
  }
  try {
    const p: preferences.Preferences = preferences.getPreferencesSync(ctx, { name: FILE })
    const raw: preferences.ValueType = p.getSync(KEY_UI, '')
    const text: string = raw as string
    if (text.length === 0) {
      return out
    }
    const s: UiPrefs = JSON.parse(text) as UiPrefs
    out.themeDark = s.themeDark ?? true
    out.lang = s.lang ?? 'zh'
    out.termFont = s.termFont ?? 14
    out.keepAlive = s.keepAlive ?? true
    out.bgTunnel = s.bgTunnel ?? true
    out.autoReconnect = s.autoReconnect ?? false
    out.logOn = s.logOn ?? false
    out.logType = s.logType ?? 2
    out.proxyOn = s.proxyOn ?? false
    out.proxyType = s.proxyType ?? 'socks5'
    out.proxyHost = s.proxyHost ?? '127.0.0.1'
    out.proxyPort = s.proxyPort ?? '1080'
  } catch (e) {
  }
  return out
}

export async function saveUiPrefs(ctx: common.Context, prefs: UiPrefs): Promise<void> {
  try {
    const p: preferences.Preferences = await get(ctx)
    await p.put(KEY_UI, JSON.stringify(prefs))
    await p.flush()
  } catch (e) {
  }
}
