import TitleCard from '../../../../../components/TitleCard'

interface objData{
  forwarding:boolean,
  location:string,
  protocol:string,
  browseInput:string,
}
@Component
export default struct X11{
  @State nowData:objData = {
    forwarding:false,
    location:'',
    protocol:'',
    browseInput:''
  }
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
  // 拉取文件选择窗口
  browse(){

  }
  build() {
    Column(){
      TitleCard({titleString:'X11 forwarding'}){
        Column({space:10}){
          Row({space:5}){
            Checkbox({name:'1',group:'forwarding'}).select(this.nowData.forwarding).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.nowData.forwarding = check)
            Text('Enable X11 forwarding')
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            Text('X display location').width('40%')
            TextInput({text:$$this.nowData.location}).inputTextStyle().width('40%')
          }
          Text('Remote X11 authentication protocol')
          Row({space:20}){
            Row({space:5}){
              Radio({value:'cookie',group:'protocol'})
                .checked(this.nowData.protocol === 'cookie')
                .onChange((check:boolean)=>{
                  check&&(this.nowData.protocol = 'cookie')
                })
              Text('MIT-Magic-Cookie-1')
            }
            Row({space:5}){
              Radio({value:'authorization',group:'protocol'})
                .checked(this.nowData.protocol === 'authorization')
                .onChange((check:boolean)=>{
                  check&&(this.nowData.protocol = 'authorization')
                })
              Text('XDM-Authorization-1')
            }
          }
          Flex({justifyContent:FlexAlign.SpaceBetween}){
            TextInput({text:$$this.nowData.browseInput}).inputTextStyle().width('80%')
            Button('Browse',{type:ButtonType.Normal}).buttonStyle(ButtonStyleMode.NORMAL).ButtonStyles().fontColor(Color.Black).fontSize(14).fontWeight(400).onClick(()=>this.browse())
          }
        }.alignItems(HorizontalAlign.Start)
      }
    }
  }
}
