import {itemType} from './type'
import { pointer } from '@kit.InputKit'

@Component
export default struct DSelectList{
  @Styles hoverStyles() {
    .hoverEffect(HoverEffect.Highlight).onHover((isHover,event) => {
      if(isHover){
        this.getUIContext().getCursorController().setCursor(pointer.PointerStyle.HAND_POINTING)
      }else{
        this.getUIContext().getCursorController().restoreDefault()
      }
    })
  }
  /**
   * 默认表格刚好能容纳item的数量，以此确定列表的高度
   */
  public defaultRow:number = 7
  /**
   * item双击事件
   */
  // public doubleClick:((value: itemType<object>) => void )| null = null

  /**
   * 单击事件
   */
  public handelClick:(()=>void) | null =null
  @Prop @Watch('indexChange') listData:itemType<object>[] // 源数据
  @Link selected:itemType<object> // 选中的值
  @Link @Watch('indexChange') selectIndex:number// 当前选中的索引
  public scrollIndex:number|undefined = undefined // 自定义滚动到索引位置（索引为从0开始）
  private scrollerForList:Scroller = new Scroller() // list滚动条对象
  private centerIndex:number = Math.floor(this.defaultRow / 2) // 当前列表的中心索引
  public controller:SelectListController = new SelectListController()

  onDoubleClick: ()=>void = () =>{}

  timer?:number
  isDoubleClick:boolean = false

  @Builder
  Item(item:itemType<object>,index:number){
    Row({space:20}){
      ForEach(item.label,(colItem:string)=>{
        Column(){
          Text(colItem).fontColor(this.selectIndex === index?Color.White:'').width('100%')
        }.width(200)
        .alignItems(HorizontalAlign.Start)
        .align(Alignment.Center)
        .flexGrow(1)
      })
    }
    .width('100%')
    // .gesture(
    //   GestureGroup(GestureMode.Exclusive,
    //     TapGesture({count:1})
    //       .onAction(()=>{
    //         this.selectIndex = index
    //         this.doubleClick && this.doubleClick(this.selected)
    //       })
    //       .tag('doubleClick'),
    //     TapGesture({count:2})
    //       .onAction(()=>{
    //         this.selectIndex = index
    //       })
    //       .tag('click'),
    //   ))
    .onClick(()=>{
      console.log('xxxxxxxxxxx')
      this.selectIndex = index
      if(this.isDoubleClick === false){
        this.isDoubleClick = true
        this.timer = setTimeout(() => {
          this.isDoubleClick = false
        }, 500)
        return
      }
      this.onDoubleClick()
      this.isDoubleClick = false
      clearTimeout(this.timer)
    })
    .backgroundColor(this.selectIndex === index?0x0078D7:'')
    .padding(2)
    .height(20)

  }

  aboutToDisappear(): void {
    clearTimeout(this.timer)
  }
  aboutToAppear(){
      // 将滚动方法赋值给控制器
    if(this.controller){
      this.controller.scrollTo = () => {
        this.scrollTo()
      }
    }
  }
  // scroll滚动的方法
  private  scrollTo(){
    if(this.scrollIndex){
      // 滚动到自定义索引位置
      this.scrollerForList.scrollToIndex(this.selectIndex<=this.scrollIndex?0:this.selectIndex-this.scrollIndex,false)
    }else{
      // 默认使选中item始终保持在列表视窗里中心偏下的位置
      this.scrollerForList.scrollToIndex(this.selectIndex<=this.centerIndex?0:this.selectIndex-this.centerIndex,false)
    }
  }
  // 监听选中索引的变化
  indexChange(){
    this.selected = this.listData[this.selectIndex]
    console.log('列表数据源',JSON.stringify(this.listData))
  }
  build() {
    Column(){
        List({initialIndex:this.selectIndex,scroller:this.scrollerForList}){
          ForEach(this.listData,(item:itemType<object>,index:number)=>{
            ListItem(){
              this.Item(item,index)
            }
          })
        }
        .height(this.defaultRow * 20 + 10)
        .padding(2)
        .border({ width: 1, style: BorderStyle.Solid })
        .backgroundColor(Color.Transparent)
        .borderRadius(8)
        .borderColor('#ECECEC')
        .hoverStyles()
    }
  }
}
// 控制器
export  class SelectListController{
  scrollTo = ()=>{
  }
}
