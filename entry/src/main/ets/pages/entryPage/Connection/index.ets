import TitleCard from '../../../components/TitleCard'
import {KvManager} from '../../../common/utils/KVManager'
import { distributedKVStore } from '@kit.ArkData'
import deepClone from '../../../common/tools/deepClone'
import {ConnectionType} from '../../../common/types/dataType'
import { pageDefaultData } from '../pageDefaultData'

@Component
export default struct DConnection {
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData:string = ''


  @State op1:string='#ECECEC'
  @State @Watch('nowDataChange') nowData:ConnectionType = deepClone(pageDefaultData.Connection)
  @State KVManager:KvManager = new KvManager({
    context: getContext(this),
    bundleName:getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id),
  })
  private allDataKvStore:distributedKVStore.SingleKVStore | undefined = undefined

  @Styles inputTextStyle() {
    .width('100%')
    .borderRadius(6)
    .backgroundColor($r('app.color.titleCar_color'))
    .border({ width: 1, style: BorderStyle.Solid, color: '#E5E5E5' })
    .padding({ left: 12, right: 12 ,top: 8, bottom: 8})
  }

  aboutToAppear(): void {
    this.init()
  }

  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('Connection', JSON.stringify(this.nowData))
  }

  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    this.nowData = deepClone(pageDefaultData.Connection)
    this.saveStore()
  }

  nowDataChange() {
    this.saveStore()
  }

  saveSessionChange() {
    try {
      console.log('connection变更了数据',JSON.stringify(JSON.parse(this.defaultData).Connection ))
      this.nowData = deepClone(JSON.parse(this.defaultData).Connection as ConnectionType)
      console.log('session列表点击时  connection的数据是'+JSON.stringify(this.nowData))
      console.log('session列表点击时  connection的ip数据是'+JSON.stringify(this.nowData.version))
      console.log('session列表点击时  connection的keepalive数据是'+JSON.stringify(this.nowData.keepalive))
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  // checkChange(isCheck: boolean, value: string) {
  //   if(isCheck) {
  //     if(this.nowData.tcpOptions.indexOf(value) < 0) {
  //       this.nowData.tcpOptions = [...this.nowData.tcpOptions, value]
  //     }
  //   } else {
  //     this.nowData.tcpOptions = this.nowData.tcpOptions.filter((item) => item !== value)
  //   }
  // }

  radioChange(value: string) {
    this.nowData.version = value
  }

  build() {
    Column({ space: 20 }) {
      TitleCard({ titleString: $r('app.string.connect_title') }) {
        Flex({ justifyContent: FlexAlign.SpaceBetween, alignItems: ItemAlign.Center }) {
          Text($r('app.string.connect_keep_alive'))
            .fontColor($r('app.color.font_color'))
            .width('40%')

          TextInput({ text: this.nowData.keepalive.toString()})
            .inputTextStyle()
            .width('55%')
            .type(InputType.Number)
            .onChange((val:string)=>{
              console.log('valvalval',val)
              this.nowData.keepalive =Number(val)
            })
            .onHover((isHover: boolean) => {
              console.log('222')
              if (isHover) {
                this.op1='#747472'
              }else {
                this.op1='#ECECEC'
              }
            })
            .borderColor(this.op1)
        }
      }

      TitleCard({ titleString: $r('app.string.connect_inter_version')}) {
        Row({ space: 32 }) {
          Row({ space: 8 }) {
            Radio({ value: 'Auto', group: 'Radio' })
              .checked(this.nowData.version == 'Auto')
              .onClick(() => this.radioChange('Auto'))
            Text($r('app.string.Auto'))
              .fontColor(this.nowData.version == 'Auto' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
          }
          Row({ space: 8 }) {
            Radio({ value: 'IPv4', group: 'Radio' })
              .checked(this.nowData.version == 'IPv4')
              .onClick(() => this.radioChange('IPv4'))
            Text('IPv4')
              .fontColor(this.nowData.version == 'IPv4' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
          }
          Row({ space: 8 }) {
            Radio({ value: 'IPv6', group: 'Radio' })
              .checked(this.nowData.version == 'IPv6')
              .onClick(() => this.radioChange('IPv6'))
            Text('IPv6')
              .fontColor(this.nowData.version == 'IPv6' ?$r('app.color.light_Select'): $r('app.color.light_noSelect'))
          }
        }
      }
    }
    .width('100%')
    .padding(24)
  }
}