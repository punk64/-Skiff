import { window } from "@kit.ArkUI"
import { BusinessError } from "@kit.BasicServicesKit"

export  class SubWindowClass{
  subWindowClass:window.Window|null = null
  windowName:string =''
  windowWidth:number = 0
  windowHeight:number = 0
  positionX:number = 0
  positionY:number = 0
  windowPath:string =''

  constructor(subWindowClass:window.Window|null,windowName:string,windowWidth:number,
    windowHeight:number,positionX:number,positionY:number ,windowPath:string) {
    this.subWindowClass = subWindowClass
    this.windowName = windowName
    this.windowWidth = windowWidth
    this.windowHeight = windowHeight
    this.positionX = positionX
    this.positionY = positionY
    this.windowPath = windowPath
  }
}
export  function  createSubWindow(sub:SubWindowClass){
  //1.获取主窗口windowStage 2.创建子窗口 3.设置子窗口属性 4.加载子窗口内容 5.显示子窗口内容
  //获取主窗口windowStage
  let windowStage_:window.WindowStage|null|undefined = null
  windowStage_ = AppStorage.get('windowStage')
  if (windowStage_ == null) {
    console.error('Failed to create the subwindow. Cause the windowStage is null')
  }
  //创建子窗口
  windowStage_?.createSubWindow(sub.windowName,(err:BusinessError,data) => {
    let errCode = err.code
    if (errCode) {
      console.error('Failed to create sub window. Cause:' + JSON.stringify(err))
      return
    }
    sub.subWindowClass= data
    //设置子窗口属性：大小，位置等
    //设置子窗口位置
    sub.subWindowClass.moveWindowTo(sub.positionX,sub.positionY,(err:BusinessError,data) => {
      if (err.code) {
        console.error('Failed to move the window.Cause:' + JSON.stringify(err))
        return
      }
      console.info('succeded in moving the sub window')
    })
    // 设置子窗口大小
    sub.subWindowClass.resize(sub.windowWidth,sub.windowHeight,(err:BusinessError,data) => {
      if (err.code) {
        console.error('Failed to set the sub window size.Cause:' + JSON.stringify(err))
        return
      }
      console.info('Succeded in setting the sub window size')
    })
    //设置子窗口是否可触
    sub.subWindowClass.setWindowTouchable(true,(err:BusinessError,data) => {
      if (err.code) {
        console.error('set window touch Failed' + JSON.stringify(err))
        return
      }
      console.info('set window touch succeeded')
    })
    // 设置子窗口是否全屏(能够设置的前提是未设置子窗口的位置和大小)
    sub.subWindowClass.setWindowLayoutFullScreen(false)
    //加载子窗口页面
    sub.subWindowClass.setUIContent(sub.windowPath,(err:BusinessError,data) => {
      if (err.code) {
        console.error('set sub ui content Failed. Cause:' + JSON.stringify(err))
        return
      }
      console.info('set sub window ui content succeeded')
    })
    //显示子窗口内容
    sub.subWindowClass.showWindow((err:BusinessError,data) => {
      if (err.code) {
        console.error('show sub window Failed.Cause:' + JSON.stringify(err))
        return
      }
      console.info('show sub window succeeded')
    })
  })
}