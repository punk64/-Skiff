@Component
export default struct TitleCard{
  @Prop titleString:string|Resource = '' // title文案
  @Prop tips:string|Resource // 提示文案
  @BuilderParam child: () => void = this.GlobalBuilder;
  @Builder
  GlobalBuilder(){

  }
  build() {
    Column({space: 8}){

        Text(this.titleString)
          .fontSize(14)
          .fontColor('#C8C8C8')
          .padding({left: 4})
      if(this.tips){
        Text(this.tips)
          .fontSize(14)
          .fontColor('#C8C8C8')
          .padding({left: 4})
      }
      Column(){
        this.child()
      }
      .width('100%')
      .backgroundColor($r('app.color.titleCar_color'))
      .borderRadius(8)
      .padding(20)
      .alignItems(HorizontalAlign.Start)
    }
    .alignItems(HorizontalAlign.Start)
  }
}