import { promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { Arrow } from '../../common/Arrow'
import { SftpEntry, TransferTask, seedSftp } from '../../common/Models'
import { L } from '../../common/I18n'

@Component
export struct FilesPage {
  @Prop connected: boolean = false
  @Prop hostName: string = ''
  @Link masking: boolean
  @StorageProp('csx_themeVer') themeVer: number = 0
  @State view: string = 'browse'
  @State cwd: string = '/home/ubuntu'
  @State entries: SftpEntry[] = seedSftp('/home/ubuntu')
  @State selectMode: boolean = false
  @State selected: string[] = []
  @State transfers: TransferTask[] = []
  @State seq: number = 1

  crumbs(): string[] {
    return this.cwd.split('/').filter((p: string) => p.length > 0)
  }

  goto(path: string): void {
    this.cwd = path
    this.entries = seedSftp(path)
    this.setSelect(false)
  }

  backDir(): void {
    const parts: string[] = this.crumbs()
    if (parts.length <= 1) {
      this.goto('/')
      return
    }
    this.goto('/' + parts.slice(0, parts.length - 1).join('/'))
  }

  setSelect(v: boolean): void {
    this.selectMode = v
    this.masking = v
    if (!v) {
      this.selected = []
    }
  }

  toggleSel(name: string): void {
    if (this.selected.includes(name)) {
      this.selected = this.selected.filter((n: string) => n !== name)
    } else {
      this.selected = this.selected.concat([name])
    }
  }

  addTask(name: string, up: boolean, isDir: boolean, total: number, files: number): void {
    const t: TransferTask = {
      id: this.seq, name: name, up: up, isDir: isDir,
      total: total, done: 0, files: files, filesDone: 0, state: 'run'
    }
    this.seq = this.seq + 1
    this.transfers = [t].concat(this.transfers)
  }

  sizeToBytes(s: string): number {
    const n: number = parseFloat(s)
    if (s.includes('GB')) {
      return n * 1073741824
    }
    if (s.includes('MB')) {
      return n * 1048576
    }
    if (s.includes('KB')) {
      return n * 1024
    }
    return n
  }

  fmt(b: number): string {
    if (b >= 1073741824) {
      return (b / 1073741824).toFixed(1) + ' GB'
    }
    if (b >= 1048576) {
      return (b / 1048576).toFixed(1) + ' MB'
    }
    if (b >= 1024) {
      return (b / 1024).toFixed(1) + ' KB'
    }
    return Math.round(b).toString() + ' B'
  }

  private timer: number = -1

  aboutToAppear(): void {
    this.timer = setInterval(() => {
      this.tick()
    }, 280)
  }

  aboutToDisappear(): void {
    if (this.timer >= 0) {
      clearInterval(this.timer)
    }
  }

  tick(): void {
    let active: number = 0
    this.transfers.forEach((t: TransferTask) => {
      if (t.state === 'run' && t.done < t.total) {
        active = active + 1
      }
    })
    if (active === 0) {
      return
    }
    this.transfers = this.transfers.map((t: TransferTask) => {
      if (t.state !== 'run' || t.done >= t.total) {
        return t
      }
      const step: number = t.total * (0.04 + Math.random() * 0.08)
      const done: number = Math.min(t.total, t.done + step)
      const fd: number = Math.min(t.files, Math.ceil(t.files * done / t.total))
      const nt: TransferTask = {
        id: t.id, name: t.name, up: t.up, isDir: t.isDir,
        total: t.total, done: done, files: t.files, filesDone: fd,
        state: done >= t.total ? 'done' : 'run'
      }
      return nt
    })
  }

  build() {
    if (this.themeVer >= 0) {
      Column() {
        if (this.view === 'browse') {
          this.BrowseView()
        } else {
          this.TransferView()
        }
      }
      .width('100%')
      .height('100%')
    }
  }

  @Builder BrowseView() {
    Column() {
      Row() {
        Text(this.connected ? `SFTP · ${this.hostName}` : L.t('SFTP · 未连接', 'SFTP · Not connected'))
          .fontSize(MT.fsSmall)
          .fontColor(MT.text3)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .layoutWeight(1)

        Text('⇅')
          .fontSize(MT.fsTitle)
          .fontColor(MT.text2)
          .textAlign(TextAlign.Center)
          .width(40)
          .height(40)
          .borderRadius(MT.radiusCard)
          .backgroundColor(MT.surface)
          .onClick(() => {
            this.view = 'xfer'
          })
        Text('↑')
          .fontSize(MT.fsTitle)
          .fontColor(this.connected ? MT.accent : MT.text3)
          .textAlign(TextAlign.Center)
          .width(40)
          .height(40)
          .borderRadius(MT.radiusCard)
          .backgroundColor(this.connected ? MT.accentSoft : MT.surface)
          .margin({ left: 8 })
          .onClick(() => {
            if (!this.connected) {
              promptAction.showToast({ message: L.t('先连接一台主机', 'Connect a host first') })
              return
            }
            this.addTask('app.tar.gz', true, false, 12.4 * 1048576, 1)
            promptAction.showToast({ message: L.t('已加入上传队列', 'Added to upload queue') })
            this.view = 'xfer'
          })
      }
      .width('100%')
      .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 8, bottom: 8 })
      .alignItems(VerticalAlign.Center)

      if (!this.connected) {
        Column() {
          Text(L.t('SFTP 未就绪', 'SFTP not ready'))
            .fontSize(MT.fsTitle)
            .fontWeight(FontWeight.Medium)
            .fontColor(MT.text2)
          Text(L.t('连接主机后可浏览、上传、下载远程文件', 'Connect a host to browse, upload and download files'))
            .fontSize(MT.fsSmall)
            .fontColor(MT.text3)
            .margin({ top: 8 })
        }
        .width('100%')
        .layoutWeight(1)
        .justifyContent(FlexAlign.Center)
      } else {
        Scroll() {
          Row() {
            Text('/')
              .fontSize(MT.fsSmall)
              .fontColor(MT.blue)
              .onClick(() => {
                this.goto('/')
              })
            ForEach(this.crumbs(), (p: string, i: number) => {
              if (i > 0) {
                Text('/')
                  .fontSize(MT.fsSmall)
                  .fontColor(MT.text3)
              }
              Text(p)
                .fontSize(MT.fsSmall)
                .fontColor(i === this.crumbs().length - 1 ? MT.text : MT.blue)
                .onClick(() => {
                  this.goto('/' + this.crumbs().slice(0, i + 1).join('/'))
                })
            }, (p: string, i: number) => p + i.toString())
          }
          .width('100%')
          .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: 10 })
        }
        .scrollable(ScrollDirection.Horizontal)
        .scrollBar(BarState.Off)
        .width('100%')

        Row() {
          Text(this.selectMode ? L.t('取消', 'Cancel') : L.t('选择', 'Select'))
            .fontSize(MT.fsSmall)
            .fontColor(MT.text2)
            .padding({ left: 10, right: 10, top: 6, bottom: 6 })
            .borderRadius(MT.radiusCtrl)
            .backgroundColor(MT.surface)
            .onClick(() => {
              this.setSelect(!this.selectMode)
            })
          Text(this.selectMode ? L.t(`已选 ${this.selected.length} 项`, `${this.selected.length} selected`) : '')
            .fontSize(MT.fsSmall)
            .fontColor(MT.text3)
            .layoutWeight(1)
            .textAlign(TextAlign.End)
        }
        .width('100%')
        .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: 10 })
        .alignItems(VerticalAlign.Center)

        Scroll() {
          Column() {
            ForEach(this.entries, (e: SftpEntry) => {
              Row() {
                if (this.selectMode) {
                  Text(this.selected.includes(e.name) ? '✓' : '')
                    .fontSize(MT.fsSmall)
                    .fontColor(MT.accentFg)
                    .textAlign(TextAlign.Center)
                    .width(20)
                    .height(20)
                    .borderRadius(MT.radiusChip)
                    .backgroundColor(this.selected.includes(e.name) ? MT.accent : MT.surface2)
                    .margin({ right: 11 })
                }
                Stack() {
                  Text(e.isDir ? '▸' : '▫')
                    .fontSize(MT.fsTitle)
                    .fontColor(e.isDir ? MT.accent : MT.text3)
                    .textAlign(TextAlign.Center)
                }
                .width(30)
                .height(30)
                .borderRadius(MT.radiusChip)
                .backgroundColor(e.isDir ? MT.accentSoft : MT.surface2)
                .margin({ right: 10 })
                Column() {
                  Text(e.name)
                    .fontSize(MT.fsBody)
                    .fontColor(MT.text)
                    .maxLines(1)
                    .textOverflow({ overflow: TextOverflow.Ellipsis })
                  Text(`${e.size} · ${e.time}`)
                    .fontSize(MT.fsCaption)
                    .fontColor(MT.text3)
                    .margin({ top: 4 })
                }
                .alignItems(HorizontalAlign.Start)
                .layoutWeight(1)
                .margin({ left: 8 })

                if (!this.selectMode) {
                  Stack() {
                    Arrow({ dir: 'right', color: MT.text3 , len: 15 })
                  }
                  .width(20)
                  .height(20)
                }
              }
              .width('100%')
              .padding(14)
              .margin({ bottom: 8 })
              .borderRadius(MT.radiusCard)
              .backgroundColor(MT.surface)
              .alignItems(VerticalAlign.Center)
              .onClick(() => {
                if (this.selectMode) {
                  this.toggleSel(e.name)
                  return
                }
                if (e.isDir) {
                  this.goto((this.cwd === '/' ? '' : this.cwd) + '/' + e.name)
                } else {
                  this.addTask(e.name, false, false, this.sizeToBytes(e.size), 1)
                  promptAction.showToast({ message: L.t('已加入下载队列', 'Added to download queue') })
                  this.view = 'xfer'
                }
              })
            }, (e: SftpEntry) => e.name)
          }
          .width('100%')
          .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: MT.tabbarSpace })
        }
        .layoutWeight(1)
        .align(Alignment.Top)
        .scrollBar(BarState.Off)

        if (this.selectMode) {
          Row() {
            Text(L.t(`下载 ${this.selected.length} 项`, `Download ${this.selected.length} items`))
              .fontSize(MT.fsBody)
              .fontColor(MT.accentFg)
              .textAlign(TextAlign.Center)
              .height(46)
              .layoutWeight(1)
              .borderRadius(MT.radiusBtn)
              .backgroundColor(MT.accent)
              .onClick(() => {
                this.selected.forEach((n: string) => {
                  const it: SftpEntry[] = this.entries.filter((x: SftpEntry) => x.name === n)
                  if (it.length > 0) {
                    this.addTask(it[0].name, false, it[0].isDir, this.sizeToBytes(it[0].size), 1)
                  }
                })
                promptAction.showToast({ message: L.t(`已加入 ${this.selected.length} 个任务`, `${this.selected.length} tasks added`) })
                this.setSelect(false)
                this.view = 'xfer'
              })
            Text(L.t('删除', 'Delete'))
              .fontSize(MT.fsBody)
              .fontColor(MT.danger)
              .textAlign(TextAlign.Center)
              .height(46)
              .width(90)
              .margin({ left: 10 })
              .borderRadius(MT.radiusBtn)
              .backgroundColor(MT.surface)
              .onClick(() => {
                this.entries = this.entries.filter((x: SftpEntry) => !this.selected.includes(x.name))
                this.setSelect(false)
                promptAction.showToast({ message: L.t('已删除', 'Deleted') })
              })
          }
          .width('100%')
          .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: 96 })
        }
      }
    }
    .width('100%')
    .height('100%')
  }

  @Builder TransferView() {
    Column() {
      Row() {
        Stack() {
          Arrow({ dir: 'left', color: MT.blue , len: 20 })
        }
        .width(38)
        .height(38)
        .onClick(() => {
          this.view = 'browse'
        })
        Column() {
          Text(L.t('传输', 'Transfers'))
            .fontSize(MT.fsTitle)
            .fontWeight(FontWeight.Medium)
            .fontColor(MT.text)
          Text(this.transfers.length > 0 ? L.t(`${this.transfers.length} 个任务`, `${this.transfers.length} tasks`) : L.t('暂无任务', 'No tasks'))
            .fontSize(MT.fsCaption)
            .fontColor(MT.text3)
            .margin({ top: 3 })
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)
        Text(L.t('清空', 'Clear'))
          .fontSize(MT.fsSmall)
          .fontColor(MT.text2)
          .onClick(() => {
            this.transfers = this.transfers.filter((t: TransferTask) => t.state === 'run')
            promptAction.showToast({ message: L.t('已清空已完成', 'Completed tasks cleared') })
          })
      }
      .width('100%')
      .padding({ left: 8, right: MT.pagePadding, top: 12, bottom: 12 })
      .alignItems(VerticalAlign.Center)

      if (this.transfers.length === 0) {
        Column() {
          Text(L.t('没有传输任务', 'No transfers'))
            .fontSize(MT.fsTitle)
            .fontWeight(FontWeight.Medium)
            .fontColor(MT.text2)
          Text(L.t('上传或下载后会在这里显示进度', 'Progress appears here after upload or download'))
            .fontSize(MT.fsSmall)
            .fontColor(MT.text3)
            .margin({ top: 8 })
        }
        .width('100%')
        .layoutWeight(1)
        .justifyContent(FlexAlign.Center)
      } else {
        Scroll() {
          Column() {
            ForEach(this.transfers, (t: TransferTask) => {
              Column() {
                Row() {
                  Text(t.up ? '↑' : '↓')
                    .fontSize(MT.fsSmall)
                    .fontColor(t.up ? MT.blue : MT.accent)
                    .width(26)
                    .height(26)
                    .textAlign(TextAlign.Center)
                    .borderRadius(MT.radiusChip)
                    .backgroundColor(t.up ? MT.blueSoft : MT.accentSoft)
                  Column() {
                    Row() {
                      Text(t.name)
                        .fontSize(MT.fsSmall)
                        .fontColor(MT.text)
                        .maxLines(1)
                        .textOverflow({ overflow: TextOverflow.Ellipsis })
                        .constraintSize({ maxWidth: 150 })
                      if (t.isDir) {
                        Text(L.t('文件夹', 'Folder'))
                          .fontSize(MT.fsMicro)
                          .fontColor(MT.text3)
                          .backgroundColor(MT.surface2)
                          .borderRadius(MT.radiusChip)
                          .padding({ left: 5, right: 5, top: 1, bottom: 1 })
                          .margin({ left: 6 })
                      }
                      Blank()
                      Text(`${Math.round(t.done / t.total * 100)}%`)
                        .fontSize(MT.fsCaption)
                        .fontColor(MT.text2)
                    }
                    .width('100%')
                    Text(`${this.fmt(t.done)} / ${this.fmt(t.total)}`)
                      .fontSize(MT.fsMicro)
                      .fontColor(MT.text3)
                      .margin({ top: 4 })
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)
                  .margin({ left: 10 })

                  Text(t.state === 'done' ? '×' : t.state === 'run' ? '❚❚' : '▶')
                    .fontSize(t.state === 'done' ? 15 : 11)
                    .fontColor(MT.text2)
                    .width(30)
                    .height(30)
                    .textAlign(TextAlign.Center)
                    .borderRadius(MT.radiusCtrl)
                    .backgroundColor(MT.surface2)
                    .onClick(() => {
                      if (t.state === 'done') {
                        this.transfers = this.transfers.filter((x: TransferTask) => x.id !== t.id)
                        return
                      }
                      this.transfers = this.transfers.map((x: TransferTask) => {
                        if (x.id !== t.id) {
                          return x
                        }
                        const nt: TransferTask = {
                          id: x.id, name: x.name, up: x.up, isDir: x.isDir,
                          total: x.total, done: x.done, files: x.files, filesDone: x.filesDone,
                          state: x.state === 'run' ? 'pause' : 'run'
                        }
                        return nt
                      })
                    })
                }
                .width('100%')
                .alignItems(VerticalAlign.Center)

                Row() {
                  Row()
                    .width(`${Math.round(t.done / t.total * 100)}%`)
                    .height(4)
                    .borderRadius(2)
                    .backgroundColor(t.state === 'run' ? MT.accent : MT.text3)
                }
                .width('100%')
                .height(4)
                .borderRadius(2)
                .backgroundColor(MT.surface2)
                .margin({ top: 10 })
                .justifyContent(FlexAlign.Start)
              }
              .width('100%')
              .padding(14)
              .margin({ bottom: 10 })
              .borderRadius(MT.radiusCard)
              .backgroundColor(MT.surface)
            }, (t: TransferTask) => `${t.id}-${Math.round(t.done)}-${t.state}`)
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
  }
}
