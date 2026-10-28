@Component
export default struct PageLoading{
  @BuilderParam child: () => void = this.GlobalBuilder;
  // loading状态
  @Prop load:boolean
  // loading文本
  @Prop loadText:Resource

  @Builder
  GlobalBuilder(){

  }
  // 页面加载
  @Builder
  pageLoading(){
    Column(){
      LoadingProgress().width(100).height(100)
      Text(this.loadText)
    }
    .justifyContent(FlexAlign.Center)
    .height('100%')
    .width('100%')
    .opacity(0.5)
  }
  build() {
    Column(){
      Column(){
        this.pageLoading()
      }.visibility(this.load?Visibility.Visible:Visibility.None)
        Column(){
          this.child()
        }.visibility(this.load?Visibility.None:Visibility.Visible)
    }
    .height('100%')
    .width('100%')

  }
}