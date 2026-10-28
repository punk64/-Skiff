import TitleCard from '../../../../components/TitleCard'
interface dataObj{
  remote:string,
  protocol:string[],
  protocolVersion:string,
  sharing:string,
  roles:string[]
}

@Component
export default struct DSSH{
  @State nowData:dataObj = {remote:'',protocol:[],protocolVersion:'2',sharing:'1',roles:['1']}
  @Styles inputTextStyle(){
    .borderRadius(0)
    .padding(2)
    .border({width:1,style:BorderStyle.Solid})
    .backgroundColor(Color.Transparent)
  }
  protocolCheckChange(isCheck:boolean,type:string){
    if(isCheck){
      this.nowData.protocol=[...this.nowData.protocol,type]
    }else{
      this.nowData.protocol = this.nowData.protocol.filter((item)=>item !== type)
    }
  }

  roleCheckChange(isCheck:boolean,type:string){
    if(isCheck){
      this.nowData.roles=[...this.nowData.roles,type]
    }else{
      this.nowData.roles = this.nowData.roles.filter((item)=>item !== type)
    }
  }
  build() {
    Column({space:20}){
      TitleCard({titleString:'Data to send to the server'}){
        Column({space:5}){
          Text('Remote command:')
          TextInput({text:$$this.nowData.remote}).inputTextStyle()
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Protocol options'}){
        Column({space:5}){
          Row(){
            Checkbox({name:'1',group:'protocolCheckbox'}).select(this.nowData.protocol.indexOf('1') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=>this.protocolCheckChange(isCheck,'1'))
            Text('Don\'t start a shell or command at all')
          }
          Row(){
            Checkbox({name:'2',group:'protocolCheckbox'}).select(this.nowData.protocol.indexOf('2') >=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=>this.protocolCheckChange(isCheck,'2'))
            Text('Enable compression')
          }
          Text('SSH protocol version:')
          Row(){
            Row(){
              Radio({value:'2',group:'versionRadio'}).checked(this.nowData.protocolVersion === '2').onChange(()=>this.nowData.protocolVersion = '2')
              Text('2')
            }.width('50%')
            Row(){
              Radio({value:'1',group:'versionRadio'}).checked(this.nowData.protocolVersion === '1').onChange(()=>this.nowData.protocolVersion = '1')
              Text('1(INSECURE)')
            }.width('50%')
          }
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Sharing an SSH connection between CrossShellNext tools'}) {
        Column(){
          Row(){
            Checkbox({name:'1',group:'sharingCheckbox'}).select(this.nowData.sharing === '1').shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=> {
              this.nowData.sharing = isCheck ? '1' : ''
            })
            Text('Share SSH connections if possible')
          }
          Text('Permitted roles in a shared connection:')
          Row(){
            Checkbox({name:'1',group:'rolesCheckbox'}).select(this.nowData.roles.indexOf('1') >= 0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=> this.roleCheckChange(isCheck,'1'))
            Text('Upstream (connecting to the real server)')
          }
          Row(){
            Checkbox({name:'2',group:'rolesCheckbox'}).select(this.nowData.roles.indexOf('2') >= 0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((isCheck:boolean)=> this.roleCheckChange(isCheck,'2'))
            Text('Downstream (connecting to the upstream CrossShellNext)')
          }
        }.alignItems(HorizontalAlign.Start)
      }
    }.alignItems(HorizontalAlign.Start)
  }
}