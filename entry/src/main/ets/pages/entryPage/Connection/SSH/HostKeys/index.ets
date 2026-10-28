import TitleCard from "../../../../../components/TitleCard"
import SelectList,{SelectListController} from '../../../../../components/SelectList/index'
import { itemType } from '../../../../../components/SelectList/type';
import { LengthUnit } from "@kit.ArkUI";
import { common, Want } from "@kit.AbilityKit";
import { BusinessError } from "@kit.BasicServicesKit";


interface dataObj{
  algorithm:string,
  keyList:itemType<object>[]
}
@Component
export default struct  HostKeys{
  @State nowData:dataObj = {
    algorithm:'1',
    keyList:[]
  }
  @State selectList:itemType<object>[] = [
    {
      label:['Ed448'],
      params:{}
    },
    {
      label:['Ed25519'],
      params:{}
    },
    {
      label:['ECDSA'],
      params:{}
    },
    {
      label:['RSA'],
      params:{}
    },
    {
      label:['DSA'],
      params:{}
    },
    {
      label:['-- warn below here--'],
      params:{}
    },
  ]
  @State selected:itemType<object> = {
    label: [],
    params: {}
  }
  @State selectIndex:number = -1
  @State keySelected:itemType<object> = {
    label: [],
    params: {}
  }
  @State keySelectIndex:number = -1
  @State keyInput:string = ''
  private context = getContext(this) as common.UIAbilityContext
  private controller:SelectListController = new SelectListController()
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
    .width(100)
    .height(30)
  }

  up(){
    if(this.selectIndex<=0)return;
    const obj:itemType<object> = this.selectList[this.selectIndex]
    this.selectList[this.selectIndex] = this.selectList[this.selectIndex-1]
    this.selectList[this.selectIndex-1] = obj
    this.selectIndex = this.selectIndex - 1
    this.controller.scrollTo()
  }

  down(){
    if(this.selectIndex >= this.selectList.length - 1 || this.selectIndex<0)return;
    const obj:itemType<object> = this.selectList[this.selectIndex]
    this.selectList[this.selectIndex] = this.selectList[this.selectIndex+1]
    this.selectList[this.selectIndex+1] = obj
    this.selectIndex = this.selectIndex + 1
    this.controller.scrollTo()
  }

  remove(){
    this.nowData.keyList = this.nowData.keyList.filter((item,index)=>this.keySelectIndex !== index)
  }

  add(){
    if (this.keyInput.length>0) {
      this.nowData.keyList = [{label:[this.keyInput],params:{}},...this.nowData.keyList]
      this.keyInput = ''
    }
  }
  configureHost(){
    // 将Context对象传递过去
    let wantInfo: Want = {
      bundleName:this.context.resourceManager.getStringSync($r('app.string.App_bundleName').id),//您自己的App对应的包名
      abilityName: 'ConfigHostAbility',//您自己新建的UIability
      parameters: { // 自定义信息
        info: '来自EntryAbility Index页面'
      },
    }
    if(this.context){
      // context为调用方UIAbility的UIAbilityContext
      this.context.startAbility(wantInfo).then(() => {
        console.log('startAbility success.');
      }).catch((error: BusinessError) => {
        console.log('startAbility failed.');
      });
    }
  }

  build() {
    Column({space:20}){
      TitleCard({titleString:'Host key algorithm preference'}){
        Column({space:10}){
          Text('Algorithm selection policy:')
          Row({space:10}){
            SelectList({listData:this.selectList,defaultRow:5,selected:this.selected,selectIndex:this.selectIndex,controller:this.controller}).width('80%')
            Column({space:10}){
              Button('Up',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.up())
              Button('Down',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.down())
            }
          }
          Row({space:5}){
            Checkbox({name:'1',group:'algorithmCheckbox'}).select(this.nowData.algorithm === '1').shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=>this.nowData.algorithm = isCheck?'1':'')
            Text('Prefer algorithms for which a host key is known')
          }
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Manually configure host keys for this connection'}){
        Column({space:5}){
          Flex({justifyContent:FlexAlign.SpaceBetween,alignItems:ItemAlign.Center}){
            Text('Host keys or fingerprints to accept:')
            Button('Remove',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.remove())
          }
          SelectList({listData:this.nowData.keyList,selected:this.keySelected,selectIndex:this.keySelectIndex,defaultRow:2})
          Flex({alignItems:ItemAlign.Center,space:{main:{value:10,unit:LengthUnit.PX}},justifyContent:FlexAlign.SpaceBetween}){
            Text('Key')
            TextInput({text:$$this.keyInput}).width('50%').inputTextStyle()
            Button('Add key',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.add())
          }.margin({top:10})
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Configure trusted certification authorities'}){
        Button('Configure host CAs',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).width('100%').onClick(()=>this.configureHost())
      }
    }.alignItems(HorizontalAlign.Start)
  }
}
