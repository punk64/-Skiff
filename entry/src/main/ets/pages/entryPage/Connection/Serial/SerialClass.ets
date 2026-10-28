import { BusinessError, serialManager } from "@kit.BasicServicesKit";
import { JSON, util } from "@kit.ArkTS";
import { ComponentContent, promptAction } from "@kit.ArkUI";
import { taskpool } from '@kit.ArkTS';

interface SerialDeviceBuilderInterface {
  serialDeviceList:serialManager.SerialPort[],selectSerialDeviceId:number,confirm:(serialId:number)=>void,close:()=>void,ctx?:UIContext
}
@Builder
function SerialDeviceSelectDialog(args:SerialDeviceBuilderInterface) {
  Column({space:5}){
    Text($r('app.string.Serial_select_device'))
      .fontSize(18)
      .fontWeight(500)
      .height(60)
    List(){
      ForEach(args.serialDeviceList,(item:serialManager.SerialPort,index:number)=>{
       ListItem(){
         Row({space:10}){
           Text($r('app.string.Serial_device',index+1)).width(80)
           Text(item.deviceName).width('calc(100% - 130vp)')
           Radio({
             value: item.deviceName,
             group: "SerialDevice"
           })
             .checked(args.selectSerialDeviceId === item.portId)
             .onChange((isChecked:boolean)=>{
             isChecked && (args.selectSerialDeviceId = item.portId)
           })
         }
         .width('100%')
       }
       .border({
         width: { top: 1, right: 0, bottom: 0, left: 0 }, // 仅底部有宽度
         color: { bottom: Color.Transparent, top: $r('app.color.serial_dividing') }
       })
      },(item:serialManager.SerialPort,index:number)=>JSON.stringify(item))
    }
    .listDirection(Axis.Vertical) // 排列方向
    .padding(10)
    .width('auto')
    .height('calc(100% - 131vp)')
    Column().width('100%').height(1).backgroundColor($r('app.color.serial_dividing'))
    Row({space:40}){
      Button($r('app.string.confirm')).onClick(()=>{
        if(args.selectSerialDeviceId<=-1){
          args.ctx?.getPromptAction().showToast({
            message:$r('app.string.select_derial_device_warn'),
            textColor:$r('app.color.warning_textColor'),
            duration:4000
          })
          return;
        }
        args.confirm(args.selectSerialDeviceId)
      })
      Button($r('app.string.cancel')).onClick(()=>{
        args.close()
      })
    }
    .height('50vp')
  }
  .borderRadius(16)
  .backgroundColor($r('app.color.titleCar_color'))
  .width(500)
  .height(320)
}
export class PromptActionClass {
  static ctx: UIContext;
  static contentNode: ComponentContent<Object>;
  static options: promptAction.BaseDialogOptions;

  static setContext(context: UIContext) {
    PromptActionClass.ctx = context;
  }

  static setContentNode(node: ComponentContent<Object>) {
    PromptActionClass.contentNode = node;
  }

  static setOptions(options: promptAction.BaseDialogOptions) {
    PromptActionClass.options = options;
  }

  static openDialog() {
    if (PromptActionClass.contentNode !== null) {
      PromptActionClass.ctx.getPromptAction()
        .openCustomDialog(PromptActionClass.contentNode, PromptActionClass.options)
        .then(() => {
          console.info('OpenCustomDialog complete.');
        })
        .catch((error: BusinessError) => {
          let message = (error as BusinessError).message;
          let code = (error as BusinessError).code;
          console.error(`OpenCustomDialog args error code is ${code}, message is ${message}`);
        })
    }
  }

  static closeDialog() {
    if (PromptActionClass.contentNode !== null) {
      PromptActionClass.ctx.getPromptAction()
        .closeCustomDialog(PromptActionClass.contentNode)
        .then(() => {
          console.info('CloseCustomDialog complete.');
        })
        .catch((error: BusinessError) => {
          let message = (error as BusinessError).message;
          let code = (error as BusinessError).code;
          console.error(`CloseCustomDialog args error code is ${code}, message is ${message}`);
        })
    }
  }
}

// 串口读取任务函数
@Concurrent
async function serialReadTask(portId: number, timeOut: number,speed:number,dataBits:number): Promise<string | null> {
  if(canIUse('SystemCapability.USB.USBManager.Serial')){
    const serialBuffer = new Uint8Array(1024)
    try{
      const readLength = await serialManager.read(portId, serialBuffer, timeOut)
      console.log('dyx task线程中读取串口信息长度', readLength)
      if(readLength > 0){
        // 根据实际读取长度截取缓冲区，避免读取未初始化的数据
        const actualBuffer = serialBuffer.slice(0, readLength)

        // 将原始字节数组返回，让主线程进行解码，避免子线程中TextDecoder乱码问题
        return Array.from(actualBuffer).join(',') // 将字节数组转换为字符串格式传递
      }else{
        return null
      }
    }catch (e) {
      return null
    }
  }
  return null
}

export default class SerialClass{
  public serialId:number = -1; // 串口设备id
  private intervalIntervalKey:number = -1; //轮询定时器key
  private ctx:UIContext
  private textDecoder:util.TextDecoder
  private task: taskpool.Task | null = null; // taskpool任务

  constructor(ctx: UIContext,readEncoding:string) {
    this.ctx = ctx
    PromptActionClass.setContext(ctx)
    this.textDecoder = util.TextDecoder.create(readEncoding)
  }

  // 修改serialId
  setSerialId = (serialId:number) => {
    this.serialId = serialId
  }

  /**
   * 识别当前连接的串口设备
   */
  getSerialDevice = async ():Promise<string>=>{
    return new Promise((resolve,reject)=>{
      if(canIUse('SystemCapability.USB.USBManager.Serial')) {
       try {
         // 获取当前所有串口设备列表
         const serialDeviceList: serialManager.SerialPort[] = serialManager.getPortList()
         console.log('dyx 所有串口设备列表', JSON.stringify(serialDeviceList))
         // 串口设备判空
         if (serialDeviceList === undefined || serialDeviceList.length === 0) {
           reject('未识别当前有已连接的串口设备，请检查后重试！')
         }
         // this.serialId = serialDeviceList[0].portId
         // 打开串口选择弹窗
         PromptActionClass.setContentNode(
           new ComponentContent(
             this.ctx,
             wrapBuilder(SerialDeviceSelectDialog),
             {
               serialDeviceList:serialDeviceList,
               selectSerialDeviceId:-1,
               confirm:(serialId:number)=>{
                 console.log('dyx 点击了确认按钮',serialId)
                 PromptActionClass.closeDialog()
                 this.serialId = serialId
                 resolve('打开成功')
               },
               close:()=>{
                 console.log('dyx 点击了取消按钮')
                 PromptActionClass.closeDialog()
               },
               ctx:this.ctx} as SerialDeviceBuilderInterface
           )
         );
         PromptActionClass.setOptions({autoCancel:false})
         PromptActionClass.openDialog();
         // resolve(`串口设备打开成功，设备名为：${serialDeviceList[0].deviceName}`)

       } catch (e) {
         console.error('dyx 串口列表获取失败',e)
         reject('获取串口列表失败：'+e)
       }
    }else{
        reject('当前设备无串口能力，请更换设备重试！')
      }
    })

  }

  /**
   * 校验当前连接权限
   */
  async serialDefault(portId:number=this.serialId):Promise<boolean>{
    try {
      if(canIUse('SystemCapability.USB.USBManager.Serial')){
        // 检测设备是否可被应用访问
        if (!serialManager.hasSerialRight(portId)) {
          return await serialManager.requestSerialRight(portId)
        }else{
          return true
        }
      }
    }catch (e) {
      console.error('dyx 授权失败',e)
    }
    return false
  }

  /**
   * 打开并设置串口设备
   */
  openSerialDevice = (attribute:serialManager.SerialAttribute):boolean => {
    // 打开串口设备
    if(canIUse('SystemCapability.USB.USBManager.Serial')){
      try {
        serialManager.open(this.serialId)
        console.log('dyx 打开串口设备成功')
        serialManager.setAttribute(this.serialId,attribute)
        console.log('dyx 串口设置成功')
        return true
      }catch (e) {
        console.error('dyx 打开串口设备失败',e)
      }
    }
    return false;
  }

  /**
   * 同步读取串口设备返回的信息
   * @param portId
   * @param timeOut
   * @returns
   */
  readSerial(portId:number=this.serialId,timeOut:number=500):string | null{
    if(canIUse('SystemCapability.USB.USBManager.Serial')){
      const serialBuffer = new Uint8Array(1024)
      try{
        const readLength = serialManager.readSync(portId,serialBuffer,timeOut)
        console.log('dyx 读取串口信息长度',readLength)
        if(readLength > 0){
          // 根据实际读取长度截取缓冲区，避免读取未初始化的数据
          const actualBuffer = serialBuffer.slice(0, readLength)
          // 解码
          let result = this.textDecoder.decodeToString(actualBuffer, { stream: true });
          // 将/n/n替换为/n
          result = result.replace( /\n{2,}/g, '\n');
          console.log('dyx 串口返回的信息',JSON.stringify(result))
          return result
        }else{
          return null
        }
      }catch (e) {
        return null
      }
    }
    return null
  }

  /**
   * 轮询读取串口设备返回信息
   */
  pollingRead(interval: number,speed:number = 9600,dataBits:number = 8, callback: (result: string|null) => void): void{
    clearInterval(this.intervalIntervalKey)

    // 创建串口读取任务
    this.task = new taskpool.Task(serialReadTask, this.serialId, 500,speed,dataBits);
    
    this.intervalIntervalKey = setInterval(async () => {
      try {
        // 在taskpool中执行串口读取任务
        const taskResult: string | null = (await taskpool.execute(this.task)) as string | null;
        
        if(taskResult){
          // 将字节数组字符串转换回Uint8Array并进行解码
          const byteArray = taskResult.split(',').map(byte => parseInt(byte.trim()));
          const actualBuffer = new Uint8Array(byteArray);
          
          // 在主线程中进行解码，避免乱码
          let result = this.textDecoder.decodeToString(actualBuffer, { stream: true });
          // 将/n/n替换为/n
          result = result.replace( /\n{2,}/g, '\n');
          console.log('dyx 主线程中串口解码后的信息', JSON.stringify(result))
          
          callback(result);
        }else{
          callback(null);
        }
      } catch (error) {
        console.error('dyx taskpool执行串口读取任务失败:', error);
        callback(null);
      }
    }, interval);
  }

  /**
   * 停止轮询
   */
  stopPolling = () => {
    clearInterval(this.intervalIntervalKey)
    // 取消taskpool任务
    if (this.task) {
      try {
        taskpool.cancel(this.task);
        console.log('dyx 取消taskpool任务成功');
      } catch (error) {
        console.error('dyx 取消taskpool任务失败:', error);
      }
      this.task = null;
    }
    console.log('dyx 销毁定时器')
  }
  /**
   * 取消串口授权并关闭串口连接
   */
  cancelSerialRight = () =>{
    serialManager.cancelSerialRight(this.serialId)
  }

  /**
   * 同步写串口信息
   * @param portId
   * @param serialBuffer
   * @param timeOut
   * @returns
   */
  writeSerial = (serialBuffer:Uint8Array,timeOut:number = 0):boolean => {
    if(canIUse('SystemCapability.USB.USBManager.Serial')){
      try {
        console.log('dyx 串口this.serialId',this.serialId)
        const writeLength =  serialManager.writeSync(this.serialId,serialBuffer,timeOut)
        console.log('dyx 串口写入成功',writeLength)
        return true
      }catch (e) {
        console.error('dyx 串口写入失败',e)
      }
    }
    return false
  }
}