import { window, promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { HostItem, cloneHost, rollStats, seedHosts } from '../../common/Models'
import { Ic } from '../../common/Icons'
import { HostsPage } from './HostsPage'
import { HostSettingsPage } from './HostSettingsPage'
import { TunnelsPage } from './TunnelsPage'
import { FilesPage } from './FilesPage'
import { SettingsPage } from './SettingsPage'
import { TerminalPro } from './TerminalPro'
import { BrowserPage } from './BrowserPage'
import { L } from '../../common/I18n'

PersistentStorage.persistProp('csx_themeDark', true)
PersistentStorage.persistProp('csx_lang', 'zh')

@Entry
@Component
struct MobileIndex {
  @State view: number = 0
  @State sessTab: number = 0
  @State hosts: HostItem[] = []
  @State activeId: number = 0
  @State editId: number = 0
  @State termShow: boolean = false
  @State showBrowser: boolean = false
  @State browserUrl: string = ''
  @State masking: boolean = false
  @StorageLink('csx_themeDark') themeDark: boolean = true
  @StorageProp('csx_themeVer') themeSeq: number = 0

  aboutToAppear(): void {
    L.lang = AppStorage.get<string>('csx_lang') ?? 'zh'
    this.hosts = seedHosts()
    MT.apply(this.themeDark)
    this.applyBars()
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
    return h === undefined ? L.t('未连接', 'Not connected') : h.name
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
        nh.latency = '12 ms'
        nh.stats = rollStats()
      } else {
        nh.online = false
        nh.latency = ''
      }
      return nh
    })
  }

  connectToggle(id: number): void {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    if (h === undefined || h.state === 'connecting') {
      return
    }
    if (h.state === 'connected') {
      this.setState(id, 'idle')
      if (this.activeId === id && this.view === 1) {
        this.view = 0
      }
      promptAction.showToast({ message: L.t(`已断开 ${h.name}`, `Disconnected ${h.name}`) })
      return
    }
    this.setState(id, 'connecting')
    promptAction.showToast({ message: L.t(`正在连接 ${h.user}@${h.host} …`, `Connecting ${h.user}@${h.host} …`) })
    setTimeout(() => {
      this.setState(id, 'connected')
      promptAction.showToast({ message: L.t(`已连接 ${h.name}`, `Connected ${h.name}`) })
    }, 1200)
  }

  openSession(id: number, tab: number): void {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === id)
    if (h === undefined) {
      return
    }
    if (h.state !== 'connected') {
      promptAction.showToast({ message: L.t('请先连接该主机', 'Connect this host first') })
      return
    }
    this.activeId = id
    this.sessTab = tab
    this.termShow = true
    this.view = 1
  }

  leaveSession(): void {
    this.view = 0
  }

  disconnectActive(): void {
    const h: HostItem | undefined = this.curHost()
    if (h === undefined) {
      return
    }
    this.setState(h.id, 'idle')
    this.view = 0
    promptAction.showToast({ message: L.t(`已断开 ${h.name}`, `Disconnected ${h.name}`) })
  }

  segLabel(i: number): string {
    if (i === 0) {
      return L.t('终端', 'Terminal')
    }
    if (i === 1) {
      return L.t('隧道', 'Tunnels')
    }
    return L.t('文件', 'Files')
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

  @Builder SegButton(i: number) {
    Row() {
      Ic({ name: this.segIcon(i), icSize: 14, color: this.sessTab === i ? MT.text : MT.text2, sw: 2 })
      Text(this.segLabel(i))
        .fontSize(MT.fsBody)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.sessTab === i ? MT.text : MT.text2)
        .margin({ left: 6 })
    }
    .justifyContent(FlexAlign.Center)
    .layoutWeight(1)
    .height(40)
    .borderRadius(11)
    .backgroundColor(this.sessTab === i ? MT.surface2 : MT.transparent)
    .animation({ duration: 180, curve: Curve.EaseOut })
    .onClick(() => {
      this.sessTab = i
    })
  }

  @Builder SessionBar() {
    Row() {
      Stack() {
        Ic({ name: 'back', icSize: 18, color: MT.text2, sw: 2 })
      }
      .width(36)
      .height(36)
      .borderRadius(12)
      .backgroundColor(MT.surface2)
      .onClick(() => {
        this.leaveSession()
      })

      Column() {
        Text(this.activeName())
          .fontSize(16.5)
          .fontWeight(FontWeight.Medium)
          .fontColor(MT.text)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
        Text(this.activeAddr() + L.t(' · 已连接', ' · Connected'))
          .fontSize(MT.fsMicro)
          .fontColor(MT.accent)
          .margin({ top: 2 })
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)
      .margin({ left: 10 })

      Row() {
        Ic({ name: 'power', icSize: 13, color: MT.danger, sw: 2 })
        Text(L.t('断开', 'Disconnect'))
          .fontSize(MT.fsCaption)
          .fontWeight(FontWeight.Medium)
          .fontColor(MT.danger)
          .margin({ left: 5 })
      }
      .height(32)
      .padding({ left: 13, right: 13 })
      .borderRadius(999)
      .backgroundColor('#1AF87171')
      .onClick(() => {
        this.disconnectActive()
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
    .backgroundColor(MT.surface)
  }

  @Builder HostSettingsView() {
    Column() {
      HostSettingsPage({
        hosts: $hosts,
        editId: this.editId,
        onBack: () => {
          this.view = 0
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
          Ic({ name: 'back', icSize: 18, color: MT.text2, sw: 2 })
        }
        .width(36)
        .height(36)
        .borderRadius(12)
        .backgroundColor(MT.surface2)
        .onClick(() => {
          this.view = 0
        })
        Blank()
      }
      .width('100%')
      .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 8, bottom: 4 })
      .alignItems(VerticalAlign.Center)

      SettingsPage({ themeDark: $themeDark })
    }
    .width('100%')
    .height('100%')
    .visibility(this.view === 3 ? Visibility.Visible : Visibility.None)
  }

  build() {
    Stack() {
      if (this.themeSeq % 2 === 0) {
        this.Body()
      } else {
        this.Body()
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.themeSeq >= 0 ? MT.bg : '#FF00FF')
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
          this.view = 3
        },
        onNewHost: () => {
          promptAction.showToast({ message: L.t('新建主机（数据层待接内核）', 'New host (data layer pending)') })
        }
      })
        .visibility(this.view === 0 ? Visibility.Visible : Visibility.None)

      Column() {
        this.SessionBar()
        this.SessionSeg()
        Stack() {
          TerminalPro({
            show: $termShow,
            hostName: this.activeName(),
            hostAddr: this.activeAddr(),
            compact: true
          })
            .visibility(this.sessTab === 0 ? Visibility.Visible : Visibility.None)

          TunnelsPage({
            hosts: $hosts,
            activeId: $activeId,
            masking: $masking,
            onBrowse: (url: string) => {
              this.browserUrl = url
              this.showBrowser = true
            },
            onExit: () => {
              this.view = 0
            }
          })
            .visibility(this.sessTab === 1 ? Visibility.Visible : Visibility.None)

          FilesPage({ connected: this.connected(), hostName: this.activeName(), masking: $masking })
            .visibility(this.sessTab === 2 ? Visibility.Visible : Visibility.None)
        }
        .width('100%')
        .layoutWeight(1)
      }
      .width('100%')
      .height('100%')
      .visibility(this.view === 1 ? Visibility.Visible : Visibility.None)

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
