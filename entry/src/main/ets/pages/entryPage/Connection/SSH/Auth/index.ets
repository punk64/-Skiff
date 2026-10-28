import TitleCard from "../../../../../components/TitleCard"
interface dataObj{
  authentication:string[],
  authenticationMethods:string[],
  authenticationOptions:string[]
}

@Component

export default struct Auth{
  @State nowData:dataObj = {authentication:['2'],authenticationMethods:['3'],authenticationOptions:['1']}

  checkChangeAuthentication(isCheck:boolean,type:string){
    if(isCheck){
      this.nowData.authentication=[...this.nowData.authentication,type]
    }else{
      this.nowData.authentication = this.nowData.authentication.filter((item)=>item !== type)
    }
  }
  checkChangeAuthenticationMethods(isCheck:boolean,type:string){
    if(isCheck){
      this.nowData.authenticationMethods=[...this.nowData.authenticationMethods,type]
    }else{
      this.nowData.authenticationMethods = this.nowData.authenticationMethods.filter((item)=>item !== type)
    }
  }
  checkChangeAuthenticationOptions(isCheck:boolean,type:string){
  if(isCheck){
    this.nowData.authenticationOptions=[...this.nowData.authenticationOptions,type]
  }else{
    this.nowData.authenticationOptions = this.nowData.authenticationOptions.filter((item)=>item !== type)
  }
}

  build() {
    Column({space:20}){
      TitleCard(){
        Column(){
          Row(){
            Checkbox({name:'1',group:'authenticationCheckbox'}).select(this.nowData.authentication.indexOf('1')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthentication(check,'1'))
            Text('Display pre-authentication banner (SSH-2 only)')
          }
          Row(){
            Checkbox({name:'2',group:'authenticationCheckbox'}).select(this.nowData.authentication.indexOf('2')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthentication(check,'2'))
            Text('Bypass authentication entirely (SSH-2 only)')
          }
          Row(){
            Checkbox({name:'3',group:'authenticationCheckbox'}).select(this.nowData.authentication.indexOf('3')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthentication(check,'3'))
            Text('Disconnect if authentication succeeds trivially')
          }
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Authentication methods'}){
        Column(){
          Row(){
            Checkbox({name:'1',group:'authenticationMethodsCheckbox'}).select(this.nowData.authenticationMethods.indexOf('1')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthenticationMethods(check,'1'))
            Text('Attempt authentication using Pageant')
          }
          Row(){
            Checkbox({name:'2',group:'authenticationMethodsCheckbox'}).select(this.nowData.authenticationMethods.indexOf('2')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthenticationMethods(check,'2'))
            Text('Attempt TIS or CryptoCard auth (SSH-1)')
          }
          Row(){
            Checkbox({name:'3',group:'authenticationMethodsCheckbox'}).select(this.nowData.authenticationMethods.indexOf('3')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthenticationMethods(check,'3'))
            Text('Attempt "keyboard-interactive" auth (SSH-2)')
          }
        }.alignItems(HorizontalAlign.Start)
      }
      TitleCard({titleString:'Other authentication-related options'}){
        Column(){
          Row(){
            Checkbox({name:'1',group:'authenticationOptionCheckbox'}).select(this.nowData.authenticationOptions.indexOf('1')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthenticationOptions(check,'1'))
            Text('Allow agent forwarding')
          }
          Row(){
            Checkbox({name:'2',group:'authenticationOptionCheckbox'}).select(this.nowData.authenticationOptions.indexOf('2')>=0).shape(CheckBoxShape.ROUNDED_SQUARE).onChange((check:boolean) => this.checkChangeAuthenticationOptions(check,'2'))
            Text('Allow attempted changes of username in SSH-2')
          }
        }.alignItems(HorizontalAlign.Start)
      }
    }.alignItems(HorizontalAlign.Start)
  }
}