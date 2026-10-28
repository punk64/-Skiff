import SelectList,{SelectListController} from '../../../../../components/SelectList/index'
import TitleCard from '../../../../../components/TitleCard'
import { itemType } from '../../../../../components/SelectList/type'

interface objData{
  pseudoTerminal:boolean,
}
@Component
export default  struct DTTY{
  private controller:SelectListController = new SelectListController() // 选择列表的控制器对象
  @State nowData:objData = {
    pseudoTerminal:true
  }
  @State @Watch('watchSelected') selected:itemType<object> = {
    label: [],
    params: {}
  }
  @State selectIndex:number = -1
  @State radioType:string|undefined = undefined
  @State typeInput:string  = ''
  @State selectList:itemType<object>[] = [
    {
      label:['Ed448','(auto)'],
      params:{}
    },
    {
      label:['Ed25519','(auto)'],
      params:{}
    },
    {
      label:['ECDSA','(auto)'],
      params:{}
    },
    {
      label:['RSA','(auto)'],
      params:{}
    },
    {
      label:['DSA','(auto)'],
      params:{}
    },
    {
      label:['Ed25519','(auto)'],
      params:{}
    },
    {
      label:['ECDSA','(auto)'],
      params:{}
    },
    {
      label:['RSA','(auto)'],
      params:{}
    },
    {
      label:['DSA','(auto)'],
      params:{}
    },
    {
      label:['Ed25519','(auto)'],
      params:{}
    },
    {
      label:['ECDSA','123123123'],
      params:{}
    },
    {
      label:['RSA','(auto)'],
      params:{}
    },
    {
      label:['DSA','(auto)'],
      params:{}
    }
  ]
  @Styles inputTextStyle(){
    .borderRadius(0)
    .padding(2)
    .border({width:1,style:BorderStyle.Solid})
    .backgroundColor(Color.Transparent)
  }
  @Styles ButtonStyles(){
    .border({width:1,style:BorderStyle.Solid})
    .borderColor(0xDCDCDC)
    .borderRadius(5)
    .width('15%')
    .height(30)
  }

  // 监听选中的item的变化
  watchSelected(){
    switch (this.selected.label[1]){
      case '(auto)':this.radioType = 'auto';break;
      case "(don't send)":this.radioType = 'noting';break;
      default:this.radioType = 'this';this.typeInput = this.selected.label[1];break;
    }
  }
  sendSet(){
    switch (this.radioType){
      case 'auto':this.selectList[this.selectIndex] = {label:[this.selectList[this.selectIndex].label[0],'(auto)'],params:this.selectList[this.selectIndex].params};break;
      case 'noting':this.selectList[this.selectIndex] = {label:[this.selectList[this.selectIndex].label[0],"(don't send)"],params:this.selectList[this.selectIndex].params};break;
      case 'this':this.selectList[this.selectIndex] = {label:[this.selectList[this.selectIndex].label[0],this.typeInput +''],params:this.selectList[this.selectIndex].params};break;
    }
    this.controller.scrollTo()
  }
  build() {
    Column({space:20}){
      TitleCard(){
        Row({space:10}){
          Checkbox({name:'1',group:'pseudoTerminal'}).select(this.nowData.pseudoTerminal).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.nowData.pseudoTerminal = check)
          Text('Don\'t allocate a pseudo-terminal')
        }
      }
      TitleCard({titleString:'Terminal modes'}){
        Column({space:10}){
          Text('Terminal modes to send:')
          SelectList(
            {
              listData:this.selectList,
              selected:this.selected,
              selectIndex:this.selectIndex,
              defaultRow:8,
              scrollIndex:7,
              controller:this.controller
            })
          Row(){
            Text('For selected mode, send:')
            Button('Set',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.sendSet())
          }
          .justifyContent(FlexAlign.SpaceBetween)
          .width('100%')
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Flex({justifyContent:FlexAlign.SpaceBetween}){
              Row({space:5}){
                Radio({
                  value: 'auto',
                  group: 'sendType'
                })
                  .checked(this.radioType === 'auto')
                  .onChange((check:boolean)=> {
                    check && (this.radioType = 'auto')
                  })
                Text('Auto')
              }
              Row({space:5}){
                Radio({
                  value: 'noting',
                  group: 'sendType'
                })
                  .checked(this.radioType === 'noting')
                  .onChange((check:boolean)=>{
                    check && (this.radioType = 'noting')
                  })
                Text('Noting')
              }
              Row({space:5}){
                Radio({
                  value: 'this',
                  group: 'sendType'
                })
                  .checked(this.radioType === 'this')
                  .onChange((check:boolean)=>{
                    check && (this.radioType = 'this')
                  })
                Text('This:')
              }
            }.width('80%')
            TextInput({text:$$this.typeInput}).inputTextStyle().width('15%').onChange(()=>{
              this.radioType = 'this'
            })
          }
        }.alignItems(HorizontalAlign.Start)
      }
    }.alignItems(HorizontalAlign.Start)
  }
}
