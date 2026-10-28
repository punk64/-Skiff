import { promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { Ic } from '../../common/Icons'
import { HostItem, TunnelItem } from '../../common/Models'
import { L } from '../../common/I18n'

@Component
export struct HostsPage {
  @Link hosts: HostItem[]
  @StorageProp('csx_themeVer') themeVer: number = 0
  onConnectToggle: (id: number) => void = () => {}
  onOpenSession: (id: number, tab: number) => void = () => {}
  onHostSettings: (id: number) => void = () => {}
  onAppSettings: () => void = () => {}
  onNewHost: () => void = () => {}
  @State keyword: string = ''

  filtered(): HostItem[] {
    const k: string = this.keyword.trim().toLowerCase()
    if (k.length === 0) {
      return this.hosts
    }
    return this.hosts.filter((h: HostItem) => h.name.toLowerCase().includes(k) || h.host.includes(k))
  }

  connectedCount(): number {
    return this.hosts.filter((h: HostItem) => h.state === 'connected').length
  }

  isOn(h: HostItem): boolean {
    return h.state === 'connected'
  }

  isBusy(h: HostItem): boolean {
    return h.state === 'connecting'
  }

  removeHost(h: HostItem): void {
    this.hosts = this.hosts.filter((x: HostItem) => x.id !== h.id)
    promptAction.showToast({ message: L.t(`已删除 ${h.name}`, `Deleted ${h.name}`) })
  }

  openQuick(h: HostItem, tab: number): void {
    if (h.state !== 'connected') {
      promptAction.showToast({ message: L.t('请先连接该主机', 'Connect this host first') })
      return
    }
    this.onOpenSession(h.id, tab)
  }

  build() {
    if (this.themeVer >= 0) {
      Column() {
        this.Header()
        this.SearchBar()
        if (this.filtered().length === 0) {
          this.EmptyHint()
        } else {
          List() {
            ForEach(this.filtered(), (h: HostItem) => {
              ListItem() {
                this.HostCard(h)
              }
              .swipeAction({ end: this.SwipeActions(h) })
            }, (h: HostItem) => `${h.id}-${h.state}-${h.name}-${h.host}-${h.port}`)
          }
          .width('100%')
          .layoutWeight(1)
          .scrollBar(BarState.Off)
          .edgeEffect(EdgeEffect.Spring)
          .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 12, bottom: 30 })
          .alignListItem(ListItemAlign.Start)
        }
      }
      .width('100%')
      .height('100%')
    }
  }

  @Builder Header() {
    Row() {
      Column() {
        Text(L.t('主机', 'Hosts'))
          .fontSize(MT.fsDisplay)
          .fontWeight(FontWeight.Bold)
          .fontColor(MT.text)
        Text(L.t(`${this.hosts.length} 台主机 · ${this.connectedCount()} 台已连接`, `${this.hosts.length} hosts · ${this.connectedCount()} connected`))
          .fontSize(MT.fsSmall)
          .fontColor(MT.text3)
          .margin({ top: 5 })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)

      Stack() {
        Ic({ name: 'gear', icSize: 18, color: MT.text2 })
      }
      .width(36)
      .height(36)
      .borderRadius(12)
      .backgroundColor(MT.surface)
      .margin({ left: 8 })
      .onClick(() => {
        this.onAppSettings()
      })

      Stack() {
        Ic({ name: 'plus', icSize: 18, color: MT.accentFg, sw: 2.2 })
      }
      .width(36)
      .height(36)
      .borderRadius(12)
      .backgroundColor(MT.accent)
      .margin({ left: 8 })
      .onClick(() => {
        this.onNewHost()
      })
    }
    .width('100%')
    .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 16, bottom: 14 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder SearchBar() {
    Row() {
      TextInput({ placeholder: L.t('搜索主机名或地址', 'Search host or address'), text: this.keyword })
        .placeholderColor(MT.text3)
        .fontColor(MT.text)
        .fontSize(MT.fsBody)
        .backgroundColor(MT.transparent)
        .padding({ left: 0, right: 0 })
        .height(44)
        .layoutWeight(1)
        .onChange((v: string) => {
          this.keyword = v
        })
    }
    .width('100%')
    .height(46)
    .padding({ left: 14, right: 14 })
    .margin({ left: MT.pagePadding, right: MT.pagePadding })
    .borderRadius(MT.radiusCard)
    .backgroundColor(MT.surface)
  }

  @Builder SwipeActions(h: HostItem) {
    Row() {
      Stack() {
        Ic({ name: 'gear', icSize: 17, color: MT.text2 })
      }
      .width(46)
      .height(46)
      .borderRadius(15)
      .backgroundColor(MT.surface2)
      .onClick(() => {
        this.onHostSettings(h.id)
      })

      Stack() {
        Ic({ name: 'trash', icSize: 17, color: MT.danger })
      }
      .width(46)
      .height(46)
      .borderRadius(15)
      .backgroundColor('#21F87171')
      .onClick(() => {
        this.removeHost(h)
      })
    }
    .height('100%')
    .padding({ left: 9, right: MT.pagePadding })
    .alignItems(VerticalAlign.Center)
  }

  @Builder QuickBtn(h: HostItem, tab: number, icon: string, label: string) {
    Column() {
      Ic({ name: icon, icSize: 19, color: this.isOn(h) ? MT.accent : MT.text3 })
      Text(label)
        .fontSize(MT.fsMicro)
        .fontColor(this.isOn(h) ? MT.text2 : MT.text3)
        .margin({ top: 4 })
    }
    .justifyContent(FlexAlign.Center)
    .layoutWeight(1)
    .height(56)
    .borderRadius(13)
    .backgroundColor(MT.surface2)
    .opacity(this.isOn(h) ? 1 : 0.38)
    .onClick(() => {
      this.openQuick(h, tab)
    })
  }

  @Builder StatCell(h: HostItem, label: string, value: string, pct: number) {
    Column() {
      Text(label)
        .fontSize(MT.fsMicro)
        .fontWeight(FontWeight.Medium)
        .fontColor(MT.text3)
      Text(value)
        .fontSize(MT.fsCaption)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.isOn(h) ? MT.text : MT.text3)
        .fontFamily('monospace')
        .margin({ top: 2 })
      Stack({ alignContent: Alignment.Start }) {
        Column()
          .width('100%')
          .height(3)
          .borderRadius(2)
          .backgroundColor(MT.surface2)
        Column()
          .width(`${pct}%`)
          .height(3)
          .borderRadius(2)
          .backgroundColor(this.isOn(h) ? MT.accent : MT.text3)
          .opacity(this.isOn(h) ? 1 : 0.3)
          .animation({ duration: 350, curve: Curve.EaseOut })
      }
      .width('100%')
      .margin({ top: 5 })
    }
    .alignItems(HorizontalAlign.Start)
    .layoutWeight(1)
  }

  @Builder HostCard(h: HostItem) {
    Column() {
      Row() {
        Circle({ width: 8, height: 8 })
          .fill(this.isOn(h) ? MT.accent : (this.isBusy(h) ? MT.warn : MT.text3))
          .opacity(this.isBusy(h) ? 0.9 : 1)

        Text(h.name)
          .fontSize(MT.fsTitle)
          .fontWeight(FontWeight.Medium)
          .fontColor(MT.text)
          .margin({ left: 9 })
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .layoutWeight(1)

        if (this.isBusy(h)) {
          LoadingProgress()
            .width(14)
            .height(14)
            .color(MT.warn)
            .margin({ right: 8 })
        }

        Text(this.isBusy(h) ? L.t('连接中', 'Connecting') : (this.isOn(h) ? L.t('断开连接', 'Disconnect') : L.t('连接', 'Connect')))
          .fontSize(MT.fsCaption)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.isOn(h) ? MT.danger : MT.accentFg)
          .height(30)
          .padding({ left: 14, right: 14 })
          .borderRadius(999)
          .backgroundColor(this.isOn(h) ? '#21F87171' : MT.accent)
          .onClick(() => {
            this.onConnectToggle(h.id)
          })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)

      Text(`${h.user}@${h.host}:${h.port}`)
        .fontSize(MT.fsCaption)
        .fontColor(MT.text2)
        .margin({ top: 6 })
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .width('100%')

      Row() {
        this.QuickBtn(h, 0, 'terminal', L.t('终端', 'Terminal'))
        this.QuickBtn(h, 1, 'tunnel', L.t('隧道', 'Tunnels'))
        this.QuickBtn(h, 2, 'folder', L.t('文件', 'Files'))
      }
      .width('100%')
      .margin({ top: 12 })

      Row() {
        this.StatCell(h, 'CPU', this.isOn(h) ? `${h.stats.cpu}%` : '--', this.isOn(h) ? h.stats.cpu : 0)
        this.StatCell(h, 'RAM', this.isOn(h) ? `${h.stats.ram}%` : '--', this.isOn(h) ? h.stats.ram : 0)
        this.StatCell(h, L.t('带宽', 'Bandwidth'), this.isOn(h) ? h.stats.net : '--', this.isOn(h) ? h.stats.netp : 0)
      }
      .width('100%')
      .margin({ top: 12 })
    }
    .alignItems(HorizontalAlign.Start)
    .width('100%')
    .padding(14)
    .margin({ bottom: 10 })
    .borderRadius(18)
    .backgroundColor(MT.surface)
    .border({ width: 1, color: this.isOn(h) ? MT.accentSoft : MT.transparent })
  }

  @Builder EmptyHint() {
    Column() {
      Text(L.t('没有匹配的主机', 'No matching hosts'))
        .fontSize(MT.fsBody)
        .fontColor(MT.text2)
      Text(L.t('换个关键词，或点右上角新建', 'Try another keyword, or tap + at top right'))
        .fontSize(MT.fsSmall)
        .fontColor(MT.text3)
        .margin({ top: 8 })
    }
    .width('100%')
    .padding({ top: 60 })
  }
}
