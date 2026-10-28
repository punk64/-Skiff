import { promptAction } from '@kit.ArkUI'
import { MT } from '../../common/MobileTheme'
import { Ic } from '../../common/Icons'
import { HostItem, cloneHost } from '../../common/Models'
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
  @State fName: string = ''
  @State fHost: string = ''
  @State fPort: string = '22'
  @State fUser: string = ''
  @State fAuth: string = '密码'
  @StorageProp('csx_themeVer') themeVer: number = 0

  reload(): void {
    const h: HostItem | undefined = this.hosts.find((x: HostItem) => x.id === this.editId)
    if (h === undefined) {
      return
    }
    this.fName = h.name
    this.fHost = h.host
    this.fPort = h.port.toString()
    this.fUser = h.user
    this.fAuth = h.auth.startsWith('私钥') ? '密钥' : '密码'
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
    this.hosts = this.hosts.map((h: HostItem) => {
      if (h.id !== this.editId) {
        return h
      }
      const nh: HostItem = cloneHost(h, h.tunnels)
      nh.name = this.fName.trim()
      nh.host = this.fHost.trim()
      nh.port = p
      nh.user = this.fUser.trim()
      nh.auth = this.fAuth
      return nh
    })
    promptAction.showToast({ message: L.t('主机设置已保存', 'Host settings saved') })
    this.onBack()
  }

  @Builder FieldRow($$: FieldSpec) {
    Row() {
      Text($$.label)
        .fontSize(MT.fsBody)
        .fontColor(MT.text)
      Blank()
      TextInput({ placeholder: $$.ph, text: $$.v })
        .placeholderColor(MT.text3)
        .fontColor(MT.text)
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

  @Builder AuthChip(val: string, label: string) {
    Text(label)
      .fontSize(MT.fsSmall)
      .fontWeight(FontWeight.Medium)
      .fontColor(this.fAuth === val ? MT.text : MT.text2)
      .height(30)
      .padding({ left: 14, right: 14 })
      .borderRadius(9)
      .backgroundColor(this.fAuth === val ? MT.surface2 : MT.transparent)
      .onClick(() => {
        this.fAuth = val
      })
  }

  build() {
    if (this.themeVer >= 0) {
      Column() {
        this.Body()
      }
      .width('100%')
      .height('100%')
    }
  }

  @Builder Body() {
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
          this.onBack()
        })
        Text(L.t('主机设置', 'Host Settings'))
          .fontSize(MT.fsTitle)
          .fontWeight(FontWeight.Medium)
          .fontColor(MT.text)
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
            .fontColor(MT.text3)
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
          .backgroundColor(MT.surface)

          Text(L.t('认证', 'Authentication'))
            .fontSize(MT.fsCaption)
            .fontWeight(FontWeight.Medium)
            .fontColor(MT.text3)
            .width('100%')
            .margin({ top: 18, bottom: 8 })

          Row() {
            Text(L.t('方式', 'Method'))
              .fontSize(MT.fsBody)
              .fontColor(MT.text)
            Blank()
            Row() {
              this.AuthChip('密码', L.t('密码', 'Password'))
              this.AuthChip('密钥', L.t('密钥', 'Key'))
            }
            .padding(2)
            .borderRadius(11)
            .backgroundColor(MT.surface2)
          }
          .width('100%')
          .height(56)
          .padding({ left: 14, right: 14 })
          .borderRadius(16)
          .backgroundColor(MT.surface)
          .alignItems(VerticalAlign.Center)

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
        .fontColor(MT.accentFg)
        .borderRadius(15)
        .backgroundColor(MT.accent)
        .margin({ left: MT.pagePadding, right: MT.pagePadding, bottom: 24 })
        .onClick(() => {
          this.save()
        })
    }
    .width('100%')
    .height('100%')
    .backgroundColor(MT.bg)
  }

  @Builder Divider() {
    Row() {
      Column()
        .width('100%')
        .height(1)
        .backgroundColor(MT.line)
    }
    .width('100%')
    .padding({ left: 14 })
  }
}
