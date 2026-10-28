import { allDataType } from 'ets/common/types/dataType'
import Terminal from 'ets/components/Terminal/index'

@Component
export default struct TerminalTab {
  private controller: TabsController = new TabsController()
  @Link tabArray: allDataType[] | null
  @State focusIndex: number = 0
  @State pre: number = 0
  @State index: number = 0
  @State test: boolean = false
  @BuilderParam child: (dataSource:allDataType | null) => void = this.GlobalBuilder;
  @Builder
  GlobalBuilder(dataSource:allDataType | null){

  }
  @Styles checkStyles(){

  }

  // 单独的页签
  @Builder
  Tab(tabName: string, tabItem: allDataType, tabIndex: number) {
    Row({ space: 20 }) {
      Text(tabName).fontSize(18)
        .fontColor(tabIndex===this.focusIndex?Color.Red:Color.Black)
    }
    .border({width:1,color:Color.Red})
    .justifyContent(FlexAlign.Center)
    .constraintSize({ minWidth: 35 })
    .width(80)
    .height(60)
    .onClick(() => {
      this.controller.changeIndex(tabIndex)
      this.focusIndex = tabIndex
    })
  }
  build() {
    Column() {
      Column() {
        // 页签
        Row({ space: 1 }) {
          Scroll() {
            Row() {
              ForEach(this.tabArray, (item: allDataType, index: number) => {
                this.Tab(item.Session.hostName || item.Session.serialLine || '', item, index)
              })
            }
            .justifyContent(FlexAlign.Start)
          }
          .align(Alignment.Start)
          .scrollable(ScrollDirection.Horizontal)
          .scrollBar(BarState.Off)
          .width('90%')
        }
        .alignItems(VerticalAlign.Bottom)
        .width('100%')
      }
      .alignItems(HorizontalAlign.Start)
      .width('100%')
      //tabs
      Tabs({ barPosition: BarPosition.Start, controller: this.controller }) {
        ForEach(this.tabArray, (item:allDataType, index: number) => {
          TabContent() {
            Terminal({configDataSource:item})
          }
        },(item:allDataType)=>JSON.stringify(item))
      }
      .width('100%')
      .barHeight(0)
      .animationDuration(100)
      .onChange((index: number) => {
        this.focusIndex = index
      })
    }
    .height('100%')
  }
}