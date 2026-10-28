import { promptAction, window } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { Arrow } from '../../common/Arrow'
import { L } from '../../common/I18n'

export interface KeyItem {
  id: number
  name: string
  alg: string
  bits: number
  fp: string
  pass: boolean
}

export interface KnownHostItem {
  id: number
  host: string
  keyType: string
  fp: string
  time: string
}

@Component
export struct SettingsPage {
  @Link themeDark: boolean
  @StorageProp('csx_themeVer') themeVer: number = 0
  @State sub: string = ''
  @State fontScale: number = 13
  @State keepAlive: boolean = true
  @State bgTunnel: boolean = true
  @State autoReconnect: boolean = false
  @State logOn: boolean = true
  @State logFormat: string = 'stamp'
  @State logPath: string = '/data/storage/el2/base/haps/entry/files/logs'
  @State logFiles: number = 12
  @State proxyOn: boolean = false
  @State proxyType: string = 'socks5'
  @State proxyHost: string = '127.0.0.1'
  @State proxyPort: string = '1080'
  @State keys: KeyItem[] = [
    { id: 1, name: 'id_ed25519', alg: 'Ed25519', bits: 256, fp: 'SHA256:q3tRmXvK9pLwZf', pass: false },
    { id: 2, name: 'id_rsa_2', alg: 'RSA', bits: 4096, fp: 'SHA256:Lm2PqT7bNcVr1s', pass: true }
  ]
  @State known: KnownHostItem[] = [
    { id: 1, host: '47.100.23.8:22', keyType: 'ssh-ed25519', fp: 'SHA256:q3tRmXvK9pLwZf', time: 'Oct 28 15:41' },
    { id: 2, host: '192.168.1.30:2222', keyType: 'ssh-ed25519', fp: 'SHA256:9dKpQ2mXvNb4Rt', time: 'Oct 20 10:08' }
  ]

  setTheme(dark: boolean): void {
    if (this.themeDark === dark) {
      return
    }
    MT.apply(dark)
    this.themeDark = dark
    AppStorage.setOrCreate('csx_themeDark', dark)
    const cur: number = AppStorage.get<number>('csx_themeVer') ?? 0
    AppStorage.setOrCreate('csx_themeVer', cur + 1)
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
    if (L.lang === lang) {
      return
    }
    L.lang = lang
    AppStorage.setOrCreate('csx_lang', lang)
    const cur: number = AppStorage.get<number>('csx_themeVer') ?? 0
    AppStorage.setOrCreate('csx_themeVer', cur + 1)
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
  }

  goSub(key: string): void {
    if (key === 'keys' || key === 'known' || key === 'proxy' || key === 'logs') {
      this.sub = key
      return
    }
    if (key === 'logDir') {
      this.logPath = '/storage/Users/currentUser/Download/logs'
      promptAction.showToast({ message: L.t('已改为 Download/logs', 'Changed to Download/logs') })
      return
    }
    if (key === 'logReset') {
      this.logPath = '/data/storage/el2/base/haps/entry/files/logs'
      promptAction.showToast({ message: L.t('已还原默认位置', 'Default location restored') })
    }
  }

  setProxyField(key: string, v: string): void {
    if (key === 'proxyHost') {
      this.proxyHost = v
    } else if (key === 'proxyPort') {
      this.proxyPort = v
    }
  }

  build() {
    if (this.themeVer >= 0) {
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
      }
      .width('100%')
      .height('100%')
    }
  }

  @Builder SubHeader(title: string) {
    Row() {
      Stack() {
        Arrow({ dir: 'left', color: MT.blue , len: 20 })
      }
      .width(38)
      .height(38)
      .onClick(() => {
        this.sub = ''
      })
      Text(title)
        .fontSize(MT.fsTitle)
        .fontWeight(FontWeight.Medium)
        .fontColor(MT.text)
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
        Text(L.t('设置', 'Settings'))
          .fontSize(MT.fsDisplay)
          .fontWeight(FontWeight.Bold)
          .fontColor(MT.text)
        Text(L.t('外观、安全与会话行为', 'Appearance, security and sessions'))
          .fontSize(MT.fsSmall)
          .fontColor(MT.text3)
          .margin({ top: 6 })
      }
      .alignItems(HorizontalAlign.Start)
      .width('100%')
      .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 14, bottom: 16 })

      Scroll() {
        Column() {
          this.GroupTitle(L.t('外观', 'Appearance'))
          Column() {
            this.ThemeRow()
            Divider().color(MT.line).margin({ left: 16 })
            this.LangRow()
            Divider().color(MT.line).margin({ left: 16 })
            Row() {
              Text(L.t('终端字号', 'Terminal font size'))
                .fontSize(MT.fsBody)
                .fontColor(MT.text)
                .width(88)
              Slider({ value: this.fontScale, min: 11, max: 18, step: 1, style: SliderStyle.InSet })
                .selectedColor(MT.accent)
                .trackColor(MT.surface2)
                .blockColor(MT.accent)
                .layoutWeight(1)
                .onChange((v: number) => {
                  this.fontScale = v
                })
              Text(`${this.fontScale}`)
                .fontSize(MT.fsSmall)
                .fontColor(MT.text2)
                .width(30)
                .textAlign(TextAlign.End)
            }
            .width('100%')
            .height(56)
            .padding({ left: 16, right: 16 })
            .alignItems(VerticalAlign.Center)
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.t('连接行为', 'Connection'))
          Column() {
            this.SwitchRow(L.t('保活', 'Keep-alive'), L.t('定时发送探测包防止连接被回收', 'Send probes periodically to keep the connection alive'), this.keepAlive, 'keepAlive')
            Divider().color(MT.line).margin({ left: 16 })
            this.SwitchRow(L.t('后台保持隧道', 'Keep tunnels in background'), L.t('切到后台后不主动断开转发', 'Do not drop forwarding when app goes to background'), this.bgTunnel, 'bgTunnel')
            Divider().color(MT.line).margin({ left: 16 })
            this.SwitchRow(L.t('断线自动重连', 'Auto reconnect'), L.t('网络抖动后自动恢复会话', 'Restore the session after network jitter'), this.autoReconnect, 'autoReconnect')
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.t('安全与网络', 'Security & network'))
          Column() {
            this.NavRow(L.t('密钥管理', 'Keys'), L.t(`${this.keys.length} 个密钥`, `${this.keys.length} keys`), 'keys')
            Divider().color(MT.line).margin({ left: 16 })
            this.NavRow(L.t('主机指纹', 'Known hosts'), L.t(`${this.known.length} 条记录`, `${this.known.length} records`), 'known')
            Divider().color(MT.line).margin({ left: 16 })
            this.NavRow(L.t('代理', 'Proxy'), this.proxyOn ? `${this.proxyType.toUpperCase()} ${this.proxyHost}:${this.proxyPort}` : L.t('未启用', 'Disabled'), 'proxy')
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.t('诊断', 'Diagnostics'))
          Column() {
            this.NavRow(L.t('会话日志', 'Session log'), this.logOn ? L.t('已开启', 'On') : L.t('已关闭', 'Off'), 'logs')
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          Text(L.t('轻舟 Skiff · 鸿蒙原生 SSH 客户端\n内核 libssh2 1.11.1 · 自编 x86_64', 'Skiff · Native SSH client for HarmonyOS\nlibssh2 1.11.1 · self-built x86_64'))
            .fontSize(MT.fsCaption)
            .fontColor(MT.text3)
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

  @Builder GroupTitle(t: string) {
    Text(t)
      .fontSize(MT.fsCaption)
      .fontWeight(FontWeight.Medium)
      .fontColor(MT.text3)
      .width('100%')
      .margin({ top: 18, bottom: 9, left: 4 })
  }

  @Builder ThemeRow() {
    Row() {
      Column() {
        Text(L.t('颜色主题', 'Theme'))
          .fontSize(MT.fsBody)
          .fontColor(MT.text)
        Text(this.themeDark ? L.t('深色 · 夜间与低光环境', 'Dark · Night and low light') : L.t('浅色 · 白天与强光环境', 'Light · Day and bright light'))
          .fontSize(MT.fsCaption)
          .fontColor(MT.text3)
          .margin({ top: 4 })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)

      Row() {
        Text(L.t('深色', 'Dark'))
          .fontSize(MT.fsCaption)
          .fontWeight(this.themeDark ? FontWeight.Medium : FontWeight.Normal)
          .fontColor(this.themeDark ? MT.accentFg : MT.text2)
          .height(28)
          .padding({ left: 13, right: 13 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(this.themeDark ? MT.accent : MT.transparent)
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setTheme(true)
          })
        Text(L.t('浅色', 'Light'))
          .fontSize(MT.fsCaption)
          .fontWeight(this.themeDark ? FontWeight.Normal : FontWeight.Medium)
          .fontColor(this.themeDark ? MT.text2 : MT.accentFg)
          .height(28)
          .padding({ left: 13, right: 13 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(this.themeDark ? MT.transparent : MT.accent)
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setTheme(false)
          })
      }
      .padding(3)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(MT.surface2)
    }
    .width('100%')
    .height(62)
    .padding({ left: 16, right: 16 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder LangRow() {
    Row() {
      Text(L.t('界面语言', 'Language'))
        .fontSize(MT.fsBody)
        .fontColor(MT.text)
        .layoutWeight(1)
      Row() {
        Text('简体中文')
          .fontSize(MT.fsCaption)
          .fontWeight(L.lang === 'zh' ? FontWeight.Medium : FontWeight.Normal)
          .fontColor(L.lang === 'zh' ? MT.accentFg : MT.text2)
          .height(28)
          .padding({ left: 11, right: 11 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(L.lang === 'zh' ? MT.accent : MT.transparent)
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setLang('zh')
          })
        Text('English')
          .fontSize(MT.fsCaption)
          .fontWeight(L.lang === 'en' ? FontWeight.Medium : FontWeight.Normal)
          .fontColor(L.lang === 'en' ? MT.accentFg : MT.text2)
          .height(28)
          .padding({ left: 11, right: 11 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(L.lang === 'en' ? MT.accent : MT.transparent)
          .textAlign(TextAlign.Center)
          .onClick(() => {
            this.setLang('en')
          })
      }
      .padding(3)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(MT.surface2)
    }
    .width('100%')
    .height(56)
    .padding({ left: 16, right: 16 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder SwitchRow(title: string, desc: string, on: boolean, key: string) {
    Row() {
      Column() {
        Text(title)
          .fontSize(MT.fsBody)
          .fontColor(MT.text)
        Text(desc)
          .fontSize(MT.fsCaption)
          .fontColor(MT.text3)
          .margin({ top: 4 })
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1)

      Toggle({ type: ToggleType.Switch, isOn: on })
        .selectedColor(MT.accent)
        .switchPointColor('#FFFFFF')
        .onChange((v: boolean) => {
          this.setSwitch(key, v)
        })
    }
    .width('100%')
    .padding({ left: 16, right: 16, top: 13, bottom: 13 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder NavRow(title: string, value: string, key: string) {
    Row() {
      Text(title)
        .fontSize(MT.fsBody)
        .fontColor(MT.text)
        .layoutWeight(1)
      Text(value)
        .fontSize(MT.fsSmall)
        .fontColor(MT.text3)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .constraintSize({ maxWidth: 160 })
      Stack() {
        Arrow({ dir: 'right', color: MT.text3 , len: 15 })
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

  @Builder KeysPage() {
    Column() {
      this.SubHeader(L.t('密钥管理', 'Keys'))
      Scroll() {
        Column() {
          ForEach(this.keys, (k: KeyItem) => {
            Row() {
              Text(k.alg.substring(0, 2).toUpperCase())
                .fontSize(MT.fsCaption)
                .fontWeight(FontWeight.Bold)
                .fontColor(MT.accent)
                .width(30)
                .height(30)
                .textAlign(TextAlign.Center)
                .borderRadius(MT.radiusCtrl)
                .backgroundColor(MT.accentSoft)
              Column() {
                Text(k.name)
                  .fontSize(MT.fsBody)
                  .fontColor(MT.text)
                Text(L.t(`${k.alg} ${k.bits} 位${k.pass ? ' · 带口令' : ''}`, `${k.alg} ${k.bits} bit${k.pass ? ' · passphrase' : ''}`))
                  .fontSize(MT.fsCaption)
                  .fontColor(MT.text3)
                  .margin({ top: 4 })
                Text(k.fp)
                  .fontSize(MT.fsMicro)
                  .fontColor(MT.text3)
                  .fontFamily('monospace')
                  .margin({ top: 3 })
              }
              .alignItems(HorizontalAlign.Start)
              .layoutWeight(1)
              .margin({ left: 12 })

              Text(L.t('删除', 'Delete'))
                .fontSize(MT.fsCaption)
                .fontColor(MT.danger)
                .padding({ left: 10, right: 10, top: 6, bottom: 6 })
                .borderRadius(MT.radiusCtrl)
                .backgroundColor(MT.surface2)
                .onClick(() => {
                  this.keys = this.keys.filter((x: KeyItem) => x.id !== k.id)
                  promptAction.showToast({ message: L.t('已删除密钥', 'Key deleted') })
                })
            }
            .width('100%')
            .padding(14)
            .margin({ bottom: 10 })
            .borderRadius(MT.radiusCard)
            .backgroundColor(MT.surface)
            .alignItems(VerticalAlign.Center)
          }, (k: KeyItem) => k.id.toString())

          Text(L.t('+ 生成新密钥', '+ Generate new key'))
            .fontSize(MT.fsBody)
            .fontColor(MT.accent)
            .textAlign(TextAlign.Center)
            .width('100%')
            .height(50)
            .borderRadius(MT.radiusBtn)
            .border({ width: 1, color: MT.line, style: BorderStyle.Dashed })
            .onClick(() => {
              const n: number = this.keys.length + 1
              const id: number = Date.now()
              this.keys = this.keys.concat([
                { id: id, name: `id_ed25519_${n}`, alg: 'Ed25519', bits: 256, fp: 'SHA256:new' + n + 'Kp9x', pass: false }
              ])
              promptAction.showToast({ message: L.t('已生成 Ed25519 密钥', 'Ed25519 key generated') })
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

  @Builder KnownPage() {
    Column() {
      this.SubHeader(L.t('主机指纹', 'Known hosts'))
      Scroll() {
        Column() {
          Text(L.t('首次连接新主机时会弹指纹确认，信任后写入 known_hosts。指纹变化说明对方密钥已更换，可能被中间人劫持。', 'A fingerprint prompt appears on first connection; after you trust it, it is written to known_hosts. A changed fingerprint means the remote key changed and may indicate a man-in-the-middle attack.'))
            .fontSize(MT.fsCaption)
            .fontColor(MT.text3)
            .lineHeight(18)
            .padding(13)
            .width('100%')
            .borderRadius(MT.radiusCard)
            .backgroundColor(MT.surface2)
            .margin({ bottom: 14 })

          ForEach(this.known, (k: KnownHostItem) => {
            Column() {
              Row() {
                Text(k.host)
                  .fontSize(MT.fsBody)
                  .fontColor(MT.text)
                  .layoutWeight(1)
                Text(k.time)
                  .fontSize(MT.fsCaption)
                  .fontColor(MT.text3)
              }
              .width('100%')
              Text(`${k.keyType}  ${k.fp}`)
                .fontSize(MT.fsMicro)
                .fontColor(MT.text3)
                .fontFamily('monospace')
                .width('100%')
                .margin({ top: 6 })
              Row() {
                Text(L.t('移除', 'Remove'))
                  .fontSize(MT.fsCaption)
                  .fontColor(MT.danger)
                  .padding({ left: 10, right: 10, top: 6, bottom: 6 })
                  .borderRadius(MT.radiusCtrl)
                  .backgroundColor(MT.surface2)
                  .onClick(() => {
                    this.known = this.known.filter((x: KnownHostItem) => x.id !== k.id)
                    promptAction.showToast({ message: L.t('已移除，下次连接将重新确认', 'Removed; it will be confirmed again next time') })
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
            .backgroundColor(MT.surface)
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
      this.SubHeader(L.t('代理', 'Proxy'))
      Scroll() {
        Column() {
          this.GroupTitle(L.t('启用', 'Enable'))
          Column() {
            this.SwitchRow(L.t('通过代理连接', 'Connect via proxy'), L.t('先经代理建立 TCP，再在隧道内握手 SSH', 'Establish TCP through the proxy, then handshake SSH inside the tunnel'), this.proxyOn, 'proxyOn')
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.t('类型', 'Type'))
          Row() {
            this.ProxyChip('socks5', 'SOCKS5')
            this.ProxyChip('http', 'HTTP CONNECT')
          }
          .width('100%')
          .padding(4)
          .borderRadius(MT.radiusCard)
          .backgroundColor(MT.surface2)

          this.GroupTitle(L.t('服务器', 'Server'))
          Column() {
            this.InputRow(L.t('地址', 'Address'), this.proxyHost, 'proxyHost')
            Divider().color(MT.line).margin({ left: 16 })
            this.InputRow(L.t('端口', 'Port'), this.proxyPort, 'proxyPort')
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          Text(L.t('测试连通', 'Test'))
            .fontSize(MT.fsBody)
            .fontColor(MT.accentFg)
            .textAlign(TextAlign.Center)
            .width('100%')
            .height(48)
            .borderRadius(MT.radiusBtn)
            .backgroundColor(MT.accent)
            .margin({ top: 20 })
            .onClick(() => {
              promptAction.showToast({ message: L.t(`代理 ${this.proxyHost}:${this.proxyPort} 可达`, `Proxy ${this.proxyHost}:${this.proxyPort} is reachable`) })
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

  @Builder ProxyChip(tp: string, label: string) {
    Text(label)
      .fontSize(MT.fsSmall)
      .fontColor(this.proxyType === tp ? MT.text : MT.text2)
      .textAlign(TextAlign.Center)
      .height(36)
      .layoutWeight(1)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(this.proxyType === tp ? MT.surface : MT.transparent)
      .onClick(() => {
        this.proxyType = tp
        this.proxyPort = tp === 'http' ? '8080' : '1080'
      })
  }

  @Builder LogsPage() {
    Column() {
      this.SubHeader(L.t('会话日志', 'Session log'))
      Scroll() {
        Column() {
          this.GroupTitle(L.t('记录', 'Record'))
          Column() {
            this.SwitchRow(L.t('记录会话日志', 'Record session log'), L.t('把终端输出写入文件', 'Write terminal output to a file'), this.logOn, 'logOn')
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          this.GroupTitle(L.t('格式', 'Format'))
          Row() {
            this.LogChip('text', L.t('纯文本', 'Plain text'))
            this.LogChip('stamp', L.t('时间戳', 'Timestamp'))
            this.LogChip('html', 'HTML')
          }
          .width('100%')
          .padding(4)
          .borderRadius(MT.radiusCard)
          .backgroundColor(MT.surface2)

          this.GroupTitle(L.t('保存位置', 'Location'))
          Column() {
            this.NavRow(L.t('目录', 'Directory'), this.logPath.endsWith('/logs') ? L.t('默认', 'Default') : L.t('自定义', 'Custom'), 'logDir')
            Divider().color(MT.line).margin({ left: 16 })
            Text(this.logPath)
              .fontSize(MT.fsMicro)
              .fontColor(MT.text3)
              .fontFamily('monospace')
              .width('100%')
              .padding({ left: 16, right: 16, top: 10, bottom: 12 })
            Divider().color(MT.line).margin({ left: 16 })
            this.NavRow(L.t('还原默认位置', 'Restore default'), '/data/storage/el2/base/.../files/logs', 'logReset')
            Divider().color(MT.line).margin({ left: 16 })
            this.NavRow(L.t('已记录', 'Recorded'), L.t(`${this.logFiles} 个文件`, `${this.logFiles} files`), 'none')
          }
          .backgroundColor(MT.surface)
          .borderRadius(MT.radiusCard)
          .clip(true)

          Row() {
            Text(L.t('清除日志', 'Clear logs'))
              .fontSize(MT.fsBody)
              .fontColor(MT.danger)
              .textAlign(TextAlign.Center)
              .height(48)
              .layoutWeight(1)
              .borderRadius(MT.radiusBtn)
              .backgroundColor(MT.surface)
            Text(L.t('导出日志', 'Export logs'))
              .fontSize(MT.fsBody)
              .fontColor(MT.accent)
              .textAlign(TextAlign.Center)
              .height(48)
              .layoutWeight(1)
              .margin({ left: 12 })
              .borderRadius(MT.radiusBtn)
              .backgroundColor(MT.accentSoft)
              .onClick(() => {
                promptAction.showToast({ message: L.t('已导出 crossshell-logs.zip', 'Exported skiff-logs.zip') })
              })
          }
          .width('100%')
          .margin({ top: 20 })
          .onClick(() => {})
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

  @Builder LogChip(f: string, label: string) {
    Text(label)
      .fontSize(MT.fsSmall)
      .fontColor(this.logFormat === f ? MT.text : MT.text2)
      .textAlign(TextAlign.Center)
      .height(36)
      .layoutWeight(1)
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(this.logFormat === f ? MT.surface : MT.transparent)
      .onClick(() => {
        this.logFormat = f
      })
  }

  @Builder InputRow(label: string, value: string, key: string) {
    Row() {
      Text(label)
        .fontSize(MT.fsBody)
        .fontColor(MT.text)
        .width(76)
      TextInput({ text: value })
        .fontSize(MT.fsBody)
        .fontColor(MT.text)
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
