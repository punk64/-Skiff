import TitleCard from '../../../../components/TitleCard'
import { TelnetType} from  '../../../../common/types/dataType'
import { KvManager } from '../../../../common/utils/KVManager'
import { distributedKVStore } from '@kit.ArkData'
import deepClone from '../../../../common/tools/deepClone'
import {pageDefaultData} from 'ets/pages/entryPage/pageDefaultData'
import { pointer } from '@kit.InputKit'

@Component
@Preview
export default struct DTelnet{
  @State @Watch("watchNowDataChange") nowData:TelnetType= pageDefaultData.Telnet
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData:string = ''
  // KVManager实例
  @State KVManager:KvManager = new KvManager({
    context: getContext(this),
    bundleName:getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id),//您自己的App对应的包名
  })
  @Styles
  inputTextStyle(){
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }
  @Styles hoverStyles() {
    .hoverEffect(HoverEffect.Highlight).onHover((isHover,event) => {
      if(isHover){
        this.getUIContext().getCursorController().setCursor(pointer.PointerStyle.HAND_POINTING)
      }else{
        this.getUIContext().getCursorController().restoreDefault()
      }
    })
  }

  // 所有数据的Store实例
  private allDataKvStore:distributedKVStore.SingleKVStore | undefined =undefined

  @State isInitialized: boolean = false

  // 监听saveSession变化
  saveSessionChange(){
    try {
      if(JSON.parse(this.defaultData).Telnet){
        this.nowData = deepClone(JSON.parse(this.defaultData).Telnet as TelnetType)
        this.saveStore()
      }
    } catch (e) {
      console.error(e)
    }
  }

  aboutToAppear(): void {
    console.log('Telnet aboutToAppear start')
    this.init()
  }

  onPageShow() {
    console.log('Telnet onPageShow - values:', JSON.stringify({
      ambiguity: this.nowData.ambiguity,
      mode: this.nowData.mode,
      keyboard: this.nowData.keyboard,
      telnetNewLine: this.nowData.telnetNewLine
    }))
  }

  // 初始化完成后处理
  initComplete() {
    this.isInitialized = true
    console.log('Telnet initialization complete')
  }

  aboutToDisappear(): void {
    this.allDataKvStore?.off('dataChange')
  }
  // 初始化
  async init(){
    try {
      console.log('Telnet init starting...')
      this.allDataKvStore = await this.KVManager.getStore('allData')
      // 先检查是否已有 Telnet 数据
      const telnetStr = await this.allDataKvStore.get('Telnet')
      if (telnetStr) {
        console.log('Found existing Telnet data:', telnetStr)
        const telnetData = JSON.parse(telnetStr as string) as TelnetType
        this.nowData = deepClone(telnetData)
      } else {
        console.log('No existing Telnet data, using default')
        this.nowData = deepClone(pageDefaultData.Telnet)
        await this.saveStore()
      }
      
      console.log('Telnet init complete - Current values:', JSON.stringify(this.nowData))
      
      this.allDataKvStore.on('dataChange',distributedKVStore.SubscribeType.SUBSCRIBE_TYPE_LOCAL,async (data)=>{
        console.log('Telnet dataChange:', JSON.stringify(data.updateEntries))
      })
      
      // 标记初始化完成
      this.initComplete()
    } catch (e) {
      console.error('Telnet init error:', e)
    }
  }
  
  // 保存当前数据
  async saveStore(){
    try {
      if (this.allDataKvStore) {
        await this.allDataKvStore.put('Telnet', JSON.stringify(this.nowData))
        console.log('Telnet data saved:', await this.allDataKvStore.get('Telnet'))
      } else {
        console.error('Cannot save Telnet data: KVStore not initialized')
      }
    } catch (e) {
      console.error('Error saving Telnet data:', e)
    }
  }
  // ambiguity
  radioChange(type:'ambiguity' | 'mode',value:string){
    this.nowData[type] = value
  }
  // mode
  radioModeChange(type:'ambiguity' | 'mode',value:string){
    this.nowData[type] = value
  }

  // 监听 nowData 改变时触发
  watchNowDataChange(){
    this.saveStore()
  }

  build() {
    // 显示当前状态
    // console.log('Telnet build - Current state:', JSON.stringify({
    //   ambiguity: this.nowData.ambiguity,
    //   mode: this.nowData.mode,
    //   keyboard: this.nowData.keyboard,
    //   telnetNewLine: this.nowData.telnetNewLine,
    //   isInitialized: this.isInitialized
    // }))
    
    Column({space:20}){
      Flex({direction:FlexDirection.Row,justifyContent:FlexAlign.SpaceBetween}){
        Column(){
          Text($r('app.string.hostname')).textAlign(TextAlign.Start).width('100%').textOverflow({overflow:TextOverflow.MARQUEE}).margin({bottom:5})
          TextInput({text:$$this.nowData.hostName}).inputTextStyle().hoverStyles().width('100%')
        }.width('45%')
        Column(){
          Text($r('app.string.Port')).textAlign(TextAlign.Start).width('100%').textOverflow({overflow:TextOverflow.MARQUEE}).margin({bottom:5})
          TextInput({text:$$this.nowData.port}).inputTextStyle().hoverStyles().width('100%')
        }.width('45%')
      }.width('100%')
      // TitleCard({titleString:'Telnet protocol adjustments'}){
        Column({space:16}){  // 调整垂直间距
          Text($r('app.string.ambiguity'))
            .fontSize(16)
            .fontWeight(FontWeight.Regular)

          Flex({ justifyContent: FlexAlign.SpaceBetween }) {
            Row({ space: 5 }){
              Radio({value:'BSD',group:'TelnetAmbiguityRadioGroup'})
                .checked(this.nowData.ambiguity === 'BSD')
                .onChange((isChecked: boolean) => {
                  if(isChecked) {
                    this.radioChange('ambiguity','BSD')
                  }
                })
              Text($r('app.string.BSD'))
                .fontSize(14)
                .fontColor(this.nowData.ambiguity == 'BSD' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                .margin({left: 8})
            }.width('50%')

            Row({ space: 5 }){
              Radio({value:'RFC',group:'TelnetAmbiguityRadioGroup'})
                .checked(this.nowData.ambiguity === 'RFC')
                .onChange((isChecked: boolean) => {
                  if(isChecked) {
                    this.radioChange('ambiguity','RFC')
                  }
                })
              Text($r('app.string.RFC'))
                .fontSize(14)
                .fontColor(this.nowData.ambiguity == 'RFC' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                .margin({left: 8})
            }.width('50%')
          }

          Text($r('app.string.Telnetnegotiationmode'))
            .fontSize(16)
            .fontWeight(FontWeight.Regular)
            .margin({top: 8})

          Flex({ justifyContent: FlexAlign.SpaceBetween }) {
            Row({ space: 5 }) {
              Radio({ value: 'Passive', group: 'TelnetModeRadioGroup' })
                .checked(this.nowData.mode === 'Passive')
                .onChange((isChecked: boolean) => {
                  if(isChecked) {
                    this.radioModeChange('mode', 'Passive')
                  }
                })
              Text($r('app.string.Passive'))
                .fontSize(14)
                .fontColor(this.nowData.mode == 'Passive' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                .margin({ left: 8 })
            }.width('50%')

            Row() {
              Radio({ value: 'Active', group: 'TelnetModeRadioGroup' })
                .checked(this.nowData.mode === 'Active')
                .onChange((isChecked: boolean) => {
                  if(isChecked) {
                    this.radioModeChange('mode', 'Active')
                  }
                })
              Text($r('app.string.Active'))
                .fontSize(14)
                .fontColor(this.nowData.mode == 'Active' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                .margin({ left: 8 })
            }.width('50%')
          }

            Row({ space: 5 }){
              Checkbox({name: 'keyboardSends',group: 'TelnetKeyboardGroup'})
                .select(this.nowData.keyboard ?? false)
                .shape(CheckBoxShape.CIRCLE)
                .onChange((value: boolean) => {
                  this.nowData.keyboard = value
                })
              Text($r('app.string.Telnetspecialcommands'))
                .fontSize(14)
                .fontColor(this.nowData.keyboard?$r('app.color.light_Select'): $r('app.color.light_noSelect'))
                .margin({left: 8})
            }.width('100%')

            Row({ space: 5 }) {
              Checkbox({name: 'telnetNewLine',group: 'TelnetKeyboardGroup'})
                .select(this.nowData.telnetNewLine ?? false)
                .shape(CheckBoxShape.CIRCLE)
                .onChange((value: boolean) => {
                  this.nowData.telnetNewLine = value
                })
              Text($r('app.string.TelnetNewLine'))
                .fontSize(14)
                .fontColor(this.nowData.telnetNewLine? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                .margin({left: 8})
            }.width('100%')

        }.alignItems(HorizontalAlign.Start)
      }
    }
  // }
}