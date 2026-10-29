import { webview } from '@kit.ArkWeb'
import { promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { SshService, SshDataHandler } from '../../common/SshService'

const TB_BG: string = '#08090C'
const TB_SURF: string = '#14171D'
const TB_LINE: string = '#262C37'
const TB_TEXT: string = '#E8EDF5'
const TB_TEXT2: string = '#98A4B6'
const TB_TEXT3: string = '#78849B'
const TB_ACCENT: string = '#3DDC97'
const TB_ACCENT_FG: string = '#04140D'
const TB_DANGER: string = '#F87171'

const CMDS: string[][] = [
  ['ls -l', 'ls -l\r'],
  ['ls -la', 'ls -la\r'],
  ['cd ..', 'cd ..\r'],
  ['pwd', 'pwd\r'],
  ['clear', 'clear\r'],
  ['df -h', 'df -h\r'],
  ['free -h', 'free -h\r'],
  ['ps aux', 'ps aux\r'],
  ['top', 'top\r'],
  ['whoami', 'whoami\r'],
  ['history', 'history\r'],
  ['uname -a', 'uname -a\r']
]

const FKEYS: string[][] = [
  ['Esc', '\x1b'],
  ['Tab', '\t'],
  ['⇧Tab', '\x1b[Z'],
  ['C-c', '\x03'],
  ['←', '\x1b[D'],
  ['↑', '\x1b[A'],
  ['↓', '\x1b[B'],
  ['→', '\x1b[C'],
  ['Enter', '\r']
]

const KB_SPECIAL: string[] = ['Esc', 'Tab', 'Caps', '⇧', '⏎', '⌫', 'Ctrl', 'Alt',
  'Del', 'Space', '←', '↑', '↓', '→']

const KB_R1: string[] = ['Esc', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12', 'Del']
const KB_U1: number[] = [1.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5]

const KB_R2: string[] = ['`~', '1!', '2@', '3#', '4$', '5%', '6^', '7&', '8*', '9(', '0)', '-_', '=+', '⌫']
const KB_U2: number[] = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2]

const KB_R3: string[] = ['Tab', 'qQ', 'wW', 'eE', 'rR', 'tT', 'yY', 'uU', 'iI', 'oO', 'pP', '[{', ']}', '\\|']
const KB_U3: number[] = [1.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5]

const KB_R4: string[] = ['Caps', 'aA', 'sS', 'dD', 'fF', 'gG', 'hH', 'jJ', 'kK', 'lL', ';:', '\'"', '⏎']
const KB_U4: number[] = [1.75, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.25]

const KB_R5: string[] = ['⇧', 'zZ', 'xX', 'cC', 'vV', 'bB', 'nN', 'mM', ',<', '.>', '/?', '', '↑', '']
const KB_U5: number[] = [2.25, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.75, 1, 1]

const KB_R6: string[] = ['Ctrl', 'Alt', 'Space', 'Alt', 'Ctrl', '←', '↓', '→']
const KB_U6: number[] = [1.2, 1.2, 7.2, 1.2, 1.2, 1, 1, 1]

function fnSeq(k: string): string {
  if (k === 'Esc') {
    return '\x1b'
  }
  if (k === 'F1') {
    return '\x1bOP'
  }
  if (k === 'F2') {
    return '\x1bOQ'
  }
  if (k === 'F3') {
    return '\x1bOR'
  }
  if (k === 'F4') {
    return '\x1bOS'
  }
  if (k === 'F5') {
    return '\x1b[15~'
  }
  if (k === 'F6') {
    return '\x1b[17~'
  }
  if (k === 'F7') {
    return '\x1b[18~'
  }
  if (k === 'F8') {
    return '\x1b[19~'
  }
  if (k === 'F9') {
    return '\x1b[20~'
  }
  if (k === 'F10') {
    return '\x1b[21~'
  }
  if (k === 'F11') {
    return '\x1b[23~'
  }
  if (k === 'F12') {
    return '\x1b[24~'
  }
  return ''
}

@Component
export struct TerminalPro {
  @Link show: boolean
  @Prop @Watch('onHostChange') hostId: number = 0
  @Prop @Watch('onHostChange') sessionSeq: number = 0
  @Prop hostName: string = ''
  @Prop hostAddr: string = ''
  @Prop compact: boolean = false
  @State ready: boolean = false
  @StorageLink('csx_termFont') @Watch('onFontChange') fontSize: number = 14
  @State closed: boolean = false
  @State kbOpen: boolean = false
  @State shiftOn: boolean = false
  @State capsOn: boolean = false
  @State ctrlOn: boolean = false
  @State altOn: boolean = false
  private controller: webview.WebviewController = new webview.WebviewController()
  private nativePorts: webview.WebMessagePort | null = null
  private dataFn: SshDataHandler | null = null
  private started: boolean = false
  private portReady: boolean = false
  private pendingText: string = ''
  private flushTimer: number = -1
  @StorageLink('csx_lang') csxLang: string = 'zh'

  t(zh: string, en: string): string {
    return this.csxLang === 'en' ? en : zh
  }

  aboutToAppear(): void {
    const fn: SshDataHandler = (hid: number, data: string, closed: boolean) => {
      if (hid !== this.hostId) {
        return
      }
      if (closed) {
        if (!this.closed) {
          this.closed = true
          this.flushText()
          this.write(`\r\n\x1b[31m${this.t('连接已断开', 'Connection closed')}\x1b[0m\r\n`)
        }
        return
      }
      this.appendText(data)
    }
    this.dataFn = fn
    SshService.get().onData(fn)
  }

  aboutToDisappear(): void {
    this.flushText()
    if (this.flushTimer >= 0) {
      clearTimeout(this.flushTimer)
      this.flushTimer = -1
    }
    if (this.dataFn !== null) {
      SshService.get().offData(this.dataFn)
      this.dataFn = null
    }
    if (this.nativePorts !== null) {
      try {
        this.nativePorts.close()
      } catch (err) {
      }
      this.nativePorts = null
    }
    this.portReady = false
    this.started = false
  }

  appendText(text: string): void {
    this.pendingText += text
    if (this.pendingText.length >= 32768) {
      this.flushText()
      return
    }
    if (this.flushTimer < 0) {
      this.flushTimer = setTimeout(() => {
        this.flushTimer = -1
        this.flushText()
      }, 16)
    }
  }

  flushText(): void {
    if (this.flushTimer >= 0) {
      clearTimeout(this.flushTimer)
      this.flushTimer = -1
    }
    if (this.pendingText.length === 0) {
      return
    }
    const s: string = this.pendingText
    this.pendingText = ''
    this.write(s)
  }

  onHostChange(): void {
    this.closed = false
    try {
      this.controller.runJavaScript(`if (typeof term !== 'undefined') { term.reset(); }`)
    } catch (err) {
    }
  }

  onFontChange(): void {
    this.applyFont(this.fontSize)
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
      const seg: string[] = s.substring(7).split(',')
      if (seg.length === 2) {
        SshService.get().resize(this.hostId, Number(seg[0]), Number(seg[1]))
      }
      return
    }
    if (!this.ready || this.closed) {
      return
    }
    SshService.get().send(this.hostId, s)
  }

  startSession(): void {
    if (this.started) {
      return
    }
    this.started = true
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
      ext.setString('\x01' + text)
      this.nativePorts.postMessageEventExt(ext)
    } catch (err) {
    }
  }

  sendKey(seq: string): void {
    if (this.closed || seq.length === 0) {
      return
    }
    try {
      SshService.get().send(this.hostId, seq)
    } catch (err) {
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
      promptAction.showToast({ message: this.t('字号范围 10~22', 'Font size range is 10~22') })
      return
    }
    this.fontSize = next
  }

  build() {
    Column() {
      this.TopBar()

      Stack({ alignContent: Alignment.Bottom }) {
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

        if (this.kbOpen) {
          this.KbPanel()
        }
      }
      .width('100%')
      .layoutWeight(1)

      this.ModBar()
    }
    .width('100%')
    .height('100%')
    .backgroundColor(TB_BG)
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
          Text(this.closed ? this.t('已断开', 'Disconnected') : (this.ready ? `${this.hostAddr} · ${this.fontSize}px` : this.t('连接中…', 'Connecting…')))
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
        Text(this.t('关闭', 'Close'))
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
    Column() {
      Row() {
        Scroll() {
          Row() {
            ForEach(CMDS, (c: string[]) => {
              Text(c[0])
                .fontSize(MT.fsCaption)
                .fontColor(TB_ACCENT)
                .fontFamily('monospace')
                .textAlign(TextAlign.Center)
                .height(32)
                .padding({ left: 11, right: 11 })
                .borderRadius(MT.radiusCtrl)
                .backgroundColor('#143DDC97')
                .margin({ right: 6 })
                .onClick(() => {
                  this.sendKey(c[1])
                })
            }, (c: string[]) => c[0])
          }
          .height(36)
        }
        .scrollable(ScrollDirection.Horizontal)
        .scrollBar(BarState.Off)
        .layoutWeight(1)

        Row() {
          this.KbIcon()
        }
        .width(38)
        .height(32)
        .borderRadius(MT.radiusCtrl)
        .justifyContent(FlexAlign.Center)
        .backgroundColor(this.kbOpen ? '#2E3DDC97' : TB_SURF)
        .margin({ left: 6 })
        .onClick(() => {
          this.kbOpen = !this.kbOpen
        })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)

      Row() {
        ForEach(FKEYS, (k: string[], i: number) => {
          Text(k[0])
            .fontSize(MT.fsCaption)
            .fontColor(k[0] === 'C-c' ? TB_DANGER : (k[0] === 'Enter' ? TB_ACCENT : TB_TEXT2))
            .fontFamily('monospace')
            .textAlign(TextAlign.Center)
            .maxLines(1)
            .height(32)
            .layoutWeight(1)
            .borderRadius(MT.radiusCtrl)
            .backgroundColor(TB_SURF)
            .margin({ right: i < FKEYS.length - 1 ? 5 : 0 })
            .onClick(() => {
              this.sendKey(k[1])
            })
        }, (k: string[]) => k[0])
      }
      .width('100%')
      .height(36)
      .margin({ top: 2 })
    }
    .width('100%')
    .padding({ left: 10, right: 8, top: 6, bottom: 8 })
    .backgroundColor(TB_BG)
    .border({ width: { top: 1 }, color: TB_LINE })
  }

  @Builder KbIcon() {
    Column() {
      Row() {
        this.KbDot()
        this.KbDot()
        this.KbDot()
      }
      Row() {
        this.KbDot()
        this.KbDot()
        this.KbDot()
      }
      .margin({ top: 3 })
    }
    .alignItems(HorizontalAlign.Center)
  }

  @Builder KbDot() {
    Row() {
    }
    .width(4)
    .height(4)
    .borderRadius(1)
    .backgroundColor(this.kbOpen ? TB_ACCENT : TB_TEXT2)
    .margin({ left: 1.5, right: 1.5 })
  }

  @Builder KbPanel() {
    Column() {
      Row() {
        Text(this.t('屏幕键盘', 'Keyboard'))
          .fontSize(MT.fsMicro)
          .fontColor(TB_TEXT3)
          .layoutWeight(1)
        Text('×')
          .fontSize(MT.fsTitle)
          .fontColor(TB_TEXT2)
          .textAlign(TextAlign.Center)
          .width(28)
          .height(22)
          .onClick(() => {
            this.kbOpen = false
          })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)
      .padding({ left: 4, right: 2 })

      this.KbRow(KB_R1, KB_U1)
      this.KbRow(KB_R2, KB_U2)
      this.KbRow(KB_R3, KB_U3)
      this.KbRow(KB_R4, KB_U4)
      this.KbRow(KB_R5, KB_U5)
      this.KbRow(KB_R6, KB_U6)
    }
    .width('100%')
    .padding({ left: 4, right: 4, top: 6, bottom: 6 })
    .backgroundColor('#0E1117')
    .border({ width: { top: 1 }, color: TB_LINE })
    .onClick(() => {
    })
  }

  kbLabel(k: string): string {
    if (KB_SPECIAL.includes(k) || fnSeq(k).length > 0) {
      return k === 'Space' ? '' : k
    }
    return this.kbChar(k)
  }

  kbChar(k: string): string {
    const lower: string = k.charAt(0)
    const upper: string = k.charAt(1)
    const lo: number = lower.charCodeAt(0)
    if (lo >= 97 && lo <= 122) {
      return this.shiftOn !== this.capsOn ? upper : lower
    }
    return this.shiftOn ? upper : lower
  }

  kbFont(k: string): number {
    if (k.length >= 3) {
      return 8
    }
    if (KB_SPECIAL.includes(k)) {
      return 10
    }
    return 11
  }

  kbActive(k: string): boolean {
    if (k === '⇧') {
      return this.shiftOn
    }
    if (k === 'Caps') {
      return this.capsOn
    }
    if (k === 'Ctrl') {
      return this.ctrlOn
    }
    if (k === 'Alt') {
      return this.altOn
    }
    return false
  }

  kbPress(k: string): void {
    if (k === '⇧') {
      this.shiftOn = !this.shiftOn
      return
    }
    if (k === 'Caps') {
      this.capsOn = !this.capsOn
      return
    }
    if (k === 'Ctrl') {
      this.ctrlOn = !this.ctrlOn
      return
    }
    if (k === 'Alt') {
      this.altOn = !this.altOn
      return
    }
    const fs: string = fnSeq(k)
    if (fs.length > 0) {
      this.sendKey(fs)
      return
    }
    let out: string = ''
    if (!KB_SPECIAL.includes(k)) {
      out = this.kbChar(k)
    } else if (k === 'Tab') {
      out = '\t'
    } else if (k === '⏎') {
      out = '\r'
    } else if (k === '⌫') {
      out = '\x7f'
    } else if (k === 'Del') {
      out = '\x1b[3~'
    } else if (k === 'Space') {
      out = ' '
    } else if (k === '←') {
      out = '\x1b[D'
    } else if (k === '↑') {
      out = '\x1b[A'
    } else if (k === '↓') {
      out = '\x1b[B'
    } else if (k === '→') {
      out = '\x1b[C'
    }
    if (this.ctrlOn && out.length === 1) {
      const code: number = out.toUpperCase().charCodeAt(0)
      if (code >= 64 && code <= 95) {
        out = String.fromCharCode(code - 64)
      }
    }
    if (this.altOn) {
      out = '\x1b' + out
    }
    this.sendKey(out)
    this.ctrlOn = false
    this.altOn = false
    if (!KB_SPECIAL.includes(k)) {
      this.shiftOn = false
    }
  }

  @Builder KbRow(keys: string[], units: number[]) {
    Row() {
      ForEach(keys, (k: string, i: number) => {
        Stack({ alignContent: Alignment.Center }) {
          if (k.length > 0) {
            Text(this.kbLabel(k))
              .fontSize(this.kbFont(k))
              .fontColor(this.kbActive(k) ? TB_ACCENT : TB_TEXT)
              .fontFamily('monospace')
              .textAlign(TextAlign.Center)
              .width('100%')
              .height(28)
              .borderRadius(5)
              .backgroundColor(this.kbActive(k) ? '#2A3DDC97' : '#1E2430')
          }
        }
        .width((units[i] / 15 * 100).toFixed(4) + '%')
        .height(31)
        .padding({ left: 1.5, right: 1.5 })
        .onClick(() => {
          if (k.length > 0) {
            this.kbPress(k)
          }
        })
      }, (k: string, i: number) => k + '#' + i.toString())
    }
    .width('100%')
  }
}
