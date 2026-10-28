@Preview
@Component
export struct TextInputSelect {
  /**
   * 下拉选项内容
   */
  public selectOptions?: Array<SelectOption> = []
  /**
   * 组件高度
   */
  public moduleHeight?: Length = 40
  /**
   * 文本框显示的文本
   */
  @Prop text?: ResourceStr
  /**
   * 文本框字体大小
   */
  public textFontSize?: Length = 18
  /**
   * 文本框字体颜色
   */
  public textFontColor?: ResourceColor = Color.Black
  /**
   * 文本框背景颜色
   */
  public textBackgroundColor?: ResourceColor = Color.Transparent

  /**
   * 右侧下拉图片
   */
  public dropDownImage?: string | PixelMap | Resource = $r('app.media.ic_expand_more_black')
  /**
   * 右侧下拉图片高度
   */
  public dropDownImageHeight?: Length = 30
  /**
   * 右侧下拉图片宽度
   */
  public dropDownImageWidth?: Length = 30
  /**
   * 右侧下拉图片背景颜色
   */
  public dropDownImageBackgroundColor?: ResourceColor = Color.Transparent
  /**
   * 右侧下拉图片 Margin
   */
  public dropDownImageMargin?: Margin | Length = {right:4}
  /**
   * 输入框背景颜色
   */
  public textInputBackgroundColor?: ResourceColor = Color.Transparent
  /**
   * 输入框 Margin
   */
  public textInputMargin?: Margin | Length = {right:34}
  /**
   * 输入框文本字体大小
   */
  public textInputFontSize?: Length = 18
  /**
   * 输入框文本字体颜色
   */
  public textInputFontColor?: ResourceColor = Color.Black
  /**
   * 输入框的提示文本
   */
  public textInputPlaceholder?: ResourceStr = ''
  /**
   * 输入内容发生变化时，触发该回调
   */
  public textInputOnChange: ((value: ESObject) => void )| null = null
  /**
   * 输入框失焦时，触发该回调
   */
  public textInputOnBurl: (() => void) | null = null
  /**
   * 下拉框文本字体大小
   */
  public dropDownFontSize?: Length = 16
  /**
   * 下拉框文本字体颜色
   */
  public dropDownFontColor?: ResourceColor = Color.Black
  /**
   * 下拉选择框 Margin
   */
  public selectModuleMargin?: Margin | Length = {left:30}
  /**
   * 下拉弹出框宽度
   */
  public SelectPopupWidth?:string | number = '80%'

  /**
   * 输入类型
   */
  public selectInputType?:InputType

  /**
   * 输入框里显示的文本
   */
  @Prop inputText: string
  /**
   * Popup弹窗，设置参数show为true弹出弹框
   */
  @State customDropDownPopup: boolean = false

  // popup构造器定义弹框内容
  @Builder popupBuilder() {
    Row({ space: 2 }) {
      List({space:10}){
        ForEach(this.selectOptions, (item: SelectOption)=>{
          ListItem(){
            Text(item.value)
              .width('100%')
              .fontSize(this.dropDownFontSize)
              .fontColor(this.dropDownFontColor)
              .padding({top: 4, bottom: 4})
              .onClick(()=>{
                this.inputText = item.value.toString()
                console.log('TextInputSelect-this.inputText= '+this.inputText)
                this.customDropDownPopup = false
              }).margin({left:15, right:15, top:4, bottom:4})
          }
        })
      }
      .padding(5)
      .listDirection(Axis.Vertical) // 排列方向
      .divider({strokeWidth: 1, color: 0xCBCBCB}) // 每行之间的分界线
      .edgeEffect(EdgeEffect.Spring) // 滑动到边缘无效果
    }
    .width(this.SelectPopupWidth)
  }

  /**
   * 组件生命周期，即一般用@Component装饰的自定义组件的生命周期
   */
  aboutToAppear() {
    //组件即将出现时回调该接口，具体时机为在创建自定义组件的新实例后，在执行其build()函数之前执行
  }

  build() {
    Column(){
      Stack(){
        // Text(this.inputText.length>0?this.inputText:'')
        //   .fontSize(this.textFontSize)
        //   .fontColor(this.textFontColor)
        //   .backgroundColor(this.textBackgroundColor)
        Stack(){
          TextInput({text: $$this.inputText, placeholder: this.textInputPlaceholder})
            .margin(this.textInputMargin)
            .backgroundColor(this.textInputBackgroundColor)
            .onChange(this.textInputOnChange)
            .onBlur(this.textInputOnBurl)
            .type(this.selectInputType)
            .fontSize(this.textInputFontSize)
            .padding({left:10,top:2,bottom:2,right:5})
              // .inputFilter('a', errValue =>{
              //   console.log('TextInputSelect-errValue= '+errValue)
              // })
          Image(this.dropDownImage)
            .height(this.dropDownImageHeight)
            .width(this.dropDownImageWidth)
            .margin(this.dropDownImageMargin)
            .backgroundColor(this.dropDownImageBackgroundColor)
            .onClick(()=>{
              console.log('TextInputSelect-点击了下拉箭头')
              this.customDropDownPopup = !this.customDropDownPopup
            })
        }
        .bindPopup(this.customDropDownPopup,{
          builder: ()=>this.popupBuilder(), //提示气泡内容的构造器
          placement: Placement.Bottom, //气泡组件优先显示的位置，当前位置显示不下时，会自动调整位置
          popupColor: '#ECECEC', //提示气泡的颜色
          enableArrow: false, //是否显示箭头
          radius:0,
          showInSubWindow: false, //是否在子窗口显示气泡
          arrowPointPosition:ArrowPointPosition.START,
          onStateChange:(e)=>{
            if (!e.isVisible) {
              this.customDropDownPopup = false
            }
          }
        })
        .margin(this.selectModuleMargin)
        .height('100%')
        .width('100%')
        .border({width:1, color:'#cbcbcb', radius:8})
        .alignContent(Alignment.End)
      }.width('100%')
      .height(this.moduleHeight)
      .alignContent(Alignment.Start)
    }
  }
}
