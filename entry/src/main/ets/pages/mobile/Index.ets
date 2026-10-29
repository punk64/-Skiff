import { window, promptAction } from '@kit.ArkUI'
import { common, ConfigurationConstant } from '@kit.AbilityKit'
import { inputMethod } from '@kit.IMEKit'
import { MT } from '../../common/MobileTheme'
import { HostItem, cloneHost, emptyHost } from '../../common/Models'
import { loadHosts, saveHosts } from '../../common/Store'
import { SshService, SshDataHandler, SessionLostHandler } from '../../common/SshService'
import { Ic } from '../../common/Icons'
import { HostsPage } from './HostsPage'
import { HostSettingsPage } from './HostSettingsPage'
import { TunnelsPage } from './TunnelsPage'
import { FilesPage } from './FilesPage'
import { SettingsPage } from './SettingsPage'
import { TerminalPro } from './TerminalPro'
import { BrowserPage } from './BrowserPage'
import { L } from '../../common/I18n'
import { AppLog } from '../../common/AppLog'

interface StatSnap {
  total: number
  idle: number
  bytes: number
  ts: number
}

const STAT_CMD: string =
  'head -1 /proc/stat; echo "###"; grep -E "^(MemTotal|MemAvailable):" /proc/meminfo; echo "###"; cat /proc/net/dev'

@Entry
@Component
struct MobileIndex {
  @StorageLink('csx_lang') csxLang: string = 'zh'
  @State view: number = 0
  @State sessTab: number = 0
  @State hosts: HostItem[] = []
  @State activeId: number = 0
  @State editId: number = 0
  @State termShow: boolean = false
  @State showBrowser: boolean = false
  @State browserUrl: string = ''
  @State masking: boolean = false
  @State sessionSeq: number = 0
  @State filesView: string = 'browse'
  @State filesDlg: string = ''
  @State tunSheet: boolean = false
  @State setSub: string = ''
  @State pendingHost: HostItem | null = null
  @StorageLink('csx_themeDark') themeDark: boolean = true
  @StorageLink('sk_fgTick') @Watch('onFgTick') fgTick: number = 0
  private retry: Map<number, number> = new Map<number, number>()
  private statTimer: number = -1
  private statSnap: Map<number, StatSnap> = new Map<number, StatSnap>()
  private statFail: Map<number, number> = new Map<number, number>()
  private statSkip: Map<number, number> = new Map<number, number>()
  private dataFn: SshDataHandler | null = null
  private lostFn: SessionLostHandler | null = null
  private hbTimer: number = -1
  private verifying: Set<number> = new Set<number>()
  private connSeq: Map<number, number> = new Map<number, number>()

  t(zh: string, en: string): string {
    return this.csxLang === 'en' ? en : zh
  }

  aboutToAppear(): void {
    L.lang = AppStorage.get<string>('csx_lang') ?? 'zh'
    const ctx = getContext(this) as common.UIAbilityContext
    ctx.getApplicationContext().setColorMode(this.themeDark ? ConfigurationConstant.ColorMode.COLOR_MODE_DARK : ConfigurationConstant.ColorMode.COLOR_MODE_LIGHT)
    this.applyBars()
    this.loadStoredHosts()
    this.startStats()
    const fn: SshDataHandler = (hid: number, data: string, closed: boolean) => {
      if (closed && this.isConnected(hid)) {
        AppLog.w('ui', `session closed by remote host=${hid}`)
        const reason: string = this.t('连接已断开', 'Connection closed')
        if (this.markLost(hid, reason)) {
          promptAction.showToast({ message: reason })
        }
      }
    }
    this.dataFn = fn
    SshService.get().onData(fn)
    const lost: SessionLostHandler = (hid: number, reason: string) => {
      this.verifyHost(hid, reason)
    }
    this.lostFn = lost
    SshService.get().onSessionLost(lost)
    this.startHeartbeat()
  }

  aboutToDisappear(): void {
    this.stopStats()
    this.stopHeartbeat()
    if (this.lostFn !== null) {
      SshService.get().offSessionLost(this.lostFn)
      this.lostFn = null
    }
    if (this.dataFn !== null) {
      SshService.get().offData(this.dataFn)
      this.dataFn = null
    }
    SshService.get().disconnectAll()
  }

  onBackPress(): boolean {
    if (this.showBrowser) {
      this.showBrowser = false
      return true
    }
    if (this.view === 2) {
      this.pendingHost = null
      this.view = 0
      return true
    }
    if (this.view === 3 && this.setSub.length > 0) {
      this.setSub = ''
      return true
    }
    if (this.view === 2 || this.view === 3) {
      this.view = 0
      return true
    }
    if (this.view === 1) {
      if (this.sessTab === 2) {
        if (this.filesDlg.length > 0) {
          this.filesDlg = ''
          return true
        }
        if (this.filesView !== 'browse') {
          this.filesView = 'browse'
          return true
        }
      }
      if (this.sessTab === 1 && this.tunSheet) {
        this.tunSheet = false
        this.masking = false
        return true
      }
      this.hideKeyboard()
      this.view = 0
      return true
    }
    return false
  }

  ctx(): common.Context {
    return getContext(this) as common.Context
  }

  loadStoredHosts(): void {
    loadHosts(this.ctx()).then((list: HostItem[]) => {
      this.hosts = list
    }).catch(() => {
      this.hosts = []
    })
  }

  persistHosts(): void {
    saveHosts(this.ctx(), this.hosts).catch(() => {
    })
  }

  isConnected(id: number): boolean {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    return h !== undefined && h.state === 'connected'
  }

  applyBars(): void {
    const dark: boolean = this.themeDark
    window.getLastWindow(getContext(this)).then((w: window.Window) => {
      w.setWindowSystemBarProperties({
        statusBarColor: dark ? '#08090C' : '#F4F6F9',
        statusBarContentColor: dark ? '#E8EDF5' : '#0F141B',
        navigationBarColor: dark ? '#08090C' : '#F4F6F9',
        navigationBarContentColor: dark ? '#78849B' : '#646F80'
      })
    }).catch(() => {
    })
  }

  curHost(): HostItem | undefined {
    return this.hosts.find((h: HostItem) => h.id === this.activeId)
  }

  activeName(): string {
    const h: HostItem | undefined = this.curHost()
    return h === undefined ? this.t('未连接', 'Not connected') : h.name
  }

  activeAddr(): string {
    const h: HostItem | undefined = this.curHost()
    return h === undefined ? '' : `${h.user}@${h.host}`
  }

  connected(): boolean {
    const h: HostItem | undefined = this.curHost()
    return h !== undefined && h.state === 'connected'
  }

  connecting(): boolean {
    const h: HostItem | undefined = this.curHost()
    return h !== undefined && h.state === 'connecting'
  }

  setState(id: number, state: string): void {
    this.hosts = this.hosts.map((h: HostItem) => {
      if (h.id !== id) {
        return h
      }
      const nh: HostItem = cloneHost(h, h.tunnels)
      nh.state = state
      if (state === 'connected') {
        nh.online = true
      } else {
        nh.online = false
        nh.latency = ''
        nh.stats = { cpu: 0, ram: 0, net: '', netp: 0 }
        this.statSnap.delete(id)
        this.statFail.delete(id)
        this.statSkip.delete(id)
      }
      return nh
    })
  }

  startStats(): void {
    if (this.statTimer >= 0) {
      return
    }
    this.statTimer = setInterval(() => {
      this.pollStats()
    }, 5000)
  }

  onFgTick(): void {
    if (this.fgTick <= 0) {
      return
    }
    this.reconcile()
    if (this.view === 0) {
      this.startStats()
      this.pollStats()
    }
  }

  reconcile(): void {
    this.reconcileProbe()
  }

  markLost(id: number, reason: string): boolean {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    if (h === undefined || h.state !== 'connected') {
      return false
    }
    AppLog.w('ssh', `mark lost host=${id} reason=${reason}`)
    this.setState(id, 'idle')
    SshService.get().disconnect(id)
    if (AppStorage.get<boolean>('sk_autoReconnect') ?? false) {
      this.scheduleReconnect(id)
    }
    return true
  }

  lostToast(id: number, reason: string): void {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    const name: string = h === undefined ? 'host' : h.name
    promptAction.showToast({
      message: this.t(`${name} 连接已断开（${reason}）`, `${name} disconnected (${reason})`),
      duration: 4000
    })
  }

  async verifyHost(id: number, reason: string): Promise<void> {
    if (!this.isConnected(id) || this.verifying.has(id)) {
      return
    }
    this.verifying.add(id)
    try {
      const ok: boolean = await this.pingHost(id)
      if (!ok && this.markLost(id, reason)) {
        this.lostToast(id, reason)
      }
    } finally {
      this.verifying.delete(id)
    }
  }

  async probeHostNow(id: number): Promise<void> {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    if (h === undefined) {
      return
    }
    if (h.state !== 'connected') {
      promptAction.showToast({ message: this.t(`${h.name} 当前未连接`, `${h.name} is not connected`) })
      return
    }
    if (this.verifying.has(id)) {
      return
    }
    this.verifying.add(id)
    let ok: boolean = false
    try {
      ok = await this.pingHost(id)
    } finally {
      this.verifying.delete(id)
    }
    if (ok) {
      promptAction.showToast({ message: this.t(`${h.name} 连接正常`, `${h.name} is alive`) })
      return
    }
    const reason: string = this.t('手动探活失败', 'manual probe failed')
    if (this.markLost(id, reason)) {
      this.lostToast(id, reason)
    }
  }

  startHeartbeat(): void {
    if (this.hbTimer >= 0) {
      return
    }
    this.hbTimer = setInterval(() => {
      this.heartbeat()
    }, 20000)
  }

  stopHeartbeat(): void {
    if (this.hbTimer >= 0) {
      clearInterval(this.hbTimer)
      this.hbTimer = -1
    }
  }

  async heartbeat(): Promise<void> {
    const list: HostItem[] = this.hosts.filter((h: HostItem) => h.state === 'connected')
    for (let i = 0; i < list.length; i++) {
      const id: number = list[i].id
      if (!this.isConnected(id) || this.verifying.has(id)) {
        continue
      }
      this.verifying.add(id)
      let ok: boolean = false
      try {
        ok = await this.pingHost(id)
      } finally {
        this.verifying.delete(id)
      }
      if (!ok && this.isConnected(id)) {
        const reason: string = this.t('心跳超时', 'heartbeat timeout')
        if (this.markLost(id, reason)) {
          this.lostToast(id, reason)
        }
      }
    }
  }

  pingHost(id: number): Promise<boolean> {
    return new Promise<boolean>((resolve: (v: boolean) => void) => {
      let done: boolean = false
      const timer: number = setTimeout(() => {
        if (done) {
          return
        }
        done = true
        resolve(false)
      }, 4000)
      SshService.get().exec(id, 'echo ok').then((out: string) => {
        if (done) {
          return
        }
        done = true
        clearTimeout(timer)
        resolve(out.indexOf('ok') >= 0)
      }).catch(() => {
        if (done) {
          return
        }
        done = true
        clearTimeout(timer)
        resolve(false)
      })
    })
  }

  async reconcileProbe(): Promise<void> {
    const list: HostItem[] = this.hosts.filter((h: HostItem) => h.state === 'connected')
    const dead: number[] = []
    for (let i = 0; i < list.length; i++) {
      const h: HostItem = list[i]
      if (SshService.get().sessionOf(h.id) < 0) {
        dead.push(h.id)
        AppLog.w('ssh', `probe host=${h.id} session gone`)
        continue
      }
      const ok: boolean = await this.pingHost(h.id)
      AppLog.i('ssh', `probe host=${h.id} ${ok ? 'alive' : 'dead'}`)
      if (!ok) {
        dead.push(h.id)
      }
    }
    for (let i = 0; i < dead.length; i++) {
      this.markLost(dead[i], this.t('探活失败', 'probe failed'))
    }
    if (dead.length > 0) {
      promptAction.showToast({
        message: this.t(`有 ${dead.length} 台主机的连接已失效`, `${dead.length} host connection(s) lost`),
        duration: 4000
      })
    }
  }

  stopStats(): void {
    if (this.statTimer >= 0) {
      clearInterval(this.statTimer)
      this.statTimer = -1
    }
  }

  async pollStats(): Promise<void> {
    if (this.view !== 0) {
      return
    }
    const list: HostItem[] = this.hosts.filter((h: HostItem) => h.state === 'connected')
    const now: number = Date.now()
    for (let i = 0; i < list.length; i++) {
      const id: number = list[i].id
      const until: number | undefined = this.statSkip.get(id)
      if (until !== undefined && now < until) {
        continue
      }
      try {
        const raw: string = await SshService.get().exec(id, STAT_CMD)
        this.statFail.delete(id)
        this.applyStats(id, raw)
      } catch (e) {
        const fails: number = (this.statFail.get(id) ?? 0) + 1
        if (fails >= 3) {
          this.statFail.delete(id)
          this.statSkip.set(id, Date.now() + 60000)
          const reason: string = this.t('状态采集失败', 'stats failed')
          if (this.markLost(id, reason)) {
            this.lostToast(id, reason)
          }
        } else {
          this.statFail.set(id, fails)
        }
      }
    }
  }

  fmtRate(bps: number): string {
    if (bps < 1024) {
      return `${Math.round(bps)} B/s`
    }
    if (bps < 1048576) {
      return `${(bps / 1024).toFixed(1)} KB/s`
    }
    return `${(bps / 1048576).toFixed(1)} MB/s`
  }

  applyStats(id: number, raw: string): void {
    const target: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    if (target === undefined || target.state !== 'connected') {
      return
    }
    const seg: string[] = raw.split('###')
    if (seg.length < 3) {
      return
    }
    const cpuParts: string[] = seg[0].trim().split(/\s+/)
    if (cpuParts.length < 5) {
      return
    }
    let total: number = 0
    for (let i = 1; i < cpuParts.length; i++) {
      total += Number(cpuParts[i]) || 0
    }
    const idle: number = (Number(cpuParts[4]) || 0) + (Number(cpuParts[5]) || 0)

    let memTotal: number = 0
    let memAvail: number = 0
    const memLines: string[] = seg[1].split('\n')
    for (let i = 0; i < memLines.length; i++) {
      const ln: string = memLines[i].trim()
      if (ln.startsWith('MemTotal:')) {
        memTotal = Number(ln.substring(9).trim().split(/\s+/)[0]) || 0
      } else if (ln.startsWith('MemAvailable:')) {
        memAvail = Number(ln.substring(13).trim().split(/\s+/)[0]) || 0
      }
    }
    const ram: number = memTotal > 0 ? Math.round((1 - memAvail / memTotal) * 100) : 0

    let bytes: number = 0
    const netLines: string[] = seg[2].split('\n')
    for (let i = 0; i < netLines.length; i++) {
      const ln: string = netLines[i].trim()
      const colon: number = ln.indexOf(':')
      if (colon <= 0 || ln.indexOf('|') >= 0) {
        continue
      }
      const name: string = ln.substring(0, colon).trim()
      if (name.length === 0 || name === 'lo') {
        continue
      }
      const nums: string[] = ln.substring(colon + 1).trim().split(/\s+/)
      if (nums.length < 9) {
        continue
      }
      bytes += (Number(nums[0]) || 0) + (Number(nums[8]) || 0)
    }

    const now: number = Date.now()
    const prev: StatSnap | undefined = this.statSnap.get(id)
    let cpu: number = 0
    let rate: number = 0
    if (prev !== undefined) {
      const dt: number = (now - prev.ts) / 1000
      const dTotal: number = total - prev.total
      const dIdle: number = idle - prev.idle
      if (dTotal > 0) {
        cpu = Math.round((1 - dIdle / dTotal) * 100)
        cpu = Math.min(100, Math.max(0, cpu))
      }
      if (dt > 0 && bytes >= prev.bytes) {
        rate = (bytes - prev.bytes) / dt
      }
    }
    this.statSnap.set(id, { total: total, idle: idle, bytes: bytes, ts: now })

    const netp: number = Math.min(100, Math.round(rate / 10485.76))
    this.hosts = this.hosts.map((h: HostItem) => {
      if (h.id !== id) {
        return h
      }
      const nh: HostItem = cloneHost(h, h.tunnels)
      nh.stats = { cpu: cpu, ram: ram, net: this.fmtRate(rate), netp: netp }
      return nh
    })
  }

  connectBlock(h: HostItem): string {
    if (h.host.length === 0) {
      return this.t('请先在主机设置里填写地址', 'Set the host address first')
    }
    if (h.auth === '密钥' || h.auth.startsWith('私钥')) {
      if (h.keyPath.length === 0) {
        return this.t('请先在主机设置里选择私钥', 'Pick a private key in host settings first')
      }
      return ''
    }
    if (h.password.length === 0) {
      return this.t('请先在主机设置里填写密码', 'Set the password in host settings first')
    }
    return ''
  }

  connectToggle(id: number): void {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    if (h === undefined) {
      return
    }
    if (h.state === 'connecting') {
      this.cancelConnect(h)
      return
    }
    if (h.state === 'connected') {
      this.setState(id, 'idle')
      SshService.get().disconnect(id)
      if (this.activeId === id && this.view === 1) {
        this.view = 0
      }
      promptAction.showToast({ message: this.t(`已断开 ${h.name}`, `Disconnected ${h.name}`) })
      return
    }
    const block: string = this.connectBlock(h)
    if (block.length > 0) {
      promptAction.showToast({ message: block })
      return
    }
    this.startConnect(h, -1)
  }

  cancelConnect(h: HostItem): void {
    this.connSeq.set(h.id, (this.connSeq.get(h.id) ?? 0) + 1)
    this.retry.delete(h.id)
    this.setState(h.id, 'idle')
    SshService.get().disconnect(h.id)
    AppLog.i('ui', `cancel connect ${h.name}`)
    promptAction.showToast({ message: this.t(`已取消连接 ${h.name}`, `Cancelled ${h.name}`) })
  }

  startConnect(h: HostItem, tab: number): void {
    this.setState(h.id, 'connecting')
    const seq: number = (this.connSeq.get(h.id) ?? 0) + 1
    this.connSeq.set(h.id, seq)
    AppLog.i('ui', `connect ${h.name} ${h.user}@${h.host}:${h.port}`)
    const hadKey: boolean = h.hostKey.length > 0
    promptAction.showToast({ message: this.t(`正在连接 ${h.user}@${h.host} …`, `Connecting ${h.user}@${h.host} …`) })
    SshService.get().connect(h).then((fp: string) => {
      if (this.connSeq.get(h.id) !== seq) {
        AppLog.w('ui', `stale connect callback ${h.name}`)
        SshService.get().disconnect(h.id)
        return
      }
      this.setState(h.id, 'connected')
      AppLog.i('ui', `connected ${h.name} fp=${fp}`)
      this.retry.delete(h.id)
      if (fp.length > 0) {
        this.applyHostKey(h.id, fp, h.hostKeyType, !hadKey)
      }
      this.sessionSeq = this.sessionSeq + 1
      this.restoreForwards(h)
      SshService.get().startLog(h.id, this.ctx())
      if (tab >= 0) {
        this.enterSession(h.id, tab)
      } else {
        promptAction.showToast({ message: this.t(`已连接 ${h.name}`, `Connected ${h.name}`) })
      }
    }).catch((e: Error) => {
      if (this.connSeq.get(h.id) !== seq) {
        return
      }
      this.setState(h.id, 'idle')
      AppLog.e('ui', `connect failed ${h.name}: ${e.message}`)
      promptAction.showToast({ message: this.errText(e.message), duration: 4000 })
    })
  }

  restoreForwards(h: HostItem): void {
    SshService.get().syncForwards(h.id, h.tunnels).then((failed: number[]) => {
      if (failed.length === 0) {
        return
      }
      AppLog.w('fwd', `${failed.length} forward(s) not restored for ${h.name}`)
      promptAction.showToast({
        message: this.t(`${failed.length} 条转发未能自动恢复，将在隧道页自动重试`,
          `${failed.length} forward(s) not restored; will retry automatically`),
        duration: 4000
      })
    })
  }

  applyHostKey(id: number, fp: string, keyType: string, persist: boolean): void {
    AppLog.i('ssh', `apply host key host=${id} fp=${fp} persist=${persist}`)
    this.hosts = this.hosts.map((h: HostItem) => {
      if (h.id !== id) {
        return h
      }
      const nh: HostItem = cloneHost(h, h.tunnels)
      nh.hostKey = fp
      nh.hostKeyType = keyType
      return nh
    })
    if (persist) {
      this.persistHosts()
    }
  }

  scheduleReconnect(id: number): void {
    const n: number = this.retry.get(id) ?? 0
    if (n >= 3) {
      this.retry.delete(id)
      promptAction.showToast({ message: this.t('自动重连已停止', 'Auto reconnect stopped') })
      return
    }
    this.retry.set(id, n + 1)
    const delay: number = 3000 * (n + 1)
    setTimeout(() => {
      const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
      if (h === undefined || h.state !== 'idle') {
        return
      }
      if (this.connectBlock(h).length > 0) {
        return
      }
      promptAction.showToast({ message: this.t(`正在重连 ${h.name}（${n + 1}/3）`, `Reconnecting ${h.name} (${n + 1}/3)`) })
      this.startConnect(h, -1)
    }, delay)
  }

  errText(raw: string): string {
    if (raw.indexOf('MISSING_KEY') >= 0) {
      return this.t('请先在主机设置里选择私钥', 'Pick a private key in host settings first')
    }
    if (raw.indexOf('KEY_UNREADABLE') >= 0) {
      return this.t('私钥文件不可读，请重新选择', 'Private key is unreadable, pick it again')
    }
    if (raw.indexOf('HOSTKEY_ERR:') >= 0) {
      const fp: string = raw.substring(raw.indexOf('HOSTKEY_ERR:') + 12)
      return this.t(`主机密钥已变更（${fp}），可能被中间人劫持`, `Host key changed (${fp}), possible man-in-the-middle`)
    }
    if (raw.indexOf('CONNECT_ERR') >= 0) {
      return this.t('无法连接到服务器，请检查地址与端口', 'Cannot reach the server, check address and port')
    }
    if (raw.indexOf('Auth') >= 0 || raw.indexOf('auth') >= 0 || raw.indexOf('认证') >= 0) {
      return this.t('认证失败，请检查用户名与密码', 'Authentication failed, check username and password')
    }
    if (raw.indexOf('timeout') >= 0 || raw.indexOf('超时') >= 0) {
      return this.t('连接超时', 'Connection timed out')
    }
    return this.t(`连接失败：${raw}`, `Failed: ${raw}`)
  }

  openSession(id: number, tab: number): void {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    if (h === undefined) {
      return
    }
    if (h.state === 'connected') {
      this.enterSession(id, tab)
      return
    }
    if (h.state === 'connecting') {
      promptAction.showToast({ message: this.t('正在连接，请稍候', 'Connecting, please wait') })
      return
    }
    const block: string = this.connectBlock(h)
    if (block.length > 0) {
      promptAction.showToast({ message: block })
      return
    }
    this.startConnect(h, tab)
  }

  enterSession(id: number, tab: number): void {
    this.activeId = id
    this.sessTab = tab
    this.termShow = true
    this.view = 1
  }

  leaveSession(): void {
    this.hideKeyboard()
    this.view = 0
  }

  disconnectActive(): void {
    const h: HostItem | undefined = this.curHost()
    if (h === undefined) {
      return
    }
    this.hideKeyboard()
    AppLog.i('ui', `disconnect active ${h.name}`)
    this.setState(h.id, 'idle')
    SshService.get().disconnect(h.id)
    this.view = 0
    promptAction.showToast({ message: this.t(`已断开 ${h.name}`, `Disconnected ${h.name}`) })
  }

  toggleActive(): void {
    const h: HostItem | undefined = this.curHost()
    if (h === undefined) {
      return
    }
    if (h.state === 'connecting') {
      this.cancelConnect(h)
      return
    }
    if (h.state === 'connected') {
      this.disconnectActive()
      return
    }
    const block: string = this.connectBlock(h)
    if (block.length > 0) {
      promptAction.showToast({ message: block })
      return
    }
    this.startConnect(h, this.sessTab)
  }

  hideKeyboard(): void {
    try {
      inputMethod.getController().stopInputSession()
    } catch (e) {
    }
  }

  segLabel(i: number): string {
    if (i === 0) {
      return this.t('终端', 'Terminal')
    }
    if (i === 1) {
      return this.t('隧道', 'Tunnels')
    }
    return this.t('文件', 'Files')
  }

  segIcon(i: number): string {
    if (i === 0) {
      return 'terminal'
    }
    if (i === 1) {
      return 'tunnel'
    }
    return 'folder'
  }

  activeStateText(): string {
    if (this.connected()) {
      return this.t('已连接', 'Connected')
    }
    if (this.connecting()) {
      return this.t('连接中', 'Connecting')
    }
    return this.t('已断开', 'Disconnected')
  }

  @Builder SegButton(i: number) {
    Row() {
      Ic({ name: this.segIcon(i), icSize: 14, color: this.sessTab === i ? $r('app.color.csx_text') : $r('app.color.csx_text2'), sw: 2 })
      Text(this.segLabel(i))
        .fontSize(MT.fsBody)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.sessTab === i ? $r('app.color.csx_text') : $r('app.color.csx_text2'))
        .margin({ left: 6 })
    }
    .justifyContent(FlexAlign.Center)
    .layoutWeight(1)
    .height(40)
    .borderRadius(11)
    .backgroundColor(this.sessTab === i ? $r('app.color.csx_surface2') : MT.transparent)
    .animation({ duration: 180, curve: Curve.EaseOut })
    .onClick(() => {
      this.hideKeyboard()
      this.sessTab = i
    })
  }

  @Builder SessionBar() {
    Row() {
      Stack() {
        Ic({ name: 'back', icSize: 18, color: $r('app.color.csx_text2'), sw: 2 })
      }
      .width(36)
      .height(36)
      .borderRadius(12)
      .backgroundColor($r('app.color.csx_surface2'))
      .onClick(() => {
        this.leaveSession()
      })

      Column() {
        Text(this.activeName())
          .fontSize(16.5)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_text'))
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
        Text(this.activeAddr() + ' · ' + this.activeStateText())
          .fontSize(MT.fsMicro)
          .fontColor(this.connected() ? $r('app.color.csx_accent') :
            (this.connecting() ? $r('app.color.csx_warn') : $r('app.color.csx_danger')))
          .margin({ top: 2 })
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)
      .margin({ left: 10 })

      Row() {
        Ic({ name: 'power', icSize: 13, color: this.connected() ? $r('app.color.csx_danger') : $r('app.color.csx_accent'), sw: 2 })
        Text(this.connected() ? this.t('断开', 'Disconnect') : (this.connecting() ? this.t('取消', 'Cancel') : this.t('重连', 'Reconnect')))
          .fontSize(MT.fsCaption)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.connected() ? $r('app.color.csx_danger') : $r('app.color.csx_accent'))
          .margin({ left: 5 })
      }
      .height(32)
      .padding({ left: 13, right: 13 })
      .borderRadius(999)
      .backgroundColor(this.connected() ? '#1AF87171' : '#1A3DDC97')
      .onClick(() => {
        this.toggleActive()
      })
    }
    .width('100%')
    .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 8, bottom: 8 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder SessionSeg() {
    Row() {
      this.SegButton(0)
      this.SegButton(1)
      this.SegButton(2)
    }
    .width('100%')
    .margin({ left: MT.pagePadding, right: MT.pagePadding })
    .padding(3)
    .borderRadius(14)
    .backgroundColor($r('app.color.csx_surface'))
  }

  @Builder HostSettingsView() {
    Column() {
      HostSettingsPage({
        hosts: $hosts,
        editId: this.editId,
        pendingHost: this.pendingHost,
        onBack: () => {
          this.pendingHost = null
          this.view = 0
        },
        onSaved: () => {
          this.pendingHost = null
          this.persistHosts()
        }
      })
    }
    .width('100%')
    .height('100%')
    .visibility(this.view === 2 ? Visibility.Visible : Visibility.None)
  }

  @Builder AppSettingsView() {
    Column() {
      Row() {
        Stack() {
          Ic({ name: 'back', icSize: 18, color: $r('app.color.csx_text2'), sw: 2 })
        }
        .width(36)
        .height(36)
        .borderRadius(12)
        .backgroundColor($r('app.color.csx_surface2'))
        .onClick(() => {
          if (this.setSub.length > 0) {
            this.setSub = ''
            return
          }
          this.view = 0
        })
        Blank()
      }
      .width('100%')
      .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 8, bottom: 4 })
      .alignItems(VerticalAlign.Center)

      SettingsPage({
        themeDark: $themeDark,
        hosts: $hosts,
        sub: $setSub,
        onHostsChanged: () => {
          this.persistHosts()
        }
      })
    }
    .width('100%')
    .height('100%')
    .visibility(this.view === 3 ? Visibility.Visible : Visibility.None)
  }

  build() {
    Stack() {
      this.Body()
    }
    .width('100%')
    .height('100%')
    .backgroundColor($r('app.color.csx_bg'))
  }

  @Builder Body() {
    Stack() {
      HostsPage({
        hosts: $hosts,
        onConnectToggle: (id: number) => {
          this.connectToggle(id)
        },
        onOpenSession: (id: number, tab: number) => {
          this.openSession(id, tab)
        },
        onHostSettings: (id: number) => {
          this.editId = id
          this.view = 2
        },
        onAppSettings: () => {
          this.setSub = ''
          this.view = 3
        },
        onNewHost: () => {
          const nid: number = this.hosts.reduce((m: number, x: HostItem) => (x.id > m ? x.id : m), 0) + 1
          this.pendingHost = emptyHost(nid, this.t('新主机', 'New Host'))
          this.editId = nid
          this.view = 2
        },
        onChanged: () => {
          this.persistHosts()
        },
        onProbe: (id: number) => {
          this.probeHostNow(id)
        }
      })
        .visibility(this.view === 0 ? Visibility.Visible : Visibility.None)

      Column() {
        this.SessionBar()
        this.SessionSeg()
        Stack() {
          TerminalPro({
            show: $termShow,
            hostId: this.activeId,
            sessionSeq: this.sessionSeq,
            hostName: this.activeName(),
            hostAddr: this.activeAddr(),
            compact: true
          })
            .visibility(this.sessTab === 0 ? Visibility.Visible : Visibility.None)

          TunnelsPage({
            hosts: $hosts,
            activeId: $activeId,
            masking: $masking,
            sheetOpen: $tunSheet,
            onBrowse: (url: string) => {
              this.browserUrl = url
              this.showBrowser = true
            },
            onToggleHost: () => {
              this.toggleActive()
            },
            onExit: () => {
              this.view = 0
            },
            onChanged: () => {
              this.persistHosts()
            }
          })
            .visibility(this.sessTab === 1 ? Visibility.Visible : Visibility.None)

          FilesPage({
            connected: this.connected(),
            hostId: this.activeId,
            hostName: this.activeName(),
            masking: $masking,
            view: $filesView,
            dlg: $filesDlg
          })
            .visibility(this.sessTab === 2 ? Visibility.Visible : Visibility.None)
        }
        .width('100%')
        .layoutWeight(1)
      }
      .width('100%')
      .height('100%')
      .visibility(this.view === 1 && !this.showBrowser ? Visibility.Visible : Visibility.None)

      this.HostSettingsView()

      this.AppSettingsView()

      if (this.showBrowser) {
        BrowserPage({ show: $showBrowser, url: this.browserUrl })
      }
    }
    .width('100%')
    .height('100%')
  }
}
