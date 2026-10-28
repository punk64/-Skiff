import TitleCard from '../../../../../components/TitleCard'
import SelectList from '../../../../../components/SelectList/index'
import { itemType } from '../../../../../components/SelectList/type'

interface objData{
  portForwarding:string[]
}
@Component
export default struct Tunnels {
  @State nowData:objData = {
    portForwarding:[]
  }
  @State selected:itemType<object> = {
    label: [],
    params: {}
  }
  @State selectIndex:number = -1
  @State selectList:itemType<object>[] = []
  @State protInput:string|undefined = undefined // 端口
  @State destination:string|undefined = undefined // ip地址
  @State concatType:string = 'local' // 链接类型
  @State agreement:string = 'auto' // 链接协议
  @Styles ButtonStyles(){
    .border({width:1,style:BorderStyle.Solid})
    .borderColor(0xDCDCDC)
    .borderRadius(5)
    .width(100)
    .height(30)
  }
  @Styles inputTextStyle(){
    .borderRadius(0)
    .padding(2)
    .border({width:1,style:BorderStyle.Solid})
    .backgroundColor(Color.Transparent)
  }
  checkboxChangeMethods(isCheck:boolean,type:string){
    if(isCheck){
      this.nowData.portForwarding=[...this.nowData.portForwarding,type]
    }else{
      this.nowData.portForwarding = this.nowData.portForwarding.filter((item)=>item !== type)
    }
  }
  add(){
    // 校验ipv6正则
    const regexIpv6 = /^\[([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}\]:(0|[1-9]\d{0,3}|[1-5]\d{4}|6[0-4]\d{3}|65[0-4]\d{2}|655[0-2]\d|6553[0-5])$/ig
    // 校验Ipv4正则
    const regexIpv4 = /^((25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(25[0-5]|2[0-4]\d|[01]?\d\d?):(0|[1-9]\d{0,3}|[1-5]\d{4}|6[0-4]\d{3}|65[0-4]\d{2}|655[0-2]\d|6553[0-5])$/ig;
    // 校验
    if(!this.protInput){
      AlertDialog.show({
        title:'CrossShellNext Error',
        message:'You need to specify a source port number',
      })
    }
  }
  remove(){

  }
  build() {
    TitleCard({titleString:'Port forwarding'}){
      Column({space:10}){
        Row({space:5}){
          Checkbox({name:'1',group:'portForwarding'}).select(this.nowData.portForwarding.indexOf('1')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkboxChangeMethods(check,'1'))
          Text('Local ports accept connections from other hosts')
        }
        Row({space:5}){
          Checkbox({name:'2',group:'portForwarding'}).select(this.nowData.portForwarding.indexOf('2')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkboxChangeMethods(check,'2'))
          Text('Remote ports do the same (SSH-2 only)')
        }
        Flex({justifyContent:FlexAlign.SpaceBetween}){
          Text('Forwarded ports:')
          Button('Remove',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.remove())
        }
        SelectList({selected:this.selected,selectIndex:this.selectIndex,listData:this.selectList,defaultRow:4})
        Text('Add new forwarded port:')
        Flex({justifyContent:FlexAlign.SpaceBetween}){
          Text('Source port').width('20%')
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            TextInput({text:$$this.protInput}).inputTextStyle().width(100)
            Button('Add',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.add())
          }.width('70%')
        }
        Flex({justifyContent:FlexAlign.SpaceBetween}){
          Text('Destination').width('20%')
          TextInput({text:$$this.destination}).inputTextStyle().width('70%')
        }
        Column(){
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Row(){
              Radio({value:'local',group:'concatType'}).checked(this.concatType === 'local').onChange((check:boolean)=>{check&&(this.concatType = 'local')})
              Text('Local')
            }.width('20%')
            Row(){
              Radio({value:'remote',group:'concatType'}).checked(this.concatType === 'remote').onChange((check:boolean)=>{check&&(this.concatType = 'remote')})
              Text('Remote')
            }.width('20%')
            Row(){
              Radio({value:'dynamic',group:'concatType'}).checked(this.concatType === 'dynamic').onChange((check:boolean)=>{check&&(this.concatType = 'dynamic')})
              Text('Dynamic')
            }.width('20%')
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Row(){
              Radio({value:'auto',group:'agreement'}).checked(this.agreement === 'auto').onChange((check:boolean)=>{check&&(this.agreement = 'local')})
              Text('Auto')
            }.width('20%')
            Row(){
              Radio({value:'ipv4',group:'agreement'}).checked(this.agreement === 'ipv4').onChange((check:boolean)=>{check&&(this.agreement = 'ipv4')})
              Text('Ipv4')
            }.width('20%')
            Row(){
              Radio({value:'ipv6',group:'agreement'}).checked(this.agreement === 'ipv6').onChange((check:boolean)=>{check&&(this.agreement = 'ipv6')})
              Text('Ipv6')
            }.width('20%')
          }
        }
      }.alignItems(HorizontalAlign.Start)
    }
  }
}
