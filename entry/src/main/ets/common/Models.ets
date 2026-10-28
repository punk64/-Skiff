import { L } from './I18n'

function items(n: number): string {
  return L.lang === 'en' ? `${n} items` : `${n} 项`
}

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
  latency: string
  online: boolean
  state: string
  stats: HostStats
  tunnels: TunnelItem[]
}

export function cloneHost(h: HostItem, tunnels: TunnelItem[]): HostItem {
  return {
    id: h.id, name: h.name, user: h.user, host: h.host, port: h.port,
    auth: h.auth, latency: h.latency, online: h.online, state: h.state,
    stats: h.stats, tunnels: tunnels
  }
}

export function rollStats(): HostStats {
  const r = (a: number, b: number): number => Math.round(a + Math.random() * (b - a))
  return {
    cpu: r(5, 75),
    ram: r(25, 88),
    net: (r(3, 90) / 10).toFixed(1) + ' MB/s',
    netp: r(10, 75)
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
}

export interface TransferTask {
  id: number
  name: string
  up: boolean
  isDir: boolean
  total: number
  done: number
  files: number
  filesDone: number
  state: string
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

export function seedHosts(): HostItem[] {
  return [
    {
      id: 1, name: 'Ubuntu Cloud', user: 'ubuntu', host: '47.100.23.8', port: 22,
      auth: '密码', latency: '', online: false, state: 'idle', stats: rollStats(),
      tunnels: [
        { id: 101, type: 'L', port: '8888', dest: '127.0.0.1:8888', on: true },
        { id: 102, type: 'D', port: '1080', dest: '', on: false }
      ]
    },
    {
      id: 2, name: 'Prod Bastion', user: 'ops', host: '10.8.0.21', port: 22,
      auth: '密码', latency: '', online: false, state: 'idle', stats: rollStats(), tunnels: []
    },
    {
      id: 3, name: 'Home NAS', user: 'admin', host: '192.168.1.30', port: 2222,
      auth: '私钥 id_ed25519', latency: '', online: false, state: 'idle', stats: rollStats(),
      tunnels: [
        { id: 301, type: 'L', port: '5000', dest: '127.0.0.1:5000', on: true }
      ]
    }
  ]
}

export function seedSftp(cwd: string): SftpEntry[] {
  if (cwd === '/') {
    return [
      { name: 'home', isDir: true, size: items(4), time: 'Oct 28 15:41' },
      { name: 'etc', isDir: true, size: items(128), time: 'Oct 20 09:12' },
      { name: 'var', isDir: true, size: items(36), time: 'Oct 27 22:03' },
      { name: 'docker-compose.yml', isDir: false, size: '1.8 KB', time: 'Oct 26 11:20' },
      { name: 'app.tar.gz', isDir: false, size: '12.4 MB', time: 'Oct 24 18:02' }
    ]
  }
  if (cwd === '/home') {
    return [
      { name: 'ubuntu', isDir: true, size: items(12), time: 'Oct 28 15:41' }
    ]
  }
  return [
    { name: 'logs', isDir: true, size: items(3), time: 'Oct 28 14:55' },
    { name: '.bashrc', isDir: false, size: '3.1 KB', time: 'Sep 12 08:30' },
    { name: '1.pem', isDir: false, size: '1.7 KB', time: 'Oct 18 10:11' },
    { name: 'deploy.sh', isDir: false, size: '842 B', time: 'Oct 27 19:26' },
    { name: 'screenshot.png', isDir: false, size: '1.8 MB', time: 'Oct 25 16:40' }
  ]
}
