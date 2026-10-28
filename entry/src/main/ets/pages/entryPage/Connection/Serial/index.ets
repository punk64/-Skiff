import TitleCard from '../../../../components/TitleCard'
import { SerialType, SessionType } from '../../../../common/types/dataType'
import { KvManager } from '../../../../common/utils/KVManager'
import { distributedKVStore } from '@kit.ArkData'
import deepClone from '../../../../common/tools/deepClone'
import {speedOptions,stopBitsOptions,parityOptions,flowControlOptions,dataBitsOption,encodingList} from './dataSource'
@Component
export default struct DSerials {
  @State @Watch('watchSerialData') serialData: SerialType = {
    serialLine: 'COM1',
    speed: '9600',
    dataBits: '8',
    stopBits: '1',
    parity: 'None',
    sendEncoding:'utf-8',
    readEncoding:'utf-8',
    sendEncodingSelected:0,
    readEncodingSelected:0,
    flowControl: 'XON/XOFF',
    paritySelected:0,
    connectionSelected:1,
    dataBitsSelected:3,
    speedSelected:12,
    stopBitsSelected:0
  }
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData: string = ''
  // KVManager实例
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })

  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  watchSerialData(){
    this.saveStore()
  }

  @Styles
  inputTextStyle(){
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }

  // 监听session变化
  saveSessionChange() {
    try {
      console.log('dyx saveSession--------->', JSON.stringify(JSON.parse(this.defaultData).Serial))
      if (JSON.parse(this.defaultData).Serial) {
        this.serialData = deepClone(JSON.parse(this.defaultData).Serial as SerialType)
        console.log('dyx this.serialData deepClone',JSON.stringify(this.serialData))
      }
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  aboutToAppear(): void {
    this.init()
  }

  aboutToDisappear(): void {
    this.allDataKvStore?.off('dataChange')
  }

  // 初始化
  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    await this.saveStore()
  }

  // 保存当前数据
  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('Serial', JSON.stringify(this.serialData))
  }

  build() {
    Column({ space: 20 }) {
      TitleCard({titleString:$r('app.string.Configure_Serial_Line'),tips:$r('app.string.serila_configura_tips')}){
        Column(){
          Flex({justifyContent:FlexAlign.SpaceBetween}) {
            Text($r('app.string.Speed'))
            Select(speedOptions)
              .inputTextStyle()
              .width('320vp')
              .value($$this.serialData.speed)
              .controlSize(ControlSize.SMALL)
              .selected($$this.serialData.speedSelected)
              .onSelect(()=>{
                this.saveStore()
              })
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Text($r('app.string.Databits'))
            Select(dataBitsOption)
              .inputTextStyle()
              .width('320vp')
              .value($$this.serialData.dataBits)
              .controlSize(ControlSize.SMALL)
              .selected($$this.serialData.dataBitsSelected)
              .onSelect(()=>{
                this.saveStore()
              })
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Text($r('app.string.Stopbits'))
            Select(stopBitsOptions)
              .inputTextStyle()
              .width('320vp')
              .value($$this.serialData.stopBits)
              .controlSize(ControlSize.SMALL)
              .selected($$this.serialData.stopBitsSelected)
              .onSelect(()=>{
                this.saveStore()
              })
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Text($r('app.string.Parity'))
            Select(parityOptions)
              .inputTextStyle()
              .width('320vp')
              .value($$this.serialData.parity)
              .controlSize(ControlSize.SMALL)
              .selected($$this.serialData.paritySelectedIndex)
              .onSelect(()=>{
                this.saveStore()
              })
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Text($r('app.string.sendEncoding'))
            Select(encodingList)
              .inputTextStyle()
              .width('320vp')
              .value($$this.serialData.sendEncoding)
              .controlSize(ControlSize.SMALL)
              .selected($$this.serialData.sendEncodingSelected)
              .onSelect(()=>{
                this.saveStore()
              })
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Text($r('app.string.readEncoding'))
            Select(encodingList)
              .inputTextStyle()
              .width('320vp')
              .value($$this.serialData.readEncoding)
              .controlSize(ControlSize.SMALL)
              .selected($$this.serialData.readEncodingSelected)
              .onSelect(()=>{
                this.saveStore()
              })
          }
          // Flex({justifyContent:FlexAlign.SpaceBetween}){
          //   Text($r('app.string.Flowcontrol'))
          //   Select(flowControlOptions)
          //     .inputTextStyle()
          //     .width('320vp')
          //     .value($$this.serialData.flowControl)
          //     .controlSize(ControlSize.SMALL)
          //     .selected($$this.serialData.connectionSelected)
          //     .onSelect(()=>{
          //       this.saveStore()
          //     })
          // }
        }
      }
    }
  }
}