import { HSBColorPicker } from '@keke/color-picker'
import TitleCard from '../../../../components/TitleCard'
import { distributedKVStore } from '@kit.ArkData'
import { KvManager } from '../../../../common/utils/KVManager'
import { pageDefaultData } from '../../pageDefaultData'
import { ColorType } from '../../../../common/types/dataType'
import deepClone from '../../../../common/tools/deepClone'
import { promptAction } from '@kit.ArkUI'

@Component
export default struct Colours {
  @Styles
  inputTextStyle(){
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }

  @Styles ButtonStyles(){
    .border({width:1,style:BorderStyle.Solid})
    .borderColor(0xDCDCDC)
    .borderRadius(5)
    .width('20%')
    .height(30)
  }

  @State @Watch('HexColorToRgb') SelectColorValue: ColorType = deepClone(pageDefaultData.Color)
  @State red: number = 0
  @State green: number = 0
  @State blue: number = 0
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  @LocalStorageLink('defaultData') defaultData: string = ''
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  // 监听 nowData 改变时触发
  watchNowDataChange() {
    this.saveStore()
  }

  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('Color', JSON.stringify(this.SelectColorValue))
    console.log('颜色存入数据库中内容' + JSON.stringify(this.SelectColorValue))
    this.allDataKvStore?.get('Color').then((res) => {
      console.log('获取数据库中颜色内容' + JSON.stringify(res))
    })
  }

  // 初始化
  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    console.log('this.SelectColorValue---》' + JSON.stringify(this.SelectColorValue))
    this.SelectColorValue.fontColor = deepClone(pageDefaultData.Color).fontColor
    console.log('初始化init获取后的值：' + JSON.stringify(this.SelectColorValue.fontColor))
    this.saveStore()
  }

  aboutToAppear(): void {
    this.init()
    this.HexColorToRgb()
  }

  async saveSessionChange() {
    try {
      this.SelectColorValue = deepClone(JSON.parse(this.defaultData).Color as ColorType)
      console.log('加载时获取到的数据颜色数据是' + JSON.stringify(this.SelectColorValue))
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  HexColorToRgb() {
    console.log('执行HexColorToRgb-----')
    // 将十六进制转换为十进制
    this.red = parseInt(this.SelectColorValue.fontColor.slice(0, 2), 16);
    this.green = parseInt(this.SelectColorValue.fontColor.slice(2, 4), 16);
    this.blue = parseInt(this.SelectColorValue.fontColor.slice(4, 6), 16);
    console.log('red'+this.red+'green'+this.green+"blue"+this.blue)
  }

  RgbToHexColor(): string {
    const hexR = this.red.toString(16).padStart(2, "0");
    const hexG = this.green.toString(16).padStart(2, "0");
    const hexB = this.blue.toString(16).padStart(2, "0");
    // 拼接成完整的十六进制颜色值
    return `${hexR}${hexG}${hexB}`;
  }

  build() {
    Column({ space: 10 }) {
      Flex({ direction: FlexDirection.Row }) {
        TitleCard({ titleString: $r('app.string.color_title') }) {
          Flex({ justifyContent: FlexAlign.SpaceBetween, direction: FlexDirection.Column }) {

            Row({ space: 10 }) {
              Column({ space: 10 }) {
                Row() {
                  Text($r('app.string.color_select'))
                }.width('100%')

                Row({ space: 10 }) {
                  HSBColorPicker({
                    //初始化时读取上次的颜色数据并进行修改
                    color:this.SelectColorValue.fontColor,
                    radius: 8,
                    onChange: (value) => {
                      // 去掉透明度部分，只保留 RGB 部分
                      this.SelectColorValue.fontColor = value.slice(3, 9)
                      console.log('选取调色盘时返回的数据'+this.SelectColorValue.fontColor)
                    }
                  })
                    .height('100%')
                    .width('55%')

                  Column({ space: 15 }) {
                    Text($r('app.string.RGBvalue'))

                    Row({ space: 10 }) {
                      Row() {
                        Text($r('app.string.red'))
                      }.width('20%')

                      TextInput({ text: this.red.toString() }).inputTextStyle().width('75%')
                        .type(InputType.Number)
                        .onChange((val: string) => {
                          this.red = Number(val)
                        })
                    }.width('100%')
                    .justifyContent(FlexAlign.SpaceBetween)

                    Row({ space: 10 }) {
                      Row() {
                        Text($r('app.string.Green'))
                      }.width('20%')

                      TextInput({ text: this.green.toString() }).inputTextStyle().width('75%')
                        .type(InputType.Number)
                        .onChange((val: string) => {
                          this.green = Number(val)
                        })
                    }.width('100%')
                    .justifyContent(FlexAlign.SpaceBetween)

                    Row({ space: 10 }) {
                      Row() {
                        Text($r('app.string.Blue'))
                      }.width('20%')

                      TextInput({ text: this.blue.toString() }).inputTextStyle().width('75%')
                        .type(InputType.Number)
                        .onChange((val: string) => {
                          this.blue = Number(val)
                        })
                    }.width('100%')
                    .justifyContent(FlexAlign.SpaceBetween)

                    // Text($r('app.string.Modify'))
                    //   .width('90%')
                    //   .height('15%')
                    //   .borderWidth(1)
                    //   .borderRadius(8)
                    //   .textAlign(TextAlign.Center)
                    //   .border({ width: 1, style: BorderStyle.Solid })
                    //   .backgroundColor("#E5E6EA")
                    //   .borderRadius(8)
                    //   .borderColor('#ECECEC')
                    Button($r('app.string.Modify'),{type:ButtonType.Normal})
                      .buttonStyle(ButtonStyleMode.NORMAL)
                      .ButtonStyles()
                      .width('90%')
                      .height(30)
                      .fontColor($r('app.color.font_color'))
                      .fontSize(14)
                      .fontWeight(400)
                      .onClick(() => {
                        if (this.red >= 0 && this.red <= 255 && this.blue >= 0 && this.blue <= 255 && this.green >= 0 && this.green <= 255) {
                          this.SelectColorValue.fontColor=this.RgbToHexColor()
                          console.log('输入rgb转换为16进制'+this.RgbToHexColor())
                          this.saveStore()
                          this.getUIContext().getPromptAction().showToast({
                            message: $r('app.string.Setup_Successful')
                          })
                        }else {
                          this.getUIContext().getPromptAction().showToast({
                            message: $r('app.string.Setup_Error_0_255')
                          })
                        }
                      })


                  }.width('40%').height('100%')
                }.width('100%')
                .justifyContent(FlexAlign.SpaceBetween)

              }.width('100%')

            }.justifyContent(FlexAlign.SpaceBetween)
            .width('100%')
            .height('90%')

          }.height('80%')
        }
      }
      .width('100%')
      .height('100%')
      .padding(20)
    }
  }
}
