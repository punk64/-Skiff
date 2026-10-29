import { promptAction } from '@kit.ArkUI'
import { common } from '@kit.AbilityKit'
import { fileIo as fs, picker } from '@kit.CoreFileKit'
import { MT } from '../../common/MobileTheme'
import { Arrow } from '../../common/Arrow'
import { SftpEntry, TransferTask } from '../../common/Models'
import { SshService, UploadFileItem } from '../../common/SshService'
import { L } from '../../common/I18n'

class EntrySource implements IDataSource {
  private data: SftpEntry[] = []
  private listeners: DataChangeListener[] = []

  totalCount(): number {
    return this.data.length
  }

  getData(index: number): SftpEntry {
    return this.data[index]
  }

  registerDataChangeListener(listener: DataChangeListener): void {
    if (this.listeners.indexOf(listener) < 0) {
      this.listeners.push(listener)
    }
  }

  unregisterDataChangeListener(listener: DataChangeListener): void {
    const pos: number = this.listeners.indexOf(listener)
    if (pos >= 0) {
      this.listeners.splice(pos, 1)
    }
  }

  reload(list: SftpEntry[]): void {
    this.data = list
    this.listeners.forEach((l: DataChangeListener) => {
      l.onDataReloaded()
    })
  }
}

@Component
export struct FilesPage {
  @Prop @Watch('onTargetChanged') connected: boolean = false
  @Prop @Watch('onTargetChanged') hostId: number = 0
  @Prop hostName: string = ''
  @Link masking: boolean
  @Link view: string
  @Link dlg: string
  @State cwd: string = ''
  @State entries: SftpEntry[] = []
  @State selectMode: boolean = false
  @State selected: string[] = []
  @State transfers: TransferTask[] = []
  @State seq: number = 1
  @State loading: boolean = false
  @State errMsg: string = ''
  @State dlgTitle: string = ''
  @State dlgBody: string = ''
  @State dlgDanger: boolean = false
  @State renameFrom: string = ''
  @State renameIsDir: boolean = false
  @State renameTo: string = ''
  @State mkName: string = ''
  @State mName: string = ''
  @State mPath: string = ''
  @State mIsDir: boolean = false
  @State mSize: string = ''
  @State mTime: string = ''

  private loadSeq: number = 0
  private src: EntrySource = new EntrySource()

  setEntries(list: SftpEntry[]): void {
    this.entries = list
    this.src.reload(list)
  }

  aboutToAppear(): void {
    this.onTargetChanged()
  }

  onTargetChanged(): void {
    this.loadSeq = this.loadSeq + 1
    this.selected = []
    this.selectMode = false
    this.masking = false
    this.errMsg = ''
    if (this.connected && this.hostId > 0) {
      this.cwd = '/'
      this.refresh(this.loadSeq)
    } else {
      this.cwd = ''
      this.setEntries([])
    }
  }

  crumbs(): string[] {
    return this.cwd.split('/').filter((p: string) => p.length > 0)
  }

  goto(path: string): void {
    this.cwd = path.length === 0 ? '/' : path
    this.setSelect(false)
    this.refresh()
  }

  backDir(): void {
    if (this.cwd === '/' || this.cwd.length === 0) {
      return
    }
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

  errText(e: Object): string {
    const raw: string = e instanceof Error ? e.message : String(e)
    if (raw.indexOf('NOT_CONNECTED') >= 0) {
      return L.t('主机未连接', 'Host not connected')
    }
    if (raw.indexOf('SFTP_INIT_FAILED') >= 0) {
      return L.t('SFTP 初始化失败', 'SFTP init failed')
    }
    if (raw.indexOf('SFTP_OP_FAILED') >= 0) {
      const rest: string = raw.substring(raw.indexOf('SFTP_OP_FAILED') + 14)
      const trimmed: string = rest.startsWith(':') ? rest.substring(1) : rest
      return trimmed.trim()
    }
    return raw
  }

  isPermErr(raw: string): boolean {
    const s: string = raw.toLowerCase()
    return s.indexOf('permission denied') >= 0 || s.indexOf('eacces') >= 0 ||
      s.indexOf('permission') >= 0 || s.indexOf('权限') >= 0 || s.indexOf('denied') >= 0 ||
      s.indexOf('sftp code: 3') >= 0
  }

  parentOf(path: string): string {
    const q: number = path.lastIndexOf('/')
    if (q <= 0) {
      return '/'
    }
    return path.substring(0, q)
  }

  closeDlg(): void {
    this.dlg = ''
    this.dlgTitle = ''
    this.dlgBody = ''
    this.dlgDanger = false
    this.renameFrom = ''
    this.renameTo = ''
    this.mkName = ''
  }

  openPerm(op: string, path: string, raw: string): void {
    this.dlg = 'perm'
    this.dlgTitle = L.t('权限不足', 'Permission denied')
    this.dlgBody = L.t(
      `无法${op}：${path}\n\n当前账号对该路径没有写权限。\n${raw}`,
      `Cannot ${op}: ${path}\n\nThe current account has no write permission on this path.\n${raw}`)
    this.dlgDanger = true
  }

  openRename(e: SftpEntry): void {
    this.dlg = 'rename'
    this.dlgTitle = L.t('重命名', 'Rename')
    this.renameFrom = e.path
    this.renameIsDir = e.isDir
    this.renameTo = e.name
  }

  openDetail(e: SftpEntry): void {
    this.dlg = 'detail'
    this.dlgTitle = L.t('详情', 'Details')
    const kind: string = e.isDir ? L.t('文件夹', 'Folder') : L.t('文件', 'File')
    this.dlgBody = L.t(
      `名称：${e.name}\n类型：${kind}\n大小：${e.size}\n修改时间：${e.time}\n路径：${e.path}`,
      `Name: ${e.name}\nType: ${kind}\nSize: ${e.size}\nModified: ${e.time}\nPath: ${e.path}`)
    this.dlgDanger = false
  }

  confirmRemove(e: SftpEntry): void {
    AlertDialog.show({
      title: L.t('删除确认', 'Confirm delete'),
      message: e.isDir
        ? L.t(`确定删除文件夹「${e.name}」及其内容？`, `Delete folder "${e.name}" and its contents?`)
        : L.t(`确定删除「${e.name}」？`, `Delete "${e.name}"?`),
      primaryButton: {
        value: L.t('取消', 'Cancel'),
        action: () => {
        }
      },
      secondaryButton: {
        value: L.t('删除', 'Delete'),
        fontColor: $r('app.color.csx_danger'),
        action: () => {
          this.removeOne(e)
        }
      }
    })
  }

  async removeOne(e: SftpEntry): Promise<void> {
    if (!this.connected || this.hostId <= 0) {
      return
    }
    try {
      await SshService.get().remove(this.hostId, e.path, e.isDir)
      promptAction.showToast({ message: L.t('已删除', 'Deleted') })
      await this.refresh()
    } catch (err) {
      const raw: string = this.errText(err)
      if (this.isPermErr(raw)) {
        this.openPerm(L.t('删除', 'delete'), e.path, raw)
        return
      }
      promptAction.showToast({ message: raw })
    }
  }

  async doRename(): Promise<void> {
    const target: string = this.renameTo.trim()
    if (target.length === 0 || target === this.renameFrom.substring(this.renameFrom.lastIndexOf('/') + 1)) {
      this.closeDlg()
      return
    }
    if (target.indexOf('/') >= 0) {
      promptAction.showToast({ message: L.t('名称不能包含 /', 'Name cannot contain /') })
      return
    }
    const dir: string = this.parentOf(this.renameFrom)
    const next: string = (dir === '/' ? '' : dir) + '/' + target
    const op: string = L.t('重命名', 'rename')
    try {
      await SshService.get().renamePath(this.hostId, this.renameFrom, next)
      this.closeDlg()
      await this.refresh()
    } catch (err) {
      const raw: string = this.errText(err)
      this.closeDlg()
      if (this.isPermErr(raw)) {
        this.openPerm(op, this.renameFrom, raw)
        return
      }
      promptAction.showToast({ message: raw })
    }
  }

  openMkdir(): void {
    if (!this.connected || this.hostId <= 0) {
      promptAction.showToast({ message: L.t('先连接一台主机', 'Connect a host first') })
      return
    }
    this.dlg = 'mkdir'
    this.dlgTitle = L.t('新建文件夹', 'New folder')
    this.mkName = ''
    this.dlgDanger = false
  }

  async doMkdir(): Promise<void> {
    const name: string = this.mkName.trim()
    if (name.length === 0) {
      promptAction.showToast({ message: L.t('请输入文件夹名称', 'Enter a folder name') })
      return
    }
    if (name.indexOf('/') >= 0) {
      promptAction.showToast({ message: L.t('名称不能包含 /', 'Name cannot contain /') })
      return
    }
    const dir: string = this.cwd.length === 0 ? '/' : this.cwd
    const path: string = (dir === '/' ? '' : dir) + '/' + name
    const op: string = L.t('新建文件夹', 'create folder')
    try {
      await SshService.get().makeDir(this.hostId, path)
      this.closeDlg()
      await this.refresh()
      promptAction.showToast({ message: L.t('已创建', 'Created') })
    } catch (err) {
      const raw: string = this.errText(err)
      this.closeDlg()
      if (this.isPermErr(raw)) {
        this.openPerm(op, dir, raw)
        return
      }
      promptAction.showToast({ message: raw })
    }
  }

  async refresh(seq?: number): Promise<void> {
    const my: number = seq === undefined ? this.loadSeq + 1 : seq
    if (seq === undefined) {
      this.loadSeq = my
    }
    if (!this.connected || this.hostId <= 0) {
      this.setEntries([])
      return
    }
    this.loading = true
    this.errMsg = ''
    try {
      const list: SftpEntry[] = await SshService.get().listDir(this.hostId, this.cwd)
      if (my === this.loadSeq) {
        this.setEntries(list)
      }
    } catch (e) {
      if (my === this.loadSeq) {
        this.errMsg = this.errText(e)
        this.setEntries([])
      }
    } finally {
      if (my === this.loadSeq) {
        this.loading = false
      }
    }
  }

  newTask(id: number, name: string, up: boolean): TransferTask {
    return { id: id, name: name, up: up, pct: 0, speed: '', state: 'run', msg: '' }
  }

  patch(id: number, pct: number, state: string, speed: string): void {
    this.transfers = this.transfers.map((t: TransferTask) => {
      if (t.id !== id) {
        return t
      }
      const nt: TransferTask = {
        id: t.id, name: t.name, up: t.up, pct: pct, speed: speed, state: state, msg: ''
      }
      return nt
    })
  }

  baseName(uri: string): string {
    const q: number = uri.lastIndexOf('/')
    const raw: string = q >= 0 ? uri.substring(q + 1) : uri
    try {
      return decodeURIComponent(raw)
    } catch (e) {
      return raw
    }
  }

  subTitle(e: SftpEntry): string {
    if (e.isDir) {
      return e.time.length > 0 ? e.time : L.t('文件夹', 'Folder')
    }
    return e.time.length > 0 ? `${e.size} · ${e.time}` : e.size
  }

  stateText(t: TransferTask): string {
    if (t.state === 'done') {
      return L.t('已完成', 'Done')
    }
    if (t.state === 'fail') {
      return L.t('失败', 'Failed')
    }
    if (t.state === 'pause') {
      return L.t('已暂停', 'Paused')
    }
    return L.t('传输中', 'Transferring')
  }

  detailText(t: TransferTask): string {
    if (t.state === 'fail') {
      if (this.isPermErr(t.speed)) {
        return L.t('失败 · 权限不足，目标文件属于其他用户或目录不可写',
          'Failed · Permission denied: the target belongs to another user or the directory is not writable')
      }
      return t.speed.length > 0 ? `${this.stateText(t)} · ${t.speed}` : this.stateText(t)
    }
    return this.stateText(t) + (t.speed.length > 0 ? ' · ' + t.speed : '')
  }

  async pickUpload(): Promise<void> {
    if (!this.connected || this.hostId <= 0) {
      promptAction.showToast({ message: L.t('先连接一台主机', 'Connect a host first') })
      return
    }
    if (this.cwd.length === 0) {
      this.cwd = '/'
    }
    let uris: string[] = []
    try {
      const ctx = getContext(this) as common.UIAbilityContext
      const dp: picker.DocumentViewPicker = new picker.DocumentViewPicker(ctx)
      const opt: picker.DocumentSelectOptions = new picker.DocumentSelectOptions()
      opt.maxSelectNumber = 10
      uris = await dp.select(opt)
    } catch (e) {
      promptAction.showToast({ message: L.t('打开文件选择器失败', 'Cannot open the file picker') })
      return
    }
    if (uris.length === 0) {
      return
    }
    const items: UploadFileItem[] = []
    const fds: number[] = []
    for (let i = 0; i < uris.length; i++) {
      try {
        const f = fs.openSync(uris[i], fs.OpenMode.READ_ONLY)
        fds.push(f.fd)
        items.push({ fd: f.fd, name: this.baseName(uris[i]) })
      } catch (e) {
      }
    }
    if (items.length === 0) {
      promptAction.showToast({ message: L.t('无法读取所选文件', 'Cannot read the selected files') })
      return
    }
    const id: number = this.seq
    this.seq = this.seq + 1
    const label: string = items.length > 1 ?
      L.t(`${items[0].name} 等 ${items.length} 个`, `${items[0].name} +${items.length - 1}`) : items[0].name
    this.transfers = [this.newTask(id, label, true)].concat(this.transfers)
    this.view = 'xfer'
    try {
      await SshService.get().upload(this.hostId, items, this.cwd,
        (pct: number, speed: string) => {
          this.patch(id, pct, 'run', speed)
        })
      this.patch(id, 100, 'done', '')
      await this.refresh()
    } catch (e) {
      const raw: string = this.errText(e)
      this.patch(id, 0, 'fail', raw)
      if (this.isPermErr(raw)) {
        this.openPerm(L.t('上传', 'upload'), this.cwd, raw)
      }
    } finally {
      for (let i = 0; i < fds.length; i++) {
        try {
          fs.closeSync(fds[i])
        } catch (e) {
        }
      }
    }
  }

  async downloadOne(e: SftpEntry): Promise<void> {
    if (!this.connected || this.hostId <= 0) {
      promptAction.showToast({ message: L.t('先连接一台主机', 'Connect a host first') })
      return
    }
    let uri: string = ''
    try {
      const ctx = getContext(this) as common.UIAbilityContext
      const dp: picker.DocumentViewPicker = new picker.DocumentViewPicker(ctx)
      const opt: picker.DocumentSaveOptions = new picker.DocumentSaveOptions()
      opt.newFileNames = [e.name]
      const uris: string[] = await dp.save(opt)
      if (uris.length === 0) {
        return
      }
      uri = uris[0]
    } catch (err) {
      promptAction.showToast({ message: L.t('打开保存对话框失败', 'Cannot open the save dialog') })
      return
    }
    const id: number = this.seq
    this.seq = this.seq + 1
    this.transfers = [this.newTask(id, e.name, false)].concat(this.transfers)
    this.view = 'xfer'
    let f: fs.File | null = null
    try {
      f = fs.openSync(uri, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC)
      const fd: number = f.fd
      await SshService.get().download(this.hostId, e.path, fd,
        (pct: number, speed: string) => {
          this.patch(id, pct, 'run', speed)
        })
      this.patch(id, 100, 'done', '')
    } catch (err) {
      this.patch(id, 0, 'fail', this.errText(err))
    } finally {
      if (f !== null) {
        try {
          fs.closeSync(f.fd)
        } catch (err) {
        }
      }
    }
  }

  async downloadSelected(): Promise<void> {
    const targets: SftpEntry[] = this.entries.filter((x: SftpEntry) => this.selected.includes(x.name) && !x.isDir)
    this.setSelect(false)
    if (targets.length === 0) {
      promptAction.showToast({ message: L.t('请选择文件，文件夹暂不支持下载', 'Select files; folders are not supported') })
      return
    }
    for (let i = 0; i < targets.length; i++) {
      await this.downloadOne(targets[i])
    }
  }

  async removeSelected(): Promise<void> {
    if (!this.connected || this.hostId <= 0) {
      return
    }
    const targets: SftpEntry[] = this.entries.filter((x: SftpEntry) => this.selected.includes(x.name))
    if (targets.length === 0) {
      return
    }
    let failed: number = 0
    let firstErr: string = ''
    for (let i = 0; i < targets.length; i++) {
      try {
        await SshService.get().remove(this.hostId, targets[i].path, targets[i].isDir)
      } catch (e) {
        failed = failed + 1
        if (firstErr.length === 0) {
          firstErr = this.errText(e)
        }
      }
    }
    this.setSelect(false)
    if (failed > 0 && this.isPermErr(firstErr)) {
      this.openPerm(L.t('删除', 'delete'), targets[0].path, firstErr)
      await this.refresh()
      return
    }
    promptAction.showToast({
      message: failed === 0 ? L.t(`已删除 ${targets.length} 项`, `Deleted ${targets.length} items`) :
        L.t(`${failed} 项删除失败`, `${failed} items failed`)
    })
    await this.refresh()
  }

  cancelTask(t: TransferTask): void {
    SshService.get().cancel(this.hostId)
    this.patch(t.id, t.pct, 'fail', '')
  }

  clearDone(): void {
    this.transfers = this.transfers.filter((t: TransferTask) => t.state === 'run')
  }

  build() {
    Stack({ alignContent: Alignment.Bottom }) {
      Column() {
        if (this.view === 'browse') {
          this.BrowseView()
        } else {
          this.TransferView()
        }
      }
      .width('100%')
      .height('100%')

      if (this.dlg === 'menu') {
        Column() {
          Blank()
          this.MenuSheet()
        }
        .width('100%')
        .height('100%')
        .backgroundColor('#66000000')
        .onClick(() => {
          this.closeDlg()
        })
      } else if (this.dlg.length > 0) {
        this.DlgLayer()
      }
    }
    .width('100%')
    .height('100%')
  }

  openMenu(e: SftpEntry): void {
    if (this.selectMode) {
      return
    }
    this.mName = e.name
    this.mPath = e.path
    this.mIsDir = e.isDir
    this.mSize = e.size
    this.mTime = e.time
    this.dlg = 'menu'
  }

  menuEntry(): SftpEntry {
    return { name: this.mName, isDir: this.mIsDir, size: this.mSize, time: this.mTime, path: this.mPath, bytes: 0 }
  }

  menuAct(act: string): void {
    const e: SftpEntry = this.menuEntry()
    this.closeDlg()
    this.runMenu(act, e)
  }

  runMenu(act: string, e: SftpEntry): void {
    if (act === 'dir') {
      this.goto(e.path)
      return
    }
    if (act === 'down') {
      this.downloadOne(e)
      return
    }
    if (act === 'rename') {
      this.openRename(e)
      return
    }
    if (act === 'detail') {
      this.openDetail(e)
      return
    }
    this.confirmRemove(e)
  }

  @Builder MenuSheet() {
    Column() {
      Row() {
        Stack() {
          Text(this.mIsDir ? '▸' : '▫')
            .fontSize(MT.fsTitle)
            .fontColor(this.mIsDir ? $r('app.color.csx_accent') : $r('app.color.csx_text3'))
        }
        .width(34)
        .height(34)
        .borderRadius(MT.radiusChip)
        .backgroundColor(this.mIsDir ? $r('app.color.csx_accent_soft') : $r('app.color.csx_surface2'))

        Column() {
          Text(this.mName)
            .fontSize(MT.fsBody)
            .fontColor($r('app.color.csx_text'))
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
          Text(this.mIsDir ? L.t('文件夹', 'Folder') : `${this.mSize} · ${this.mTime}`)
            .fontSize(MT.fsCaption)
            .fontColor($r('app.color.csx_text3'))
            .margin({ top: 3 })
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)
        .margin({ left: 10 })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)
      .padding({ bottom: 14 })

      Divider()
        .color($r('app.color.csx_line'))

      this.SheetItem(this.mIsDir ? L.t('打开', 'Open') : L.t('下载', 'Download'),
        this.mIsDir ? 'dir' : 'down', false)
      this.SheetItem(L.t('重命名', 'Rename'), 'rename', false)
      this.SheetItem(L.t('详情', 'Details'), 'detail', false)
      this.SheetItem(L.t('删除', 'Delete'), 'remove', true)
    }
    .width('100%')
    .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 16, bottom: 30 })
    .borderRadius({ topLeft: 22, topRight: 22 })
    .backgroundColor($r('app.color.csx_surface'))
    .onClick(() => {
    })
  }

  @Builder SheetItem(label: string, act: string, danger: boolean) {
    Text(label)
      .fontSize(MT.fsBody)
      .fontColor(danger ? $r('app.color.csx_danger') : $r('app.color.csx_text'))
      .width('100%')
      .height(50)
      .textAlign(TextAlign.Start)
      .onClick(() => {
        this.menuAct(act)
      })
  }

  @Builder DlgLayer() {
    Column() {
      Column() {
        Text(this.dlgTitle)
          .fontSize(MT.fsTitle)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_text'))
          .width('100%')

        if (this.dlg === 'rename') {
          TextInput({ text: this.renameTo })
            .fontSize(MT.fsBody)
            .fontColor($r('app.color.csx_text'))
            .height(44)
            .width('100%')
            .margin({ top: 16 })
            .padding({ left: 12, right: 12 })
            .borderRadius(MT.radiusCtrl)
            .backgroundColor($r('app.color.csx_surface2'))
            .onChange((v: string) => {
              this.renameTo = v
            })
        } else if (this.dlg === 'mkdir') {
          TextInput({ text: this.mkName, placeholder: L.t('文件夹名称', 'Folder name') })
            .fontSize(MT.fsBody)
            .fontColor($r('app.color.csx_text'))
            .placeholderColor($r('app.color.csx_text3'))
            .height(44)
            .width('100%')
            .margin({ top: 16 })
            .padding({ left: 12, right: 12 })
            .borderRadius(MT.radiusCtrl)
            .backgroundColor($r('app.color.csx_surface2'))
            .onChange((v: string) => {
              this.mkName = v
            })
        } else {
          Text(this.dlgBody)
            .fontSize(MT.fsSmall)
            .fontColor(this.dlgDanger ? $r('app.color.csx_danger') : $r('app.color.csx_text2'))
            .width('100%')
            .margin({ top: 12 })
        }

        Row() {
          Text(L.t('取消', 'Cancel'))
            .fontSize(MT.fsBody)
            .fontColor($r('app.color.csx_text2'))
            .textAlign(TextAlign.Center)
            .height(44)
            .layoutWeight(1)
            .borderRadius(MT.radiusBtn)
            .backgroundColor($r('app.color.csx_surface2'))
            .onClick(() => {
              this.closeDlg()
            })
          if (this.dlg === 'rename') {
            Text(L.t('确定', 'OK'))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_accent_fg'))
              .textAlign(TextAlign.Center)
              .height(44)
              .layoutWeight(1)
              .margin({ left: 10 })
              .borderRadius(MT.radiusBtn)
              .backgroundColor($r('app.color.csx_accent'))
              .onClick(() => {
                this.doRename()
              })
          } else if (this.dlg === 'mkdir') {
            Text(L.t('创建', 'Create'))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_accent_fg'))
              .textAlign(TextAlign.Center)
              .height(44)
              .layoutWeight(1)
              .margin({ left: 10 })
              .borderRadius(MT.radiusBtn)
              .backgroundColor($r('app.color.csx_accent'))
              .onClick(() => {
                this.doMkdir()
              })
          } else {
            Text(L.t('知道了', 'Got it'))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_accent_fg'))
              .textAlign(TextAlign.Center)
              .height(44)
              .layoutWeight(1)
              .margin({ left: 10 })
              .borderRadius(MT.radiusBtn)
              .backgroundColor($r('app.color.csx_accent'))
              .onClick(() => {
                this.closeDlg()
              })
          }
        }
        .width('100%')
        .margin({ top: 18 })
      }
      .width('100%')
      .padding(18)
      .borderRadius(20)
      .backgroundColor($r('app.color.csx_surface'))
      .border({ width: 1, color: $r('app.color.csx_line') })
    }
    .width('100%')
    .padding({ left: 22, right: 22, bottom: 40 })
    .backgroundColor('#99000000')
    .justifyContent(FlexAlign.Center)
    .onClick(() => {
      if (this.dlg !== 'perm') {
        this.closeDlg()
      }
    })
  }

  @Builder BrowseView() {
    Column() {
      Row() {
        Text(this.connected ? `SFTP · ${this.hostName}` : L.t('SFTP · 未连接', 'SFTP · Not connected'))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text3'))
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .layoutWeight(1)

        Text('⇅')
          .fontSize(MT.fsTitle)
          .fontColor($r('app.color.csx_text2'))
          .textAlign(TextAlign.Center)
          .width(40)
          .height(40)
          .borderRadius(MT.radiusCard)
          .backgroundColor($r('app.color.csx_surface'))
          .onClick(() => {
            this.view = 'xfer'
          })
        Text('↑')
          .fontSize(MT.fsTitle)
          .fontColor(this.connected ? $r('app.color.csx_accent') : $r('app.color.csx_text3'))
          .textAlign(TextAlign.Center)
          .width(40)
          .height(40)
          .borderRadius(MT.radiusCard)
          .backgroundColor(this.connected ? $r('app.color.csx_accent_soft') : $r('app.color.csx_surface'))
          .margin({ left: 8 })
          .onClick(() => {
            this.pickUpload()
          })
        Text('＋')
          .fontSize(MT.fsTitle)
          .fontColor($r('app.color.csx_text2'))
          .textAlign(TextAlign.Center)
          .width(40)
          .height(40)
          .borderRadius(MT.radiusCard)
          .backgroundColor($r('app.color.csx_surface'))
          .margin({ left: 8 })
          .onClick(() => {
            this.openMkdir()
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
            .fontColor($r('app.color.csx_text2'))
          Text(L.t('连接主机后可浏览、上传、下载远程文件', 'Connect a host to browse, upload and download files'))
            .fontSize(MT.fsSmall)
            .fontColor($r('app.color.csx_text3'))
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
              .fontColor($r('app.color.csx_blue'))
              .onClick(() => {
                this.goto('/')
              })
            ForEach(this.crumbs(), (p: string, i: number) => {
              if (i > 0) {
                Text('/')
                  .fontSize(MT.fsSmall)
                  .fontColor($r('app.color.csx_text3'))
              }
              Text(p)
                .fontSize(MT.fsSmall)
                .fontColor(i === this.crumbs().length - 1 ? $r('app.color.csx_text') : $r('app.color.csx_blue'))
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
            .fontColor($r('app.color.csx_text2'))
            .padding({ left: 10, right: 10, top: 6, bottom: 6 })
            .borderRadius(MT.radiusCtrl)
            .backgroundColor($r('app.color.csx_surface'))
            .onClick(() => {
              this.setSelect(!this.selectMode)
            })
          Text(this.selectMode ? L.t(`已选 ${this.selected.length} 项`, `${this.selected.length} selected`) : '')
            .fontSize(MT.fsSmall)
            .fontColor($r('app.color.csx_text3'))
            .layoutWeight(1)
            .textAlign(TextAlign.End)
          Text(L.t('刷新', 'Refresh'))
            .fontSize(MT.fsSmall)
            .fontColor($r('app.color.csx_blue'))
            .padding({ left: 10, right: 10, top: 6, bottom: 6 })
            .onClick(() => {
              this.refresh()
            })
        }
        .width('100%')
        .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: 10 })
        .alignItems(VerticalAlign.Center)

        Column() {
          if (this.loading && this.entries.length === 0) {
            Column() {
              Text(L.t('加载中…', 'Loading…'))
                  .fontSize(MT.fsBody)
                  .fontColor($r('app.color.csx_text3'))
              }
              .width('100%')
              .padding({ top: 60 })
            } else if (this.errMsg.length > 0) {
              Column() {
                Text(L.t('读取目录失败', 'Cannot read directory'))
                  .fontSize(MT.fsBody)
                  .fontColor($r('app.color.csx_danger'))
                Text(this.errMsg)
                  .fontSize(MT.fsCaption)
                  .fontColor($r('app.color.csx_text3'))
                  .margin({ top: 8 })
                  .maxLines(3)
                  .textOverflow({ overflow: TextOverflow.Ellipsis })
              }
              .width('100%')
              .padding({ top: 60, left: MT.pagePadding, right: MT.pagePadding })
            } else if (this.entries.length === 0) {
              Column() {
                Text(L.t('暂无文件', 'No files'))
                  .fontSize(MT.fsBody)
                  .fontColor($r('app.color.csx_text2'))
                Text(L.t('目录为空', 'Empty directory'))
                  .fontSize(MT.fsSmall)
                  .fontColor($r('app.color.csx_text3'))
                  .margin({ top: 8 })
              }
              .width('100%')
              .padding({ top: 60 })
            }

            List({ space: 0 }) {
              LazyForEach(this.src, (e: SftpEntry) => {
                ListItem() {
                  Row() {
                if (this.selectMode) {
                  Text(this.selected.includes(e.name) ? '✓' : '')
                    .fontSize(MT.fsSmall)
                    .fontColor($r('app.color.csx_accent_fg'))
                    .textAlign(TextAlign.Center)
                    .width(20)
                    .height(20)
                    .borderRadius(MT.radiusChip)
                    .backgroundColor(this.selected.includes(e.name) ? $r('app.color.csx_accent') : $r('app.color.csx_surface2'))
                    .margin({ right: 11 })
                }
                Stack() {
                  Text(e.isDir ? '▸' : '▫')
                    .fontSize(MT.fsTitle)
                    .fontColor(e.isDir ? $r('app.color.csx_accent') : $r('app.color.csx_text3'))
                    .textAlign(TextAlign.Center)
                }
                .width(30)
                .height(30)
                .borderRadius(MT.radiusChip)
                .backgroundColor(e.isDir ? $r('app.color.csx_accent_soft') : $r('app.color.csx_surface2'))
                .margin({ right: 10 })
                Column() {
                  Text(e.name)
                    .fontSize(MT.fsBody)
                    .fontColor($r('app.color.csx_text'))
                    .maxLines(1)
                    .textOverflow({ overflow: TextOverflow.Ellipsis })
                  Text(this.subTitle(e))
                    .fontSize(MT.fsCaption)
                    .fontColor($r('app.color.csx_text3'))
                    .margin({ top: 4 })
                    .maxLines(1)
                    .textOverflow({ overflow: TextOverflow.Ellipsis })
                }
                .alignItems(HorizontalAlign.Start)
                .layoutWeight(1)
                .margin({ left: 8 })

                if (!this.selectMode) {
                  Stack() {
                    Arrow({ dir: 'right', color: $r('app.color.csx_text3'), len: 15 })
                  }
                  .width(20)
                  .height(20)
                }
              }
              .width('100%')
              .padding(14)
              .margin({ bottom: 8 })
              .borderRadius(MT.radiusCard)
              .backgroundColor($r('app.color.csx_surface'))
              .alignItems(VerticalAlign.Center)
              .onClick(() => {
                if (this.selectMode) {
                  this.toggleSel(e.name)
                  return
                }
                if (e.isDir) {
                  this.goto(e.path)
                } else {
                  this.openDetail(e)
                }
              })
              .gesture(LongPressGesture({ repeat: false, duration: 400 })
                .onAction(() => {
                  this.openMenu(e)
                }))
                }
              }, (e: SftpEntry) => `${e.name}-${e.size}-${e.time}`)
            }
            .width('100%')
            .layoutWeight(1)
            .cachedCount(8)
            .scrollBar(BarState.Off)
            .padding({ left: MT.pagePadding, right: MT.pagePadding, bottom: MT.tabbarSpace })
          }
          .width('100%')
          .layoutWeight(1)

        if (this.selectMode) {
          Row() {
            Text(L.t(`下载 ${this.selected.length} 项`, `Download ${this.selected.length} items`))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_accent_fg'))
              .textAlign(TextAlign.Center)
              .height(46)
              .layoutWeight(1)
              .borderRadius(MT.radiusBtn)
              .backgroundColor($r('app.color.csx_accent'))
              .onClick(() => {
                this.downloadSelected()
              })
            Text(L.t('删除', 'Delete'))
              .fontSize(MT.fsBody)
              .fontColor($r('app.color.csx_danger'))
              .textAlign(TextAlign.Center)
              .height(46)
              .width(90)
              .margin({ left: 10 })
              .borderRadius(MT.radiusBtn)
              .backgroundColor($r('app.color.csx_surface'))
              .onClick(() => {
                this.removeSelected()
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
          Arrow({ dir: 'left', color: $r('app.color.csx_blue'), len: 20 })
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
            .fontColor($r('app.color.csx_text'))
          Text(this.transfers.length > 0 ? L.t(`${this.transfers.length} 个任务`, `${this.transfers.length} tasks`) : L.t('暂无任务', 'No tasks'))
            .fontSize(MT.fsCaption)
            .fontColor($r('app.color.csx_text3'))
            .margin({ top: 3 })
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)
        Text(L.t('清空', 'Clear'))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text2'))
          .onClick(() => {
            this.clearDone()
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
            .fontColor($r('app.color.csx_text2'))
          Text(L.t('上传或下载后会在这里显示进度', 'Progress appears here after upload or download'))
            .fontSize(MT.fsSmall)
            .fontColor($r('app.color.csx_text3'))
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
                    .fontColor(t.up ? $r('app.color.csx_blue') : $r('app.color.csx_accent'))
                    .width(26)
                    .height(26)
                    .textAlign(TextAlign.Center)
                    .borderRadius(MT.radiusChip)
                    .backgroundColor(t.up ? $r('app.color.csx_blue_soft') : $r('app.color.csx_accent_soft'))
                  Column() {
                    Row() {
                      Text(t.name)
                        .fontSize(MT.fsSmall)
                        .fontColor($r('app.color.csx_text'))
                        .maxLines(1)
                        .textOverflow({ overflow: TextOverflow.Ellipsis })
                        .constraintSize({ maxWidth: 150 })
                      Blank()
                      Text(`${Math.round(t.pct)}%`)
                        .fontSize(MT.fsCaption)
                        .fontColor($r('app.color.csx_text2'))
                    }
                    .width('100%')
                    Text(this.detailText(t))
                      .fontSize(MT.fsMicro)
                      .fontColor(t.state === 'fail' ? $r('app.color.csx_danger') : $r('app.color.csx_text3'))
                      .margin({ top: 4 })
                      .maxLines(2)
                      .textOverflow({ overflow: TextOverflow.Ellipsis })
                      .wordBreak(WordBreak.BREAK_WORD)
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)
                  .margin({ left: 10 })

                  Text(t.state === 'run' ? '×' : '×')
                    .fontSize(15)
                    .fontColor($r('app.color.csx_text2'))
                    .width(30)
                    .height(30)
                    .textAlign(TextAlign.Center)
                    .borderRadius(MT.radiusCtrl)
                    .backgroundColor($r('app.color.csx_surface2'))
                    .onClick(() => {
                      if (t.state === 'run') {
                        this.cancelTask(t)
                        return
                      }
                      this.transfers = this.transfers.filter((x: TransferTask) => x.id !== t.id)
                    })
                }
                .width('100%')
                .alignItems(VerticalAlign.Center)

                Row() {
                  Row()
                    .width(`${Math.round(t.pct)}%`)
                    .height(4)
                    .borderRadius(2)
                    .backgroundColor(t.state === 'fail' ? $r('app.color.csx_danger') : $r('app.color.csx_accent'))
                }
                .width('100%')
                .height(4)
                .borderRadius(2)
                .backgroundColor($r('app.color.csx_surface2'))
                .margin({ top: 10 })
                .justifyContent(FlexAlign.Start)
              }
              .width('100%')
              .padding(14)
              .margin({ bottom: 10 })
              .borderRadius(MT.radiusCard)
              .backgroundColor($r('app.color.csx_surface'))
            }, (t: TransferTask) => `${t.id}-${Math.round(t.pct)}-${t.state}`)
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
