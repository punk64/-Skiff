@Component
export default  struct PageNavDestination{
  @Builder
  TitleBar(title:string = ''){
    Text(title)
      .fontColor('#182431')
      .fontSize(14)
      .width('100%')
      .lineHeight(20)
      .height(40)
      .padding(5)
      .border({width:'4px',style:BorderStyle.Solid})
      .margin({ bottom: 10 })
      .textAlign(TextAlign.Center)
  }
  @Builder customBuilder() {}
  @BuilderParam customBuilderParam: () => void = this.customBuilder;
  public title:string = ''

  build() {
    NavDestination(){
      this.customBuilderParam()
    }
    .margin({left:10})
    .title(this.TitleBar(this.title))
    .hideToolBar(false)
    .hideTitleBar(false)
  }
}