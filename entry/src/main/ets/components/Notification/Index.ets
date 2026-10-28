import { UIContext } from "@kit.ArkUI"

@Component

export default struct Notification{
  @BuilderParam child:() => void = this.children
  @Prop title:string|Resource|null = null
  @Prop customPosition:Edges = {top:50,right:0}
  @Link @Watch('changeShowType') show:boolean

  @Builder children(){}

  changeShowType(){
    if(this.show){
      // this.getUIContext().getFocusController().requestFocus('customNotification')
    }
  }

  build() {
    Column({space:16}){
      if(this.title){
        Text(this.title).textAlign(TextAlign.Center).fontColor('#1A1A1A').fontSize(18).fontWeight(700)
      }
      this.child()
    }
    .constraintSize({
      minHeight: 100,
    })
    .id('customNotification')
    .width(400)
    .padding({top:20,left:16,right:16,bottom:16})
    .borderRadius(16)
    .position(this.show?this.customPosition:{top:this.customPosition.top,right:-400})
    .zIndex(99)
    .backgroundColor(Color.White)
    .focusable(true)
    .defaultFocus(true)
    .onFocus(()=>{
      console.log('获焦了')
    })
    .onBlur(()=>{
      this.show = false
      console.log('失焦了',this.show)
    })
  }
}