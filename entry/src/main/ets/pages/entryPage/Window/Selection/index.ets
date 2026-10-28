import { SelectType, TerminalType } from "../../../../common/types/dataType"
import { TextInputSelect } from "../../../../components/InputSelect"
import TitleCard from "../../../../components/TitleCard"
import { pageDefaultData } from "../../pageDefaultData"
import { distributedKVStore } from "@kit.ArkData"
import { KvManager } from "../../../../common/utils/KVManager"
import deepClone from "../../../../common/tools/deepClone"

@Component
export default struct Selection {

  private options: Array<SelectOption> = [{value:'No action'},{value:'System clipboard'}]
  private options1: Array<SelectOption> = [{value:'No action'},{value:'System clipboard'}]
  @State @Watch('watchNowDataChange') nowData: SelectType = deepClone(pageDefaultData.Select)
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  @LocalStorageLink('defaultData') defaultData: string = ''
  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  // 监听 nowData 改变时触发
  watchNowDataChange() {
    console.log('save Select执行了')
    this.saveStore()
  }

  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('Select', JSON.stringify(this.nowData))
  }

  // 初始化
  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    this.nowData = deepClone(pageDefaultData.Select)
    this.saveStore()
  }

  aboutToAppear(): void {
    this.init()
  }

  async saveSessionChange() {
    try {
      this.nowData = deepClone(JSON.parse(this.defaultData).Character as SelectType)
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  checkChange(isCheck: boolean) {
    this.nowData.AutoCopy=isCheck
  }

  build() {
    Column({ space: 10 }) {
      TitleCard({ titleString: 'Assign copy/paste actions to clipboards' }) {
        Column({space:8}) {

          Row({ space: 5 }){
            Checkbox({ name: '1', group: 'tcpGroup' })
              .select(this.nowData.AutoCopy)
              .shape(CheckBoxShape.ROUNDED_SQUARE)
              .onChange((isCheck: boolean) => this.checkChange(isCheck))
            Text('Auto-copy selected text to system clipboard')
              .fontColor(this.nowData.AutoCopy ? '#333333' : '#999999')
          }

          Column({ space: 15 }) {

            Row({ space: 10 }) {
              Row() {
                Text('{Ctrl,Shift)+Ins:')
              }.width('30%')

              TextInputSelect({
                text:this.nowData.CtrlIns,
                selectOptions:this.options,
                textInputOnChange:(inputValue:String)=>{this.nowData.CtrlIns = inputValue as string},
                inputText:this.nowData.CtrlIns,
                selectModuleMargin:0,
                SelectPopupWidth:'55%',
              })
            }.width('100%')
            .justifyContent(FlexAlign.SpaceBetween)

            Row({ space: 10 }) {
              Row() {
                Text('Ctrl Shift +{C,V}:')
              }.width('30%')

              TextInputSelect({
                text:this.nowData.CtrlCV,
                selectOptions:this.options1,
                textInputOnChange:(inputValue:String)=>{this.nowData.CtrlCV = inputValue as string},
                inputText:this.nowData.CtrlCV,
                selectModuleMargin:0,
                SelectPopupWidth:'55%',
              })
            }.width('100%')
            .justifyContent(FlexAlign.SpaceBetween)

          }.width('100%').height(80)

        }.alignItems(HorizontalAlign.Start)
      }
    }
    .width('100%')
    .height('100%')
    .padding(20)
  }
}
