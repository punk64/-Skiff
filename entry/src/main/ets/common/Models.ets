export interface TunnelItem {
  id: number
  type: string
  port: string
  dest: string
  on: boolean
}

export interface HostStats {
  cpu: number
  ram: number
  net: string
  netp: number
}

export interface HostItem {
  id: number
  name: string
  user: string
  host: string
  port: number
  auth: string
  password: string
  keyFd: number
  keyPath: string
  hostKey: string
  hostKeyType: string
  registeredAt: string
  expireAt: string
  autoConnect: boolean
  latency: string
  online: boolean
  state: string
  stats: HostStats
  tunnels: TunnelItem[]
}

export function cloneHost(h: HostItem, tunnels: TunnelItem[]): HostItem {
  return {
    id: h.id, name: h.name, user: h.user, host: h.host, port: h.port,
    auth: h.auth, password: h.password, keyFd: h.keyFd, keyPath: h.keyPath,
    hostKey: h.hostKey, hostKeyType: h.hostKeyType,
    registeredAt: h.registeredAt, expireAt: h.expireAt, autoConnect: h.autoConnect,
    latency: h.latency, online: h.online, state: h.state,
    stats: h.stats, tunnels: tunnels
  }
}

export function emptyHost(id: number, name: string): HostItem {
  return {
    id: id, name: name, user: 'root', host: '', port: 22,
    auth: '密码', password: '', keyFd: -1, keyPath: '',
    hostKey: '', hostKeyType: '',
    registeredAt: '', expireAt: '', autoConnect: false,
    latency: '', online: false, state: 'idle',
    stats: { cpu: 0, ram: 0, net: '', netp: 0 }, tunnels: []
  }
}

export interface TerminalTab {
  id: number
  title: string
  lines: string[]
}

export interface SftpEntry {
  name: string
  isDir: boolean
  size: string
  time: string
  path: string
  bytes: number
}

export interface TransferTask {
  id: number
  name: string
  up: boolean
  pct: number
  speed: string
  state: string
  msg: string
}

export function tunnelLabel(t: TunnelItem): string {
  return t.type + t.port
}

export function tunnelHint(t: TunnelItem): string {
  if (t.type === 'D') {
    return 'SOCKS5'
  }
  return t.dest
}

export function tunnelCmdLine(h: HostItem): string {
  let s: string = 'ssh'
  h.tunnels.forEach((t: TunnelItem) => {
    if (!t.on) {
      return
    }
    if (t.type === 'D') {
      s += ' -D ' + t.port
    } else {
      s += ' -' + t.type + ' ' + t.port + ':' + t.dest
    }
  })
  return s + ' ' + h.user + '@' + h.host
}
