import TitleCard from '../../../../components/TitleCard'
import SelectList from '../../../../components/SelectList/index'
import {KvManager} from '../../../../common/utils/KVManager'
import { distributedKVStore } from '@kit.ArkData'
import deepClone from '../../../../common/tools/deepClone'
import { itemType } from '../../../../components/SelectList/type';

interface dataObj{
  username:string,
  specified:string,
  terminalType:string,
  terminalSpeeds:Number,
  variableList:itemType<object>[]
}

@Component
export default struct DData{
  @State listSelected:itemType<object> = {
    label: [],
    params: {}
  } //当前选中的数据
  @State selectIndex:number = -1 //当前选中的列表索引
  @State nowData:dataObj = {username:'',specified:'username',terminalType:'',terminalSpeeds:-1,variableList:[]}
  @Styles inputTextStyle(){
    .borderRadius(0)
    .padding(2)
    .border({width:1,style:BorderStyle.Solid})
    .backgroundColor(Color.Transparent)
  }
  @Styles ButtonStyles(){
    .border({width:1,style:BorderStyle.Solid})
    .borderColor(0xDCDCDC)
    .height(25)
    .borderRadius(5)
    .width(100)
    .height(30)
  }
  @State localName:string = '256832'
  @State variable:string = ''
  @State value:string = ''

  loginRadioChange(value:string){
    this.nowData.specified = value
  }

  add(){
    if(this.variable.length>0 && this.value.length>0){
      this.nowData.variableList=[{label:[this.variable,this.value],params:{}},...this.nowData.variableList]
      this.variable = ''
      this.value = ''
    }
  }
  remove(){
     this.nowData.variableList = this.nowData.variableList.filter((item,index)=>this.selectIndex !== index)
  }
  build() {
    Column({space:20}){
      TitleCard({titleString:'Login details'}){
        Column({space:10}){
          Flex({justifyContent:FlexAlign.SpaceBetween,alignItems:ItemAlign.Center}){
            Text('Auto-login username')
            TextInput({text:$$this.nowData.username}).inputTextStyle().width('50%')
          }
          Text('When username is not specified:')
          Row({space:20}){
            Row(){
              Radio({value:'prompt',group:'loginRadio'}).checked(this.nowData.specified === 'prompt').onChange(()=>this.loginRadioChange('prompt'))
              Text('Prompt')
            }
            Row(){
              Radio({value:'username',group:'loginRadio'}).checked(this.nowData.specified === 'username').onChange(()=>this.loginRadioChange('username'))
              Text(`Use system username (${this.localName})`)
            }
          }
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Terminal details'}){
        Column({space:10}){
          Flex({justifyContent:FlexAlign.SpaceBetween,alignItems:ItemAlign.Center}){
            Text('Terminal-type string')
            TextInput({text:$$this.nowData.terminalType}).inputTextStyle().width('40%')
          }
          Flex({justifyContent:FlexAlign.SpaceBetween,alignItems:ItemAlign.Center}){
            Text('Terminal speeds')
            TextInput({text:$$this.nowData.terminalSpeeds}).type(InputType.Number).inputTextStyle().width('40%')
          }
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Login details'}){
        Column({space:10}){
          Flex({justifyContent:FlexAlign.SpaceBetween,alignItems:ItemAlign.Center}){
            Text('Variable').width('20%')
            TextInput({text:$$this.variable}).inputTextStyle().width('40%')
            Button('Add',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.add())
          }
          Flex({justifyContent:FlexAlign.SpaceBetween,alignItems:ItemAlign.Center}){
            Text('Value').width('20%')
            TextInput({text:$$this.value}).inputTextStyle().width('40%')
            Button('Remove',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.remove())
          }
          SelectList({listData:this.nowData.variableList,selected:this.listSelected,selectIndex:this.selectIndex,defaultRow:3})
        }.alignItems(HorizontalAlign.Start)
      }
    }
  }
}
