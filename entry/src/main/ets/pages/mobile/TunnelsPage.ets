import { promptAction } from '@kit.ArkUI'
import { isPortForwardingActive } from 'libentry.so'
import { MT } from '../../common/MobileTheme'
import { HostItem, TunnelItem, cloneHost, tunnelCmdLine } from '../../common/Models'
import { SshService } from '../../common/SshService'
import { probeHttp, ProbeState } from '../../common/PortProbe'
import { AppLog } from '../../common/AppLog'
import { L, Bi } from '../../common/I18n'

@Component
export struct TunnelsPage {
  @Link hosts: HostItem[]
  @Link @Watch('onHostSwitch') activeId: number
  @Link masking: boolean
  @Link sheetOpen: boolean
  @State editingId: number = 0
  @State fType: string = 'L'
  @State fPort: string = '8888'
  @State fDest: string = '127.0.0.1:8888'
  @State probeSeq: number = 0
  @StorageLink('csx_lang') csxLang: string = 'zh'
  private probeMap: Map<string, string> = new Map<string, string>()
  private recTimer: number = -1
  onBrowse: (url: string) => void = () => {}
  onToggleHost: () => void = () => {}
  onExit: () => void = () => {}
  onChanged: () => void = () => {}

  t(zh: string, en: string): string {
    return this.csxLang === 'en' ? en : zh
  }

  pick(v: Bi): string {
    return this.csxLang === 'en' ? v.en : v.zh
  }

  curHost(): HostItem | undefined {
    return this.hosts.find((h: HostItem) => h.id === this.activeId)
  }

  aboutToAppear(): void {
    this.startReconcile()
  }

  aboutToDisappear(): void {
    if (this.recTimer >= 0) {
      clearInterval(this.recTimer)
      this.recTimer = -1
    }
  }

  onHostSwitch(): void {
    this.probeMap.clear()
    this.probeSeq = this.probeSeq + 1
  }

  hostReady(): boolean {
    const h: HostItem | undefined = this.curHost()
    return h !== undefined && h.online
  }

  probeKey(t: TunnelItem): string {
    return this.activeId.toString() + ':' + t.id.toString()
  }

  probeStateOf(t: TunnelItem): string {
    if (t.type !== 'L' || !t.on || !this.hostReady()) {
      return 'idle'
    }
    const v: string | undefined = this.probeMap.get(this.probeKey(t))
    return v === undefined ? 'unknown' : v
  }

  probeText(t: TunnelItem): string {
    const st: string = this.probeStateOf(t)
    if (st === 'UP') {
      return this.t('可达', 'Up')
    }
    if (st === 'LISTEN') {
      return this.t('仅监听', 'Listen only')
    }
    if (st === 'DOWN') {
      return this.t('未通', 'Down')
    }
    if (st === 'unknown') {
      return this.t('未探测', 'Unprobed')
    }
    return ''
  }

  probeColorOf(t: TunnelItem): ResourceColor {
    const st: string = this.probeStateOf(t)
    if (st === 'UP') {
      return $r('app.color.csx_accent')
    }
    if (st === 'LISTEN') {
      return $r('app.color.csx_warn')
    }
    if (st === 'DOWN') {
      return $r('app.color.csx_danger')
    }
    return $r('app.color.csx_text3')
  }

  setProbe(key: string, st: string): void {
    this.probeMap.set(key, st)
    this.probeSeq = this.probeSeq + 1
    AppLog.i('probe', `tunnel ${key} -> ${st}`)
  }

  probeOne(t: TunnelItem): void {
    if (!/^\d+$/.test(t.port)) {
      return
    }
    probeHttp('127.0.0.1', Number(t.port), 1500).then((st: ProbeState) => {
      this.setProbe(this.probeKey(t), st)
    })
  }

  probeAll(): void {
    const h: HostItem | undefined = this.curHost()
    if (!h || !h.online) {
      promptAction.showToast({ message: this.t('请先连接主机', 'Connect a host first') })
      return
    }
    for (let i = 0; i < h.tunnels.length; i++) {
      const t: TunnelItem = h.tunnels[i]
      if (t.type === 'L' && t.on) {
        this.probeOne(t)
      }
    }
  }

  startReconcile(): void {
    if (this.recTimer >= 0) {
      return
    }
    this.recTimer = setInterval(() => {
      this.reconcileForwards()
    }, 15000)
  }

  async reconcileForwards(): Promise<void> {
    const h: HostItem | undefined = this.curHost()
    if (h === undefined || !h.online) {
      return
    }
    const sid: number = SshService.get().sessionOf(h.id)
    if (sid < 0) {
      return
    }
    let changed: boolean = false
    for (let i = 0; i < h.tunnels.length; i++) {
      const t: TunnelItem = h.tunnels[i]
      if (!t.on) {
        continue
      }
      let active: boolean = true
      try {
        active = await isPortForwardingActive(sid, Number(t.port), t.type === 'R')
      } catch (e) {
        continue
      }
      if (active) {
        continue
      }
      AppLog.w('fwd', `reconcile ${t.type}${t.port} not active, mark off`)
      this.applyToggle(t.id, false)
      this.probeMap.delete(this.probeKey(t))
      this.probeSeq = this.probeSeq + 1
      changed = true
    }
    if (changed) {
      promptAction.showToast({
        message: this.t('有转发已失效，已自动关闭', 'Some forwards were lost and turned off'),
        duration: 4000
      })
    }
  }

  askReset(port: number): Promise<boolean> {
    return new Promise<boolean>((resolve: (v: boolean) => void) => {
      promptAction.showDialog({
        title: this.t('隧道未打通', 'Tunnel not working'),
        message: this.t(`端口 ${port} 已在监听，但没有数据返回，通常是服务器拒绝了 direct-tcpip。是否重置该隧道再试？`,
          `Port ${port} is listening but returns no data. The server likely refused direct-tcpip. Reset the tunnel and retry?`),
        buttons: [
          { text: this.t('取消', 'Cancel'), color: '#8A93A6' },
          { text: this.t('重置', 'Reset'), color: '#3DDC97' }
        ]
      }).then((r: promptAction.ShowDialogSuccessResponse) => {
        resolve(r.index === 1)
      }).catch(() => {
        resolve(false)
      })
    })
  }

  async resetTunnel(h: HostItem, t: TunnelItem): Promise<string> {
    try {
      await SshService.get().stopForward(h.id, t)
      await SshService.get().startForward(h.id, t)
    } catch (e) {
      const msg: string = e instanceof Error ? e.message : String(e)
      AppLog.e('probe', `reset tunnel ${t.type}${t.port} failed: ${msg}`)
      return 'DOWN'
    }
    const st: ProbeState = await probeHttp('127.0.0.1', Number(t.port), 2000)
    this.setProbe(this.probeKey(t), st)
    return st
  }

  async openBrowser(t: TunnelItem): Promise<void> {
    const h: HostItem | undefined = this.curHost()
    if (!h || !h.online) {
      promptAction.showToast({ message: this.t('请先连接主机', 'Connect a host first') })
      return
    }
    const port: number = Number(t.port)
    if (!t.on) {
      try {
        await SshService.get().startForward(h.id, t)
        this.applyToggle(t.id, true)
      } catch (e) {
        const msg: string = e instanceof Error ? e.message : String(e)
        promptAction.showToast({ message: this.fwdErr(msg), duration: 4000 })
        return
      }
    }
    const st: ProbeState = await probeHttp('127.0.0.1', port, 1500)
    this.setProbe(this.probeKey(t), st)
    if (st === 'UP') {
      this.onBrowse(`http://127.0.0.1:${port}`)
      return
    }
    if (st === 'LISTEN') {
      const go: boolean = await this.askReset(port)
      if (!go) {
        return
      }
      const st2: string = await this.resetTunnel(h, t)
      if (st2 === 'UP') {
        this.onBrowse(`http://127.0.0.1:${port}`)
        return
      }
      const parts: string[] = SshService.get().forwardStats(h.id).split(',')
      const fails: string = parts.length > 1 ? parts[1] : '0'
      promptAction.showToast({
        message: this.t(`重置后仍取不到数据（通道失败 ${fails} 次），请检查服务器是否允许端口转发`,
          `Still no data after reset (${fails} channel failure(s)); check whether the server allows forwarding`),
        duration: 4000
      })
      return
    }
    promptAction.showToast({
      message: this.t(`端口 ${port} 未监听，隧道未建立成功`, `Port ${port} is not listening; the tunnel is down`),
      duration: 4000
    })
  }

  toggleTunnel(tid: number): void {
    const h: HostItem | undefined = this.curHost()
    if (h === undefined) {
      return
    }
    const t: TunnelItem | undefined = h.tunnels.find((x: TunnelItem) => x.id === tid)
    if (t === undefined) {
      return
    }
    const turnOn: boolean = !t.on
    if (turnOn && !h.online) {
      promptAction.showToast({ message: this.t('请先连接主机', 'Connect a host first') })
      return
    }
    const prev: boolean = t.on
    this.applyToggle(tid, turnOn)
    const job: Promise<void> = turnOn
      ? SshService.get().startForward(h.id, t)
      : SshService.get().stopForward(h.id, t)
    job.then(() => {
      if (turnOn && t.type === 'L') {
        this.probeOne(t)
      }
      promptAction.showToast({ message: turnOn
        ? this.t(`已开启 ${t.type}${t.port}`, `Enabled ${t.type}${t.port}`)
        : this.t(`已停止 ${t.type}${t.port}`, `Stopped ${t.type}${t.port}`) })
    }).catch((e: Error) => {
      this.applyToggle(tid, prev)
      promptAction.showToast({ message: this.fwdErr(e.message), duration: 4000 })
    })
  }

  applyToggle(tid: number, on: boolean): void {
    this.hosts = this.hosts.map((h: HostItem) => {
      if (h.id !== this.activeId) {
        return h
      }
      const ts: TunnelItem[] = h.tunnels.map((t: TunnelItem) => {
        if (t.id !== tid) {
          return t
        }
        const nt: TunnelItem = { id: t.id, type: t.type, port: t.port, dest: t.dest, on: on }
        return nt
      })
      return cloneHost(h, ts)
    })
    this.onChanged()
  }

  fwdErr(raw: string): string {
    if (raw.indexOf('NOT_CONNECTED') >= 0) {
      return this.t('请先连接主机', 'Connect a host first')
    }
    if (raw.indexOf('FORWARD_FAILED') >= 0) {
      return this.t('转发启动失败：端口被占用，或服务器禁止转发', 'Forward failed: port busy or forwarding disabled')
    }
    return raw
  }

  removeTunnel(tid: number): void {
    const h: HostItem | undefined = this.curHost()
    if (h === undefined) {
      return
    }
    const t: TunnelItem | undefined = h.tunnels.find((x: TunnelItem) => x.id === tid)
    if (t !== undefined && t.on && h.online) {
      SshService.get().stopForward(h.id, t).catch((): void => {})
    }
    this.hosts = this.hosts.map((x: HostItem) => {
      if (x.id !== this.activeId) {
        return x
      }
      return cloneHost(x, x.tunnels.filter((y: TunnelItem) => y.id !== tid))
    })
    this.onChanged()
  }

  openSheet(t: TunnelItem | undefined): void {
    if (t) {
      this.editingId = t.id
      this.fType = t.type
      this.fPort = t.port
      this.fDest = t.dest
    } else {
      this.editingId = 0
      this.fType = 'L'
      this.fPort = '8888'
      this.fDest = '127.0.0.1:8888'
    }
    this.sheetOpen = true
    this.masking = true
  }

  pickType(tp: string): void {
    if (this.fType === tp) {
      return
    }
    this.fType = tp
    if (tp === 'L') {
      this.fPort = '8888'
      this.fDest = '127.0.0.1:8888'
    } else if (tp === 'R') {
      this.fPort = '9000'
      this.fDest = '192.168.1.10:22'
    } else {
      this.fPort = '1080'
      this.fDest = ''
    }
  }

  saveTunnel(): void {
    const host = this.curHost()
    if (!host) {
      return
    }
    const p: number = Number(this.fPort)
    if (!/^\d+$/.test(this.fPort) || p < 1 || p > 65535) {
      promptAction.showToast({ message: this.t('端口需为 1-65535 的数字', 'Port must be a number between 1 and 65535') })
      return
    }
    if (this.fType !== 'D') {
      const parts: string[] = this.fDest.split(':')
      if (parts.length !== 2 || !/^\d+$/.test(parts[1])) {
        promptAction.showToast({ message: this.t('目标地址格式应为 host:port', 'Target must be in host:port format') })
        return
      }
      if (this.fType === 'R' && !/^\d+\.\d+\.\d+\.\d+$/.test(parts[0])) {
        promptAction.showToast({ message: this.t('远程转发目标只支持 IP，不支持域名', 'Remote target supports IP only, not domain names') })
        return
      }
    }
    const dest: string = this.fType === 'D' ? '' : this.fDest
    const dup: TunnelItem[] = host.tunnels.filter((t: TunnelItem) =>
      t.type === this.fType && t.port === this.fPort && t.id !== this.editingId)
    if (dup.length > 0) {
      promptAction.showToast({ message: this.t(`${this.fType}${this.fPort} 已存在`, `${this.fType}${this.fPort} already exists`) })
      return
    }
    if (this.fType === 'L' || this.fType === 'D') {
      const other: string = this.fType === 'L' ? 'D' : 'L'
      const clash: TunnelItem[] = host.tunnels.filter((t: TunnelItem) =>
        t.type === other && t.port === this.fPort && t.id !== this.editingId)
      if (clash.length > 0) {
        promptAction.showToast({ message: this.t(`本地与动态转发不能共用端口 ${this.fPort}`, `Local and dynamic forwards cannot share port ${this.fPort}`) })
        return
      }
    }
    if (this.editingId > 0) {
      const before: TunnelItem | undefined = host.tunnels.find((t: TunnelItem) => t.id === this.editingId)
      const wasOn: boolean = before !== undefined && before.on
      this.hosts = this.hosts.map((h: HostItem) => {
        if (h.id !== this.activeId) {
          return h
        }
        const ts: TunnelItem[] = h.tunnels.map((t: TunnelItem) => {
          if (t.id !== this.editingId) {
            return t
          }
          const nt: TunnelItem = { id: t.id, type: this.fType, port: this.fPort, dest: dest, on: t.on }
          return nt
        })
        return cloneHost(h, ts)
      })
      promptAction.showToast({ message: this.t('已保存', 'Saved') })
      if (host.online && before !== undefined && wasOn) {
        const cur: HostItem | undefined = this.curHost()
        if (cur !== undefined) {
          const after: TunnelItem | undefined = cur.tunnels.find((t: TunnelItem) => t.id === this.editingId)
          if (after !== undefined) {
            const target: TunnelItem = after
            SshService.get().stopForward(host.id, before).then(() => {
              return SshService.get().startForward(host.id, target)
            }).catch((e: Error) => {
              this.applyToggle(this.editingId, false)
              promptAction.showToast({ message: this.fwdErr(e.message), duration: 4000 })
            })
          }
        }
      }
    } else {
      const nextId: number = Date.now()
      const nt: TunnelItem = { id: nextId, type: this.fType, port: this.fPort, dest: dest, on: host.online }
      this.hosts = this.hosts.map((h: HostItem) => {
        if (h.id !== this.activeId) {
          return h
        }
        return cloneHost(h, h.tunnels.concat([nt]))
      })
      promptAction.showToast({ message: this.t('已添加转发', 'Forwarding added') })
      if (host.online) {
        SshService.get().startForward(host.id, nt).catch((e: Error) => {
          this.applyToggle(nextId, false)
          promptAction.showToast({ message: this.fwdErr(e.message), duration: 4000 })
        })
      }
    }
    this.onChanged()
    this.sheetOpen = false
    this.masking = false
  }

  build() {
    Stack({ alignContent: Alignment.Bottom }) {
      Column() {
        if (this.curHost() === undefined) {
          this.EmptyState()
        } else {
          Scroll() {
            Column() {
              this.StatusCard()
              if (!this.curHost()!.online) {
                Row() {
                  Text(this.t('主机未连接，转发开关当前不可用', 'Host not connected; forwarding is unavailable'))
                    .fontSize(MT.fsCaption)
                    .fontColor($r('app.color.csx_warn'))
                    .layoutWeight(1)
                  Text(this.t('去连接', 'Connect'))
                    .fontSize(MT.fsCaption)
                    .fontColor($r('app.color.csx_accent'))
                    .padding({ left: 10, right: 4, top: 4, bottom: 4 })
                    .onClick(() => {
                      this.onToggleHost()
                    })
                }
                .width('100%')
                .padding({ left: 13, right: 13, top: 11, bottom: 11 })
                .margin({ bottom: 14 })
                .borderRadius(MT.radiusCard)
                .backgroundColor($r('app.color.csx_surface2'))
              }
              Row() {
                Text(this.t('隧道状态', 'Tunnel status'))
                  .fontSize(MT.fsCaption)
                  .fontColor($r('app.color.csx_text3'))
                  .layoutWeight(1)
                Text(this.t('重新探测', 'Re-probe'))
                  .fontSize(MT.fsCaption)
                  .fontColor($r('app.color.csx_accent'))
                  .padding({ left: 8, right: 2, top: 4, bottom: 4 })
                  .onClick(() => {
                    this.probeAll()
                  })
              }
              .width('100%')
              .margin({ left: 4, right: 4, bottom: 8 })

              ForEach(this.curHost()!.tunnels, (t: TunnelItem) => {
                this.TunnelRow(t)
              }, (t: TunnelItem) => `${t.id}-${t.on}-${t.type}-${t.port}-${t.dest}-${this.curHost()!.online}-${this.probeSeq}`)
              Row() {
                Text(this.t('+ 新建端口转发', '+ New port forwarding'))
                  .fontSize(MT.fsBody)
                  .fontColor($r('app.color.csx_accent'))
              }
              .width('100%')
              .height(50)
              .justifyContent(FlexAlign.Center)
              .borderRadius(MT.radiusBtn)
              .border({ width: 1, color: $r('app.color.csx_line'), style: BorderStyle.Dashed })
              .margin({ top: 4 })
              .onClick(() => {
                this.openSheet(undefined)
              })
              Text(tunnelCmdLine(this.curHost()!))
                .fontSize(MT.fsCaption)
                .fontColor($r('app.color.csx_text3'))
                .fontFamily('monospace')
                .width('100%')
                .margin({ top: 16 })
                .padding(12)
                .borderRadius(MT.radiusCtrl)
                .backgroundColor($r('app.color.csx_surface2'))
            }
            .width('100%')
            .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: MT.tabbarSpace })
          }
          .layoutWeight(1)
          .align(Alignment.Top)
          .scrollBar(BarState.Off)
        }
      }
      .width('100%')
      .height('100%')

      if (this.sheetOpen) {
        this.TunnelSheet()
      }
    }
    .width('100%')
    .height('100%')
  }

  @Builder StatusCard() {
    Row() {
      Circle({ width: 8, height: 8 })
        .fill(this.curHost()!.online ? $r('app.color.csx_accent') : $r('app.color.csx_text3'))
      Column() {
        Text(this.curHost()!.online ? this.t('SSH 已连接', 'SSH connected') : this.t('未连接', 'Not connected'))
          .fontSize(MT.fsBody)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_text'))
        Text(`${this.curHost()!.user}@${this.curHost()!.host}:${this.curHost()!.port}`)
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text3'))
          .margin({ top: 5 })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)
      .margin({ left: 10 })

      Text(this.curHost()!.online ? this.t('断开', 'Disconnect') : this.t('连接', 'Connect'))
        .fontSize(MT.fsSmall)
        .fontColor(this.curHost()!.online ? $r('app.color.csx_danger') : $r('app.color.csx_accent'))
        .padding({ left: 12, right: 12, top: 7, bottom: 7 })
        .borderRadius(MT.radiusCtrl)
        .backgroundColor($r('app.color.csx_surface2'))
        .onClick(() => {
          this.onToggleHost()
        })
    }
    .width('100%')
    .padding(15)
    .margin({ bottom: 18 })
    .borderRadius(MT.radiusCard)
    .backgroundColor($r('app.color.csx_surface'))
    .alignItems(VerticalAlign.Center)
  }

  @Builder TunnelRow(t: TunnelItem) {
    Row() {
      Text(t.type)
        .fontSize(MT.fsCaption)
        .fontWeight(FontWeight.Bold)
        .fontColor(t.type === 'L' ? $r('app.color.csx_accent') : t.type === 'R' ? $r('app.color.csx_blue') : $r('app.color.csx_violet'))
        .width(22)
        .height(22)
        .textAlign(TextAlign.Center)
        .borderRadius(MT.radiusChip)
        .backgroundColor(t.type === 'L' ? $r('app.color.csx_accent_soft') : t.type === 'R' ? $r('app.color.csx_blue_soft') : '#26B48BFF')

      Column() {
        Text(this.t(`${t.type === 'D' ? '动态' : t.type === 'L' ? '本地' : '远程'}转发 · ${t.port}`, `${t.type === 'D' ? 'Dynamic' : t.type === 'L' ? 'Local' : 'Remote'} forward · ${t.port}`))
          .fontSize(MT.fsBody)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_text'))
        Row() {
          Text(t.type === 'D' ? this.t('作为 SOCKS5 代理端口', 'Acts as a SOCKS5 proxy port') : `→ ${t.dest}`)
            .fontSize(MT.fsCaption)
            .fontColor($r('app.color.csx_text3'))
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
            .layoutWeight(1)
          if (this.probeText(t).length > 0) {
            Text(this.probeText(t))
              .fontSize(MT.fsMicro)
              .fontColor(this.probeColorOf(t))
              .padding({ left: 6, right: 6, top: 2, bottom: 2 })
              .borderRadius(6)
              .backgroundColor($r('app.color.csx_surface2'))
              .margin({ left: 6 })
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => {
                this.probeOne(t)
              })
          }
        }
        .width('100%')
        .margin({ top: 5 })
        .alignItems(VerticalAlign.Center)
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)
      .margin({ left: 11 })

      if (t.type === 'L') {
        Text(this.t('浏览', 'Browse'))
          .fontSize(MT.fsCaption)
          .fontColor(t.on && this.curHost()!.online ? $r('app.color.csx_accent') : $r('app.color.csx_text3'))
          .height(28)
          .padding({ left: 11, right: 11 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor($r('app.color.csx_surface2'))
          .margin({ right: 8 })
          .hitTestBehavior(HitTestMode.Block)
          .onClick(() => {
            this.openBrowser(t)
          })
      }

      Toggle({ type: ToggleType.Switch, isOn: t.on && this.curHost()!.online })
        .selectedColor($r('app.color.csx_accent'))
        .switchPointColor('#FFFFFF')
        .enabled(this.curHost()!.online)
        .onChange(() => {
          this.toggleTunnel(t.id)
        })
    }
    .width('100%')
    .padding(14)
    .margin({ bottom: 10 })
    .borderRadius(MT.radiusCard)
    .backgroundColor($r('app.color.csx_surface'))
    .border({ width: 1, color: t.on ? $r('app.color.csx_accent_soft') : MT.transparent })
    .alignItems(VerticalAlign.Center)
    .onClick(() => {
      this.openSheet(t)
    })
  }

  @Builder EmptyState() {
    Column() {
      Text(this.t('还没连接主机', 'No host connected'))
        .fontSize(MT.fsTitle)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('app.color.csx_text2'))
      Text(this.t('端口转发需要一条活动的 SSH 连接', 'Port forwarding needs an active SSH connection'))
        .fontSize(MT.fsSmall)
        .fontColor($r('app.color.csx_text3'))
        .margin({ top: 8 })
      Text(this.t('去连接主机', 'Connect a host'))
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_accent_fg'))
        .textAlign(TextAlign.Center)
        .width(150)
        .height(44)
        .borderRadius(MT.radiusBtn)
        .backgroundColor($r('app.color.csx_accent'))
        .margin({ top: 20 })
        .onClick(() => {
          this.sheetOpen = false
          this.masking = false
          this.onExit()
        })
    }
    .width('100%')
    .layoutWeight(1)
    .justifyContent(FlexAlign.Center)
    .padding({ bottom: 80 })
  }

  @Builder TunnelSheet() {
    Stack({ alignContent: Alignment.Bottom }) {
      Column()
        .width('100%')
        .height('100%')
        .backgroundColor('#A6000000')
        .onClick(() => {
          this.sheetOpen = false
          this.masking = false
        })

      Column() {
        Row() {
          Text(this.editingId > 0 ? this.t('编辑端口转发', 'Edit port forwarding') : this.t('新建端口转发', 'New port forwarding'))
            .fontSize(MT.fsTitle)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('app.color.csx_text'))
            .layoutWeight(1)
          if (this.editingId > 0) {
            Text(this.t('删除', 'Delete'))
              .fontSize(MT.fsSmall)
              .fontColor($r('app.color.csx_danger'))
              .onClick(() => {
                this.removeTunnel(this.editingId)
                this.sheetOpen = false
                this.masking = false
                promptAction.showToast({ message: this.t('已删除', 'Deleted') })
              })
          }
        }
        .width('100%')
        .margin({ bottom: 16 })

        Row() {
          this.TypeChip('L', L.b('本地', 'Local'))
          this.TypeChip('R', L.b('远程', 'Remote'))
          this.TypeChip('D', L.b('动态', 'Dynamic'))
        }
        .width('100%')
        .padding(4)
        .borderRadius(MT.radiusCard)
        .backgroundColor($r('app.color.csx_surface2'))

        Text(this.typeDesc())
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text3'))
          .lineHeight(18)
          .width('100%')
          .margin({ top: 12, bottom: 14 })

        this.PortField()
        if (this.fType !== 'D') {
          this.DestField()
        }

        Text(this.editingId > 0 ? this.t('保存', 'Save') : this.t('添加', 'Add'))
          .fontSize(MT.fsTitle)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_accent_fg'))
          .textAlign(TextAlign.Center)
          .width('100%')
          .height(50)
          .borderRadius(MT.radiusBtn)
          .backgroundColor($r('app.color.csx_accent'))
          .margin({ top: 8 })
          .onClick(() => {
            this.saveTunnel()
          })
      }
      .width('100%')
      .padding({ left: 20, right: 20, top: 20, bottom: 34 })
      .backgroundColor($r('app.color.csx_surface'))
      .borderRadius({ topLeft: 24, topRight: 24 })
    }
    .width('100%')
    .height('100%')
  }

  @Builder TypeChip(tp: string, label: Bi) {
    Text(this.pick(label))
      .fontSize(MT.fsSmall)
      .fontColor(this.fType === tp ? $r('app.color.csx_text') : $r('app.color.csx_text2'))
      .textAlign(TextAlign.Center)
      .height(36)
      .layoutWeight(1)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(this.fType === tp ? $r('app.color.csx_surface') : MT.transparent)
      .onClick(() => {
        this.pickType(tp)
      })
  }

  @Builder PortField() {
    Row() {
      Text(this.t('端口', 'Port'))
        .fontSize(MT.fsSmall)
        .fontColor($r('app.color.csx_text2'))
        .width(96)
      TextInput({ text: this.fPort })
        .type(InputType.Number)
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
        .backgroundColor(MT.transparent)
        .padding({ left: 0, right: 0 })
        .height(44)
        .layoutWeight(1)
        .onChange((v: string) => {
          this.fPort = v
        })
    }
    .width('100%')
    .height(48)
    .padding({ left: 14, right: 14 })
    .margin({ bottom: 10 })
    .borderRadius(MT.radiusCard)
    .backgroundColor($r('app.color.csx_surface2'))
    .alignItems(VerticalAlign.Center)
  }

  @Builder DestField() {
    Row() {
      Text(this.fType === 'R' ? this.t('手机侧目标', 'Phone-side target') : this.t('目标地址', 'Target address'))
        .fontSize(MT.fsSmall)
        .fontColor($r('app.color.csx_text2'))
        .width(96)
      TextInput({ text: this.fDest, placeholder: 'host:port' })
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
        .placeholderColor($r('app.color.csx_text3'))
        .backgroundColor(MT.transparent)
        .padding({ left: 0, right: 0 })
        .height(44)
        .layoutWeight(1)
        .onChange((v: string) => {
          this.fDest = v
        })
    }
    .width('100%')
    .height(48)
    .padding({ left: 14, right: 14 })
    .margin({ bottom: 10 })
    .borderRadius(MT.radiusCard)
    .backgroundColor($r('app.color.csx_surface2'))
    .alignItems(VerticalAlign.Center)
  }

  typeDesc(): string {
    if (this.fType === 'L') {
      return this.t('在手机上开一个端口。访问手机的 127.0.0.1:端口，流量经 SSH 通道送到服务器，由服务器去连目标。填 127.0.0.1 时指的是服务器本机。', 'Open a port on the phone. Traffic to 127.0.0.1:port on the phone goes through the SSH channel to the server, which then connects to the target. 127.0.0.1 means the server itself.')
    }
    if (this.fType === 'R') {
      return this.t('在服务器上开一个端口。别人连服务器的该端口时，由手机去连目标再回传。目标只支持 IP；服务器需开启 GatewayPorts。', 'Open a port on the server. When someone connects to that port, the phone connects to the target and relays the data back. Target must be an IP; GatewayPorts must be enabled on the server.')
    }
    return this.t('在手机上开一个 SOCKS5 端口，目标在每次连接时才指定。浏览器代理填 127.0.0.1:端口 即可让全部流量走服务器。', 'Open a SOCKS5 port on the phone; the target is chosen per connection. Set your browser proxy to 127.0.0.1:port to route all traffic through the server.')
  }
}
