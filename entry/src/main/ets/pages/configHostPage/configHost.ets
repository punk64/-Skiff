import TitleCard from "../../components/TitleCard"
import SelectList from '../../components/SelectList/index'
import { itemType } from '../../components/SelectList/type';
import { common } from "@kit.AbilityKit";
import { fileIo as fs, picker } from '@kit.CoreFileKit';

interface dataObj{
  CAName:string,
  certifyKey:string,
  signatureTypes:string[],
  file?:fs.File
}

@Entry
@Component
struct Index{
  @State listSelected:itemType<object> = {
    label: [],
    params: {}
  } //当前选中的数据
  @State selectIndex:number = -1 //当前选中的列表索引
  @State selectList:itemType<object>[]=[{label:['default Settings'],params:{}}] //选中列表数据源
  @State nowData:dataObj = {CAName:'',certifyKey:'',signatureTypes:[]}
  private context = getContext(this) as common.UIAbilityContext
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
    .height(30)
    .width('100%')
  }

  checkboxChange(isCheck:boolean,type:string){
    if(isCheck){
      this.nowData.signatureTypes=[...this.nowData.signatureTypes,type]
    }else{
      this.nowData.signatureTypes = this.nowData.signatureTypes.filter((item)=>item !== type)
    }
  }

  // load按钮点击回调
  load(){
    console.log('load')
  }

  // save按钮点击回调
  save(){
    console.log('save')
  }

  // delete按钮点击回调
  delete(){
    if(this.selectList && this.selectList.length > 1 && this.selectIndex>=0){}
    console.log('delete')
  }

  //读取文件
  async  readFile(){
    let documentPicker =  new picker.DocumentViewPicker(this.context)
    const file = await documentPicker.select({maxSelectNumber:1,})
    this.nowData.file = fs.openSync(file[0],fs.OpenMode.READ_ONLY)
    console.log('file------------->',this.nowData.file)
  }

  build() {
    Column({space:20}){
      TitleCard({titleString:'Load, save or delete a host CA record'}){
        Column({space:10}){
          Column({space:5}){
            Text('Name for this CA (shown in log messages)')
            TextInput({text:$$this.nowData.CAName}).inputTextStyle().width('80%')
          }.alignItems(HorizontalAlign.Start)
          Row({space:10}){
            Column() {
              SelectList({listData:this.selectList,selected:this.listSelected,selectIndex:this.selectIndex,defaultRow:6})
            }
            .width('80%')
            .alignItems(HorizontalAlign.Start)
            Column({space:10}){
              Button('Load',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.load())
              Button('Save',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.save())
              Button('Delete',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.delete())
            }
            .width('20%')
            .alignItems(HorizontalAlign.Start)
          }.alignItems(VerticalAlign.Top)
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Public key for this CA record'}){
        Column({space:5}){
          Text('Public key of certification authority')
          Row({space:10}){
            TextInput().inputTextStyle().width('80%')
            Button('Read from file',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.readFile()).width('20%')
          }
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'What this CA is trusted to do'}){
        Column({space:5}){
          Text('Valid hosts this key is trusted to certify')
          TextInput({text:$$this.nowData.certifyKey}).inputTextStyle()
          Flex({alignItems:ItemAlign.Center,justifyContent:FlexAlign.SpaceBetween}){
            Text('Signature types (RSA keys only):').width('40%')
            Row(){
              Row(){
                Checkbox({name:'SHA-1',group:'signatureCheckbox'}).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=>this.checkboxChange(isCheck,'SHA-1'))
                Text('SHA-1')
              }.width('33%')
              Row(){
                Checkbox({name:'SHA-256',group:'signatureCheckbox'}).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=>this.checkboxChange(isCheck,'SHA-256'))
                Text('SHA-256')
              }.width('33%')
              Row(){
                Checkbox({name:'SHA-512',group:'signatureCheckbox'}).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=>this.checkboxChange(isCheck,'SHA-512'))
                Text('SHA-512')
              }.width('33%')
            }.width("50%")
          }
        }.alignItems(HorizontalAlign.Start)
      }
    }
    .width('100%')
    .height('100%')
    .padding(20)
  }
}
