import { AbilityConstant, UIAbility, Want} from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { BusinessError, emitter } from '@kit.BasicServicesKit';
import { closeProtocol, setLogging } from 'libentry.so';
import { fileIo as fs } from '@kit.CoreFileKit';
import preferences from '@ohos.data.preferences'
import preferenceUtil from '../common/utils/PreferenceUtil';
import { pageDefaultData } from '../pages/entryPage/pageDefaultData';
import { windowSizeUtils } from '../common/utils/WindowSizeUtils';

interface WindowSizeType{
  col:number;
  row:number;
}

let file:fs.File
let windowInfo:WindowSizeType
let localWindowStage:window.WindowStage

export default class shellWindowAbility extends UIAbility {
  private sessionid:number = -1
  private serialId:number = -1
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {

    console.log('shellWindow onCreated')
    emitter.on('EntryOnDestroy',()=>{
      if(this.context){
        try{
          this.context.terminateSelf((err:BusinessError)=>{
            if (err.code) {
              console.error(err.message)
            }
          })
        } catch (err) {
          // 捕获同步的参数错误
          let code = (err as BusinessError).code;
          let message = (err as BusinessError).message;
          console.error(`terminateSelf failed, code is ${code}, message is ${message}`);
        }
      }
    })
    AppStorage.setOrCreate('TerminalType',true)
    if (want.parameters!=undefined) {
      windowInfo=JSON.parse(want.parameters["info"] as string)
      console.log('want 接受到的数据是'+JSON.stringify(windowInfo))
      console.log('want 接受到的数据类型是'+typeof windowInfo)
      this.sessionid=want.parameters["sessionId"] as number
      console.log('want 接受到的数据是  sessionid',this.sessionid)
      let perf= preferences.getPreferences(this.context,'shellWindowWant')
      perf.then((res)=>{
       try {
         res.put('sessionId',this.sessionid)
         console.log('dyx want.parameters?.serialId',want.parameters?.serialId)
         if(want.parameters?.serialId !== undefined){
           res.put('serialId',want.parameters.serialId)
           this.serialId = want.parameters.serialId as number
           const serialList = AppStorage.get('SerialList') as number[] || []
           serialList.push(want.parameters.serialId as number)
           console.log('dyx ability serialList',JSON.stringify(serialList))
           AppStorage.setOrCreate('SerialList',serialList)
           console.log('dyx serialId存储成功')
         }
         console.log('sessionId存储成功')
       }catch (e){
         console.error('sessionId存储错误')
       }
      })
    }

    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onCreate');
    // this.init()
  }

  async onDestroy(): Promise<void> {
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onDestroy');
    // 关闭ssh连接
    AppStorage.set('TerminalType',false)
    // emitter.off('EntryOnDestroy')
    const serialList = AppStorage.get('SerialList') as number[] || []
    AppStorage.setOrCreate('SerialList',serialList.filter(item=>item!=this.serialId))
    emitter.emit('shellLoggingEntryOnDestroy')

    try{
      console.log('sessionId---------->',this.sessionid)
      await closeProtocol(this.sessionid)
    }catch (e){
      console.error('断开连接失败',e)
    }
    // this.stopContinuousTask()
  }
  // 仅在2in1设备上触发
  // onPrepareToTerminate(){
  //   const delConfirm:CustomDialogController = new CustomDialogController({
  //     builder: AlertDialog({
  //       primaryTitle: '系统提示',
  //       content: `关闭配置界面会导致关闭已打开的终端界面，请确认当前操作`,
  //       primaryButton: {
  //         value: '取消',
  //         action: () => {
  //           delConfirm.close();
  //         },
  //         fontColor:'#0A59F7',
  //         background:'#fff'
  //       },
  //       secondaryButton: {
  //         value: '确认关闭',
  //         action: () => {
  //          this.context.terminateSelf()
  //         },
  //         fontColor:'#fff',
  //         background:'#0A59F7'
  //       }
  //     }),
  //   })
  //   delConfirm.open()
  //   // if(AppStorage.get('TerminalType')){
  //   //   delConfirm.open()
  //   // }
  //   return true
  // }


  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    localWindowStage = windowStage
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onWindowStageCreate');
    windowStage.getMainWindow()
      .then((window)=>{
        let col:number=Number(windowInfo.col)
        let row:number=Number(windowInfo.row)
        console.log("获取到的宽是："+ col+"获取到的高是："+row)

        try {
          window.resize(col ,row)
          console.log("设置终端大小成功：")
          console.log("设置终端大小成功："+JSON.stringify(window.getWindowProperties()))
        }catch (e) {
          console.log("设置终端大小失败："+e.code+e.message)
        }
    })

    windowStage.loadContent('pages/shellWindowPage/ShellWindow', (err) => {
      if (err.code) {
        hilog.error(0x0000, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(0x0000, 'testTag', 'Succeeded in loading the content.');
    });
    emitter.on('windowResizedId', () => {
      windowSizeUtils.isForbidResizingCompletely(windowStage)
    })
    windowSizeUtils.isForbidResizingCompletely(windowStage)
  }

  //再次通过startAbility启动该UIAbility时调用
  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability newWant');
    if(localWindowStage){
      if (want.parameters!=undefined) {
        windowInfo=JSON.parse(want.parameters["info"] as string)
        console.log('want 接受到的数据是'+JSON.stringify(windowInfo))
        console.log('want 接受到的数据类型是'+typeof windowInfo)
        localWindowStage.getMainWindow()
          .then((window)=>{
            let col:number=Number(windowInfo.col)
            let row:number=Number(windowInfo.row)
            window.resize(col ,row)
          })
      }
      localWindowStage.loadContent('pages/shellWindowPage/ShellWindow')
    }
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    emitter.off('windowResizedId')
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onBackground');
  }
}

