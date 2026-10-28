import { WindowSizeType } from "../../../common/types/dataType";
import TitleCard from "../../../components/TitleCard";
import { pageDefaultData } from "../pageDefaultData";
import { distributedKVStore } from "@kit.ArkData";
import { KvManager } from "../../../common/utils/KVManager";
import json from "@ohos.util.json";
import deepClone from "../../../common/tools/deepClone";
import { Prompt } from "@kit.ArkUI";
import { emitter } from "@kit.BasicServicesKit";
import { pointer } from "@kit.InputKit";

@Component
export default struct Window {
  @Styles
  inputTextStyle(){
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }

  @State op:string='#ECECEC'
  @State op1:string='#ECECEC'
  @State @Watch('watchNowDataChange') nowData: WindowSizeType = deepClone(pageDefaultData.WindowSize)
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  @LocalStorageLink('defaultData') defaultData: string = ''
  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  @Styles
  hoverStyles() {
    .hoverEffect(HoverEffect.Highlight).onHover((isHover, event) => {
      if (isHover) {
        this.getUIContext().getCursorController().setCursor(pointer.PointerStyle.HAND_POINTING)
      } else {
        this.getUIContext().getCursorController().restoreDefault()
      }
    })
  }

  // 监听 nowData 改变时触发
  watchNowDataChange() {
    console.log('123')
    this.saveStore()
  }

  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('WindowSize', JSON.stringify(this.nowData))
  }

  // 初始化
  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    this.nowData = deepClone(pageDefaultData.WindowSize)
    this.saveStore()
  }

  aboutToAppear(): void {
    this.init()
  }

  async saveSessionChange() {
    try {
      console.log('saveSession--------->', JSON.stringify(JSON.parse(this.defaultData).WindowSize))
      this.nowData = deepClone(JSON.parse(this.defaultData).WindowSize as WindowSizeType)
      console.log('加载时获取到的数据win大小数据是' + JSON.stringify(this.nowData))
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  build() {
    Scroll(){
      Column({ space: 10 }) {
        TitleCard({ titleString: $r('app.string.window_size') }) {
          Flex({ justifyContent: FlexAlign.SpaceBetween, direction: FlexDirection.Column }) {
            Row({ space: 10 }) {
              Column({ space: 10 }) {
                Row() {
                  Text($r('app.string.window_Col'))
                }.width('100%')

                TextInput({ text: this.nowData.col.toString() })
                  .inputTextStyle()
                  .type(InputType.Number)
                  .placeholderColor('#999999')
                  .borderColor(this.op)
                  .onChange((val)=>{
                    this.nowData.col=Number(val)
                    if (val.length > 2) {
                      if (!/^\d+$/.test(this.nowData.col.toString())) {
                        console.log("yu-----> 输入无效：请输入数字");
                        return;
                      }
                      // 检查输入是否在 800 到 6000 之间
                      const regex = /^(8\d{2}|9\d{2}|[1-5]\d{3}|6000)$/;
                      const isInRange = regex.test(this.nowData.col.toString());

                      if (isInRange) {
                        console.log("yu-----> 输入有效");
                      } else {
                        console.log("yu-----> 输入无效：请输入 800 到 6000 之间的数字");
                        this.getUIContext().getPromptAction().showToast({message:$r('app.string.Window_Col_Toast')})
                      }
                    }
                  })


                  // .backgroundColor(this.op)
                  .onHover((isHover: boolean) => {
                    console.log('111')
                    if (isHover) {
                      this.op='#747472'
                    }else {
                      this.op='#ECECEC'
                    }
                  })
                  .onClick(()=>{
                    this.op='#0A59F7'
                  })
              }.width('48%')

              Column({ space: 10 }) {
                Row() {
                  Text($r('app.string.window_row'))
                }.width('100%')

                TextInput({ text: this.nowData.row.toString() })
                  .inputTextStyle()
                  .type(InputType.Number)
                  .placeholderColor('#999999')
                  .borderColor(this.op1)
                  .onChange((val)=>{
                    if (val.length>2) {
                      this.nowData.row=Number(val)

                      if (!/^\d+$/.test(this.nowData.row.toString())) {
                        console.log("yu-----> 输入无效：请输入数字");
                        return;
                      }
                      // 检查输入是否在 800 到 6000 之间
                      const regex = /^(8\d{2}|9\d{2}|[1-5]\d{3}|6000)$/;
                      const isInRange = regex.test(this.nowData.row.toString());

                      if (isInRange) {
                        console.log("yu-----> 输入有效");
                      } else {
                        console.log("yu-----> 输入无效：请输入 800 到 6000 之间的数字");
                        this.getUIContext().getPromptAction().showToast({message:$r('app.string.Window_Col_Toast')})
                      }
                    }
                  })
                  // .onBlur(() => {
                  //   if (!/^\d+$/.test(this.nowData.row.toString())) {
                  //     console.log("yu-----> 输入无效：请输入数字");
                  //     return;
                  //   }
                  //   // 检查输入是否在 800 到 6000 之间
                  //   const regex = /^(8\d{2}|9\d{2}|[1-5]\d{3}|6000)$/;
                  //   const isInRange = regex.test(this.nowData.row.toString());
                  //
                  //   if (isInRange) {
                  //     console.log("yu-----> 输入有效");
                  //   } else {
                  //     console.log("yu-----> 输入无效：请输入 800 到 6000 之间的数字");
                  //     Prompt.showToast({message:'请输入 800 到 6000 之间的数字'})
                  //   }
                  // })
                  .onHover((isHover: boolean) => {
                    console.log('222')
                    if (isHover) {
                      this.op1='#747472'
                    }else {
                      this.op1='#ECECEC'
                    }
                  })
                  .onClick(()=>{
                    this.op1='#0A59F7'
                  })
              }.width('48%')

            }.justifyContent(FlexAlign.SpaceBetween)
            .width('100%')
          }.height(60)
        }
        TitleCard({ titleString: $r('app.string.When_window_is_resized') }) {
          Column({space:5}){
            Row() {
              Radio({ value: '1', group: 'When_window_is_resized' })
                .checked(this.nowData.windowResizedId==='1')
                .onClick(() => {
                  this.nowData.windowResizedId = '1'
                  emitter.emit('windowResizedId')
                })
              Text($r('app.string.Change_the_number_of_rows_and_columns'))
                .textAlign(TextAlign.Start)
                .width('75%')
                .textOverflow({ overflow: TextOverflow.MARQUEE })
                .fontColor(this.nowData.windowResizedId == '1' ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            }.hoverStyles()

            Row() {
              Radio({ value: '2', group: 'When_window_is_resized' })
                .checked(this.nowData.windowResizedId==='2')
                .onClick(() => {
                  this.nowData.windowResizedId = '2'
                  emitter.emit('windowResizedId')
                })
              Text($r('app.string.Change_the_size_of_the_font'))
                .textAlign(TextAlign.Start)
                .width('75%')
                .textOverflow({ overflow: TextOverflow.MARQUEE })
                .fontColor(this.nowData.windowResizedId == '2' ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            }.hoverStyles()

            Row() {
              Radio({ value: '3', group: 'When_window_is_resized' })
                .checked(this.nowData.windowResizedId==='3')
                .onClick(() => {
                  this.nowData.windowResizedId = '3'
                  emitter.emit('windowResizedId')
                })
              Text($r('app.string.Change_font_size_only_when_maximised'))
                .textAlign(TextAlign.Start)
                .width('75%')
                .textOverflow({ overflow: TextOverflow.MARQUEE })
                .fontColor(this.nowData.windowResizedId == '3' ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            }.hoverStyles()
            Row() {
              Radio({ value: '4', group: 'When_window_is_resized' })
                .checked(this.nowData.windowResizedId==='4')
                .onClick(() => {
                  this.nowData.windowResizedId = '4'
                  emitter.emit('windowResizedId')
                })
              Text($r('app.string.Forbid_resizing_completely'))
                .textAlign(TextAlign.Start)
                .width('75%')
                .textOverflow({ overflow: TextOverflow.MARQUEE })
                .fontColor(this.nowData.windowResizedId == '4' ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            }.hoverStyles()
          }
        }

      }
      .width('100%')
      .constraintSize({minHeight:'100%'})
    }
    .layoutWeight(1)
    .constraintSize({minHeight:'100%'})
    .padding(20)
  }
}
