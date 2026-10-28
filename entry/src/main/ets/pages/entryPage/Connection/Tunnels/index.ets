import TitleCard from '../../../../components/TitleCard'
import SelectList from '../../../../components/SelectList/index'
import { itemType } from '../../../../components/SelectList/type'
import {pageDefaultData} from 'ets/pages/entryPage/pageDefaultData'
import { TunnelsType } from '../../../../common/types/dataType'
import { KvManager } from '../../../../common/utils/KVManager'
import { distributedKVStore } from '@kit.ArkData'
import deepClone from '../../../../common/tools/deepClone'

interface objData {
  portForwarding: string[]
}

// 定义端口转发项的参数类型
interface PortForwardParams {
  type: string;
  sourcePort: string;
  destination: string;
}

@Component
export default struct DTunnels {

  @State @Watch("watchNowDataChange") TunnelsData:TunnelsType= pageDefaultData.Tunnels
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData:string = ''

  // KVManager实例
  @State KVManager:KvManager = new KvManager({
    context: getContext(this),
    bundleName:getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id),//您自己的App对应的包名
  })
  // 所有数据的Store实例
  private allDataKvStore:distributedKVStore.SingleKVStore | undefined =undefined

  // 监听saveSession变化
  saveSessionChange(){
    try {
      if(JSON.parse(this.defaultData).Tunnels){
        this.TunnelsData = deepClone(JSON.parse(this.defaultData).Tunnels as TunnelsType)
        
        // 从forwardedPorts恢复selectList数据
        if(this.TunnelsData.forwardedPorts) {
          try {
            this.selectList = JSON.parse(this.TunnelsData.forwardedPorts)
            console.log('恢复转发端口列表:', JSON.stringify(this.selectList))
          } catch(e) {
            console.error('解析forwardedPorts失败:', e)
            this.selectList = []
          }
        } else {
          this.selectList = []
        }
        
        this.saveStore()
      }
    } catch (e) {
      console.error(e)
    }
  }

  aboutToAppear(): void {
    this.init()
    
    // 确保一开始就从TunnelsData.forwardedPorts中加载数据
    if(this.TunnelsData && this.TunnelsData.forwardedPorts) {
      try {
        this.selectList = JSON.parse(this.TunnelsData.forwardedPorts)
        console.log('初始化转发端口列表:', JSON.stringify(this.selectList))
      } catch(e) {
        console.error('初始化解析forwardedPorts失败:', e)
        this.selectList = []
      }
    }
  }
  aboutToDisappear(): void {
    this.allDataKvStore?.off('dataChange')
  }
  // 初始化
  async init(){
    this.allDataKvStore = await this.KVManager.getStore('allData')
    
    // 从KV存储中获取Tunnels数据
    const tunnelsData = await this.allDataKvStore?.get('Tunnels')
    if (tunnelsData) {
      try {
        const parsedData = JSON.parse(tunnelsData as string) as TunnelsType
        this.TunnelsData = parsedData
        
        // 从forwardedPorts恢复selectList数据
        if(parsedData.forwardedPorts) {
          try {
            this.selectList = JSON.parse(parsedData.forwardedPorts)
            console.log('从KV存储初始化转发端口列表:', JSON.stringify(this.selectList))
          } catch(e) {
            console.error('从KV存储解析forwardedPorts失败:', e)
          }
        }
      } catch(e) {
        console.error('解析Tunnels数据失败:', e)
      }
    }
    
    await this.saveStore()
    this.allDataKvStore.on('dataChange',distributedKVStore.SubscribeType.SUBSCRIBE_TYPE_LOCAL,async (data)=>{
      console.log(JSON.stringify(data.updateEntries))
      // data.updateEntries.map(item=>{
      //   if(item.key === 'Session'){
      //   }
      // })
    })
  }
  // 保存当前数据
  async saveStore(){
    this.allDataKvStore && await this.allDataKvStore.put('Tunnels',JSON.stringify(this.TunnelsData))
    console.log('nowData',await this.allDataKvStore?.get('Tunnels'))
  }
  // ambiguity
  radioChange(type:'ambiguity'| 'mode',value:string){
    this.TunnelsData[type] = value
  }
  // mode

  // 监听 nowData 改变时触发
  watchNowDataChange(){
    this.saveStore()
  }


  @State nowData1: objData = {
    portForwarding: []
  }
  @State @Watch('onSelectedItemChange') selected: itemType<PortForwardParams> = {
    label: [],
    params: {} as PortForwardParams
  }
  @State @Watch('onSelectIndexChange') selectIndex: number = -1
  @State selectList: itemType<PortForwardParams>[] = []
  @State protInput: string | undefined = undefined // 端口
  @State destination: string | undefined = undefined // ip地址
  @State concatType: string = 'local' // 链接类型
  @State agreement: string = 'auto' // 链接协议

  @Styles
  ButtonStyles(){
    .border({ width: 1, style: BorderStyle.Solid })
    .borderColor(0xDCDCDC)
    .borderRadius(5)
    .width(100)
    .height(30)
  }

  @Styles inputTextStyle(){
    .borderRadius(4)
    .padding({ left: 8, right: 8 })  // 增加左右内边距
    .border({width:1,style:BorderStyle.Solid,color:'#E1E1E1'})
    .backgroundColor(Color.White)  // 修改背景色为白色
  }



  checkboxChangeMethods(isCheck: boolean, type: string) {
    if (isCheck) {
      this.nowData1.portForwarding = [...this.nowData1.portForwarding, type]
    } else {
      this.nowData1.portForwarding = this.nowData1.portForwarding.filter((item) => item !== type)
    }
  }

  add() {
    console.log("开始添加=====",this.TunnelsData.sourcePort)
    console.log("开始添加=====",this.TunnelsData.destination)
  // 校验Destination的格式，只需要包含冒号，且冒号左右两边都有值

     const destinationParts = this.TunnelsData.destination.split(':')
    if (destinationParts.length !== 2 || !destinationParts[0].trim() || !destinationParts[1].trim()){
      AlertDialog.show({
        title: 'CrossShellNext Error',
        message: '您需要指定目标地址，格式为"host.name:port"'
      })
      return
    }

    // 校验
    if (!this.TunnelsData.sourcePort) {
      AlertDialog.show({
        title: 'CrossShellNext Error',
        message: 'You need to specify a source port number',
      })
      return
    }
    
    // 构建Forwarded ports的显示内容
    // 根据连接类型添加前缀：L-本地，R-远程，D-动态
    const prefix = this.TunnelsData.concatType === 'local' ? 'L' : 
                   this.TunnelsData.concatType === 'remote' ? 'R' : 'D'
    
    // 检查是否存在相同的端口转发配置
    const isDuplicate = this.selectList.some(item => {
      const isSameSourcePort = item.label[0] === `${prefix}${this.TunnelsData.sourcePort}`
      const isSameDestination = item.label[1] === this.TunnelsData.destination
      const isSameType = (item.params as PortForwardParams)?.type === this.TunnelsData.concatType
      
      // 如果源端口、目标地址和转发类型都相同，则认为是重复配置
      return isSameSourcePort && isSameDestination && isSameType
    })
    
    // 如果是重复配置，弹出提示并返回
    if (isDuplicate) {
      AlertDialog.show({
        title: 'CrossShellNext Error',
        message: 'Specified forwarding already exists'
      })
      return
    }
    
    // 创建新的端口转发项
    const newForwardItem: itemType<PortForwardParams> = {
      label: [`${prefix}${this.TunnelsData.sourcePort}`, this.TunnelsData.destination],
      params: {
        type: this.TunnelsData.concatType,
        sourcePort: this.TunnelsData.sourcePort,
        destination: this.TunnelsData.destination
      }
    }
    
    // 添加到列表中
    this.selectList = [...this.selectList, newForwardItem]
    
    // 不再清空输入
    // this.TunnelsData.sourcePort = ''
    // this.TunnelsData.destination = ''
    
    // 更新forwardedPorts字段(用于存储所有端口转发项的JSON字符串)
    this.TunnelsData.forwardedPorts = JSON.stringify(this.selectList)
    
    // 触发数据变更
    this.watchNowDataChange()
  }


  remove(){
    // 检查是否有选中的项
    if (this.selectIndex >= 0 && this.selectIndex < this.selectList.length) {
      // 从列表中移除选中项
      this.selectList = [
        ...this.selectList.slice(0, this.selectIndex),
        ...this.selectList.slice(this.selectIndex + 1)
      ]
      
      // 更新 forwardedPorts
      this.TunnelsData.forwardedPorts = JSON.stringify(this.selectList)
      
      // 重置选中索引
      this.selectIndex = -1
      this.selected = { label: [], params: {} as PortForwardParams }
      
      // 触发数据变更
      this.watchNowDataChange()
    } else {
      AlertDialog.show({
        title: 'CrossShellNext Error',
        message: '请先选择要删除的端口转发项'
      })
    }
  }

  build() {
    TitleCard({ titleString: $r('app.string.Portforwarding') }) {
      Column({ space: 10 }) {
        // Row({ space: 5 }) {
        //   Checkbox({ name: '1', group: 'portForwarding' })
        //     .select(this.nowData.portForwarding.indexOf('1') >= 0)
        //     .shape(CheckBoxShape.ROUNDED_SQUARE)
        //     .onChange((check: boolean) => this.checkboxChangeMethods(check, '1'))
        //   Text('Local ports accept connections from other hosts')
        // }

        // Row({ space: 5 }) {
        //   Checkbox({ name: '2', group: 'portForwarding' })
        //     .select(this.nowData.portForwarding.indexOf('2') >= 0)
        //     .shape(CheckBoxShape.ROUNDED_SQUARE)
        //     .onChange((check: boolean) => this.checkboxChangeMethods(check, '2'))
        //   Text('Remote ports do the same (SSH-2 only)')
        // }

        Flex({ justifyContent: FlexAlign.SpaceBetween }) {
          Text($r('app.string.Forwardedports'))
          Button($r('app.string.Remove'), { type: ButtonType.Normal })
            .buttonStyle(ButtonStyleMode.NORMAL)
            .ButtonStyles()
            .fontColor($r('app.color.font_color'))
            .fontSize(14)
            .fontWeight(400)
            .onClick(() => this.remove())
        }

        SelectList({
          selected: this.selected,
          selectIndex: this.selectIndex,
          listData: this.selectList,
          defaultRow: 4
        })
        Text($r('app.string.Addnewforwardedport'))
        Flex({ justifyContent: FlexAlign.SpaceBetween }) {
          Text($r('app.string.Sourceport')).width('20%')
          Flex({ justifyContent: FlexAlign.SpaceBetween }) {
            TextInput({ text: $$this.TunnelsData.sourcePort })
            .inputTextStyle() 
            .type(InputType.Number)
            .width('70%')
            .height(32)    // 设置固定高度
            .margin({ right: 10 }) // 添加右侧间距
            Button($r('app.string.Add'), { type: ButtonType.Normal })
              .buttonStyle(ButtonStyleMode.NORMAL)
              .ButtonStyles()
              .fontSize(14)
              .fontColor($r('app.color.font_color'))
              .fontWeight(400)
              .onClick(() => this.add())
          }.width('70%')
        }

        Flex({ justifyContent: FlexAlign.SpaceBetween }) {
          Text($r('app.string.Destination')).width('20%')
          TextInput({ text: $$this.TunnelsData.destination })
          .inputTextStyle()
          .width('70%')
          .height(32)  // 设置固定高度
        }

        Column() {
          Flex({ justifyContent: FlexAlign.SpaceBetween }) {
            Row() {
              Radio({ value: 'local', group: 'concatType' })
                .checked(this.TunnelsData.concatType === 'local')
                .onChange((check: boolean) => {
                  check && (this.TunnelsData.concatType = 'local')
                })
              Text($r('app.string.Local','')) .fontColor(this.TunnelsData.concatType == 'local' ?$r('app.color.light_Select'): $r('app.color.light_noSelect'))
            }.width('20%')

            Row() {
              Radio({ value: 'remote', group: 'concatType' })
                .checked(this.TunnelsData.concatType === 'remote')
                .onChange((check: boolean) => {
                  check && (this.TunnelsData.concatType = 'remote')
                })
              Text($r('app.string.Remote')).fontColor(this.TunnelsData.concatType == 'remote' ?$r('app.color.light_Select'): $r('app.color.light_noSelect'))
            }.width('20%')

            Row() {
              Radio({ value: 'dynamic', group: 'concatType' })
                .checked(this.TunnelsData.concatType === 'dynamic')
                .onChange((check: boolean) => {
                  check && (this.TunnelsData.concatType = 'dynamic')
                })
              Text($r('app.string.Dynamic')).fontColor(this.TunnelsData.concatType == 'dynamic' ?$r('app.color.light_Select'): $r('app.color.light_noSelect'))
            }.width('20%')
          }
        }
      }.alignItems(HorizontalAlign.Start)
    }
  }

  // 监听选中项变化
  onSelectedItemChange() {
    console.log('选中项变化：', JSON.stringify(this.selected));
  }

  // 监听选中索引变化
  onSelectIndexChange() {
    console.log('选中索引变化：', this.selectIndex);
    
    // 保存选中的端口转发项
    if (this.selectIndex >= 0 && this.selectIndex < this.selectList.length) {
      this.TunnelsData.selectedForward = JSON.stringify(this.selectList[this.selectIndex]);
      console.log('保存选中的端口转发项：', this.TunnelsData.selectedForward);
      this.saveStore(); // 保存到KV存储
    } else {
      this.TunnelsData.selectedForward = undefined;
      this.saveStore();
    }
  }
}
