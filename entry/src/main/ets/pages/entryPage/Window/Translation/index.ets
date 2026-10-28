import { CharacterType, WindowSizeType } from "../../../../common/types/dataType";
import TitleCard from "../../../../components/TitleCard";
import { pageDefaultData } from "../../pageDefaultData";
import { distributedKVStore } from "@kit.ArkData";
import { KvManager } from "../../../../common/utils/KVManager";
import json from "@ohos.util.json";
import deepClone from "../../../../common/tools/deepClone";
import { TextInputSelect } from "../../../../components/InputSelect";

// let localStorage: LocalStorage = new LocalStorage();
// localStorage.setOrCreate('saveSession', '');
// let localStorageDefault: LocalStorage = new LocalStorage();
// localStorageDefault.setOrCreate('defaultData', '');

@Component
export default struct Translation {
  @Styles
  inputTextStyle(){
    .borderRadius(0)
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }

  private options: Array<SelectOption> = [{value:'utf-8'},{value:'utf-8'}]
  @State @Watch('watchNowDataChange') nowData: CharacterType = pageDefaultData.Character
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
    console.log('save Translation执行了')
    this.saveStore()
  }

  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('Character', JSON.stringify(this.nowData))
  }

  // 初始化
  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    this.nowData = pageDefaultData.Character
    this.saveStore()
  }

  aboutToAppear(): void {
    this.init()
  }

  async saveSessionChange() {
    try {
      this.nowData = deepClone(JSON.parse(this.defaultData).Character as CharacterType)
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  build() {
    Column({ space: 10 }) {

      TitleCard({ titleString: 'Character set translation' }) {
        Flex({ justifyContent: FlexAlign.SpaceBetween, direction: FlexDirection.Column }) {

          Row({ space: 10 }) {
            Column({ space: 10 }) {
              Row() {
                Text('Remote character set:')
              }.width('100%')

              TextInputSelect({
                text:this.nowData.value,
                selectOptions:this.options,
                textInputOnChange:(inputValue:String)=>{this.nowData.value = inputValue as string},
                inputText:this.nowData.value,
                selectModuleMargin:0,
                SelectPopupWidth:'55%',
              })
              Row() {
                Text('(Codepages supported by Windows but not listed here,such as CP866 on many systems,can be entered manually)')
                  .fontColor('#999999')
                  .fontSize(14)
              }.width('100%')

            }.width('100%')

          }.justifyContent(FlexAlign.SpaceBetween)
          .width('100%')

        }.height(80)
      }

    }
    .width('100%')
    .height('100%')
    .padding(20)
  }
}
