import { webview } from '@kit.ArkWeb'
import { promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { DemoShell } from '../../common/DemoShell'
import { L } from '../../common/I18n'

const TB_BG: string = '#08090C'
const TB_SURF: string = '#14171D'
const TB_LINE: string = '#262C37'
const TB_TEXT: string = '#E8EDF5'
const TB_TEXT2: string = '#98A4B6'
const TB_TEXT3: string = '#78849B'
const TB_ACCENT: string = '#3DDC97'
const TB_ACCENT_FG: string = '#04140D'
const TB_DANGER: string = '#F87171'

const KEYS: string[][] = [
  ['C-c', '\x03'],
  ['C-d', '\x04'],
  ['C-l', '\x0c'],
  ['Esc', '\x1b'],
  ['Tab', '\t'],
  ['↑', '\x1b[A'],
  ['↓', '\x1b[B'],
  ['←', '\x1b[D'],
  ['→', '\x1b[C'],
  ['Home', '\x1b[H'],
  ['End', '\x1b[F'],
  ['PgUp', '\x1b[5~'],
  ['PgDn', '\x1b[6~'],
  ['|', '|'],
  ['~', '~'],
  ['/', '/'],
  ['-', '-']
]

@Component
export struct TerminalPro {
  @Link show: boolean
  @Prop hostName: string = ''
  @Prop hostAddr: string = ''
  @Prop compact: boolean = false
  @StorageProp('csx_themeVer') themeVer: number = 0
  @State ready: boolean = false
  @State fontSize: number = 14
  @State closed: boolean = false
  private controller: webview.WebviewController = new webview.WebviewController()
  private nativePorts: webview.WebMessagePort | null = null
  private shell: DemoShell = new DemoShell()
  private started: boolean = false
  private portReady: boolean = false

  aboutToAppear(): void {
  }

  setupPort(): void {
    if (this.portReady) {
      return
    }
    try {
      const ports: webview.WebMessagePort[] = this.controller.createWebMessagePorts(true)
      this.nativePorts = ports[1]
      this.nativePorts.onMessageEventExt((e: webview.WebMessageExt) => {
        try {
          if (e.getType() !== webview.WebMessageType.STRING) {
            return
          }
          this.onH5(e.getString())
        } catch (err) {
        }
      })
      this.controller.postMessage('__init_port__', [ports[0]], '*')
      this.portReady = true
    } catch (err) {
      this.portReady = false
    }
  }

  onH5(s: string): void {
    if (s === 'ready') {
      return
    }
    if (s === '__terminal_ready__') {
      this.ready = true
      this.startSession()
      return
    }
    if (s.startsWith('resize:')) {
      return
    }
    if (!this.ready || this.closed) {
      return
    }
    const out: string = this.shell.handle(s)
    if (out.length > 0) {
      this.write(out)
    }
  }

  startSession(): void {
    if (this.started) {
      return
    }
    this.started = true
    this.write(this.shell.banner())
    setTimeout(() => {
      this.applyFont(this.fontSize)
    }, 260)
  }

  write(text: string): void {
    if (!this.nativePorts) {
      return
    }
    try {
      const ext: webview.WebMessageExt = new webview.WebMessageExt()
      ext.setType(webview.WebMessageType.STRING)
      ext.setString('\x1b[0m' + text)
      this.nativePorts.postMessageEventExt(ext)
    } catch (err) {
    }
  }

  sendKey(seq: string): void {
    if (this.closed) {
      return
    }
    if (seq === '\x03') {
      this.shell.handle('\x03')
      this.write('\x03\r\n')
      const out: string = this.shell.prompt()
      this.write(out)
      return
    }
    if (seq === '\x04') {
      this.closed = true
      this.write('\r\nlogout\r\nConnection to web-01 closed.\r\n')
      return
    }
    const out: string = this.shell.handle(seq)
    if (out.length > 0) {
      this.write(out)
    }
  }

  applyFont(size: number): void {
    try {
      this.controller.runJavaScript(
        `if (typeof term !== 'undefined') { term.options.fontSize = ${size}; term.options.wordWrap = false; var refit = function(){ try { fitAddon.fit(); term.refresh(0, term.rows - 1); } catch (e) {} }; refit(); if (!window.__csxFont) { window.__csxFont = 1; if (document.fonts) { document.fonts.load('${size}px CsxMono').then(refit).catch(refit); } } }`
      )
    } catch (err) {
    }
  }

  zoom(delta: number): void {
    const next: number = this.fontSize + delta
    if (next < 10 || next > 22) {
      promptAction.showToast({ message: L.t('字号范围 10~22', 'Font size range is 10~22') })
      return
    }
    this.fontSize = next
    this.applyFont(next)
  }

  build() {
    if (this.themeVer >= 0) {
      Column() {
        this.TopBar()

        Stack({ alignContent: Alignment.Top }) {
          Web({ src: $rawfile('xterm/xterm.html'), controller: this.controller })
            .width('100%')
            .height('100%')
            .javaScriptAccess(true)
            .domStorageAccess(true)
            .fileAccess(true)
            .zoomAccess(false)
            .horizontalScrollBarAccess(false)
            .onPageEnd(() => {
              setTimeout(() => {
                this.setupPort()
              }, 60)
            })
        }
        .width('100%')
        .layoutWeight(1)

        this.ModBar()
      }
      .width('100%')
      .height('100%')
      .backgroundColor(TB_BG)
    }
  }

  @Builder TopBar() {
    Row() {
      Circle({ width: 7, height: 7 })
        .fill(this.closed ? TB_DANGER : TB_ACCENT)

      if (!this.compact) {
        Column() {
          Text(this.hostName.length > 0 ? this.hostName : 'web-01')
            .fontSize(MT.fsSmall)
            .fontWeight(FontWeight.Medium)
            .fontColor(TB_TEXT)
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
          Text(this.closed ? L.t('已断开', 'Disconnected') : (this.ready ? `${this.hostAddr} · ${this.fontSize}px` : L.t('连接中…', 'Connecting…')))
            .fontSize(MT.fsMicro)
            .fontColor(this.closed ? TB_DANGER : TB_TEXT3)
            .margin({ top: 2 })
            .maxLines(1)
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)
        .margin({ left: 9 })
      } else {
        Blank()
      }

      Text('A-')
        .fontSize(MT.fsCaption)
        .fontColor(TB_TEXT2)
        .textAlign(TextAlign.Center)
        .width(34)
        .height(30)
        .borderRadius(MT.radiusCtrl)
        .backgroundColor(TB_SURF)
        .onClick(() => {
          this.zoom(-1)
        })

      Text('A+')
        .fontSize(MT.fsCaption)
        .fontColor(TB_TEXT2)
        .textAlign(TextAlign.Center)
        .width(34)
        .height(30)
        .borderRadius(MT.radiusCtrl)
        .backgroundColor(TB_SURF)
        .margin({ left: 6 })
        .onClick(() => {
          this.zoom(1)
        })

      if (!this.compact) {
        Text(L.t('关闭', 'Close'))
          .fontSize(MT.fsCaption)
          .fontColor(TB_ACCENT)
          .textAlign(TextAlign.Center)
          .height(30)
          .padding({ left: 12, right: 12 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor('#1A3EE8A0')
          .margin({ left: 6 })
          .onClick(() => {
            this.show = false
          })
      }
    }
    .width('100%')
    .padding({ left: 14, right: 12, top: 8, bottom: 8 })
    .alignItems(VerticalAlign.Center)
    .backgroundColor(TB_SURF)
    .border({ width: { bottom: 1 }, color: TB_LINE })
  }

  @Builder ModBar() {
    Row() {
      ForEach(KEYS, (k: string[]) => {
        Text(k[0])
          .fontSize(MT.fsCaption)
          .fontColor(k[0] === 'C-c' ? TB_DANGER : TB_TEXT2)
          .fontFamily('monospace')
          .textAlign(TextAlign.Center)
          .height(32)
          .padding({ left: 9, right: 9 })
          .borderRadius(MT.radiusCtrl)
          .backgroundColor(TB_SURF)
          .margin({ right: 5 })
          .onClick(() => {
            this.sendKey(k[1])
          })
      }, (k: string[]) => k[0])
    }
    .width('100%')
    .padding({ left: 10, right: 6, top: 7, bottom: 9 })
    .backgroundColor(TB_BG)
    .border({ width: { top: 1 }, color: TB_LINE })
  }
}
