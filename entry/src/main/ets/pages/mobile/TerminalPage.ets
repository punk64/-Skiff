import { promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { Arrow } from '../../common/Arrow'
import { TerminalTab } from '../../common/Models'
import { L } from '../../common/I18n'

const TERM_BG: string = '#05070A'
const TERM_TEXT: string = '#D9E3F0'
const TERM_ACCENT: string = '#3EE8A0'
const TERM_DANGER: string = '#FF6B6B'

@Component
export struct TerminalPage {
  @Link show: boolean
  @Prop hostName: string = ''
  @Prop hostAddr: string = ''
  @StorageProp('csx_themeVer') themeVer: number = 0
  @State tabs: TerminalTab[] = []
  @State activeId: number = 1
  @State input: string = ''
  @State ctrl: boolean = false
  @State alt: boolean = false
  @State seq: number = 2
  private scroller: Scroller = new Scroller()

  aboutToAppear(): void {
    this.tabs = [
      { id: 1, title: 'ubuntu@47.100', lines: ['Welcome to Ubuntu 22.04.4 LTS (GNU/Linux 5.15.0-101-generic x86_64)', 'Last login: Tue Oct 28 15:41:07 2026 from 112.65.12.4', ''] }
    ]
  }

  cur(): TerminalTab {
    const t: TerminalTab[] = this.tabs.filter((x: TerminalTab) => x.id === this.activeId)
    return t.length > 0 ? t[0] : this.tabs[0]
  }

  append(line: string): void {
    this.tabs = this.tabs.map((t: TerminalTab) => {
      if (t.id !== this.activeId) {
        return t
      }
      const nt: TerminalTab = { id: t.id, title: t.title, lines: t.lines.concat([line]) }
      return nt
    })
    setTimeout(() => {
      this.scroller.scrollEdge(Edge.Bottom)
    }, 30)
  }

  addTab(): void {
    const id: number = this.seq
    this.seq = this.seq + 1
    this.tabs = this.tabs.concat([{ id: id, title: 'ubuntu@47.100', lines: [''] }])
    this.activeId = id
  }

  closeTab(id: number): void {
    if (this.tabs.length <= 1) {
      this.show = false
      return
    }
    this.tabs = this.tabs.filter((t: TerminalTab) => t.id !== id)
    if (this.activeId === id) {
      this.activeId = this.tabs[0].id
    }
  }

  run(): void {
    const cmd: string = this.input.trim()
    this.input = ''
    this.append('ubuntu@web-prod:~$ ' + cmd)
    if (cmd.length === 0) {
      return
    }
    const c: string = cmd.toLowerCase()
    if (c === 'clear') {
      this.tabs = this.tabs.map((t: TerminalTab) => {
        if (t.id !== this.activeId) {
          return t
        }
        const nt: TerminalTab = { id: t.id, title: t.title, lines: [''] }
        return nt
      })
      return
    }
    if (c === 'exit') {
      this.closeTab(this.activeId)
      return
    }
    if (c === 'pwd') {
      this.append('/home/ubuntu')
    } else if (c === 'whoami') {
      this.append('ubuntu')
    } else if (c === 'ls' || c === 'ls -lh') {
      this.append('total 32K')
      this.append('drwxr-xr-x 3 ubuntu ubuntu 4.0K Oct 28 14:55 logs')
      this.append('-rw-r--r-- 1 ubuntu ubuntu 3.1K Sep 12 08:30 .bashrc')
      this.append('-rw------- 1 ubuntu ubuntu 1.7K Oct 18 10:11 1.pem')
      this.append('-rwxr-xr-x 1 ubuntu ubuntu  842 Oct 27 19:26 deploy.sh')
    } else if (c === 'date') {
      this.append('Wed Oct 28 15:57:12 CST 2026')
    } else if (c === 'uname -a') {
      this.append('Linux web-prod 5.15.0-101-generic #111-Ubuntu SMP x86_64 GNU/Linux')
    } else if (c === 'df -h') {
      this.append('Filesystem      Size  Used Avail Use% Mounted on')
      this.append('/dev/vda1        40G   18G   20G  48% /')
      this.append('tmpfs           1.9G     0  1.9G   0% /dev/shm')
    } else if (c === 'free -h') {
      this.append('               total        used        free      shared')
      this.append('Mem:           3.8Gi       1.2Gi       1.1Gi        18Mi')
    } else if (c === 'docker ps') {
      this.append('CONTAINER ID   IMAGE          STATUS         PORTS')
      this.append('a3f9c81e7b20   nginx:alpine   Up 3 days      0.0.0.0:8888->80/tcp')
      this.append('72d10bc4e9a1   redis:7        Up 3 days      6379/tcp')
    } else if (c === 'help') {
      this.append(L.t('可用命令：ls, pwd, whoami, date, uname -a, df -h, free -h, docker ps, clear, exit', 'Commands: ls, pwd, whoami, date, uname -a, df -h, free -h, docker ps, clear, exit'))
    } else {
      this.append('bash: ' + cmd.split(' ')[0] + ': command not found')
    }
    this.append('')
  }

  press(k: string): void {
    if (k === 'Enter') {
      this.run()
    }
  }

  build() {
    if (this.themeVer >= 0) {
      Column() {
        this.TabBar()
        Scroll(this.scroller) {
          Column() {
            ForEach(this.cur().lines, (l: string, i: number) => {
              Text(l.length === 0 ? ' ' : l)
                .fontSize(MT.fsSmall)
                .fontColor(l.startsWith('ubuntu@') ? TERM_ACCENT : l.startsWith('bash:') ? TERM_DANGER : TERM_TEXT)
                .fontFamily('monospace')
                .lineHeight(19)
                .width('100%')
            }, (l: string, i: number) => i.toString())
          }
          .width('100%')
          .padding({ left: 14, right: 14, top: 10, bottom: 10 })
          .alignItems(HorizontalAlign.Start)
        }
        .layoutWeight(1)
        .align(Alignment.Top)
        .scrollBar(BarState.Off)
        .backgroundColor(TERM_BG)

        this.ModBar()

        Row() {
          Text('$')
            .fontSize(MT.fsSmall)
            .fontColor(MT.accent)
            .fontFamily('monospace')
          TextInput({ text: this.input, placeholder: L.t('输入命令', 'Type a command') })
            .fontSize(MT.fsSmall)
            .fontColor(MT.text)
            .placeholderColor(MT.text3)
            .fontFamily('monospace')
            .backgroundColor(MT.transparent)
            .padding({ left: 6, right: 0 })
            .height(42)
            .layoutWeight(1)
            .onChange((v: string) => {
              this.input = v
            })
            .onSubmit(() => {
              this.run()
            })
          Text('↵')
            .fontSize(MT.fsTitle)
            .fontColor(MT.accentFg)
            .textAlign(TextAlign.Center)
            .width(36)
            .height(32)
            .borderRadius(MT.radiusCtrl)
            .backgroundColor(MT.accent)
            .onClick(() => {
              this.run()
            })
        }
        .width('100%')
        .padding({ left: 14, right: 14, top: 8, bottom: 10 })
        .backgroundColor(MT.surface)
        .alignItems(VerticalAlign.Center)
      }
      .width('100%')
      .height('100%')
      .backgroundColor(TERM_BG)
    }
  }

  @Builder TabBar() {
    Row() {
      Stack() {
        Arrow({ dir: 'left', color: MT.text2 , len: 19 })
      }
      .width(32)
      .height(36)
      .onClick(() => {
        this.show = false
      })
      Scroll() {
        Row() {
          ForEach(this.tabs, (t: TerminalTab) => {
            Row() {
              Text(t.title)
                .fontSize(MT.fsCaption)
                .fontColor(this.activeId === t.id ? MT.accent : MT.text3)
                .maxLines(1)
              Text('×')
                .fontSize(MT.fsCaption)
                .fontColor(MT.text3)
                .margin({ left: 6 })
                .onClick(() => {
                  this.closeTab(t.id)
                })
            }
            .height(30)
            .padding({ left: 11, right: 9 })
            .margin({ right: 6 })
            .borderRadius(MT.radiusCtrl)
            .backgroundColor(this.activeId === t.id ? MT.accentSoft : MT.surface2)
            .onClick(() => {
              this.activeId = t.id
            })
          }, (t: TerminalTab) => t.id.toString())
        }
      }
      .scrollable(ScrollDirection.Horizontal)
      .scrollBar(BarState.Off)
      .layoutWeight(1)
      .margin({ left: 4 })

      Text('+')
        .fontSize(MT.fsDisplay)
        .fontColor(MT.text2)
        .width(32)
        .height(30)
        .textAlign(TextAlign.Center)
        .borderRadius(MT.radiusCtrl)
        .backgroundColor(MT.surface2)
        .onClick(() => {
          this.addTab()
        })
    }
    .width('100%')
    .padding({ left: 8, right: 10, top: 8, bottom: 8 })
    .backgroundColor(MT.surface)
    .border({ width: { bottom: 1 }, color: MT.line })
    .alignItems(VerticalAlign.Center)
  }

  @Builder ModBar() {
    Row() {
      this.Key('Ctrl', this.ctrl)
      this.Key('Alt', this.alt)
      this.Key('Esc', false)
      this.Key('Tab', false)
      this.Key('↑', false)
      this.Key('↓', false)
    }
    .width('100%')
    .padding({ left: 10, right: 10, top: 7, bottom: 7 })
    .backgroundColor(MT.surface)
    .border({ width: { top: 1 }, color: MT.line })
  }

  @Builder Key(label: string, on: boolean) {
    Text(label)
      .fontSize(MT.fsCaption)
      .fontColor(on ? MT.accent : MT.text2)
      .textAlign(TextAlign.Center)
      .height(30)
      .layoutWeight(1)
      .margin({ left: 3, right: 3 })
      .borderRadius(MT.radiusChip)
      .backgroundColor(on ? MT.accentSoft : MT.surface2)
      .onClick(() => {
        if (label === 'Ctrl') {
          this.ctrl = !this.ctrl
          return
        }
        if (label === 'Alt') {
          this.alt = !this.alt
          return
        }
        if (label === 'Esc') {
          this.input = ''
          return
        }
        if (label === 'Tab') {
          this.input = this.input + '  '
          return
        }
        if (label === '↑') {
          this.input = 'docker ps'
          return
        }
        if (label === '↓') {
          this.input = ''
          return
        }
      })
  }
}
