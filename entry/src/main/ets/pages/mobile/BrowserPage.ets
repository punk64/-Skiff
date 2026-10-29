import { webview } from '@kit.ArkWeb'
import { MT } from '../../common/MobileTheme'
import { AppLog } from '../../common/AppLog'

@Component
export struct BrowserPage {
  @Link show: boolean
  @Prop @Watch('onUrlChanged') url: string = ''
  @State input: string = ''
  @State progress: number = 0
  @State loading: boolean = false
  @State failed: boolean = false
  @State failText: string = ''
  @State canBack: boolean = false
  @State canForward: boolean = false
  @State desktop: boolean = false
  @State diag: string = ''
  @StorageLink('csx_lang') csxLang: string = 'zh'
  private controller: webview.WebviewController = new webview.WebviewController()
  private diagTimer: number = -1
  private readonly uaMobile: string = 'Mozilla/5.0 (Linux; Android 14; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36'
  private readonly uaDesktop: string = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'

  t(zh: string, en: string): string {
    return this.csxLang === 'en' ? en : zh
  }

  aboutToAppear(): void {
    this.input = this.url
    this.loading = this.url.length > 0
    AppLog.i('browser', 'open ' + this.url)
  }

  aboutToDisappear(): void {
    this.clearDiag()
  }

  clearDiag(): void {
    if (this.diagTimer >= 0) {
      clearTimeout(this.diagTimer)
      this.diagTimer = -1
    }
  }

  armDiag(): void {
    this.clearDiag()
    this.diagTimer = setTimeout(() => {
      this.diagTimer = -1
      if (!this.loading) {
        return
      }
      if (this.progress >= 20) {
        AppLog.i('browser', 'data flowing, onPageEnd pending, progress=' + this.progress)
        return
      }
      this.diag = this.t('页面 8 秒未完成加载，隧道可能未真正打通', 'Page did not finish loading in 8s; the tunnel may not be working')
      AppLog.w('browser', 'no response in 8s: ' + this.input)
    }, 8000)
  }

  onUrlChanged(): void {
    if (this.url.length === 0 || this.url === this.input) {
      return
    }
    this.input = this.url
    this.failed = false
    this.loading = true
    this.diag = ''
    this.armDiag()
    try {
      this.controller.loadUrl(this.url)
    } catch (e) {
      this.failed = true
      this.loading = false
      this.failText = this.t('地址无法加载，请检查端口是否正确', 'Cannot load address, please check the port')
      AppLog.e('browser', 'loadUrl threw: ' + this.url)
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
    this.diag = ''
    this.armDiag()
    try {
      this.controller.loadUrl(u)
    } catch (e) {
      this.failed = true
      this.loading = false
      this.failText = this.t('地址无法加载，请检查端口是否正确', 'Cannot load address, please check the port')
      AppLog.e('browser', 'loadUrl threw: ' + u)
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
      this.failText = this.t('页面尚未加载，无法刷新', 'Page not loaded yet, cannot refresh')
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

  applyUa(): void {
    try {
      this.controller.setCustomUserAgent(this.desktop ? this.uaDesktop : this.uaMobile)
    } catch (e) {
    }
  }

  toggleUa(): void {
    this.desktop = !this.desktop
    this.applyUa()
    if (this.input.length === 0) {
      return
    }
    this.go(this.input)
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
    Column() {
      this.TopBar()
      if (this.loading) {
        this.ProgressBar()
      }
      if (this.diag.length > 0) {
        this.DiagBar()
      }
      Stack({ alignContent: Alignment.Top }) {
        if (this.failed) {
          this.FailState()
        }
        Web({ src: this.url, controller: this.controller })
            .width('100%')
            .height('100%')
            .visibility(this.failed ? Visibility.None : Visibility.Visible)
            .backgroundColor($r('app.color.csx_bg'))
            .javaScriptAccess(true)
            .domStorageAccess(true)
            .fileAccess(false)
            .zoomAccess(true)
            .mixedMode(MixedMode.All)
            .cacheMode(CacheMode.Default)
            .horizontalScrollBarAccess(false)
            .onControllerAttached(() => {
              AppLog.i('browser', 'controller attached, src=' + this.url)
              this.applyUa()
            })
            .onPageBegin(() => {
              this.loading = true
              this.failed = false
              this.progress = 0
              this.diag = ''
              this.armDiag()
              AppLog.i('browser', 'onPageBegin ' + this.input)
            })
            .onProgressChange((e: OnProgressChangeEvent) => {
              this.progress = e.newProgress
            })
            .onConsole((e: OnConsoleEvent) => {
              if (e && e.message) {
                AppLog.w('browser', 'console: ' + e.message.getMessage())
              }
              return false
            })
            .onHttpErrorReceive((e: OnHttpErrorReceiveEvent) => {
              if (e && e.response) {
                AppLog.e('browser', 'http status ' + e.response.getResponseCode())
              }
            })
            .onPageVisible(() => {
              this.clearDiag()
              this.diag = ''
              this.loading = false
              this.syncNav()
              AppLog.i('browser', 'onPageVisible ' + this.input)
            })
            .onPageEnd(() => {
              this.clearDiag()
              this.diag = ''
              this.loading = false
              this.progress = 100
              this.syncNav()
              AppLog.i('browser', 'onPageEnd ' + this.input)
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
              this.clearDiag()
              this.diag = ''
              this.loading = false
              this.failed = true
              this.failText = `${e.error.getErrorInfo()}（${e.error.getErrorCode()}）`
              AppLog.e('browser', `error ${e.error.getErrorCode()} ${e.error.getErrorInfo()} @ ${this.input}`)
            })
      }
      .layoutWeight(1)
      .width('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor($r('app.color.csx_bg'))
  }

  @Builder DiagBar() {
    Row() {
      Text(this.diag)
        .fontSize(MT.fsMicro)
        .fontColor($r('app.color.csx_warn'))
        .layoutWeight(1)
        .maxLines(2)
      Text(this.t('关闭', 'Close'))
        .fontSize(MT.fsMicro)
        .fontColor($r('app.color.csx_text2'))
        .padding({ left: 10, right: 4, top: 2, bottom: 2 })
        .onClick(() => {
          this.diag = ''
        })
    }
    .width('100%')
    .padding({ left: 14, right: 10, top: 8, bottom: 8 })
    .alignItems(VerticalAlign.Center)
    .backgroundColor($r('app.color.csx_surface2'))
  }

  @Builder TopBar() {
    Row() {
      this.CloseBtn()
      this.BackBtn()
      this.ForwardBtn()

      Row() {
        TextInput({ text: this.input })
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text'))
          .placeholderColor($r('app.color.csx_text3'))
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
      .backgroundColor($r('app.color.csx_surface2'))
      .alignItems(VerticalAlign.Center)

      this.UaBtn()
      this.ReloadBtn()
    }
    .width('100%')
    .padding({ left: 10, right: 10, top: 6, bottom: 6 })
    .alignItems(VerticalAlign.Center)
    .backgroundColor($r('app.color.csx_surface'))
    .border({ width: { bottom: 1 }, color: $r('app.color.csx_line') })
  }

  @Builder UaBtn() {
    Text(this.desktop ? this.t('电脑', 'PC') : this.t('手机', 'Phone'))
      .fontSize(MT.fsMicro)
      .fontWeight(FontWeight.Medium)
      .fontColor($r('app.color.csx_accent'))
      .textAlign(TextAlign.Center)
      .width(40)
      .height(26)
      .borderRadius(9)
      .backgroundColor($r('app.color.csx_accent_soft'))
      .margin({ right: 2 })
      .onClick(() => {
        this.toggleUa()
      })
  }

  @Builder CloseBtn() {
    Row() {
      SymbolGlyph($r('sys.symbol.xmark'))
        .fontSize(MT.fsTitle)
        .fontColor([$r('app.color.csx_text2')])
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
        .fontColor([this.canBack ? $r('app.color.csx_text') : $r('app.color.csx_text3')])
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
        .fontColor([this.canForward ? $r('app.color.csx_text') : $r('app.color.csx_text3')])
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
        .fontColor([$r('app.color.csx_accent')])
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
        .backgroundColor($r('app.color.csx_accent'))
    }
    .width('100%')
    .height(2)
    .backgroundColor(MT.transparent)
  }

  @Builder FailState() {
    Column() {
      Text(this.t('无法访问', 'Unreachable'))
        .fontSize(MT.fsTitle)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('app.color.csx_text'))
      Text(this.input)
        .fontSize(MT.fsSmall)
        .fontColor($r('app.color.csx_accent'))
        .fontFamily('monospace')
        .margin({ top: 8 })
        .maxLines(2)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .textAlign(TextAlign.Center)
      Text(this.failText)
        .fontSize(MT.fsCaption)
        .fontColor($r('app.color.csx_text3'))
        .margin({ top: 6 })
        .maxLines(2)
        .textAlign(TextAlign.Center)

      Column() {
        Text(this.t('可能原因', 'Possible reasons'))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text3'))
          .width('100%')
        Text(this.t('· 端口转发尚未真正建立', '· Port forwarding is not actually established'))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text2'))
          .width('100%')
          .margin({ top: 5 })
        Text(this.t('· 目标服务未在服务器上运行', '· Target service is not running on the server'))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text2'))
          .width('100%')
          .margin({ top: 4 })
        Text(this.t('· 端口填写与实际监听不一致', '· Port does not match the actual listener'))
          .fontSize(MT.fsCaption)
          .fontColor($r('app.color.csx_text2'))
          .width('100%')
          .margin({ top: 4 })
      }
      .width('100%')
      .padding(14)
      .margin({ top: 18 })
      .borderRadius(MT.radiusCard)
      .backgroundColor($r('app.color.csx_surface2'))
      .alignItems(HorizontalAlign.Start)

      Row() {
        Text(this.t('重试', 'Retry'))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_accent_fg'))
          .textAlign(TextAlign.Center)
          .height(42)
          .layoutWeight(1)
          .borderRadius(MT.radiusCard)
          .backgroundColor($r('app.color.csx_accent'))
          .onClick(() => {
            this.reload()
          })
        Text(this.t('返回隧道', 'Back to tunnels'))
          .fontSize(MT.fsSmall)
          .fontColor($r('app.color.csx_text2'))
          .textAlign(TextAlign.Center)
          .height(42)
          .layoutWeight(1)
          .borderRadius(MT.radiusCard)
          .backgroundColor($r('app.color.csx_surface2'))
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
    .backgroundColor($r('app.color.csx_bg'))
    .alignItems(HorizontalAlign.Center)
  }
}
