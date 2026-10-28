import { MT } from './MobileTheme'

@Component
export struct Arrow {
  @Prop dir: string = 'right'
  @Prop color: string = MT.text3
  @Prop len: number = 18

  build() {
    Path()
      .width(this.len)
      .height(this.len)
      .commands(this.dir === 'left' ? 'M12 4 L5.5 9.5 L12 15' : 'M6 4 L12.5 9.5 L6 15')
      .stroke(this.color)
      .strokeWidth(1.9)
      .strokeLineCap(LineCapStyle.Round)
      .strokeLineJoin(LineJoinStyle.Round)
      .fillOpacity(0)
  }
}
