import { webview } from '@kit.ArkWeb'
import { MT } from '../../common/MobileTheme'
import { L } from '../../common/I18n'

@Component
export struct BrowserPage {
  @Link show: boolean
  @Prop @Watch('onUrlChanged') url: string = ''
  @StorageProp('csx_themeVer') themeVer: number = 0
  @State input: string = ''
  @State progress: number = 0
  @State loading: boolean = false
  @State failed: boolean = false
  @State failText: string = ''
  @State canBack: boolean = false
  @State canForward: boolean = false
  private controller: webview.WebviewController = new webview.WebviewController()

  aboutToAppear(): void {
    this.input = this.url
    this.loading = this.url.length > 0
  }

  onUrlChanged(): void {
    if (this.url.length === 0 || this.url === this.input) {
      return
    }
    this.input = this.url
    this.failed = false
    this.loading = true
    try {
      this.controller.loadUrl(this.url)
    } catch (e) {
      this.failed = true
      this.loading = false
      this.failText = L.t('地址无法加载，请检查端口是否正确', 'Cannot load address, please check the port')
    }
  }

  go(raw: string): void {
    let u: string = raw.trim()
    if (u.length === 0) {
      return
    }
    if (!/^https?:\/\//i.test(u)) {
      u = 'http://' + u
    }
    this.input = u
    this.failed = false
    this.loading = true
    this.progress = 0
    try {
      this.controller.loadUrl(u)
    } catch (e) {
      this.failed = true
      this.loading = false
      this.failText = L.t('地址无法加载，请检查端口是否正确', 'Cannot load address, please check the port')
    }
  }

  reload(): void {
    this.failed = false
    this.loading = true
    this.progress = 0
    try {
      this.controller.refresh()
    } catch (e) {
      this.failed = true
      this.loading = false
      this.failText = L.t('页面尚未加载，无法刷新', 'Page not loaded yet, cannot refresh')
    }
  }

  syncNav(): void {
    try {
      this.canBack = this.controller.accessBackward()
      this.canForward = this.controller.accessForward()
    } catch (e) {
      this.canBack = false
      this.canForward = false
    }
  }

  back(): void {
    if (!this.canBack) {
      return
    }
    try {
      this.controller.backward()
    } catch (e) {
      return
    }
  }

  forward(): void {
    if (!this.canForward) {
      return
    }
    try {
      this.controller.forward()
    } catch (e) {
      return
    }
  }

  build() {
    if (this.themeVer >= 0) {
      Column() {
        this.TopBar()
        Stack({ alignContent: Alignment.Top }) {
          Web({ src: this.url, controller: this.controller })
            .width('100%')
            .height('100%')
            .backgroundColor(MT.bg)
            .javaScriptAccess(true)
            .domStorageAccess(true)
            .fileAccess(false)
            .zoomAccess(true)
            .mixedMode(MixedMode.All)
            .cacheMode(CacheMode.Default)
            .horizontalScrollBarAccess(false)
            .onPageBegin(() => {
              this.loading = true
              this.failed = false
              this.progress = 0
            })
            .onProgressChange((e: OnProgressChangeEvent) => {
              this.progress = e.newProgress
            })
            .onPageEnd(() => {
              this.loading = false
              this.progress = 100
              this.syncNav()
              try {
                const cur: string = this.controller.getUrl()
                if (cur.length > 0) {
                  this.input = cur
                }
              } catch (err) {
                this.syncNav()
              }
            })
            .onErrorReceive((e: OnErrorReceiveEvent) => {
              if (!e || !e.request || !e.error) {
                return
              }
              if (!e.request.isMainFrame()) {
                return
              }
              this.loading = false
              this.failed = true
              this.failText = `${e.error.getErrorInfo()}（${e.error.getErrorCode()}）`
            })

          if (this.loading) {
            this.ProgressBar()
          }

          if (this.failed) {
            this.FailState()
          }
        }
        .layoutWeight(1)
        .width('100%')
      }
      .width('100%')
      .height('100%')
      .backgroundColor(MT.bg)
    }
  }

  @Builder TopBar() {
    Row() {
      this.CloseBtn()
      this.BackBtn()
      this.ForwardBtn()

      Row() {
        TextInput({ text: this.input })
          .fontSize(MT.fsSmall)
          .fontColor(MT.text)
          .placeholderColor(MT.text3)
          .backgroundColor(MT.transparent)
          .padding({ left: 0, right: 0 })
          .height(38)
          .layoutWeight(1)
          .onChange((v: string) => {
            this.input = v
          })
          .onSubmit(() => {
            this.go(this.input)
          })
      }
      .layoutWeight(1)
      .height(38)
      .margin({ left: 4, right: 4 })
      .padding({ left: 12, right: 12 })
      .borderRadius(MT.radiusCtrl)
      .backgroundColor(MT.surface2)
      .alignItems(VerticalAlign.Center)

      this.ReloadBtn()
    }
    .width('100%')
    .padding({ left: 10, right: 10, top: 6, bottom: 6 })
    .alignItems(VerticalAlign.Center)
    .backgroundColor(MT.surface)
    .border({ width: { bottom: 1 }, color: MT.line })
  }

  @Builder CloseBtn() {
    Row() {
      SymbolGlyph($r('sys.symbol.xmark'))
        .fontSize(MT.fsTitle)
        .fontColor([MT.text2])
    }
    .width(34)
    .height(34)
    .borderRadius(MT.radiusCtrl)
    .justifyContent(FlexAlign.Center)
    .onClick(() => {
      this.show = false
    })
  }

  @Builder BackBtn() {
    Row() {
      SymbolGlyph($r('sys.symbol.chevron_left'))
        .fontSize(MT.fsTitle)
        .fontColor([this.canBack ? MT.text : MT.text3])
    }
    .width(34)
    .height(34)
    .borderRadius(MT.radiusCtrl)
    .justifyContent(FlexAlign.Center)
    .onClick(() => {
      this.back()
    })
  }

  @Builder ForwardBtn() {
    Row() {
      SymbolGlyph($r('sys.symbol.chevron_right'))
        .fontSize(MT.fsTitle)
        .fontColor([this.canForward ? MT.text : MT.text3])
    }
    .width(34)
    .height(34)
    .borderRadius(MT.radiusCtrl)
    .justifyContent(FlexAlign.Center)
    .onClick(() => {
      this.forward()
    })
  }

  @Builder ReloadBtn() {
    Row() {
      SymbolGlyph($r('sys.symbol.arrow_clockwise'))
        .fontSize(MT.fsTitle)
        .fontColor([MT.accent])
    }
    .width(34)
    .height(34)
    .borderRadius(MT.radiusCtrl)
    .justifyContent(FlexAlign.Center)
    .onClick(() => {
      this.reload()
    })
  }

  @Builder ProgressBar() {
    Row() {
      Row()
        .width(`${this.progress}%`)
        .height(2)
        .backgroundColor(MT.accent)
    }
    .width('100%')
    .height(2)
    .backgroundColor(MT.transparent)
  }

  @Builder FailState() {
    Column() {
      Text(L.t('无法访问', 'Unreachable'))
        .fontSize(MT.fsTitle)
        .fontWeight(FontWeight.Medium)
        .fontColor(MT.text)
      Text(this.input)
        .fontSize(MT.fsSmall)
        .fontColor(MT.accent)
        .fontFamily('monospace')
        .margin({ top: 8 })
        .maxLines(2)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .textAlign(TextAlign.Center)
      Text(this.failText)
        .fontSize(MT.fsCaption)
        .fontColor(MT.text3)
        .margin({ top: 6 })
        .maxLines(2)
        .textAlign(TextAlign.Center)

      Column() {
        Text(L.t('可能原因', 'Possible reasons'))
          .fontSize(MT.fsCaption)
          .fontColor(MT.text3)
          .width('100%')
        Text(L.t('· 端口转发尚未真正建立', '· Port forwarding is not actually established'))
          .fontSize(MT.fsCaption)
          .fontColor(MT.text2)
          .width('100%')
          .margin({ top: 5 })
        Text(L.t('· 目标服务未在服务器上运行', '· Target service is not running on the server'))
          .fontSize(MT.fsCaption)
          .fontColor(MT.text2)
          .width('100%')
          .margin({ top: 4 })
        Text(L.t('· 端口填写与实际监听不一致', '· Port does not match the actual listener'))
          .fontSize(MT.fsCaption)
          .fontColor(MT.text2)
          .width('100%')
          .margin({ top: 4 })
      }
      .width('100%')
      .padding(14)
      .margin({ top: 18 })
      .borderRadius(MT.radiusCard)
      .backgroundColor(MT.surface2)
      .alignItems(HorizontalAlign.Start)

      Row() {
        Text(L.t('重试', 'Retry'))
          .fontSize(MT.fsSmall)
          .fontColor(MT.accentFg)
          .textAlign(TextAlign.Center)
          .height(42)
          .layoutWeight(1)
          .borderRadius(MT.radiusCard)
          .backgroundColor(MT.accent)
          .onClick(() => {
            this.reload()
          })
        Text(L.t('返回隧道', 'Back to tunnels'))
          .fontSize(MT.fsSmall)
          .fontColor(MT.text2)
          .textAlign(TextAlign.Center)
          .height(42)
          .layoutWeight(1)
          .borderRadius(MT.radiusCard)
          .backgroundColor(MT.surface2)
          .margin({ left: 10 })
          .onClick(() => {
            this.show = false
          })
      }
      .width('100%')
      .margin({ top: 18 })
    }
    .width('100%')
    .height('100%')
    .padding({ left: 28, right: 28, top: 60 })
    .backgroundColor(MT.bg)
    .alignItems(HorizontalAlign.Center)
  }
}
