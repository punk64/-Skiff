import { promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { Ic } from '../../common/Icons'
import { HostItem, TunnelItem } from '../../common/Models'
import { SshService } from '../../common/SshService'
import { L, Bi } from '../../common/I18n'

@Component
export struct HostsPage {
  @Link hosts: HostItem[]
  onConnectToggle: (id: number) => void = () => {}
  onOpenSession: (id: number, tab: number) => void = () => {}
  onHostSettings: (id: number) => void = () => {}
  onAppSettings: () => void = () => {}
  onNewHost: () => void = () => {}
  onChanged: () => void = () => {}
  onProbe: (id: number) => void = () => {}
  @State keyword: string = ''
  @StorageLink('csx_lang') csxLang: string = 'zh'

  t(zh: string, en: string): string {
    return this.csxLang === 'en' ? en : zh
  }

  pick(v: Bi): string {
    return this.csxLang === 'en' ? v.en : v.zh
  }

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
    const opts: promptAction.ShowDialogOptions = {
      title: this.t('删除主机', 'Delete host'),
      message: this.t(`确定删除「${h.name}」？该操作无法撤销。`, `Delete "${h.name}"? This cannot be undone.`),
      buttons: [
        { text: this.t('取消', 'Cancel'), color: $r('app.color.csx_text2') },
        { text: this.t('删除', 'Delete'), color: $r('app.color.csx_danger') }
      ]
    }
    promptAction.showDialog(opts).then((res: promptAction.ShowDialogSuccessResponse) => {
      if (res.index !== 1) {
        return
      }
      SshService.get().disconnect(h.id)
      this.hosts = this.hosts.filter((x: HostItem) => x.id !== h.id)
      this.onChanged()
      promptAction.showToast({ message: this.t(`已删除 ${h.name}`, `Deleted ${h.name}`) })
    })
  }

  openQuick(h: HostItem, tab: number): void {
    this.onOpenSession(h.id, tab)
  }

  build() {
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
          }, (h: HostItem) => `${h.id}-${h.state}-${h.name}-${h.host}-${h.port}-${h.stats.cpu}-${h.stats.ram}-${h.stats.net}`)
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

  @Builder Header() {
    Row() {
      Column() {
        Text(this.t('主机', 'Hosts'))
          .fontSize(MT.fsDisplay)
          .fontWeight(FontWeight.Bold)
          .fontColor($r('app.color.csx_text'))
        Text(this.t(`${this.hosts.length} 台主机 · ${this.connectedCount()} 台已连接`, `${this.hosts.length} hosts · ${this.connectedCount()} connected`))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text3'))
          .margin({ top: 5 })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)

      Stack() {
        Ic({ name: 'gear', icSize: 18, color: $r('app.color.csx_text2'), filled: true })
      }
      .width(36)
      .height(36)
      .borderRadius(12)
      .backgroundColor($r('app.color.csx_surface'))
      .margin({ left: 8 })
      .onClick(() => {
        this.onAppSettings()
      })

      Stack() {
        Ic({ name: 'plus', icSize: 18, color: $r('app.color.csx_accent_fg'), sw: 2.2 })
      }
      .width(36)
      .height(36)
      .borderRadius(12)
      .backgroundColor($r('app.color.csx_accent'))
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
      TextInput({ placeholder: this.t('搜索主机名或地址', 'Search host or address'), text: this.keyword })
        .placeholderColor($r('app.color.csx_text3'))
        .fontColor($r('app.color.csx_text'))
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
    .backgroundColor($r('app.color.csx_surface'))
  }

  @Builder SwipeActions(h: HostItem) {
    Row() {
      Stack() {
        Ic({ name: 'gear', icSize: 16, color: $r('app.color.csx_text2'), filled: true })
      }
      .width(44)
      .height(44)
      .borderRadius(14)
      .backgroundColor($r('app.color.csx_surface2'))
      .onClick(() => {
        this.onHostSettings(h.id)
      })

      Stack() {
        Ic({ name: 'trash', icSize: 16, color: $r('app.color.csx_danger') })
      }
      .width(44)
      .height(44)
      .borderRadius(14)
      .backgroundColor('#21F87171')
      .onClick(() => {
        this.removeHost(h)
      })
    }
    .height('100%')
    .padding({ left: 7, right: 10 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder QuickBtn(h: HostItem, tab: number, icon: string, label: Bi) {
    Column() {
      Ic({ name: icon, icSize: 18, color: this.isOn(h) ? $r('app.color.csx_accent') : $r('app.color.csx_text3') })
      Text(this.pick(label))
        .fontSize(MT.fsMicro)
        .fontColor(this.isOn(h) ? $r('app.color.csx_text2') : $r('app.color.csx_text3'))
        .margin({ top: 4 })
    }
    .justifyContent(FlexAlign.Center)
    .layoutWeight(1)
    .height(56)
    .borderRadius(13)
    .backgroundColor($r('app.color.csx_surface2'))
    .opacity(this.isOn(h) ? 1 : 0.38)
    .onClick(() => {
      this.openQuick(h, tab)
    })
  }

  @Builder StatCell(h: HostItem, label: Bi, value: string, pct: number) {
    Column() {
      Text(this.pick(label))
        .fontSize(MT.fsMicro)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('app.color.csx_text3'))
      Text(value)
        .fontSize(MT.fsCaption)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.isOn(h) ? $r('app.color.csx_text') : $r('app.color.csx_text3'))
        .fontFamily('monospace')
        .animation({ duration: 300, curve: Curve.EaseOut })
        .margin({ top: 2 })
      Stack({ alignContent: Alignment.Start }) {
        Column()
          .width('100%')
          .height(3)
          .borderRadius(2)
          .backgroundColor($r('app.color.csx_surface2'))
        Column()
          .width(`${pct}%`)
          .height(3)
          .borderRadius(2)
          .backgroundColor(this.isOn(h) ? $r('app.color.csx_accent') : $r('app.color.csx_text3'))
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
          .fill(this.isOn(h) ? $r('app.color.csx_accent') : (this.isBusy(h) ? $r('app.color.csx_warn') : $r('app.color.csx_text3')))
          .opacity(this.isBusy(h) ? 0.9 : 1)
          .shadow({ radius: this.isOn(h) ? 6 : 0, color: $r('app.color.csx_accent') })
          .animation({ duration: 320, curve: Curve.EaseOut })

        Text(h.name)
          .fontSize(MT.fsTitle)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_text'))
          .margin({ left: 9 })
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .layoutWeight(1)

        if (this.isBusy(h)) {
          LoadingProgress()
            .width(14)
            .height(14)
            .color($r('app.color.csx_warn'))
            .margin({ right: 8 })
        }

        Text(this.isBusy(h) ? this.t('连接中', 'Connecting') : (this.isOn(h) ? this.t('断开连接', 'Disconnect') : this.t('连接', 'Connect')))
          .fontSize(MT.fsCaption)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.isOn(h) ? $r('app.color.csx_danger') : $r('app.color.csx_accent_fg'))
          .height(30)
          .padding({ left: 14, right: 14 })
          .borderRadius(999)
          .backgroundColor(this.isOn(h) ? '#21F87171' : $r('app.color.csx_accent'))
          .onClick(() => {
            this.onConnectToggle(h.id)
          })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)

      Text(h.host.length === 0 ? this.t('未设置地址', 'No address set') : `${h.user}@${h.host}:${h.port}`)
        .fontSize(MT.fsCaption)
        .fontColor($r('app.color.csx_text2'))
        .margin({ top: 6 })
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .width('100%')

      Row() {
        this.QuickBtn(h, 0, 'terminal', L.b('终端', 'Terminal'))
        this.QuickBtn(h, 1, 'tunnel', L.b('隧道', 'Tunnels'))
        this.QuickBtn(h, 2, 'folder', L.b('文件', 'Files'))
      }
      .width('100%')
      .margin({ top: 12 })

      Row() {
        this.StatCell(h, L.s('CPU'), h.stats.cpu > 0 ? `${h.stats.cpu}%` : '--', h.stats.cpu)
        this.StatCell(h, L.s('RAM'), h.stats.ram > 0 ? `${h.stats.ram}%` : '--', h.stats.ram)
        this.StatCell(h, L.b('带宽', 'Bandwidth'), h.stats.net.length > 0 ? h.stats.net : '--', h.stats.netp)
      }
      .width('100%')
      .margin({ top: 12 })
    }
    .alignItems(HorizontalAlign.Start)
    .width('100%')
    .padding(14)
    .margin({ bottom: 10 })
    .borderRadius(18)
    .backgroundColor($r('app.color.csx_surface'))
    .border({ width: 1, color: this.isOn(h) ? $r('app.color.csx_accent_soft') : MT.transparent })
    .gesture(LongPressGesture({ duration: 500 }).onAction(() => {
      this.onProbe(h.id)
    }))
  }

  @Builder TipRow(text: Bi) {
    Row() {
      Text('·')
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_accent'))
        .margin({ right: 8 })
      Text(this.pick(text))
        .fontSize(MT.fsSmall)
        .fontColor($r('app.color.csx_text2'))
        .layoutWeight(1)
    }
    .width('100%')
    .margin({ top: 6 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder EmptyHint() {
    if (this.hosts.length === 0) {
      Column() {
        Text('⇅')
          .fontSize(38)
          .fontColor($r('app.color.csx_text3'))
        Text(this.t('还没有主机', 'No hosts yet'))
          .fontSize(MT.fsTitle)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_text'))
          .margin({ top: 14 })
        Text(this.t('添加一台服务器，随时用手机连上去干活', 'Add a server and connect from your phone anytime'))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text3'))
          .margin({ top: 8 })
          .textAlign(TextAlign.Center)
        Button(this.t('新建主机', 'Add host'))
          .fontSize(MT.fsBody)
          .fontColor($r('app.color.csx_accent_fg'))
          .width('100%')
          .height(46)
          .borderRadius(MT.radiusBtn)
          .backgroundColor($r('app.color.csx_accent'))
          .margin({ top: 26 })
          .onClick(() => {
            this.onNewHost()
          })
        Column() {
          this.TipRow(L.b('密码或密钥登录，支持 TOFU 指纹校验', 'Password or key login with TOFU host key check'))
          this.TipRow(L.b('终端、SFTP 文件管理、端口转发', 'Terminal, SFTP files and port forwarding'))
          this.TipRow(L.b('主机 CPU／内存／带宽实时监控', 'Live CPU, memory and bandwidth'))
        }
        .width('100%')
        .margin({ top: 28 })
        .padding(14)
        .borderRadius(MT.radiusCard)
        .backgroundColor($r('app.color.csx_surface'))
        .alignItems(HorizontalAlign.Start)
      }
      .width('100%')
      .padding({ top: 44, left: MT.pagePadding, right: MT.pagePadding })
    } else {
      Column() {
        Text(this.t('没有匹配的主机', 'No matching hosts'))
          .fontSize(MT.fsBody)
          .fontColor($r('app.color.csx_text2'))
        Text(this.t('换个关键词试试', 'Try another keyword'))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text3'))
          .margin({ top: 8 })
      }
      .width('100%')
      .padding({ top: 60 })
    }
  }
}
