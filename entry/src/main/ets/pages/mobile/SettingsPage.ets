import { promptAction, window } from '@kit.ArkUI'
import { common, ConfigurationConstant } from '@kit.AbilityKit'
import { fileIo as fs, picker } from '@kit.CoreFileKit'
import { saveUiPrefs } from '../../common/Store'
import { HostItem } from '../../common/Models'
import { ManagedKey, listKeys, createKey, removeKey } from '../../common/KeyStore'
import { SshService, LogFileInfo } from '../../common/SshService'
import { AppLog, AppLogFile } from '../../common/AppLog'
import { probeTcp } from '../../common/PortProbe'
import { MT } from '../../common/MobileTheme'
import { Arrow } from '../../common/Arrow'
import { L, Bi } from '../../common/I18n'

export interface KnownHostItem {
  id: number
  host: string
  keyType: string
  fp: string
}

function sizeText(b: number): string {
  if (b >= 1048576) {
    return (b / 1048576).toFixed(1) + ' MB'
  }
  if (b >= 1024) {
    return (b / 1024).toFixed(1) + ' KB'
  }
  return b.toString() + ' B'
}

function clockText(ms: number): string {
  if (ms <= 0) {
    return ''
  }
  const d: Date = new Date(ms)
  const p = (n: number): string => n < 10 ? '0' + n.toString() : n.toString()
  return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate()) + ' ' +
    p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds())
}

@Component
export struct SettingsPage {
  @Link themeDark: boolean
  @Link hosts: HostItem[]
  @Link sub: string
  onHostsChanged: () => void = () => {}
  @StorageLink('csx_termFont') fontScale: number = 14
  @StorageLink('sk_keepAlive') keepAlive: boolean = true
  @StorageLink('sk_bgTunnel') bgTunnel: boolean = true
  @StorageLink('sk_autoReconnect') autoReconnect: boolean = false
  @StorageLink('sk_logOn') logOn: boolean = false
  @StorageLink('sk_logType') logType: number = 2
  @State logs: LogFileInfo[] = []
  @State appLogs: AppLogFile[] = []
  @State logTab: number = 0
  @State logOpen: string = ''
  @State logTitle: string = ''
  @State logText: string = ''
  @State testing: boolean = false
  @StorageLink('sk_proxyOn') proxyOn: boolean = false
  @StorageLink('sk_proxyType') proxyType: string = 'socks5'
  @StorageLink('sk_proxyHost') proxyHost: string = '127.0.0.1'
  @StorageLink('sk_proxyPort') proxyPort: string = '1080'
  @State keys: ManagedKey[] = []
  @State genOpen: boolean = false
  @State genAlg: string = 'Ed25519'
  @State genPass: string = ''
  @State genBusy: boolean = false
  @State known: KnownHostItem[] = []
  @StorageLink('csx_lang') csxLang: string = 'zh'

  t(zh: string, en: string): string {
    return this.csxLang === 'en' ? en : zh
  }

  pick(v: Bi): string {
    return this.csxLang === 'en' ? v.en : v.zh
  }

  aboutToAppear(): void {
    this.refreshKeys()
    this.refreshLogs()
  }

  refreshKeys(): void {
    const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
    try {
      this.keys = listKeys(ctx)
    } catch (e) {
      this.keys = []
    }
  }

  async doGenerate(): Promise<void> {
    if (this.genBusy) {
      return
    }
    this.genBusy = true
    const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
    try {
      const k: ManagedKey = await createKey(ctx, this.genAlg, this.genPass)
      this.refreshKeys()
      this.genOpen = false
      this.genPass = ''
      promptAction.showToast({ message: this.t(`已生成 ${k.name}`, `Generated ${k.name}`) })
    } catch (e) {
      const msg: string = e instanceof Error ? e.message : String(e)
      promptAction.showToast({ message: this.t(`生成失败：${msg}`, `Failed: ${msg}`) })
    } finally {
      this.genBusy = false
    }
  }

  dropKey(k: ManagedKey): void {
    removeKey(k)
    this.refreshKeys()
    promptAction.showToast({ message: this.t('已删除密钥', 'Key deleted') })
  }

  knownList(): KnownHostItem[] {
    const out: KnownHostItem[] = []
    this.hosts.forEach((h: HostItem) => {
      if (h.hostKey.length > 0) {
        const item: KnownHostItem = {
          id: h.id,
          host: h.host + ':' + h.port.toString(),
          keyType: h.hostKeyType,
          fp: h.hostKey
        }
        out.push(item)
      }
    })
    return out
  }

  forgetHostKey(id: number): void {
    this.hosts = this.hosts.map((h: HostItem) => {
      if (h.id !== id) {
        return h
      }
      h.hostKey = ''
      h.hostKeyType = ''
      return h
    })
    this.onHostsChanged()
    promptAction.showToast({ message: this.t('已移除，下次连接将重新信任', 'Removed; it will be trusted again next time') })
  }

  persistUi(): void {
    const ctx = getContext(this) as common.UIAbilityContext
    saveUiPrefs(ctx, {
      themeDark: this.themeDark,
      lang: this.csxLang,
      termFont: this.fontScale,
      keepAlive: this.keepAlive,
      bgTunnel: this.bgTunnel,
      autoReconnect: this.autoReconnect,
      logOn: this.logOn,
      logType: this.logType,
      proxyOn: this.proxyOn,
      proxyType: this.proxyType,
      proxyHost: this.proxyHost,
      proxyPort: this.proxyPort
    })
  }

  setTheme(dark: boolean): void {
    if (this.themeDark === dark) {
      return
    }
    this.themeDark = dark
    AppStorage.setOrCreate('csx_themeDark', dark)
    this.persistUi()
    const ctx = getContext(this) as common.UIAbilityContext
    ctx.getApplicationContext().setColorMode(dark ? ConfigurationConstant.ColorMode.COLOR_MODE_DARK : ConfigurationConstant.ColorMode.COLOR_MODE_LIGHT)
    window.getLastWindow(getContext(this)).then((w: window.Window) => {
      w.setWindowSystemBarProperties({
        statusBarColor: dark ? '#0A0C10' : '#F1F3F7',
        statusBarContentColor: dark ? '#E9EFF7' : '#10151C',
        navigationBarColor: dark ? '#0A0C10' : '#F1F3F7',
        navigationBarContentColor: dark ? '#59647A' : '#98A2B3'
      })
    }).catch(() => {
    })
  }

  setLang(lang: string): void {
    if (this.csxLang === lang) {
      return
    }
    this.csxLang = lang
    L.lang = lang
    this.persistUi()
  }

  setSwitch(key: string, v: boolean): void {
    if (key === 'keepAlive') {
      this.keepAlive = v
    } else if (key === 'bgTunnel') {
      this.bgTunnel = v
    } else if (key === 'autoReconnect') {
      this.autoReconnect = v
    } else if (key === 'proxyOn') {
      this.proxyOn = v
    } else if (key === 'logOn') {
      this.logOn = v
    }
    this.persistUi()
  }

  goSub(key: string): void {
    if (key === 'keys' || key === 'known' || key === 'proxy' || key === 'logs') {
      this.sub = key
      if (key === 'logs') {
        this.refreshLogs()
      }
      return
    }
  }

  logDir(): string {
    const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
    return SshService.get().logDirOf(ctx)
  }

  refreshLogs(): void {
    const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
    try {
      this.logs = SshService.get().logFiles(ctx)
    } catch (e) {
      this.logs = []
    }
    try {
      this.appLogs = AppLog.files()
    } catch (e) {
      this.appLogs = []
    }
  }

  logBytes(): number {
    let b: number = 0
    for (let i = 0; i < this.logs.length; i++) {
      b += this.logs[i].size
    }
    return b
  }

  logSummary(): Bi {
    if (this.logs.length === 0) {
      return L.b('暂无日志', 'No logs')
    }
    return L.b(`${this.logs.length} 个文件 · ${sizeText(this.logBytes())}`,
      `${this.logs.length} files · ${sizeText(this.logBytes())}`)
  }

  appLogBytes(): number {
    let b: number = 0
    for (let i = 0; i < this.appLogs.length; i++) {
      b += this.appLogs[i].size
    }
    return b
  }

  appLogSummary(): Bi {
    if (this.appLogs.length === 0) {
      return L.b('暂无日志', 'No logs')
    }
    return L.b(`${this.appLogs.length} 个文件 · ${sizeText(this.appLogBytes())}`,
      `${this.appLogs.length} files · ${sizeText(this.appLogBytes())}`)
  }

  clearAppLogs(): void {
    const n: number = AppLog.clear()
    this.refreshLogs()
    if (n === 0) {
      promptAction.showToast({ message: this.t('没有可清除的日志', 'Nothing to clear') })
      return
    }
    AppLog.i('app', `application logs cleared: ${n}`)
    promptAction.showToast({ message: this.t(`已清除 ${n} 个日志文件`, `Cleared ${n} log files`) })
  }

  openAppLog(f: AppLogFile): void {
    const cap: number = 131072
    this.logTitle = f.name
    this.logText = AppLog.read(f.path, cap)
    if (f.size > cap) {
      const hint: string = this.t(`（仅显示末尾 ${sizeText(cap)}，共 ${sizeText(f.size)}）\n`,
        `(last ${sizeText(cap)} of ${sizeText(f.size)})\n`)
      this.logText = hint + this.logText
    }
    this.logOpen = f.path
  }

  async exportAppLogs(): Promise<void> {
    if (this.appLogs.length === 0) {
      promptAction.showToast({ message: this.t('没有可导出的日志', 'No logs to export') })
      return
    }
    let uri: string = ''
    try {
      const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
      const dp: picker.DocumentViewPicker = new picker.DocumentViewPicker(ctx)
      const opt: picker.DocumentSaveOptions = new picker.DocumentSaveOptions()
      opt.newFileNames = ['skiff-app-logs-' + Date.now().toString() + '.txt']
      const uris: string[] = await dp.save(opt)
      if (uris.length === 0) {
        return
      }
      uri = uris[0]
    } catch (e) {
      promptAction.showToast({ message: this.t('打开保存对话框失败', 'Cannot open the save dialog') })
      return
    }
    let body: string = ''
    const cap: number = 1048576
    for (let i = 0; i < this.appLogs.length && body.length < cap; i++) {
      const f: AppLogFile = this.appLogs[i]
      body += '\n===== ' + f.name + ' (' + clockText(f.mtime) + ') =====\n'
      body += AppLog.read(f.path, 524288)
    }
    try {
      const f = fs.openSync(uri, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC)
      fs.writeSync(f.fd, body)
      fs.closeSync(f.fd)
      promptAction.showToast({ message: this.t('已导出全部日志', 'All logs exported') })
    } catch (e) {
      promptAction.showToast({ message: this.t('写入失败，请换个位置', 'Write failed, try another location') })
    }
  }

  clearLogs(): void {
    const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
    let n: number = 0
    try {
      n = SshService.get().purgeLogs(ctx)
    } catch (e) {
      n = 0
    }
    if (n === 0) {
      promptAction.showToast({ message: this.t('没有可清除的日志', 'Nothing to clear') })
      this.refreshLogs()
      return
    }
    this.refreshLogs()
    if (this.logOn) {
      SshService.get().restartLogs(ctx)
      setTimeout(() => {
        this.refreshLogs()
      }, 400)
    }
    promptAction.showToast({ message: this.t(`已清除 ${n} 个日志文件`, `Cleared ${n} log files`) })
  }

  openLog(f: LogFileInfo): void {
    this.logTitle = f.name
    this.logText = SshService.get().readLog(f.path, 131072)
    this.logOpen = f.path
  }

  closeLog(): void {
    this.logOpen = ''
    this.logTitle = ''
    this.logText = ''
  }

  async exportLogs(): Promise<void> {
    if (this.logs.length === 0) {
      promptAction.showToast({ message: this.t('没有可导出的日志', 'No logs to export') })
      return
    }
    let uri: string = ''
    try {
      const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
      const dp: picker.DocumentViewPicker = new picker.DocumentViewPicker(ctx)
      const opt: picker.DocumentSaveOptions = new picker.DocumentSaveOptions()
      opt.newFileNames = ['skiff-logs-' + Date.now().toString() + '.txt']
      const uris: string[] = await dp.save(opt)
      if (uris.length === 0) {
        return
      }
      uri = uris[0]
    } catch (e) {
      promptAction.showToast({ message: this.t('打开保存对话框失败', 'Cannot open the save dialog') })
      return
    }
    let body: string = ''
    const cap: number = 1048576
    for (let i = 0; i < this.logs.length && body.length < cap; i++) {
      const f: LogFileInfo = this.logs[i]
      body += '\n===== ' + f.name + ' (' + clockText(f.mtime) + ') =====\n'
      body += SshService.get().readLog(f.path, 524288)
    }
    try {
      const f = fs.openSync(uri, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC)
      fs.writeSync(f.fd, body)
      fs.closeSync(f.fd)
      promptAction.showToast({ message: this.t('已导出全部日志', 'All logs exported') })
    } catch (e) {
      promptAction.showToast({ message: this.t('写入失败，请换个位置', 'Write failed, try another location') })
    }
  }

  testProxy(): void {
    const host: string = this.proxyHost.trim()
    const port: number = Number(this.proxyPort)
    if (host.length === 0) {
      promptAction.showToast({ message: this.t('请填写代理地址', 'Enter the proxy address') })
      return
    }
    if (!/^\d+$/.test(this.proxyPort) || port < 1 || port > 65535) {
      promptAction.showToast({ message: this.t('端口需为 1-65535 的数字', 'Port must be 1-65535') })
      return
    }
    if (this.testing) {
      return
    }
    this.testing = true
    probeTcp(host, port, 3000).then((ok: boolean) => {
      this.testing = false
      promptAction.showToast({
        message: ok
          ? this.t(`代理 ${host}:${port} 可达`, `Proxy ${host}:${port} is reachable`)
          : this.t(`无法连接 ${host}:${port}，请检查地址与端口`, `Cannot reach ${host}:${port}`),
        duration: 3500
      })
    })
  }

  setProxyField(key: string, v: string): void {
    if (key === 'proxyHost') {
      this.proxyHost = v
    } else if (key === 'proxyPort') {
      this.proxyPort = v
    }
    this.persistUi()
  }

  build() {
    Stack({ alignContent: Alignment.Top }) {
      if (this.sub === '') {
        this.MainList()
      } else if (this.sub === 'keys') {
        this.KeysPage()
      } else if (this.sub === 'known') {
        this.KnownPage()
      } else if (this.sub === 'proxy') {
        this.ProxyPage()
      } else if (this.sub === 'logs') {
        this.LogsPage()
      }
      if (this.logOpen.length > 0) {
        this.LogViewer()
      }
    }
    .width('100%')
    .height('100%')
  }

  @Builder SubHeader(title: Bi) {
    Row() {
      Stack() {
        Arrow({ dir: 'left', color: $r('app.color.csx_blue') , len: 20 })
      }
      .width(38)
      .height(38)
      .onClick(() => {
        this.sub = ''
      })
      Text(this.pick(title))
        .fontSize(MT.fsTitle)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('app.color.csx_text'))
        .layoutWeight(1)
        .textAlign(TextAlign.Center)
      Text('')
        .width(38)
    }
    .width('100%')
    .padding({ left: 6, right: 6, top: 10, bottom: 10 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder MainList() {
    Column() {
      Column() {
        Text(this.t('设置', 'Settings'))
          .fontSize(MT.fsDisplay)
          .fontWeight(FontWeight.Bold)
          .fontColor($r('app.color.csx_text'))
        Text(this.t('外观、安全与会话行为', 'Appearance, security and sessions'))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text3'))
          .margin({ top: 6 })
      }
      .alignItems(HorizontalAlign.Start)
      .width('100%')
      .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 14, bottom: 16 })

      Scroll() {
        Column() {
          this.GroupTitle(L.b('外观', 'Appearance'))
          Column() {
            this.ThemeRow()
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            this.LangRow()
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            Row() {
              Text(this.t('终端字号', 'Terminal font size'))
                .fontSize(MT.fsBody)
                .fontColor($r('app.color.csx_text'))
                .width(88)
              Slider({ value: this.fontScale, min: 11, max: 18, step: 1, style: SliderStyle.InSet })
                .selectedColor($r('app.color.csx_accent'))
                .trackColor($r('app.color.csx_surface2'))
                .blockColor($r('app.color.csx_accent'))
                .layoutWeight(1)
                .onChange((v: number) => {
                  this.fontScale = v
                  this.persistUi()
                })
              Text(`${this.fontScale}`)
                .fontSize(MT.fsSmall)
                .fontColor($r('app.color.csx_text2'))
                .width(30)
                .textAlign(TextAlign.End)
            }
            .width('100%')
            .height(56)
            .padding({ left: 16, right: 16 })
            .alignItems(VerticalAlign.Center)
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.b('连接行为', 'Connection'))
          Column() {
            this.SwitchRow(L.b('保活', 'Keep-alive'), L.b('定时发送探测包防止连接被回收', 'Send probes periodically to keep the connection alive'), this.keepAlive, 'keepAlive')
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            this.SwitchRow(L.b('后台保持隧道', 'Keep tunnels in background'), L.b('切到后台后不主动断开转发', 'Do not drop forwarding when app goes to background'), this.bgTunnel, 'bgTunnel')
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            this.SwitchRow(L.b('断线自动重连', 'Auto reconnect'), L.b('网络抖动后自动恢复会话', 'Restore the session after network jitter'), this.autoReconnect, 'autoReconnect')
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.b('安全与网络', 'Security & network'))
          Column() {
            this.NavRow(L.b('密钥管理', 'Keys'), L.b(`${this.keys.length} 个密钥`, `${this.keys.length} keys`), 'keys')
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            this.NavRow(L.b('主机指纹', 'Known hosts'), L.b(`${this.knownList().length} 条记录`, `${this.knownList().length} records`), 'known')
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            this.NavRow(L.b('代理', 'Proxy'), this.proxyOn ? L.s(`${this.proxyType.toUpperCase()} ${this.proxyHost}:${this.proxyPort}`) : L.b('未启用', 'Disabled'), 'proxy')
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.b('诊断', 'Diagnostics'))
          Column() {
            this.NavRow(L.b('会话日志', 'Session log'), this.logOn ? L.b('已开启', 'On') : L.b('已关闭', 'Off'), 'logs')
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          Text(this.t('轻舟 Skiff · 鸿蒙原生 SSH 客户端\n内核 libssh2 1.11.1 · arm64-v8a + x86_64', 'Skiff · Native SSH client for HarmonyOS\nlibssh2 1.11.1 · arm64-v8a + x86_64'))
            .fontSize(MT.fsCaption)
            .fontColor($r('app.color.csx_text3'))
            .textAlign(TextAlign.Center)
            .lineHeight(18)
            .width('100%')
            .margin({ top: 24, bottom: 20 })
        }
        .width('100%')
        .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: MT.tabbarSpace })
      }
      .layoutWeight(1)
      .align(Alignment.Top)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
  }

  @Builder GroupTitle(title: Bi) {
    Text(this.pick(title))
      .fontSize(MT.fsCaption)
      .fontWeight(FontWeight.Medium)
      .fontColor($r('app.color.csx_text3'))
      .width('100%')
      .margin({ top: 18, bottom: 9, left: 4 })
  }

  @Builder ThemeRow() {
    Row() {
      Column() {
        Text(this.t('颜色主题', 'Theme'))
          .fontSize(MT.fsBody)
          .fontColor($r('app.color.csx_text'))
        Text(this.themeDark ? this.t('深色 · 夜间与低光环境', 'Dark · Night and low light') : this.t('浅色 · 白天与强光环境', 'Light · Day and bright light'))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text3'))
          .margin({ top: 4 })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)

      Row() {
        Text(this.t('深色', 'Dark'))
          .fontSize(MT.fsCaption)
          .fontWeight(this.themeDark ? FontWeight.Medium : FontWeight.Normal)
          .fontColor(this.themeDark ? $r('app.color.csx_accent_fg') : $r('app.color.csx_text2'))
          .height(28)
          .padding({ left: 13, right: 13 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(this.themeDark ? $r('app.color.csx_accent') : MT.transparent)
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setTheme(true)
          })
        Text(this.t('浅色', 'Light'))
          .fontSize(MT.fsCaption)
          .fontWeight(this.themeDark ? FontWeight.Normal : FontWeight.Medium)
          .fontColor(this.themeDark ? $r('app.color.csx_text2') : $r('app.color.csx_accent_fg'))
          .height(28)
          .padding({ left: 13, right: 13 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(this.themeDark ? MT.transparent : $r('app.color.csx_accent'))
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setTheme(false)
          })
      }
      .padding(3)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor($r('app.color.csx_surface2'))
    }
    .width('100%')
    .height(62)
    .padding({ left: 16, right: 16 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder LangRow() {
    Row() {
      Text(this.t('界面语言', 'Language'))
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
        .layoutWeight(1)
      Row() {
        Text('简体中文')
          .fontSize(MT.fsCaption)
          .fontWeight(this.csxLang === 'zh' ? FontWeight.Medium : FontWeight.Normal)
          .fontColor(this.csxLang === 'zh' ? $r('app.color.csx_accent_fg') : $r('app.color.csx_text2'))
          .height(28)
          .padding({ left: 11, right: 11 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(this.csxLang === 'zh' ? $r('app.color.csx_accent') : MT.transparent)
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setLang('zh')
          })
        Text('English')
          .fontSize(MT.fsCaption)
          .fontWeight(this.csxLang === 'en' ? FontWeight.Medium : FontWeight.Normal)
          .fontColor(this.csxLang === 'en' ? $r('app.color.csx_accent_fg') : $r('app.color.csx_text2'))
          .height(28)
          .padding({ left: 11, right: 11 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(this.csxLang === 'en' ? $r('app.color.csx_accent') : MT.transparent)
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setLang('en')
          })
      }
      .padding(3)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor($r('app.color.csx_surface2'))
    }
    .width('100%')
    .height(56)
    .padding({ left: 16, right: 16 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder SwitchRow(title: Bi, desc: Bi, on: boolean, key: string) {
    Row() {
      Column() {
        Text(this.pick(title))
          .fontSize(MT.fsBody)
          .fontColor($r('app.color.csx_text'))
        Text(this.pick(desc))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text3'))
          .margin({ top: 4 })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)

      Toggle({ type: ToggleType.Switch, isOn: on })
        .selectedColor($r('app.color.csx_accent'))
        .switchPointColor('#FFFFFF')
        .onChange((v: boolean) => {
          this.setSwitch(key, v)
        })
    }
    .width('100%')
    .padding({ left: 16, right: 16, top: 13, bottom: 13 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder NavRow(title: Bi, value: Bi, key: string) {
    Row() {
      Text(this.pick(title))
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
        .layoutWeight(1)
      Text(this.pick(value))
        .fontSize(MT.fsSmall)
        .fontColor($r('app.color.csx_text3'))
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .constraintSize({ maxWidth: 160 })
      Stack() {
        Arrow({ dir: 'right', color: $r('app.color.csx_text3') , len: 15 })
      }
      .width(20)
      .height(20)
      .margin({ left: 4 })
    }
    .width('100%')
    .height(52)
    .padding({ left: 16, right: 16 })
    .alignItems(VerticalAlign.Center)
    .onClick(() => {
      this.goSub(key)
    })
  }

  @Builder GenChip(val: string, label: string) {
    Text(label)
      .fontSize(MT.fsSmall)
      .fontWeight(FontWeight.Medium)
      .fontColor(this.genAlg === val ? $r('app.color.csx_text') : $r('app.color.csx_text2'))
      .textAlign(TextAlign.Center)
      .height(34)
      .layoutWeight(1)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(this.genAlg === val ? $r('app.color.csx_surface2') : MT.transparent)
      .onClick(() => {
        this.genAlg = val
      })
  }

  @Builder KeysPage() {
    Column() {
      this.SubHeader(L.b('密钥管理', 'Keys'))
      Scroll() {
        Column() {
          ForEach(this.keys, (k: ManagedKey) => {
            Row() {
              Text(k.alg.substring(0, 2).toUpperCase())
                .fontSize(MT.fsCaption)
                .fontWeight(FontWeight.Bold)
                .fontColor($r('app.color.csx_accent'))
                .width(30)
                .height(30)
                .textAlign(TextAlign.Center)
                .borderRadius(MT.radiusCtrl)
                .backgroundColor($r('app.color.csx_accent_soft'))
              Column() {
                Text(k.name)
                  .fontSize(MT.fsBody)
                  .fontColor($r('app.color.csx_text'))
                Text(this.t(`${k.alg} ${k.bits} 位${k.encrypted ? ' · 带口令' : ''}`, `${k.alg} ${k.bits} bit${k.encrypted ? ' · passphrase' : ''}`))
                  .fontSize(MT.fsCaption)
                  .fontColor($r('app.color.csx_text3'))
                  .margin({ top: 4 })
              }
              .alignItems(HorizontalAlign.Start)
              .layoutWeight(1)
              .margin({ left: 12 })

              Text(this.t('删除', 'Delete'))
                .fontSize(MT.fsCaption)
                .fontColor($r('app.color.csx_danger'))
                .padding({ left: 10, right: 10, top: 6, bottom: 6 })
                .borderRadius(MT.radiusCtrl)
                .backgroundColor($r('app.color.csx_surface2'))
                .onClick(() => {
                  this.dropKey(k)
                })
            }
            .width('100%')
            .padding(14)
            .margin({ bottom: 10 })
            .borderRadius(MT.radiusCard)
            .backgroundColor($r('app.color.csx_surface'))
            .alignItems(VerticalAlign.Center)
          }, (k: ManagedKey) => k.name)

          if (this.genOpen) {
            Column() {
              Row() {
                this.GenChip('Ed25519', 'Ed25519')
                this.GenChip('ECDSA', 'ECDSA P-256')
                this.GenChip('RSA', 'RSA 2048')
              }
              .width('100%')

              TextInput({ placeholder: this.t('私钥口令，可留空', 'Passphrase, optional'), text: this.genPass })
                .type(InputType.Password)
                .placeholderColor($r('app.color.csx_text3'))
                .fontColor($r('app.color.csx_text'))
                .fontSize(MT.fsBody)
                .height(42)
                .margin({ top: 12 })
                .backgroundColor($r('app.color.csx_surface2'))
                .onChange((v: string) => {
                  this.genPass = v
                })

              Row() {
                Text(this.t('取消', 'Cancel'))
                  .fontSize(MT.fsBody)
                  .fontColor($r('app.color.csx_text2'))
                  .textAlign(TextAlign.Center)
                  .height(42)
                  .layoutWeight(1)
                  .borderRadius(MT.radiusCtrl)
                  .backgroundColor($r('app.color.csx_surface2'))
                  .onClick(() => {
                    this.genOpen = false
                  })
                Text(this.genBusy ? this.t('生成中…', 'Generating…') : this.t('生成', 'Generate'))
                  .fontSize(MT.fsBody)
                  .fontWeight(FontWeight.Medium)
                  .fontColor($r('app.color.csx_accent_fg'))
                  .textAlign(TextAlign.Center)
                  .height(42)
                  .layoutWeight(1)
                  .margin({ left: 10 })
                  .borderRadius(MT.radiusCtrl)
                  .backgroundColor($r('app.color.csx_accent'))
                  .onClick(() => {
                    this.doGenerate()
                  })
              }
              .width('100%')
              .margin({ top: 12 })
            }
            .width('100%')
            .padding(14)
            .margin({ bottom: 10 })
            .borderRadius(MT.radiusCard)
            .backgroundColor($r('app.color.csx_surface'))
          } else {
            Text(this.t('+ 生成新密钥', '+ Generate new key'))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_accent'))
              .textAlign(TextAlign.Center)
              .width('100%')
              .height(50)
              .borderRadius(MT.radiusBtn)
              .border({ width: 1, color: $r('app.color.csx_line'), style: BorderStyle.Dashed })
              .onClick(() => {
                this.genOpen = true
              })
          }
        }
        .width('100%')
        .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 6, bottom: MT.tabbarSpace })
      }
      .layoutWeight(1)
      .align(Alignment.Top)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
  }

  @Builder KnownPage() {
    Column() {
      this.SubHeader(L.b('主机指纹', 'Known hosts'))
      Scroll() {
        Column() {
          Text(this.t('首次连接新主机会自动记录对方指纹并保存在本机，之后每次连接都会比对。指纹发生变化时会拒绝连接并提示，说明对方密钥已更换，可能被中间人劫持。', 'The fingerprint of a new host is recorded on first connect and compared on every later connect. If it changes, the connection is rejected — the remote key changed and this may indicate a man-in-the-middle attack.'))
            .fontSize(MT.fsCaption)
            .fontColor($r('app.color.csx_text3'))
            .lineHeight(18)
            .padding(13)
            .width('100%')
            .borderRadius(MT.radiusCard)
            .backgroundColor($r('app.color.csx_surface2'))
            .margin({ bottom: 14 })

          ForEach(this.knownList(), (k: KnownHostItem) => {
            Column() {
              Row() {
                Text(k.host)
                  .fontSize(MT.fsBody)
                  .fontColor($r('app.color.csx_text'))
                  .layoutWeight(1)
              }
              .width('100%')
              Text(`${k.keyType}  ${k.fp}`)
                .fontSize(MT.fsMicro)
                .fontColor($r('app.color.csx_text3'))
                .fontFamily('monospace')
                .width('100%')
                .margin({ top: 6 })
              Row() {
                Text(this.t('移除', 'Remove'))
                  .fontSize(MT.fsCaption)
                  .fontColor($r('app.color.csx_danger'))
                  .padding({ left: 10, right: 10, top: 6, bottom: 6 })
                  .borderRadius(MT.radiusCtrl)
                  .backgroundColor($r('app.color.csx_surface2'))
                  .onClick(() => {
                    this.forgetHostKey(k.id)
                  })
              }
              .width('100%')
              .justifyContent(FlexAlign.End)
              .margin({ top: 10 })
            }
            .width('100%')
            .padding(14)
            .margin({ bottom: 10 })
            .borderRadius(MT.radiusCard)
            .backgroundColor($r('app.color.csx_surface'))
          }, (k: KnownHostItem) => k.id.toString())
        }
        .width('100%')
        .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 6, bottom: MT.tabbarSpace })
      }
      .layoutWeight(1)
      .align(Alignment.Top)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
  }

  @Builder ProxyPage() {
    Column() {
      this.SubHeader(L.b('代理', 'Proxy'))
      Scroll() {
        Column() {
          this.GroupTitle(L.b('启用', 'Enable'))
          Column() {
            this.SwitchRow(L.b('通过代理连接', 'Connect via proxy'), L.b('先经代理建立 TCP，再在隧道内握手 SSH', 'Establish TCP through the proxy, then handshake SSH inside the tunnel'), this.proxyOn, 'proxyOn')
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.b('类型', 'Type'))
          Row() {
            this.ProxyChip('socks5', L.s('SOCKS5'))
            this.ProxyChip('http', L.s('HTTP CONNECT'))
          }
          .width('100%')
          .padding(4)
          .borderRadius(MT.radiusCard)
          .backgroundColor($r('app.color.csx_surface2'))

          this.GroupTitle(L.b('服务器', 'Server'))
          Column() {
            this.InputRow(L.b('地址', 'Address'), this.proxyHost, 'proxyHost')
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            this.InputRow(L.b('端口', 'Port'), this.proxyPort, 'proxyPort')
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          Text(this.testing ? this.t('测试中…', 'Testing…') : this.t('测试连通', 'Test'))
            .fontSize(MT.fsBody)
            .fontColor($r('app.color.csx_accent_fg'))
            .textAlign(TextAlign.Center)
            .width('100%')
            .height(48)
            .borderRadius(MT.radiusBtn)
            .backgroundColor($r('app.color.csx_accent'))
            .opacity(this.testing ? 0.6 : 1)
            .margin({ top: 20 })
            .onClick(() => {
              this.testProxy()
            })
        }
        .width('100%')
        .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 6, bottom: MT.tabbarSpace })
      }
      .layoutWeight(1)
      .align(Alignment.Top)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
  }

  @Builder ProxyChip(tp: string, label: Bi) {
    Text(this.pick(label))
      .fontSize(MT.fsSmall)
      .fontColor(this.proxyType === tp ? $r('app.color.csx_text') : $r('app.color.csx_text2'))
      .textAlign(TextAlign.Center)
      .height(36)
      .layoutWeight(1)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(this.proxyType === tp ? $r('app.color.csx_surface') : MT.transparent)
      .onClick(() => {
        this.proxyType = tp
        this.proxyPort = tp === 'http' ? '8080' : '1080'
        this.persistUi()
      })
  }

  @Builder LogsPage() {
    Column() {
      this.SubHeader(L.b('日志', 'Logs'))
      this.LogTabs()
      Scroll() {
        Column() {
          this.GroupTitle(L.b('记录', 'Record'))
          Column() {
            this.SwitchRow(L.b('记录会话日志', 'Record session log'), L.b('把终端输入输出写入沙箱文件', 'Write terminal I/O into sandbox files'), this.logOn, 'logOn')
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.b('记录范围', 'Scope'))
          Row() {
            this.LogChip(2, L.b('全部输出', 'All output'))
            this.LogChip(1, L.b('仅可打印', 'Printable only'))
          }
          .width('100%')
          .padding(4)
          .borderRadius(MT.radiusCard)
          .backgroundColor($r('app.color.csx_surface2'))

          this.GroupTitle(L.b('保存位置', 'Location'))
          Column() {
            Text(this.logDir())
              .fontSize(MT.fsMicro)
              .fontColor($r('app.color.csx_text2'))
              .fontFamily('monospace')
              .width('100%')
              .padding({ left: 16, right: 16, top: 13, bottom: 13 })
            Divider().color($r('app.color.csx_line')).margin({ left: 16 })
            this.NavRow(L.b('已记录', 'Recorded'), this.logSummary(), 'none')
          }
          .backgroundColor($r('app.color.csx_surface'))
          .borderRadius(MT.radiusCard)
          .clip(true)

          Text(this.t('日志固定写在应用沙箱内，一条会话一个文件，最多保留 20 个，超出自动清理最旧的。', 'Logs live inside the app sandbox, one file per session, keeping the newest 20.'))
            .fontSize(MT.fsCaption)
            .fontColor($r('app.color.csx_text3'))
            .lineHeight(18)
            .width('100%')
            .margin({ top: 10, left: 4 })

          if (this.logs.length > 0) {
            Column() {
              ForEach(this.logs, (f: LogFileInfo) => {
                Row() {
                  Column() {
                    Text(f.name)
                      .fontSize(MT.fsSmall)
                      .fontColor($r('app.color.csx_text'))
                      .maxLines(1)
                      .textOverflow({ overflow: TextOverflow.Ellipsis })
                      .width('100%')
                      .textAlign(TextAlign.Start)
                    Text(sizeText(f.size) + ' · ' + clockText(f.mtime))
                      .fontSize(MT.fsMicro)
                      .fontColor($r('app.color.csx_text3'))
                      .margin({ top: 4 })
                      .width('100%')
                      .textAlign(TextAlign.Start)
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)

                  Text(this.t('查看', 'View'))
                    .fontSize(MT.fsCaption)
                    .fontColor($r('app.color.csx_blue'))
                    .padding({ left: 12, right: 2, top: 6, bottom: 6 })
                    .onClick(() => {
                      this.openLog(f)
                    })
                }
                .width('100%')
                .padding({ left: 16, right: 16, top: 11, bottom: 11 })
                .alignItems(VerticalAlign.Center)
              }, (f: LogFileInfo) => f.path)
            }
            .width('100%')
            .margin({ top: 10 })
            .borderRadius(MT.radiusCard)
            .backgroundColor($r('app.color.csx_surface'))
            .clip(true)
          }

          Row() {
            Text(this.t('清除日志', 'Clear logs'))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_danger'))
              .textAlign(TextAlign.Center)
              .height(48)
              .layoutWeight(1)
              .borderRadius(MT.radiusBtn)
              .backgroundColor($r('app.color.csx_surface'))
              .onClick(() => {
                this.clearLogs()
              })
            Text(this.t('导出日志', 'Export logs'))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_accent'))
              .textAlign(TextAlign.Center)
              .height(48)
              .layoutWeight(1)
              .margin({ left: 12 })
              .borderRadius(MT.radiusBtn)
              .backgroundColor($r('app.color.csx_accent_soft'))
              .onClick(() => {
                this.exportLogs()
              })
          }
          .width('100%')
          .margin({ top: 20 })
        }
        .width('100%')
        .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 6, bottom: MT.tabbarSpace })
      }
      .layoutWeight(1)
      .align(Alignment.Top)
      .scrollBar(BarState.Off)
      .visibility(this.logTab === 0 ? Visibility.Visible : Visibility.None)

      this.AppLogsPage()
    }
    .width('100%')
    .height('100%')
  }

  @Builder LogTabs() {
    Row() {
      Row() {
        this.LogTabChip(0, L.b('会话日志', 'Session'))
        this.LogTabChip(1, L.b('应用日志', 'App'))
      }
      .padding(3)
      .borderRadius(14)
      .backgroundColor($r('app.color.csx_surface'))
    }
    .width('100%')
    .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: 8 })
  }

  @Builder LogTabChip(t: number, label: Bi) {
    Text(this.pick(label))
      .fontSize(MT.fsSmall)
      .fontColor(this.logTab === t ? $r('app.color.csx_text') : $r('app.color.csx_text2'))
      .textAlign(TextAlign.Center)
      .height(34)
      .layoutWeight(1)
      .borderRadius(11)
      .backgroundColor(this.logTab === t ? $r('app.color.csx_surface2') : MT.transparent)
      .onClick(() => {
        this.logTab = t
        this.refreshLogs()
      })
  }

  @Builder AppLogsPage() {
    Scroll() {
      Column() {
        this.GroupTitle(L.b('应用运行日志', 'Application log'))
        Column() {
          Text(AppLog.dirOf())
            .fontSize(MT.fsMicro)
            .fontColor($r('app.color.csx_text2'))
            .fontFamily('monospace')
            .width('100%')
            .padding({ left: 16, right: 16, top: 13, bottom: 13 })
          Divider().color($r('app.color.csx_line')).margin({ left: 16 })
          this.NavRow(L.b('已记录', 'Recorded'), this.appLogSummary(), 'none')
        }
        .backgroundColor($r('app.color.csx_surface'))
        .borderRadius(MT.radiusCard)
        .clip(true)

        Text(this.t('记录连接、转发、异常与崩溃信息，按天写文件，最多保留 7 天。', 'Records connects, forwards, exceptions and crashes; one file per day, keeping the newest 7.'))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text3'))
          .lineHeight(18)
          .width('100%')
          .margin({ top: 10, left: 4 })

        if (this.appLogs.length > 0) {
          Column() {
            ForEach(this.appLogs, (f: AppLogFile) => {
              Row() {
                Column() {
                  Text(f.name)
                    .fontSize(MT.fsSmall)
                    .fontColor($r('app.color.csx_text'))
                    .maxLines(1)
                    .textOverflow({ overflow: TextOverflow.Ellipsis })
                    .width('100%')
                    .textAlign(TextAlign.Start)
                  Text(sizeText(f.size) + ' · ' + clockText(f.mtime))
                    .fontSize(MT.fsMicro)
                    .fontColor($r('app.color.csx_text3'))
                    .margin({ top: 4 })
                    .width('100%')
                    .textAlign(TextAlign.Start)
                }
                .alignItems(HorizontalAlign.Start)
                .layoutWeight(1)

                Text(this.t('查看', 'View'))
                  .fontSize(MT.fsCaption)
                  .fontColor($r('app.color.csx_blue'))
                  .padding({ left: 12, right: 2, top: 6, bottom: 6 })
                  .onClick(() => {
                    this.openAppLog(f)
                  })
              }
              .width('100%')
              .padding({ left: 16, right: 16, top: 11, bottom: 11 })
              .alignItems(VerticalAlign.Center)
            }, (f: AppLogFile) => f.path)
          }
          .width('100%')
          .margin({ top: 10 })
          .borderRadius(MT.radiusCard)
          .backgroundColor($r('app.color.csx_surface'))
          .clip(true)
        }

        Row() {
          Text(this.t('清除日志', 'Clear logs'))
            .fontSize(MT.fsBody)
            .fontColor($r('app.color.csx_danger'))
            .textAlign(TextAlign.Center)
            .height(48)
            .layoutWeight(1)
            .borderRadius(MT.radiusBtn)
            .backgroundColor($r('app.color.csx_surface'))
            .onClick(() => {
              this.clearAppLogs()
            })
          Text(this.t('导出日志', 'Export logs'))
            .fontSize(MT.fsBody)
            .fontColor($r('app.color.csx_accent'))
            .textAlign(TextAlign.Center)
            .height(48)
            .layoutWeight(1)
            .margin({ left: 12 })
            .borderRadius(MT.radiusBtn)
            .backgroundColor($r('app.color.csx_accent_soft'))
            .onClick(() => {
              this.exportAppLogs()
            })
        }
        .width('100%')
        .margin({ top: 20 })
      }
      .width('100%')
      .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 6, bottom: MT.tabbarSpace })
    }
    .layoutWeight(1)
    .align(Alignment.Top)
    .scrollBar(BarState.Off)
    .visibility(this.logTab === 1 ? Visibility.Visible : Visibility.None)
  }

  @Builder LogViewer() {
    Column() {
      Row() {
        Text(this.logTitle)
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text'))
          .layoutWeight(1)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
        Text(this.t('关闭', 'Close'))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_accent'))
          .padding({ left: 12, right: 4, top: 6, bottom: 6 })
          .onClick(() => {
            this.closeLog()
          })
      }
      .width('100%')
      .padding({ left: 16, right: 12, top: 12, bottom: 10 })
      .alignItems(VerticalAlign.Center)

      Scroll() {
        Text(this.logText.length > 0 ? this.logText : this.t('日志文件为空', 'The log file is empty'))
          .fontSize(10)
          .fontColor($r('app.color.csx_text2'))
          .fontFamily('monospace')
          .width('100%')
          .textAlign(TextAlign.Start)
      }
      .layoutWeight(1)
      .align(Alignment.Top)
      .scrollBar(BarState.Auto)
      .width('100%')
      .padding({ left: 14, right: 14, bottom: 20 })
    }
    .width('100%')
    .height('100%')
    .backgroundColor($r('app.color.csx_bg'))
  }

  @Builder LogChip(t: number, label: Bi) {
    Text(this.pick(label))
      .fontSize(MT.fsSmall)
      .fontColor(this.logType === t ? $r('app.color.csx_text') : $r('app.color.csx_text2'))
      .textAlign(TextAlign.Center)
      .height(36)
      .layoutWeight(1)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(this.logType === t ? $r('app.color.csx_surface') : MT.transparent)
      .onClick(() => {
        this.logType = t
        this.persistUi()
      })
  }

  @Builder InputRow(label: Bi, value: string, key: string) {
    Row() {
      Text(this.pick(label))
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
        .width(76)
      TextInput({ text: value })
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
        .backgroundColor(MT.transparent)
        .padding({ left: 0, right: 0 })
        .height(50)
        .layoutWeight(1)
        .onChange((v: string) => {
          this.setProxyField(key, v)
        })
    }
    .width('100%')
    .padding({ left: 16, right: 16 })
    .alignItems(VerticalAlign.Center)
  }
}
