import { promptAction } from '@kit.ArkUI'
import { common } from '@kit.AbilityKit'
import { fileIo as fs, picker } from '@kit.CoreFileKit'
import { MT } from '../../common/MobileTheme'
import { Ic } from '../../common/Icons'
import { HostItem, cloneHost } from '../../common/Models'
import { ManagedKey, listKeys } from '../../common/KeyStore'
import { L } from '../../common/I18n'

interface FieldSpec {
  label: string
  ph: string
  v: string
  num: boolean
}

@Component
export struct HostSettingsPage {
  @Link hosts: HostItem[]
  @Prop @Watch('reload') editId: number = 0
  onBack: () => void = () => {}
  onSaved: () => void = () => {}
  @State fName: string = ''
  @State fHost: string = ''
  @State fPort: string = '22'
  @State fUser: string = ''
  @State fAuth: string = '密码'
  @State fPass: string = ''
  @State fKeyPath: string = ''
  @State fHostKey: string = ''
  @State fHostKeyType: string = ''
  @State storeOpen: boolean = false
  @State storeKeys: ManagedKey[] = []

  reload(): void {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === this.editId)
    if (h === undefined) {
      return
    }
    this.fName = h.name
    this.fHost = h.host
    this.fPort = h.port.toString()
    this.fUser = h.user
    this.fAuth = (h.auth === '密钥' || h.auth.startsWith('私钥')) ? '密钥' : '密码'
    this.fPass = h.password
    this.fKeyPath = h.keyPath
    this.fHostKey = h.hostKey
    this.fHostKeyType = h.hostKeyType
  }

  keyName(): string {
    const i: number = this.fKeyPath.lastIndexOf('/')
    return i < 0 ? this.fKeyPath : this.fKeyPath.substring(i + 1)
  }

  async pickKey(): Promise<void> {
    try {
      const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
      const docPicker: picker.DocumentViewPicker = new picker.DocumentViewPicker(ctx)
      const uris: string[] = await docPicker.select({ maxSelectNumber: 1 })
      if (uris.length === 0) {
        return
      }
      const uri: string = uris[0]
      let name: string = uri.substring(uri.lastIndexOf('/') + 1)
      try {
        name = decodeURIComponent(name)
      } catch (e) {
      }
      if (name.length === 0) {
        name = 'key.pem'
      }
      const dir: string = ctx.filesDir + '/keys/h' + this.editId.toString()
      if (!fs.accessSync(dir)) {
        fs.mkdirSync(dir, true)
      }
      const dest: string = dir + '/' + name
      if (fs.accessSync(dest)) {
        fs.unlinkSync(dest)
      }
      const src: fs.File = fs.openSync(uri, fs.OpenMode.READ_ONLY)
      fs.copyFileSync(src.fd, dest)
      fs.closeSync(src)
      this.fKeyPath = dest
      promptAction.showToast({ message: L.t('已导入 ' + name, 'Imported ' + name) })
    } catch (e) {
      promptAction.showToast({ message: L.t('私钥导入失败', 'Failed to import the key') })
    }
  }

  pickFromStore(): void {
    const ctx: common.UIAbilityContext = getContext(this) as common.UIAbilityContext
    let list: ManagedKey[] = []
    try {
      list = listKeys(ctx)
    } catch (e) {
      list = []
    }
    if (list.length === 0) {
      promptAction.showToast({ message: L.t('密钥库为空，请先在设置里生成', 'Key store is empty, generate one in settings first') })
      return
    }
    this.storeKeys = list
    this.storeOpen = true
  }

  save(): void {
    const p: number = Number(this.fPort)
    if (!/^\d+$/.test(this.fPort) || p < 1 || p > 65535) {
      promptAction.showToast({ message: L.t('端口需为 1-65535 的数字', 'Port must be a number between 1 and 65535') })
      return
    }
    if (this.fName.trim().length === 0 || this.fHost.trim().length === 0 || this.fUser.trim().length === 0) {
      promptAction.showToast({ message: L.t('名称、地址、用户名不能为空', 'Name, address and user cannot be empty') })
      return
    }
    if (this.fAuth === '密钥' && this.fKeyPath.length === 0) {
      promptAction.showToast({ message: L.t('请先选择私钥文件', 'Pick a private key file first') })
      return
    }
    this.hosts = this.hosts.map((h: HostItem) => {
      if (h.id !== this.editId) {
        return h
      }
      const nh: HostItem = cloneHost(h, h.tunnels)
      const moved: boolean = nh.host !== this.fHost.trim() || nh.port !== p
      nh.name = this.fName.trim()
      nh.host = this.fHost.trim()
      nh.port = p
      nh.user = this.fUser.trim()
      nh.auth = this.fAuth
      nh.password = this.fPass
      nh.keyPath = this.fKeyPath
      nh.hostKey = moved ? '' : this.fHostKey
      nh.hostKeyType = moved ? '' : this.fHostKeyType
      return nh
    })
    promptAction.showToast({ message: L.t('主机设置已保存', 'Host settings saved') })
    this.onSaved()
    this.onBack()
  }

  @Builder FieldRow($$: FieldSpec) {
    Row() {
      Text($$.label)
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
      Blank()
      TextInput({ placeholder: $$.ph, text: $$.v })
        .placeholderColor($r('app.color.csx_text3'))
        .fontColor($r('app.color.csx_text'))
        .fontSize(MT.fsBody)
        .textAlign(TextAlign.End)
        .backgroundColor(MT.transparent)
        .padding({ left: 0, right: 0 })
        .width(210)
        .height(40)
        .type(InputType.Normal)
        .onChange((v: string) => {
          if ($$.label === L.t('名称', 'Name')) {
            this.fName = v
          } else if ($$.label === L.t('主机地址', 'Host address')) {
            this.fHost = v
          } else if ($$.label === L.t('端口', 'Port')) {
            this.fPort = v
          } else {
            this.fUser = v
          }
        })
    }
    .width('100%')
    .height(52)
    .padding({ left: 14, right: 14 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder PwdRow() {
    Row() {
      Text(L.t('密码', 'Password'))
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
      Blank()
      TextInput({ placeholder: L.t('登录密码', 'Login password'), text: this.fPass })
        .type(InputType.Password)
        .placeholderColor($r('app.color.csx_text3'))
        .fontColor($r('app.color.csx_text'))
        .fontSize(MT.fsBody)
        .textAlign(TextAlign.End)
        .backgroundColor(MT.transparent)
        .padding({ left: 0, right: 0 })
        .width(210)
        .height(40)
        .onChange((v: string) => {
          this.fPass = v
        })
    }
    .width('100%')
    .height(52)
    .padding({ left: 14, right: 14 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder KeyRow() {
    Row() {
      Text(L.t('私钥文件', 'Private key'))
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
      Blank()
      Text(this.fKeyPath.length === 0 ? L.t('未选择', 'Not selected') : this.keyName())
        .fontSize(MT.fsSmall)
        .fontColor(this.fKeyPath.length === 0 ? $r('app.color.csx_text3') : $r('app.color.csx_text2'))
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .constraintSize({ maxWidth: 120 })
      Text(L.t('选择', 'Pick'))
        .fontSize(MT.fsSmall)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('app.color.csx_text'))
        .height(28)
        .padding({ left: 12, right: 12 })
        .borderRadius(9)
        .backgroundColor($r('app.color.csx_surface2'))
        .margin({ left: 10 })
        .onClick(() => {
          this.pickKey()
        })
      Text(L.t('密钥库', 'Key store'))
        .fontSize(MT.fsSmall)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('app.color.csx_text'))
        .height(28)
        .padding({ left: 12, right: 12 })
        .borderRadius(9)
        .backgroundColor($r('app.color.csx_surface2'))
        .margin({ left: 8 })
        .onClick(() => {
          this.pickFromStore()
        })
    }
    .width('100%')
    .height(52)
    .padding({ left: 14, right: 14 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder StoreList() {
    Column() {
      ForEach(this.storeKeys, (k: ManagedKey) => {
        Row() {
          Column() {
            Text(k.name)
              .fontSize(MT.fsSmall)
              .fontColor($r('app.color.csx_text'))
              .maxLines(1)
              .textOverflow({ overflow: TextOverflow.Ellipsis })
            Text(k.alg + ' ' + k.bits.toString())
              .fontSize(MT.fsMicro)
              .fontColor($r('app.color.csx_text3'))
              .margin({ top: 3 })
          }
          .alignItems(HorizontalAlign.Start)
          .layoutWeight(1)
          if (this.fKeyPath === k.path) {
            Text(L.t('当前', 'Current'))
              .fontSize(MT.fsMicro)
              .fontColor($r('app.color.csx_accent'))
              .margin({ left: 8 })
          }
        }
        .width('100%')
        .padding({ top: 9, bottom: 9 })
        .onClick(() => {
          this.fKeyPath = k.path
          this.fAuth = '密钥'
          this.storeOpen = false
        })
      }, (k: ManagedKey) => k.name)
    }
    .width('100%')
    .padding({ left: 14, right: 14, top: 2, bottom: 6 })
  }

  @Builder PassRow() {
    Row() {
      Text(L.t('私钥口令', 'Passphrase'))
        .fontSize(MT.fsBody)
        .fontColor($r('app.color.csx_text'))
      Blank()
      TextInput({ placeholder: L.t('无口令可留空', 'Leave empty if none'), text: this.fPass })
        .type(InputType.Password)
        .placeholderColor($r('app.color.csx_text3'))
        .fontColor($r('app.color.csx_text'))
        .fontSize(MT.fsBody)
        .textAlign(TextAlign.End)
        .backgroundColor(MT.transparent)
        .padding({ left: 0, right: 0 })
        .width(210)
        .height(40)
        .onChange((v: string) => {
          this.fPass = v
        })
    }
    .width('100%')
    .height(52)
    .padding({ left: 14, right: 14 })
    .alignItems(VerticalAlign.Center)
  }

  @Builder AuthChip(val: string, label: string) {
    Text(label)
      .fontSize(MT.fsSmall)
      .fontWeight(FontWeight.Medium)
      .fontColor(this.fAuth === val ? $r('app.color.csx_text') : $r('app.color.csx_text2'))
      .height(30)
      .padding({ left: 14, right: 14 })
      .borderRadius(9)
      .backgroundColor(this.fAuth === val ? $r('app.color.csx_surface2') : MT.transparent)
      .onClick(() => {
        this.fAuth = val
      })
  }

  build() {
    Column() {
      this.Body()
    }
    .width('100%')
    .height('100%')
  }

  @Builder Body() {
    Column() {
      Row() {
        Stack() {
          Ic({ name: 'back', icSize: 18, color: $r('app.color.csx_text2'), sw: 2 })
        }
        .width(36)
        .height(36)
        .borderRadius(12)
        .backgroundColor($r('app.color.csx_surface2'))
        .onClick(() => {
          this.onBack()
        })
        Text(L.t('主机设置', 'Host Settings'))
          .fontSize(MT.fsTitle)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('app.color.csx_text'))
          .margin({ left: 12 })
        Blank()
      }
      .width('100%')
      .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 10, bottom: 8 })
      .alignItems(VerticalAlign.Center)

      Scroll() {
        Column() {
          Text(L.t('连接', 'Connect'))
            .fontSize(MT.fsCaption)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('app.color.csx_text3'))
            .width('100%')
            .margin({ bottom: 8 })

          Column() {
            this.FieldRow({ label: L.t('名称', 'Name'), ph: 'My Server', v: this.fName, num: false })
            this.Divider()
            this.FieldRow({ label: L.t('主机地址', 'Host address'), ph: 'example.com', v: this.fHost, num: false })
            this.Divider()
            this.FieldRow({ label: L.t('端口', 'Port'), ph: '22', v: this.fPort, num: true })
            this.Divider()
            this.FieldRow({ label: L.t('用户名', 'Username'), ph: 'root', v: this.fUser, num: false })
          }
          .width('100%')
          .borderRadius(16)
          .backgroundColor($r('app.color.csx_surface'))

          Text(L.t('认证', 'Authentication'))
            .fontSize(MT.fsCaption)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('app.color.csx_text3'))
            .width('100%')
            .margin({ top: 18, bottom: 8 })

          Column() {
            Row() {
              Text(L.t('方式', 'Method'))
                .fontSize(MT.fsBody)
                .fontColor($r('app.color.csx_text'))
              Blank()
              Row() {
                this.AuthChip('密码', L.t('密码', 'Password'))
                this.AuthChip('密钥', L.t('密钥', 'Key'))
              }
              .padding(2)
              .borderRadius(11)
              .backgroundColor($r('app.color.csx_surface2'))
            }
            .width('100%')
            .height(56)
            .padding({ left: 14, right: 14 })
            .alignItems(VerticalAlign.Center)

            this.Divider()

            if (this.fAuth === '密码') {
              this.PwdRow()
            } else {
              this.KeyRow()
              if (this.storeOpen) {
                this.StoreList()
              }
              this.Divider()
              this.PassRow()
            }
          }
          .width('100%')
          .borderRadius(16)
          .backgroundColor($r('app.color.csx_surface'))

          Text(L.t('安全', 'Security'))
            .fontSize(MT.fsCaption)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('app.color.csx_text3'))
            .width('100%')
            .margin({ top: 18, bottom: 8 })

          Column() {
            Row() {
              Text(L.t('主机指纹', 'Host key'))
                .fontSize(MT.fsBody)
                .fontColor($r('app.color.csx_text'))
              Blank()
              Text(this.fHostKey.length === 0 ? L.t('未记录', 'Not recorded') : this.fHostKeyType)
                .fontSize(MT.fsSmall)
                .fontColor($r('app.color.csx_text3'))
            }
            .width('100%')
            .height(52)
            .padding({ left: 14, right: 14 })
            .alignItems(VerticalAlign.Center)

            if (this.fHostKey.length > 0) {
              this.Divider()
              Row() {
                Text(this.fHostKey)
                  .fontSize(MT.fsMicro)
                  .fontColor($r('app.color.csx_text2'))
                  .fontFamily('monospace')
                  .maxLines(1)
                  .textOverflow({ overflow: TextOverflow.Ellipsis })
                  .layoutWeight(1)
                Text(L.t('重置', 'Reset'))
                  .fontSize(MT.fsSmall)
                  .fontColor($r('app.color.csx_danger'))
                  .padding({ left: 10, right: 10, top: 6, bottom: 6 })
                  .borderRadius(MT.radiusCtrl)
                  .backgroundColor($r('app.color.csx_surface2'))
                  .margin({ left: 10 })
                  .onClick(() => {
                    this.fHostKey = ''
                    this.fHostKeyType = ''
                    promptAction.showToast({ message: L.t('已清除，下次连接重新信任', 'Cleared; trust again next time') })
                  })
              }
              .width('100%')
              .padding({ left: 14, right: 14, top: 10, bottom: 14 })
              .alignItems(VerticalAlign.Center)
            }
          }
          .width('100%')
          .borderRadius(16)
          .backgroundColor($r('app.color.csx_surface'))

          Blank()
        }
        .width('100%')
        .constraintSize({ minHeight: '100%' })
        .padding({ left: MT.pagePadding, right: MT.pagePadding, top: 4, bottom: 24 })
      }
      .layoutWeight(1)
      .scrollBar(BarState.Off)
      .align(Alignment.Top)

      Button(L.t('保存', 'Save'))
        .width('100%')
        .height(48)
        .fontSize(15)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('app.color.csx_accent_fg'))
        .borderRadius(15)
        .backgroundColor($r('app.color.csx_accent'))
        .margin({ left: MT.pagePadding, right: MT.pagePadding, bottom: 24 })
        .onClick(() => {
          this.save()
        })
    }
    .width('100%')
    .height('100%')
    .backgroundColor($r('app.color.csx_bg'))
  }

  @Builder Divider() {
    Row() {
      Column()
        .width('100%')
        .height(1)
        .backgroundColor($r('app.color.csx_line'))
    }
    .width('100%')
    .padding({ left: 14 })
  }
}
