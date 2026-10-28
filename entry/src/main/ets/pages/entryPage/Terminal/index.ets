import TitleCard from '../../../components/TitleCard'
import { TextInputSelect } from '../../../components/InputSelect/index'
import { distributedKVStore } from '@kit.ArkData'
import { common } from '@kit.AbilityKit'
import {KvManager} from '../../../common/utils/KVManager'
import deepClone from '../../../common/tools/deepClone'
import {TerminalType} from '../../../common/types/dataType'
import { pageDefaultData } from '../pageDefaultData'

@Component
export default struct Terminal{

  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData:string = ''
  @State @Watch('watchNowDataChange') nowData:TerminalType = pageDefaultData.Terminal
  private options: Array<SelectOption> = [{value:'None (printing disabled)'},{value:'导出为WPS PDF'},{value:'OneNote (Desktop)'},{value:'Microsoft Print to PDF'}]
  private context = getContext(this) as common.UIAbilityContext
  // KVManager实例
  @State KVManager:KvManager = new KvManager({
    context: getContext(this),
    bundleName:getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id),//您自己的App对应的包名
  })
  // 所有数据的Store实例
  private allDataKvStore:distributedKVStore.SingleKVStore | undefined =undefined

  @Styles
  inputTextStyle(){
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }

  async saveSessionChange(){
    try {
      console.log('saveSession--------->',JSON.stringify(JSON.parse(this.defaultData).Terminal))
      await this.getData()
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  aboutToAppear(): void {
    this.init()
  }

  async getData(){
    this.nowData = deepClone(JSON.parse(this.defaultData).Terminal as TerminalType)
  }

  // 初始化
  async init(){
    this.allDataKvStore = await this.KVManager.getStore('allData')
    await this.getData()
    this.saveStore()
  }
  async  saveStore(){
    this.allDataKvStore && await this.allDataKvStore.put('Terminal',JSON.stringify(this.nowData))
  }

  // 监听 nowData 改变时触发
  watchNowDataChange(){
    this.saveStore()
  }
  checkChange(isCheck:boolean,type:string){
    if(isCheck){
      if(this.nowData.variousCheck.indexOf(type) < 0){
        // 不存在于variousCheck数组中时
        this.nowData.variousCheck=[...this.nowData.variousCheck,type]
      }
    }else{
      this.nowData.variousCheck = this.nowData.variousCheck.filter((item)=>item !== type)
    }
  }
  echoRadio(type:string){
    this.nowData.echo = type
  }
  editingRadio(type:string){
    this.nowData.editing = type
  }
  build() {
    Column({space:20}){
      TitleCard({titleString:$r('app.string.Set_various_terminal_options')}){
        Column({space:5}){
          // Row({space:5}){
          //   Checkbox({name:'1',group:'variousTerminalCheckbox'}).select(this.nowData.variousCheck.indexOf('1') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean):void=>this.checkChange(isCheck,'1'))
          //   Text($r('app.string.wrap_mode'))
          // }
          // Row({space:5}){
          //   Checkbox({name:'2',group:'variousTerminalCheckbox'}).select(this.nowData.variousCheck.indexOf('2') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean):void=>this.checkChange(isCheck,'2'))
          //   Text('DEC Origin Mode initially on')
          // }
          // Row({space:5}){
          //   Checkbox({name:'3',group:'variousTerminalCheckbox'}).select(this.nowData.variousCheck.indexOf('3') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean):void=>this.checkChange(isCheck,'3'))
          //   Text($r('app.string.Implicit_CR_in_every_LF'))
          // }
          Row({space:5}){
            Checkbox({name:'4',group:'variousTerminalCheckbox'}).select(this.nowData.variousCheck.indexOf('4') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean):void=>this.checkChange(isCheck,'4'))
            Text($r('app.string.Implicit_LF_in_every_CR'))
          }
          // Row({space:5}){
          //   Checkbox({name:'5',group:'variousTerminalCheckbox'}).select(this.nowData.variousCheck.indexOf('5') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean):void=>this.checkChange(isCheck,'5'))
          //   Text('Use background colour to erase screen')
          // }
          // Row({space:5}){
          //   Checkbox({name:'6',group:'variousTerminalCheckbox'}).select(this.nowData.variousCheck.indexOf('6') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean):void=>this.checkChange(isCheck,'6'))
          //   Text('Enable blinking text')
          // }
          // Column({space:5}){
          //   Text('Answerback to ^E:')
          //   TextInput({text:$$this.nowData.answerBackInput}).inputTextStyle()
          // }.alignItems(HorizontalAlign.Start)
        }.alignItems(HorizontalAlign.Start)
      }
      // TitleCard({titleString:'Set various terminal options'}){
      //   Column(){
      //     Text('Local echo:')
      //     Flex({justifyContent:FlexAlign.SpaceBetween}){
      //       Row({space:5}){
      //         Radio({value:'auto',group:'echoRadio'}).checked(this.nowData.echo === 'auto').onClick(()=>this.echoRadio('auto'))
      //         Text('Auto')
      //       }
      //       Row({space:5}){
      //         Radio({value:'forceOn',group:'echoRadio'}).checked(this.nowData.echo === 'forceOn').onClick(()=>this.echoRadio('forceOn'))
      //         Text('Force On')
      //       }
      //       Row({space:5}){
      //         Radio({value:'forceOff',group:'echoRadio'}).checked(this.nowData.echo === 'forceOff').onClick(()=>this.echoRadio('forceOff'))
      //         Text('Force Off')
      //       }
      //     }
      //     Text('Local line editing:')
      //     Flex({justifyContent:FlexAlign.SpaceBetween}){
      //       Row({space:5}){
      //         Radio({value:'auto',group:'editingRadio'}).checked(this.nowData.editing === 'auto').onClick(()=>this.editingRadio('auto'))
      //         Text('Auto')
      //       }
      //       Row({space:5}){
      //         Radio({value:'forceOn',group:'editingRadio'}).checked(this.nowData.editing === 'forceOn').onClick(()=>this.editingRadio('forceOn'))
      //         Text('Force On')
      //       }
      //       Row({space:5}){
      //         Radio({value:'forceOff',group:'editingRadio'}).checked(this.nowData.editing === 'forceOff').onClick(()=>this.editingRadio('forceOff'))
      //         Text('Force Off')
      //       }
      //     }
      //   }.alignItems(HorizontalAlign.Start)
      // }
      // TitleCard({titleString:'Remote-controlled printing'}){
      //   Column({space:5}){
      //     Text('Printer to send ANSI printer output to:')
      //     TextInputSelect({
      //       text:this.nowData.remoteControlledInput,
      //       selectOptions:this.options,
      //       textInputOnChange:(inputValue:String)=>{this.nowData.remoteControlledInput = inputValue as string},
      //       inputText:this.nowData.remoteControlledInput,
      //       selectModuleMargin:0,
      //       SelectPopupWidth:'55%',
      //     })
      //   }.alignItems(HorizontalAlign.Start)
      // }
    }.alignItems(HorizontalAlign.Start)
  }
}