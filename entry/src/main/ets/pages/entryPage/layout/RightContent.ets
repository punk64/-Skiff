import {pageType} from '../pagesConfig/pageTypes'
import DynamicsComponent from './DynamicsComponent'

@Component
struct RightContent {
  @Link pageParams:pageType

  build() {
        Column() {
          Text(this.pageParams ? this.pageParams.title:'')
            .fontColor($r('app.color.font_color'))
            .fontSize(22)
            .fontWeight(600)
            .width('100%')
            .lineHeight(50)
            .height(50)
            .textAlign(TextAlign.Start)

          Scroll(){
            DynamicsComponent({componentName:this.pageParams.name,componentId:this.pageParams.id}).margin({top:10})
          }.height('calc(100% - 68px)').align(Alignment.TopStart).scrollBar(BarState.Off)
        }
        .height('100%')
        .padding('20rpx')
        // .padding({top:0,left:20,right:0,bottom:20})
    }
}
export  default RightContent