import { openDialogType } from "../../pages/shellWindowPage/ShellWindow"

@CustomDialog
@Component
export default struct InputCustomDialog{
  @State inputValue: string = ''
  controller?: CustomDialogController
  // 若尝试在CustomDialog中传入多个其他的Controller，以实现在CustomDialog中打开另一个或另一些CustomDialog，那么此处需要将指向自己的controller放在所有controller的后面

  public confirm: (input:string) => void = () => {
  }

  build() {
    Column() {
      TextInput({ placeholder: '', text: $$this.inputValue })
        .height(40).width('90%')
        .defaultFocus(true)
        .enableKeyboardOnFocus(true)
        .key('ADD_TASK')
        .onSubmit((enterKeyType)=>{
          if(enterKeyType && this.controller != undefined){
            this.controller.close()
            this.confirm(this.inputValue)
          }
        })
    }
    .borderRadius(20)
    .padding({top:15,bottom:15})
  }
}
