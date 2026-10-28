import { display } from '@kit.ArkUI'
import { MT } from './MobileTheme'

const ANGLES: number[] = [232, 206, 180, 154, 128]
const LABELS: string[] = ['主机', '隧道', '终端', '文件', '设置']
const RADIUS: number = 130
const FAB_SIZE: number = 56
const ITEM_SIZE: number = 48
const HOLD_MS: number = 2000

@Component
export struct NavFab {
  @Prop active: number = 0
  onPick: (i: number) => void = () => {}
  @State open: boolean = false
  @State sw: number = 366
  @State sh: number = 809
  private tm: number = -1

  aboutToAppear(): void {
    const d: display.Display = display.getDefaultDisplaySync()
    if (d.width > 0 && d.densityPixels > 0 && d.height > 0) {
      this.sw = d.width / d.densityPixels
      this.sh = d.height / d.densityPixels
    }
  }

  aboutToDisappear(): void {
    this.clearTimer()
  }

  clearTimer(): void {
    if (this.tm >= 0) {
      clearTimeout(this.tm)
      this.tm = -1
    }
  }

  arm(): void {
    this.clearTimer()
    this.tm = setTimeout(() => {
      this.tm = -1
      this.collapse()
    }, HOLD_MS)
  }

  expand(): void {
    animateTo({ duration: 280, curve: Curve.FastOutSlowIn }, () => {
      this.open = true
    })
    this.arm()
  }

  collapse(): void {
    if (!this.open) {
      return
    }
    this.clearTimer()
    animateTo({ duration: 220, curve: Curve.FastOutSlowIn }, () => {
      this.open = false
    })
  }

  centerX(): number {
    return this.open ? this.sw - 63 : this.sw - 10
  }

  centerY(): number {
    return this.sh * 0.72
  }

  itemX(i: number): number {
    const r: number = this.open ? RADIUS : 0
    const a: number = ANGLES[i] * Math.PI / 180
    return this.centerX() + r * Math.cos(a) - ITEM_SIZE / 2
  }

  itemY(i: number): number {
    const r: number = this.open ? RADIUS : 0
    const a: number = ANGLES[i] * Math.PI / 180
    return this.centerY() + r * Math.sin(a) - ITEM_SIZE / 2
  }

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      if (this.open) {
        Column()
          .width('100%')
          .height('100%')
          .backgroundColor(MT.transparent)
          .zIndex(1)
          .onClick(() => {
            this.collapse()
          })
      }

      ForEach(LABELS, (lb: string, i: number) => {
        Text(lb)
          .fontSize(MT.fsCaption)
          .fontWeight(this.active === i ? FontWeight.Medium : FontWeight.Normal)
          .fontColor(this.active === i ? MT.accentFg : MT.text)
          .textAlign(TextAlign.Center)
          .width(ITEM_SIZE)
          .height(ITEM_SIZE)
          .borderRadius(ITEM_SIZE / 2)
          .backgroundColor(this.active === i ? MT.accent : MT.surface2)
          .border({ width: 1, color: this.active === i ? MT.accent : MT.line })
          .shadow({ radius: 10, color: '#33000000', offsetY: 3 })
          .position({ x: this.itemX(i), y: this.itemY(i) })
          .scale({ x: this.open ? 1 : 0.2, y: this.open ? 1 : 0.2 })
          .opacity(this.open ? 1 : 0)
          .zIndex(3)
          .onClick(() => {
            this.onPick(i)
            this.collapse()
          })
      }, (lb: string) => lb)

      Stack() {
        Row()
          .width(17)
          .height(2)
          .borderRadius(1)
          .backgroundColor(MT.accentFg)
          .translate({ y: this.open ? 0 : -6 })
          .rotate({ angle: this.open ? 45 : 0 })
        Row()
          .width(17)
          .height(2)
          .borderRadius(1)
          .backgroundColor(MT.accentFg)
          .opacity(this.open ? 0 : 1)
        Row()
          .width(17)
          .height(2)
          .borderRadius(1)
          .backgroundColor(MT.accentFg)
          .translate({ y: this.open ? 0 : 6 })
          .rotate({ angle: this.open ? -45 : 0 })
      }
      .width(FAB_SIZE)
      .height(FAB_SIZE)
      .borderRadius(FAB_SIZE / 2)
      .backgroundColor(MT.accent)
      .shadow({ radius: 14, color: '#55000000', offsetY: 4 })
      .position({ x: this.centerX() - FAB_SIZE / 2, y: this.centerY() - FAB_SIZE / 2 })
      .zIndex(4)
      .onClick(() => {
        if (this.open) {
          this.collapse()
        } else {
          this.expand()
        }
      })
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.None)
  }
}
