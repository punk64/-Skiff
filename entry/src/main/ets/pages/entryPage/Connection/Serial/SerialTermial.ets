import { SerialType} from 'ets/common/types/dataType';
import json from '@ohos.util.json';
import { JSON, util } from '@kit.ArkTS';
import { preferences } from '@kit.ArkData';
import SerialClass from './SerialClass';
import { KeyCode } from '@kit.InputKit';
import { Font } from '@kit.ArkUI'

@ObservedV2
class SerialDataItem{
  @Trace type:'in'|'out'
  @Trace message:string
  @Trace date:string

  constructor(type:'in'|'out', message:string,date:string) {
    this.type = type
    this.message = message
    this.date = date
  }

  // 添加方法来更新消息并手动触发通知
  updateMessage(newMessage: string) {
    this.message = newMessage;
  }

  // 添加方法来追加消息
  appendMessage(additionalMessage: string, scrollCallback?: () => void) {
    this.message = this.message + additionalMessage;
    // 立即触发滚动
    if(scrollCallback) scrollCallback();
  }
}

@ObservedV2
class SerialData{
  @Trace serialDataList:SerialDataItem[]
  @Trace isPagingMode:boolean = false // 标记是否处于分页模式

  constructor(data:SerialDataItem[]) {
    this.serialDataList = data
  }

  // 转字符串
  concatenateToString(): string {
    let result = '';
    for (const item of this.serialDataList) {
      result += `${item.date} ${(item.type==='in'?'\uE60D':'\ue614')} ${item.message}\r\n`;
    }
    return result;
  }

/**
 * 清除消息列表
 */
  clearSerialDataList(){
    this.serialDataList = []
  }

}
function getCurrentDateTime() {
  const now = new Date();

  const year = now.getFullYear();
  const month = String(now.getMonth() + 1).padStart(2, '0');
  const day = String(now.getDate()).padStart(2, '0');
  const hours = String(now.getHours()).padStart(2, '0');
  const minutes = String(now.getMinutes()).padStart(2, '0');
  const seconds = String(now.getSeconds()).padStart(2, '0');

  return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
}

@ComponentV2
export default struct SerialTerminal {
  @Param @Require SerialDataSource: SerialType; // 所有配置的数据源
  // 串口端口id
  @Local intervalKey:number = -1
  @Local inputValue:string = ''
  @Local serial :SerialClass|undefined = undefined
  @Local serialIndex:number=-1
  @Local SerialData:SerialData = new SerialData([])
  private listScroller:Scroller = new Scroller()
  private isShiftPressed: boolean = false; //标志位，用于跟踪Shift键的状态
  private isAltPressed: boolean = false; //标志位，用于跟踪Alt键的状态
  aboutToAppear() {
    this.initConfig();
    let font: Font = this.getUIContext().getFont();
    font.registerFont({
      familyName: 'iconfont',
      familySrc: '/font/iconfont.ttf'
    })
  }

  initConfig() {
    if (!this.SerialDataSource) {
      return;
    }
    let perf= preferences.getPreferences(this.getUIContext().getHostContext(),'shellWindowWant')
    perf.then( res=>{

      res.get('serialId',undefined).then((result)=>{
        this.serial = new SerialClass(this.getUIContext(),this.SerialDataSource.readEncoding)
        this.serial.setSerialId(new Number(result).valueOf())
        this.serial.pollingRead(100,new Number(this.SerialDataSource.speed).valueOf(),new Number(this.SerialDataSource.dataBits).valueOf(),(message:string|null)=>{
          this.getAndWriteSerial(message)
        })
        console.log('dyx 获取serial成功',this.serial)
      })
    })
  }
  aboutToDisappear(): void {
    this.serial?.stopPolling()
    this.serial?.cancelSerialRight()
  }

  // 发送命令到串口设备
  sendCommand(command: string):void { // 返回 void
    if(this.serial !== undefined){
      if(this.serial.serialId<=-1)return;
      
      // 如果处于分页模式，暂时不发送用户命令
      if(this.SerialData.isPagingMode){
        this.getUIContext().getPromptAction().showToast({
          message:'设备正在显示分页内容，请稍后再试',
          textColor:Color.Orange,
          duration:2000
        })
        return;
      }
      
      console.info("发送命令到串口设备:", this.serial?.serialId,command);
      // if (command === undefined || command === null || command === '') {
      //   const errorMsg = "命令不能为空";
      //   console.error(errorMsg);
      //   return
      // }
      const nowDate = getCurrentDateTime()
      this.SerialData.serialDataList.push(new SerialDataItem('out', command,nowDate))
      this.listScroller.scrollEdge(Edge.Bottom)
      try {
        let textEncoder:util.TextEncoder = new util.TextEncoder(this.SerialDataSource.sendEncoding);
        let buffer:ArrayBuffer = new ArrayBuffer(64);
        let encodeResult:Uint8Array = new Uint8Array(buffer);
        // 编码
        encodeResult = textEncoder.encodeInto(command + '\r\n');
        this.serial.writeSerial(encodeResult)
      } catch (err) {
        const errorMsg = err instanceof Error ? err.message : String(err);
        console.error(`dyx 终端命令处理错误: ${errorMsg}`);
      }
    }
  }

  // 获取串口设备返回的数据，并写入,当返回为null时，则为消息结尾
  getAndWriteSerial=(command:string | null)=>{
    if(typeof command === 'string'){
      // 检测分页提示符（常见的有 "--More--", "---- More ----", "[Press space to continue]" 等）
      const pagingPatterns = /--\s*More\s*--|\[Press space to continue\]|按任意键继续|More\?|\s*\(more\)\s*/i;
      
      if(pagingPatterns.test(command)){
        console.log('dyx 检测到分页提示符，发送空格继续');
        this.SerialData.isPagingMode = true;
        // 自动发送空格继续显示
        this.sendPagingContinue();
        return;
      }
      
      // 当command为string时，表示当前为数据块，拼接所有数据块并更新当前的数据列表
      if(this.SerialData.serialDataList.length === 0 || this.serialIndex >= this.SerialData.serialDataList.length || this.SerialData.serialDataList[this.serialIndex].type !== 'in'){
        // 如果当前没有数据项或当前项不是接收类型，创建新的数据项
        const nowDate = getCurrentDateTime()
        this.SerialData.serialDataList.push(new SerialDataItem('in', command,nowDate))
        this.serialIndex = this.SerialData.serialDataList.length - 1
        // 创建新数据项后立即滚动
        this.listScroller.scrollEdge(Edge.Bottom)
      }else{
        // 拼接数据块到当前数据项，使用专门的方法触发响应式更新
        this.SerialData.serialDataList[this.serialIndex].appendMessage(command, () => {
          // 在每次追加消息时立即滚动
          this.listScroller.scrollEdge(Edge.Bottom)
        });
        console.log('dyx 拼接数据',json.stringify(this.SerialData.serialDataList))
      }
      console.log('dyx serialClass',json.stringify(this.SerialData.serialDataList))
    }else if(command === null){
      // 当command为null时，表示当条数据发送完成，更新当前的数据指针
      console.log('dyx 数据块发送完成')
      // 指针指向下一个位置，准备接收新消息
      this.serialIndex = this.SerialData.serialDataList.length
      // 重置分页模式
      this.SerialData.isPagingMode = false;
    }
    
    console.log('dyx 获取的串口数据', command, '当前指针:', this.serialIndex)
  }

  // 发送分页继续信号（空格）
  private sendPagingContinue = () => {
    setTimeout(() => {
      if(this.serial && this.SerialData.isPagingMode){
        try {
          let textEncoder:util.TextEncoder = new util.TextEncoder('utf-8');
          let buffer:ArrayBuffer = new ArrayBuffer(8);
          let encodeResult:Uint8Array = new Uint8Array(buffer);
          // 发送空格字符继续分页显示
          encodeResult = textEncoder.encodeInto(' ');
          this.serial.writeSerial(encodeResult)
          console.log('dyx 发送分页继续信号');
        } catch (err) {
          const errorMsg = err instanceof Error ? err.message : String(err);
          console.error(`dyx 分页继续信号发送错误: ${errorMsg}`);
        }
      }
    }, 200); // 延迟200ms确保分页提示符处理完成
  }

  build() {
    Column() {
      Scroll(this.listScroller){
        Text(this.SerialData.concatenateToString())
          .textAlign(TextAlign.Start)
          .width('100%')
          .copyOption(CopyOptions.LocalDevice)
          .fontFamily('iconfont')
      }
      .align(Alignment.TopStart)
      .padding({top:10,bottom:10,left:20,right:20})
      .height('calc(100% - 80vp)')
      Stack(){
        Row({space:20}){
          TextArea({text:this.inputValue!!})
            .enterKeyType(EnterKeyType.Send)
            .defaultFocus(true)
            .focusable(true)
            .onSubmit(()=>{
              this.sendCommand(this.inputValue)
              this.inputValue = ''
            })
            .onBlur(()=>{
              this.getUIContext().getFocusController().requestFocus('textAreaId')
            })
            .onKeyEvent((event: KeyEvent) => {
              const keyCode: KeyCode = event.keyCode; //获取按键码
              const action: number = event.type; //获取按键动作（按下或释放）

              //监听Shift键
              if (keyCode === KeyCode.KEYCODE_SHIFT_LEFT || keyCode === KeyCode.KEYCODE_SHIFT_RIGHT) {
                if (action === KeyType.Down) {
                  this.isShiftPressed = true; //Shift键按下
                } else {
                  this.isShiftPressed = false; //Shift键释放
                }
              }

              if (keyCode === KeyCode.KEYCODE_ALT_LEFT || keyCode === KeyCode.KEYCODE_ALT_RIGHT){
                if (action === KeyType.Down) {
                  this.isAltPressed = true; //Alt键按下
                } else {
                  this.isAltPressed = false; //Alt键释放
                }
              }

              //监听Enter键
              if ((keyCode === KeyCode.KEYCODE_ENTER ||
                keyCode === KeyCode.KEYCODE_NUMPAD_ENTER) && action === KeyType.Down) {
                console.log('dyx 触发enter',this.isShiftPressed,this.isAltPressed)
                if (this.isShiftPressed || this.isAltPressed) {
                  // 在这里处理Shift + Enter 组合键的逻辑
                  this.inputValue += '\n'; //添加换行符
                }
              }
            })
            .showCounter(true)
            .maxLength(500)
            .width('calc(100% - 220vp)')
            .id('textAreaId')

          Row({space:10}){
            Button($r('app.string.clear_window')).onClick(()=>{
              this.SerialData.clearSerialDataList()
              this.getUIContext().getPromptAction().showToast({
                message:$r('app.string.clear_success'),
                textColor:Color.Green,
                duration:4000
              })
            })
              .buttonStyle(ButtonStyleMode.NORMAL)
              .border({ width: 1, style: BorderStyle.Solid })
              .borderColor(0xDCDCDC)
              .borderRadius(5)
              .fontColor(0xE84026)
              .border({width:1,radius:10,color:0xDCDCDC})
              .width('100vp')
            Button($r('app.string.send_button')).onClick(()=>{
              this.sendCommand(this.inputValue)
              this.inputValue = ''
            })
              .width('80vp')
          }.width(190)
        }
        .justifyContent(FlexAlign.SpaceAround)
        .padding({left:10,right:10})
        .height('100%')
        .width('100%')
      }
      .border({
        width: { top: 1, right: 0, bottom: 0, left: 0 }, // 仅底部有宽度
        color: { bottom: Color.Transparent, top: $r('app.color.serial_dividing') }
      })
      .height('80vp')
      .position({bottom:0})
    }
    .backgroundColor($r('app.color.serial_message_list_background'))
    .borderRadius(0)
    .width('100%')
    .height('100%')
  }
}
