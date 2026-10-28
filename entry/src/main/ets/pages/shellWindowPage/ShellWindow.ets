import { distributedKVStore, preferences, unifiedDataChannel } from '@kit.ArkData';
import { common} from '@kit.AbilityKit';
import {
  allDataType,
  CharacterType,
  ColorType,
  ConnectionType,
  CursorType,
  LoggingType,
  ProxyType,
  SelectType,
  SerialType,
  SessionType,
  TelnetType,
  TunnelsType,
  TerminalType,
  WindowSizeType,
  KeyGenType,
  AppearType,
  CloseWindowMode} from '../../common/types/dataType';
import { KvManager } from '../../common/utils/KVManager';
import Terminal from '../../components/Terminal';
import SerialTerminal from 'ets/pages/entryPage/Connection/Serial/SerialTermial'
import FileList, { fileNodeType } from '../../components/FileList';
import {listDirectory,deleteDirectory, deleteFile,rename,
  createDirectory,uploadFile,
  downloadFile,
  initSftp as sftp,
  UploadProgressItem,
  cancelTransfer,
  DownloadProgressItem,
  listActivePortForwardings,
  stopPortForwarding,
  PortForwardingInfo,
  enableDirectoryTracking,
  registerDirectoryChangeCallback,
  startLocalPortForwarding,
  startRemotePortForwarding,
  startDynamicPortForwarding
  } from 'libentry.so'
import { AlertDialog, LengthUnit, promptAction } from '@kit.ArkUI';
import InputCustomDialog from '../../components/CustomDialog/InputCustomDialog';
import PortForwardingDialog, { ProtDialogDataSourceClass } from '../../components/CustomDialog/PortForwardingDialog';
import { picker } from '@kit.CoreFileKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import taskpool from '@ohos.taskpool';
import { BusinessError, emitter } from '@kit.BasicServicesKit';
import Notification from 'ets/components/Notification/Index'
import { localStorage } from '../entryPage/Index';

export  type openDialogType = 'rename' | 'addDirectory' | 'addPortForwarding' | null

let Message_Warning:Resource[] = [
  $r('app.string.OPERATION_SUCCESSFUL'),
  $r('app.string.FILE_READ_COMPLETED'),
  $r('app.string.FILE_OR_DIRECTORY_NOT_EXIST'),
  $r('app.string.PERMISSION_DENIED'),
  $r('app.string.SYSTEM_EXCEPTION'),
  $r('app.string.SFTP_MESSAGE_FORMAT_ERROR'),
  $r('app.string.CONNECTION_FAILED'),
  $r('app.string.CONNECTION_LOST'),
  $r('app.string.OPERATION_NOT_SUPPORTED'),
  $r('app.string.INVALID_FILE_HANDLE'),
  $r('app.string.PATH_NOT_EXIST'),
  $r('app.string.FILE_ALREADY_EXISTS'),
  $r('app.string.READ_ONLY_FILE_SYSTEM'),
  $r('app.string.NO_MEDIA'),
  $r('app.string.FILESYSTEM_SPACE_INSUFFICIENT'),
  $r('app.string.USER_QUOTA_EXCEEDED'),
  $r('app.string.UNKNOWN_PRINCIPAL'),
  $r('app.string.FILE_LOCK_CONFLICT'),
  $r('app.string.DIRECTORY_NOT_EMPTY'),
  $r('app.string.NOT_A_DIRECTORY'),
  $r('app.string.INVALID_FILENAME'),
  $r('app.string.TOO_MANY_SYMBOLIC_LINKS')
]

@Concurrent
function cancelTransferTask(sessionId:number){
  cancelTransfer(sessionId)
}

let dropUpLoadFile:fs.File[] = []
const MAX_SELECT_NUMBER = 5 //上传最多选择文件数

@Entry(localStorage)
@Component
struct Index {
  @State context:common.UIAbilityContext = getContext(this) as common.UIAbilityContext
  @State allDataKvStore:distributedKVStore.SingleKVStore | null =null
  // 文件选择器对象
  @State documentPicker:picker.DocumentViewPicker|null = null
  @State PageAllDataArr:allDataType[]  =  []
  @State KVManager:KvManager = new KvManager({
    context: getContext(this),
    bundleName:this.context.resourceManager.getStringSync($r('app.string.App_bundleName').id),//您自己的App对应的包名
  })
  @State nowAllConfigData:allDataType | undefined = undefined
  // 当前的文件路径
  @State filePath:string = ''
  // 文件列表加载状态
  @State fileListLoading:boolean = false
  // 当前选中的文件
  @State selectedFile:fileNodeType | null = null
  // 当前选中的文件名
  @State selectedFileName:string = ''
  //dataSource 的源数据
  @State dataSource:fileNodeType[] = []
  // 导航栏数据
  @State pathNodeObj:fileNodeType[] = []
  // 当前点开的弹窗名称
  @State openDialogName:openDialogType = null
  // taskpool文件上传线程管理对象
  @State taskFileUpGroup:taskpool.TaskGroup = new taskpool.TaskGroup()
  // 文件上传loading变量
  @State fileUpLoading:boolean = false
  // 文件上传进度条
  @State fileUpProgress:number = 0
  // 文件下载loading变量
  @State fileDownLoading:boolean = false
  // 文件下载进度条
  @State fileDownProgress:number = 0
  // 显示 | 隐藏 文件传输进度
  @State showNotification:boolean = false
  // 文件上传进度条对象
  @State uploadProgress:UploadProgressItem |null = null
  // 文件下载进度条对象
  @State downProgress:DownloadProgressItem | null = null
  // 上传或下载延时器id
  @State upOrDownSetTimer:number | null = null
  // 标记当前传输的状态（-1：未传输，0：取消传输，1：传输成功）
  @State upOrDownType:-1 | 0 | 1 = -1;
  // 端口转发相关状态
  @State portForwardings: ProtDialogDataSourceClass= new ProtDialogDataSourceClass([]) // 存储活动中的端口转发规则
  @State showPortForwardingDialog: boolean = false // 控制端口转发弹窗显示
  // 添加端口转发的数据
  @State sourcePort: string = ''
  @State destination: string = ''
  @State forwardingType: 'local' | 'remote' | 'dynamic' = 'local'
  
  @State showFlag:boolean=false

  @State sessionId: number = -1

  //todo 异常退出弹窗
  abNormalExitDialog: CustomDialogController = new CustomDialogController({
    builder: AlertDialog({
      theme: AppStorage.get('currentColorMode'),
      primaryTitle: $r('app.string.abNormalExitTitle'),
      content: $r('app.string.abNormalExitContent'),
      primaryButton: {
        value: $r('app.string.abNormalExitConfirm'),
        action: () => {
          if (this.nowAllConfigData?.Session.closeWindowMode == CloseWindowMode.Always) {
            this.closeWindow(); // 调用关闭窗口的逻辑
          }
          console.log(`SessionId (${this.sessionId}) 匹配，准备关闭窗口...`);
        },
      },
    }),
    autoCancel:false
  })

  // --- 修改事件监听处理，接收 EventData ---
  handleSessionLogoutRequest = (event: emitter.EventData) => {
    // 从 event.data 中解构 sessionId
    const eventSessionId = event.data?.sessionId as number | undefined;
    const eventStatus:boolean | undefined = event.data?.status as boolean | undefined
    console.log(`ShellWindow (${this.sessionId}) 收到退出请求事件，来源 sessionId: ${eventSessionId}`);
    if (eventSessionId !== undefined && eventSessionId === this.sessionId) {
      if(eventStatus){
        console.log(`SessionId (${this.sessionId}) 匹配，准备关闭窗口...`);
        this.closeWindow(); // 调用关闭窗口的逻辑
      }else{
        this.abNormalExitDialog.open()
      }
    } else {
      console.log(`SessionId (${this.sessionId}) 不匹配或事件数据无效，忽略事件。`);
    }
  }


  // --- 封装关闭窗口的逻辑 ---
  closeWindow() {
    console.log(`ShellWindow (${this.sessionId}) 执行关闭操作`);
    if (this.context) {
      try {
        this.context.terminateSelf((err: BusinessError) => {
          if (err.code) {
            console.error(`terminateSelf 失败, sessionId: ${this.sessionId}, code: ${err.code}, message: ${err.message}`);
          } else {
            console.log(`terminateSelf 成功, sessionId: ${this.sessionId}`);
          }
        });
      } catch (err) {
        // 捕获同步的参数错误
        let code = (err as BusinessError).code;
        let message = (err as BusinessError).message;
        console.error(`terminateSelf 同步调用失败, sessionId: ${this.sessionId}, code is ${code}, message is ${message}`);
      }
    } else {
      console.error(`无法关闭窗口，context 为 null, sessionId: ${this.sessionId}`);
    }
  }

  onPageShow(): void {

  }

  backPathMap(path:string,sessionId:number){
    console.log('当前服务端返回的地址变化了地址',this.sessionId,sessionId)
    if(path){
      if(path.indexOf('/~')>-1){
        this.filePath = (path.split('/~')[1] === '/')? '/': path.split('/~')[1] + '/'
      }else{
        this.filePath = path==='/'?'/': path+ '/'
      }
      this.filePathChange()
      this.selectedFileName = ''
      this.selectedFile = null
    }
  }

  async aboutToAppear(): Promise<void> {

    let perf= preferences.getPreferences(this.context,'shellWindowWant')
    perf.then(async (ref)=>{
     this.sessionId=await ref.get('sessionId',-1) as number
      console.log('首选项获取到的sessionId是'+this.sessionId)
      // 重新监听一次，确保 sessionId 是最新的
      emitter.off('session_logout_request', this.handleSessionLogoutRequest); // 先移除旧监听
      emitter.on('session_logout_request', this.handleSessionLogoutRequest); // 用新 sessionId 监听
      console.log(`ShellWindow (${this.sessionId}) 监听到最新sessionId并重新监听事件`);

      // 在获取到 sessionId 后执行 init 逻辑
      await this.init();
      this.documentPicker = new picker.DocumentViewPicker(this.context);

    })
    this.allDataKvStore = await this.KVManager.getStore('allData')
    // 初始化
    // await this.init() // 移到 perf.then 内部
    // this.documentPicker = new picker.DocumentViewPicker(this.context) // 移到 perf.then 内部
    console.log('AppStorage.link(\'EntryAbilityType\')',AppStorage.link('EntryAbilityType').get())

    // --- 添加事件监听 (初始监听，可能 sessionId 还不准确) ---
    console.log(`ShellWindow (${this.sessionId}) 开始监听 session_logout_request 事件 (初始)`);
    emitter.on('session_logout_request', this.handleSessionLogoutRequest);
    // --- 监听结束 ---

  }
  getPwdPathCallBack = (sessionId:number,path:string) => {
    let eventData: emitter.EventData = {
      data: {
        path,
        sessionId,
      }
    }
    emitter.emit('back_path_'+ sessionId,eventData)
    console.log('当前服务端返回的地址变化了地址 - ' + 'back_path_'+ sessionId,path)
  }

  private delConfirm: CustomDialogController | null = new CustomDialogController({
    builder: AlertDialog({
      primaryTitle: $r('app.string.SYSTEM_PROMPT'),
      content: `您正在删除「${this.selectedFile?.name}」，该操作将永久移除文件，无法恢复`,
      primaryButton: {
        value: $r('app.string.CANCEL'),
        action: () => {
          this.delConfirm?.close();
        },
        fontColor: '#0A59F7',
        background: '#fff'
      },
      secondaryButton: {
        value: $r('app.string.Delete'),
        action: () => {
          if(this.selectedFile){
            this.deleteListFile(this.selectedFile)
          }
        },
        fontColor: '#fff',
        background: '#0A59F7'
      }
    }),
  })

  // 显示端口转发列表的弹窗
  private portForwardingsDialog: CustomDialogController | null = new CustomDialogController({
    builder: PortForwardingDialog({
      portForwardings: this.portForwardings,
      stopForwarding: this.stopForwarding.bind(this)
    }),
    autoCancel: true,
    alignment: DialogAlignment.Center,
    customStyle: true
  })

  async  aboutToDisappear(): Promise<void> {
    console.log(`ShellWindow (${this.sessionId}) 即将消失，移除事件监听`);
    // --- 移除事件监听 ---
    emitter.off('session_logout_request', this.handleSessionLogoutRequest);
    // --- 移除结束 ---
    emitter.off('back_path_'+this.sessionId)
    console.log('卸载')
    // 使用异步方式获取最新的 Session 数据
    if (this.allDataKvStore) {
      const sessionData = await this.allDataKvStore.get('Session')
      if (sessionData) {
        try {
          const session = JSON.parse(sessionData as string) as SessionType
          console.log("开始关闭连接======1", session.connectionType)
          // 只有当 connectionType 为 SSH 时才调用
          if (session.connectionType === 'SSH') {
            console.log("开始关闭SSH连接======", session.connectionType,this.sessionId)
            // const task = new taskpool.GenericsTask<[number],void>(cancelTransfer,this.sessionId)
            // await taskpool.execute(task,taskpool.Priority.HIGH)
            try {
              cancelTransfer(this.sessionId)
            }catch (e) {
              console.error('dyx 调用cancelTransfer失败',e)
            }
          }
        } catch (e) {
          console.error('解析 Session 数据失败', e)
        }
      }
    }
  }

  async init(){
    try{
      if(this.allDataKvStore){
        const res = await Promise.all(
          [
            this.allDataKvStore.get('Session'),
            this.allDataKvStore.get('Terminal'),
            this.allDataKvStore.get('Connection'),
            this.allDataKvStore.get('Serial'),
            this.allDataKvStore.get('Telnet'),
            this.allDataKvStore.get('Tunnels'),
            this.allDataKvStore.get('Logging'),
            this.allDataKvStore.get('WindowSize'),
            this.allDataKvStore.get('Character'),
            this.allDataKvStore.get('Cursor'),
            this.allDataKvStore.get('Color'),
            this.allDataKvStore.get('Select'),
            this.allDataKvStore.get('KeyGen'),
            this.allDataKvStore.get('Appear'),
            this.allDataKvStore.get('Proxy'),
          ]
        )
        const Session:SessionType = JSON.parse(res[0] as string) as SessionType
        const Terminal:TerminalType = JSON.parse(res[1]as string) as TerminalType
        const Connection:ConnectionType = JSON.parse(res[2] as string) as ConnectionType
        const Serial:SerialType = JSON.parse(res[3] as string) as SerialType
        const Telnet:TelnetType = JSON.parse(res[4] as string) as TelnetType
        const Tunnels:TunnelsType = JSON.parse(res[5] as string) as TunnelsType
        const Logging:LoggingType = JSON.parse(res[6] as string) as LoggingType
        const WindowSize: WindowSizeType = JSON.parse(res[7] as string) as WindowSizeType
        const Character: CharacterType = JSON.parse(res[8] as string) as CharacterType
        const Cursor: CursorType = JSON.parse(res[9] as string) as CursorType
        const Color: ColorType = JSON.parse(res[10] as string) as ColorType
        const Select: SelectType = JSON.parse(res[11] as string) as SelectType
        const KeyGen: KeyGenType = JSON.parse(res[12] as string) as KeyGenType
        const Appear: AppearType = JSON.parse(res[13] as string) as AppearType
        let Proxy: ProxyType = { type: 'None', host: '', port: '', username: '', password: '', enabled: false }
        try {
          const p = res[14]
          if (p) {
            const parsed = JSON.parse(p as string) as ProxyType
            if (parsed && parsed.type !== undefined) {
              Proxy = parsed
            }
          }
        } catch (e) {
          console.error('ShellWindow 解析 Proxy 失败', e)
        }
        this.filePath = '/'+ (Session.userName==='root'?'root':'home/'+Session.userName) + '/'
        const initPathArr: fileNodeType[]= Session.userName==='root'?[{name:'root',isDirectory:true}]:[{name:'home',isDirectory:true},{name:Session.userName,isDirectory:true}]
        this.pathNodeObj = [{name:'/',isDirectory:true},...initPathArr]
        this.filePathChange()
        this.nowAllConfigData = {Session,Logging,Terminal,Connection,Serial,Telnet,Tunnels, WindowSize,
          Character,
          Cursor,
          Color,
          Select,
          KeyGen,Appear,Proxy}
        let te=Session.connectionType
        console.log('yu------>'+te)
        if (te=='Telnet' || te=='Serial') {
          this.showFlag=true
          console.log('yu----->111111')
        }else {
          this.showFlag=false
          console.log('yu----->222222')
        }
        console.log('this.nowAllConfigData?.Session?.connectionType',this.nowAllConfigData?.Session?.connectionType)
        // 判断是否启用终端目录追踪功能（为SSH连接时启用）
        if (this.nowAllConfigData?.Session?.connectionType === 'SSH') {

          const result = await enableDirectoryTracking(this.sessionId,true)
          console.log('启用终端目录追踪功能',this.sessionId)
          if(result){
            console.log('终端目录追踪功能启用成功')
            registerDirectoryChangeCallback(this.getPwdPathCallBack)
          }
        }
        emitter.on('back_path_'+this.sessionId,(data)=>{
          if(data.data){
            this.backPathMap(data.data.path,data.data.sessionId)
          }
        })

        await this.reloadFileList();
      }
    }catch (e) {
      console.error('shellWindow初始化',e)
    }
  }

  // 当选中的文件变化时,替换pathNodeObj的最后一个文件
  async replaceFileNode(fileNode:fileNodeType){
    if(this.selectedFile){
      this.pathNodeObj.pop()
    }
    this.pathNodeObj.push(fileNode)
  }

  // 单击选中ListItem
  clickListItem = async (fileNode:fileNodeType) => {
    this.selectedFileName = fileNode.name;
    console.log('点击了ListItem',JSON.stringify(fileNode))
    await this.replaceFileNode(fileNode)
    this.selectedFile = fileNode
    this.navScroller.scrollPage({next:true})
  }

  // 双击进入文件夹
  doubleClick = async (fileNode:fileNodeType) => {
    if(fileNode.isDirectory){
      // await this.reloadFileList()
      console.log('filePath',this.filePath)
      this.filePath += fileNode.name + '/'
      this.selectedFileName = ''
      this.selectedFile = null
      this.filePathChange()
    }
  }

  // 下载文件
  async  downLoadFile(fileNode:fileNodeType){
      // 检查连接类型，只有SSH连接才能使用SFTP功能
      if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
        return;
      }

      const pathArr = await this.documentPicker?.save({newFileNames:[fileNode.name]})
      if(!fileNode.isDirectory && pathArr){
        // 下载文件（异步方式）
        try{
          console.log('this.upOrDownSetTimer',this.upOrDownSetTimer)
          if(this.uploadProgress){
            clearTimeout(this.upOrDownSetTimer)
            this.uploadProgress = null
            this.upOrDownType = -1
          }
          this.fileDownLoading = true
          const file = await fs.open(pathArr[0], fs.OpenMode.READ_WRITE);

          downloadFile(this.sessionId,this.filePath + fileNode.name, file.fd,(err,progress)=>{
            console.log('文件下载err',err?.message)
            console.log('文件下载路径',this.filePath)
            if(progress){
              this.downProgress = progress
              console.log('文件下载进度---------->',JSON.stringify(progress))
            }else{
              console.error('文件下载进度条异常',err?.message)
            }
          },(err,success) => {
            console.log('文件下载',err)
            if (!err) {
              this.getUIContext().getPromptAction().showToast({
                message:$r('app.string.File_Download_Successful'),
                showMode:promptAction.ToastShowMode.DEFAULT,
                textColor:Color.Green,
                duration:4000
              })
              this.upOrDownType = 1
              // 在回调中关闭文件描述符
              fs.close(file.fd);
              // 处理成功逻辑
            } else {
              this.getUIContext().getPromptAction().showToast({
                message:this.upOrDownType === 0 ?$r('app.string.File_Download_Canceled'):$r('app.string.File_Download_Failed'),
                showMode:promptAction.ToastShowMode.DEFAULT,
                textColor:Color.Red,
                duration:4000
              })
              // 文件下载失败时，删除已创建的文件
              fs.unlink(file.path)
            }
            this.upOrDownSetTimer = setTimeout(()=>{
              this.downProgress = null
              this.upOrDownType = -1
              this.showNotification = false
            },3000)
            this.fileDownLoading = false
            this.fileDownProgress = 0
          });
        }catch (e) {
          console.log('文件下载 error',e)
          this.fileDownLoading = false
          this.fileDownProgress = 0
        }finally {

        }
      }
  }

  // 通过文件fd上传文件
  uploadFileByFd(fdArr:fs.File[]){
    console.log('文件上传开始',JSON.stringify(fdArr))
    try{
      if(this.downProgress){
        clearTimeout(this.upOrDownSetTimer)
        this.downProgress = null
        this.upOrDownType = -1
      }
      this.fileUpLoading = true
      console.log('yu-------->',this.filePath)
      uploadFile(this.sessionId,fdArr,this.filePath,(err,progressDetails)=>{
        if(progressDetails){
          this.uploadProgress = progressDetails
          console.log('文件上传中。。。。',progressDetails.completed,progressDetails.remotePath)
        }else{
          console.error('文件上传进度条回调异常',err?.message)
        }
      },
        (err,result)=>{
          console.log('fdArr文件上传')
          if (!err) {
            this.getUIContext().getPromptAction().showToast({
              message:$r('app.string.File_Uploaded_Successfully'),
              showMode:promptAction.ToastShowMode.DEFAULT,
              textColor:Color.Green,
              duration:4000
            })
            this.upOrDownType = 1
            fdArr.map(item=>{
              fs.close(item)
            })
            this.reloadFileList()
          }else{
            this.getUIContext().getPromptAction().showToast({
              message:this.upOrDownType=== 0 ?$r('app.string.File_Transfer_Canceled'):$r('app.string.File_Upload_Failed'),
              showMode:promptAction.ToastShowMode.DEFAULT,
              textColor:Color.Red,
              duration:4000
            })
          }
          this.upOrDownSetTimer = setTimeout(()=>{
            this.uploadProgress = null
            this.upOrDownType = -1
            this.showNotification = false
          },3000)
          this.fileUpLoading = false
          this.fileUpProgress = 0
        })
    }catch (e) {
      this.fileUpLoading = false
      this.fileUpProgress = 0
      console.error(e)
    }finally {

      console.log('文件上传结束')
    }
  }


  decodeUriPath(uri: string): string {
    try {
      return decodeURIComponent(uri);
    } catch (error) {
      console.error('URI解码失败:', error);
      return uri;
    }
  }

  // 上传文件
  uploadFile = async ()=>{
      // 检查连接类型，只有SSH连接才能使用SFTP功能
      if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
        return;
      }

      // 文件fd对象集合
      let fdArr:fs.File[]
      if( this.documentPicker){
        let resultArr:string[] = []
        // 判断当前设备是否包含文件和文件夹选择的能力
        if(canIUse('SystemCapability.FileManagement.UserFileService.FolderSelection')){
          resultArr = await this.documentPicker.select({
            // maxSelectNumber:MAX_SELECT_NUMBER, // 最大上传个数
            selectMode:picker.DocumentSelectMode.FILE,
          })
        }else{
          resultArr = await this.documentPicker.select({
            // maxSelectNumber:MAX_SELECT_NUMBER, // 最大上传个数
          })
        }
        let tempResult:string[]=[]
        console.log('resultArr',JSON.stringify(resultArr))
        resultArr.forEach(async (item:string)=>{
          tempResult.push(this.decodeUriPath(item))
        })
        console.log('tempResult'+tempResult)
        // 通过fs.open获取File对象中的fd[]
        console.log("open 开始")
        if(tempResult.length > 0){
          try {
            fdArr = await Promise.all(tempResult.map(item=>fs.open(item,fs.OpenMode.READ_WRITE)))
            console.log('fdArr',JSON.stringify(fdArr))
            this.uploadFileByFd(fdArr)
          }catch (e) {
            console.log('open文件失败',e.code+e.message)
          }
        }
        console.log("open 结束")
      }
  }
  /**
   * 拖拽上传二次确认弹窗
   */
  private dropConfirm:CustomDialogController | null = new CustomDialogController({
    builder: AlertDialog({
      primaryTitle: $r('app.string.SYSTEM_PROMPT'),
      content: `您正在上传文件（${dropUpLoadFile.length ===1? dropUpLoadFile[0].name:dropUpLoadFile[0].name+`...等${dropUpLoadFile.length}个文件`}），确定要上传此文件吗？`,
      primaryButton: {
        value: $r('app.string.CANCEL'),
        action: () => {
          this.dropConfirm?.close();
        },
        fontColor:'#0A59F7',
        background:'#fff'
      },
      secondaryButton: {
        value: '确认',
        action: () => {
          this.uploadFileByFd(dropUpLoadFile);
        },
        fontColor:'#fff',
        background:'#0A59F7'
      }
    }),
  })

  // 终止传输
  async closeTransmission(){
    console.log('dyx sessionId',this.sessionId)
    cancelTransfer(this.sessionId)
    this.upOrDownType = 0
  }

  // 拖拽进文件
  async  dropFileIn(event:DragEvent){
    // 检查连接类型，只有SSH连接才能使用SFTP功能
    if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
      return;
    }

    try {
      let data = event.getData();  //  这里拿到的是undefined,报错走catch了
      let record = data.getRecords();
      dropUpLoadFile = []
      for(let i = 0;i<record.length;i++){
        console.log('record',record[i].getType())
        let filePath:string = (record[i] as unifiedDataChannel.File).uri
        if(record[i].getType()==='general.folder'){
          this.getUIContext().getPromptAction().showToast({
            message: $r('app.string.Drag_Drop_Upload_Error'),
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: $r('app.color.warning_textColor'),
            duration: 4000
          });
          return;
        }
        let file = await fs.open(filePath)
        dropUpLoadFile.push(file)
      }
      if (this.fileUpLoading) {
        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.File_Uploading_Error'),
          showMode: promptAction.ToastShowMode.DEFAULT,
          textColor: $r('app.color.warning_textColor'),
          duration: 4000
        });
        return;
        // }else if(dropUpLoadFile.length <= MAX_SELECT_NUMBER){
        //   this.dropConfirm?.open()
        // } else {
        //   promptAction.showToast({
        //     message:`最大支持上传${MAX_SELECT_NUMBER}个文件，请重新选择后再次上传！`,
        //     showMode:promptAction.ToastShowMode.DEFAULT,
        //     textColor:Color.Orange,
        //     duration:4000
        //   })
        // }
      }else{
        this.dropConfirm?.open()
      }
    } catch (err) {
      console.error(`err: ${JSON.stringify(err.code)}, ${JSON.stringify(err.message)}`)
    }
  }

  // 新增文件夹
  async addDirectory(input:string){

      // 检查连接类型，只有SSH连接才能使用SFTP功能
      if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
        return;
      }
      console.log('this.filePath+input+\'/\'',this.filePath+input+'/')
      createDirectory(this.sessionId,this.filePath+input+'/',(err,success)=>{
        if(!err){
          this.getUIContext().getPromptAction().showToast({
            message:$r('app.string.Successfully_Added_Folder'),
            showMode:promptAction.ToastShowMode.DEFAULT,
            textColor:Color.Green,
            duration:4000
          })
          this.reloadFileList()
        }else{
          const msg:Resource = JSON.parse(JSON.stringify(err)).code ?Message_Warning[JSON.parse(JSON.stringify(err)).code as number]:$r('app.string.Failed_Add_Folder')
          this.getUIContext().getPromptAction().showToast({
            message:msg,
            showMode:promptAction.ToastShowMode.DEFAULT,
            textColor:Color.Red,
            duration:4000
          })
        }
      })
  }

  // 删除文件
  async deleteListFile(fileNode:fileNodeType):Promise<void>{


    // 检查连接类型，只有SSH连接才能使用SFTP功能
    if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
      return;
    }

    const delFilePath:string = this.filePath+fileNode.name
    console.log('delFilePath-------->',fileNode.name === this.uploadProgress?.remotePath.split('/').slice(-1)[0])
    const promise:Promise<Resource> = new Promise((resolve,reject)=>{
      if(fileNode.name === this.uploadProgress?.remotePath.split('/').slice(-1)[0] || fileNode.name === this.downProgress?.remotePath.split('/').slice(-1)[0]){
        reject($r('app.string.File_Transferred_Cannot_Deleted'))
        return;
      }
      try{
        if(fileNode.isDirectory){
          console.log('删除文件夹，文件name：',fileNode)
          deleteDirectory(this.sessionId,delFilePath,(err,success)=>{
            if(!err){
              this.selectedFile = null
              this.filePathChange()
              resolve($r('app.string.Deleted_Successfully'))
            }else{
              const msg = JSON.parse(JSON.stringify(err)).code ?Message_Warning[JSON.parse(JSON.stringify(err)).code as number]:$r('app.string.Delete_Error')
              reject(msg)
            }
          })
        }else{
           deleteFile(this.sessionId,delFilePath,(err,success)=>{
             if(!err){
               this.selectedFile = null
               this.filePathChange()
               resolve($r('app.string.Deleted_Successfully'))
             }else{
               const msg = JSON.parse(JSON.stringify(err)).code ?Message_Warning[JSON.parse(JSON.stringify(err)).code as number]:$r('app.string.Delete_Error')
               reject(msg)
             }
           })
        }
      }catch (e) {
        reject(e)
      }
    })
    try{
      const result = await promise
      this.getUIContext().getPromptAction().showToast({
        message:result,
        showMode:promptAction.ToastShowMode.DEFAULT,
        textColor:Color.Green,
        duration:4000
      })
      this.selectedFileName = ''
      this.selectedFile = null
      await this.reloadFileList()
    } catch (e) {
      const _em = (e instanceof Error) ? e.message : JSON.stringify(e)
      this.getUIContext().getPromptAction().showToast({
        message: _em || '',
        showMode:promptAction.ToastShowMode.DEFAULT,
        textColor:Color.Red,
        duration:4000
      })
    }
  }
  // 刷新文件列表
   reloadFileList = async ():Promise<void>=>{

    // 初始化fstp，获取根目录
       this.fileListLoading = true
       this.dataSource = []

       // 检查连接类型，只有SSH连接才能使用SFTP功能
       if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
         this.fileListLoading = false
         return;
       }
       
       listDirectory(this.sessionId,this.filePath,(err,result)=>{
         console.log('获取文件路径目录',this,this.filePath)
        if(err){
          console.error('获取文件目录失败',err.message,JSON.parse(JSON.stringify(err)).code)
          const msg = JSON.parse(JSON.stringify(err)).code ?Message_Warning[JSON.parse(JSON.stringify(err)).code as number]:$r('app.string.Connection_error')
          this.filePath =this.filePath.split('/').splice(0,this.filePath.split('/').length -2).join('/') + '/'
          this.getUIContext().getPromptAction().showToast({
            message:msg,
            showMode:promptAction.ToastShowMode.DEFAULT,
            textColor:Color.Red,
            duration:4000
          })
        }
       this.dataSource = result || []
       this.fileListLoading =false
         // 滚动到末尾
         this.navScroller.scrollPage({next:true})
      })
  }
  // 重命名
   renameFun = async(fileNode:fileNodeType | null,fileRename:string)=>{``

    // 检查连接类型，只有SSH连接才能使用SFTP功能
    if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
      return;
    }
    
    if(fileNode){
      if(fileNode.name === this.uploadProgress?.remotePath.split('/').slice(-1)[0] || fileNode.name === this.downProgress?.remotePath.split('/').slice(-1)[0]){
        this.getUIContext().getPromptAction().showToast({
          message:$r('app.string.FILE_IN_TRANSFER_RENAME_NOT_ALLOWED'),
          showMode:promptAction.ToastShowMode.DEFAULT,
          textColor:Color.Red,
          duration:4000
        })
        return;
      }
      rename(this.sessionId,this.filePath+fileNode.name+(fileNode.isDirectory?'/':''),this.filePath+fileRename+(fileNode.isDirectory?'/':''),(err,success)=>{
        if(!err){
          this.getUIContext().getPromptAction().showToast({
            message:$r('app.string.RENAME_SUCCESSFUL'),
            showMode:promptAction.ToastShowMode.DEFAULT,
            textColor:Color.Green,
            duration:4000
          })
          this.selectedFileName = ''
          this.selectedFile = null
          this.filePathChange()
        }else{
          const msg = JSON.parse(JSON.stringify(err)).code ?Message_Warning[JSON.parse(JSON.stringify(err)).code as number]:$r('app.string.RENAME_FAILED')
          this.getUIContext().getPromptAction().showToast({
            message:msg,
            showMode:promptAction.ToastShowMode.DEFAULT,
            textColor:Color.Red,
            duration:4000
          })
        }
      })
    }else{

    }
  }

  // 查询活动中的端口转发规则
  async getPortForwardings(): Promise<void> {

    try {
      // 只有当 connectionType 为 SSH 时才调用
      if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.PORT_FORWARDING_SSH_ONLY'),
          showMode: promptAction.ToastShowMode.DEFAULT,
          textColor: $r('app.color.warning_textColor'),
          duration: 4000
        })
        return;
      }
      
      const forwardings = await listActivePortForwardings(this.sessionId);
      this.portForwardings.dataSource = forwardings;
      console.log("获取到活动中的端口", JSON.stringify(forwardings))

      // 无论是否有端口转发，都直接显示端口转发列表弹窗
      if (this.portForwardingsDialog) {
        this.portForwardingsDialog.close();
      }

      this.portForwardingsDialog = new CustomDialogController({
        builder: PortForwardingDialog({
          portForwardings: this.portForwardings,
          stopForwarding: this.stopForwarding.bind(this),
          addPortForwarding: (sourcePort, destination, forwardingType) => {
            this.sourcePort = sourcePort;
            this.destination = destination;
            this.forwardingType = forwardingType;
            this.addPortForwarding();
          }
        }),
        autoCancel: true,
        alignment: DialogAlignment.Center,
        customStyle: true
      });

      this.showPortForwardingDialog = true;
      this.portForwardingsDialog.open();
    } catch (error) {
      console.error('获取端口转发规则失败', error);
      this.getUIContext().getPromptAction().showToast({
        message: $r('app.string.GET_PORT_FORWARDING_RULES_FAILED'),
        showMode: promptAction.ToastShowMode.DEFAULT,
        textColor: Color.Red,
        duration: 4000
      })
    }
  }

  // 停止端口转发
  async stopForwarding(port: number, isRemote: boolean): Promise<void> {

    try {
      const success = await stopPortForwarding(this.sessionId,port, isRemote);
      console.log('dyx 停止端口转发',this.sessionId,port,isRemote,success)
      if (success) {
        this.getUIContext().getPromptAction().showToast({
          // message: $r('app.string.STOP_FORWARDING_SUCCESS',port+''),
          message: $r('app.string.STOP_FORWARDING_SUCCESS',port),
          showMode: promptAction.ToastShowMode.DEFAULT,
          textColor: Color.Green,
          duration: 4000
        })
        // 刷新端口转发列表
        const forwardings = await listActivePortForwardings(this.sessionId);
        this.portForwardings.dataSource = forwardings;
        
        // 如果删除后没有端口转发规则了，只关闭当前弹窗
        if (forwardings.length === 0) {
          this.portForwardingsDialog?.close();
          // 不再显示 noPortsDialog
        } else {
          // 如果还有剩余规则，直接刷新弹窗内容，不重新创建弹窗
          // 端口转发列表会自动更新，因为绑定了 this.portForwardings
        }
      } else {
        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.STOP_FORWARDING_FAILED',port+''),
          showMode: promptAction.ToastShowMode.DEFAULT,
          textColor: Color.Red,
          duration: 4000
        })
      }
    } catch (error) {
      console.error(`停止端口 ${port} 转发失败`, error);
      this.getUIContext().getPromptAction().showToast({
        message: $r('app.string.STOP_FORWARDING_FAILED',port+''),
        showMode: promptAction.ToastShowMode.DEFAULT,
        textColor: Color.Red,
        duration: 4000
      })
    }
  }

  // 添加端口转发
  async addPortForwarding(): Promise<void> {
    try {
      // 检查是否为SSH连接
      if (this.nowAllConfigData?.Session?.connectionType !== 'SSH') {
        this.getUIContext().getPromptAction().showToast({
          message:$r('app.string.PORT_FORWARDING_SSH_ONLY'),
          showMode: promptAction.ToastShowMode.DEFAULT,
          textColor: $r('app.color.warning_textColor'),
          duration: 4000
        })
        return;
      }

      // 检查源端口是否有效
      const port = parseInt(this.sourcePort);
      if (isNaN(port) || port <= 0 || port > 65535) {
        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.ENTER_VALID_SOURCE_PORT'),
          showMode: promptAction.ToastShowMode.DEFAULT,
          textColor: Color.Red,
          duration: 4000
        })
        return;
      }

      // 对于本地和远程端口转发，检查目标地址格式
      if (this.forwardingType !== 'dynamic') {
        const destinationParts = this.destination.split(':');
        if (destinationParts.length !== 2 || !destinationParts[0].trim() || !destinationParts[1].trim()) {
          this.getUIContext().getPromptAction().showToast({
            message: $r('app.string.ENTER_VALID_TARGET_FORMAT'),
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: Color.Red,
            duration: 4000
          })
          return;
        }

        const targetHost = destinationParts[0].trim();
        const targetPort = parseInt(destinationParts[1].trim());
        
        if (isNaN(targetPort) || targetPort <= 0 || targetPort > 65535) {
          this.getUIContext().getPromptAction().showToast({
            message: $r('app.string.ENTER_VALID_SOURCE_PORT'),
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: Color.Red,
            duration: 4000
          })
          return;
        }

        let success = false;
        
        // 根据类型启动相应的端口转发
        if (this.forwardingType === 'local') {
          success = await startLocalPortForwarding(this.sessionId,port, targetHost, targetPort, true);
        } else if (this.forwardingType === 'remote') {
          success = await startRemotePortForwarding(this.sessionId,port, targetHost, targetPort);
        }
        
        if (success) {
          this.getUIContext().getPromptAction().showToast({
            message: this.forwardingType === 'local' ? $r('app.string.LOCAL') : $r('app.string.REMOTE'),
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: Color.Green,
            duration: 4000
          })
          // 刷新转发端口列表
          await this.getPortForwardings();
        } else {
          this.getUIContext().getPromptAction().showToast({
            message: `${this.forwardingType === 'local' ? $r('app.string.PORT_FORWARDING_FAILED') : $r('app.string.PORT_FORWARDING_FAILED')}}`,
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: Color.Red,
            duration: 4000
          })
        }
      } else {
        // 动态端口转发
        const success = await startDynamicPortForwarding(this.sessionId,port, true);
        
        if (success) {
          this.getUIContext().getPromptAction().showToast({
            message: $r('app.string.DYNAMIC_SOCKS_STARTED'),
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: Color.Green,
            duration: 4000
          })
          // 刷新转发端口列表
          await this.getPortForwardings();
        } else {
          this.getUIContext().getPromptAction().showToast({
            message: $r('app.string.DYNAMIC_SOCKS_START_FAILED'),
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: Color.Red,
            duration: 4000
          })
        }
      }
    } catch (error) {
      console.error('添加端口转发失败', error);
      this.getUIContext().getPromptAction().showToast({
        message: $r('app.string.ADD_PORT_FORWARDING_FAILED',(error instanceof Error ? error.message : String(error))),
        showMode: promptAction.ToastShowMode.DEFAULT,
        textColor: Color.Red,
        duration: 4000
      })
    }
  }

  // 弹窗提交
  dialogConfirm = (input:string)=>{
    console.log('type----test---->',this.openDialogName)
    if(this.openDialogName){
      switch (this.openDialogName){
        case 'rename':this.renameFun(this.selectedFile,input);break;
        case 'addDirectory':this.addDirectory(input);break;
        case 'addPortForwarding':
          // 这个函数处理已经在打开弹窗时单独实现
          break;
        default :break;
      }
    }
  }

  /**
   * 新增文件夹 | 重命名弹窗 | 添加端口转发弹窗
   */
  private dialogController: CustomDialogController | null = null

  openDialog(type:openDialogType): void{
    if (type === 'addPortForwarding') {
      // 直接调用getPortForwardings来显示集成的端口转发对话框
      this.getPortForwardings();
    } else {
      // 创建普通输入弹窗
      this.dialogController = new CustomDialogController({
        builder: InputCustomDialog({
          confirm: this.dialogConfirm,
        }),
        autoCancel: true,
        alignment: DialogAlignment.Top,
        customStyle: false,
        cornerRadius: 10,
      });
      
      this.openDialogName = type;
      this.dialogController.open();
    }
  }

  filePathChange(){
    this.reloadFileList()
    console.log('this.filePath',this.filePath)
    if (this.filePath === '/'){
      this.pathNodeObj = [{name:'/',isDirectory:true}]
      return;
    }
    const filePathArr = this.filePath.split('/')
    filePathArr.pop()
    this.pathNodeObj = filePathArr.map((item:string,index:number):fileNodeType=>{
        if(index===0){
          return {name:'/',isDirectory:true}
        }else{
          return {name:item,isDirectory:true}
        }
    })
    console.log('this.pathNodeObj',JSON.stringify(this.pathNodeObj))
    // 滚动到末尾
    this.navScroller.scrollPage({next:true})
  }

  /**
   * 点击导航栏单个fileNode时触发
   */
  async  clickNavFileNode(file:fileNodeType,index:number){
    console.log('点击了',JSON.stringify(file),index,JSON.stringify( this.pathNodeObj))
    if(this.filePath !== '/'){
      const  nowPath = this.pathNodeObj.slice(1,index+1).map(item=>item.name)
      this.filePath =nowPath.length === 0? '/': '/'+ nowPath.join('/') + '/'
    }
    this.selectedFileName = ''
    this.selectedFile = null
    this.filePathChange()
  }

  // 工具栏滚动条
  private  navScroller: Scroller = new Scroller()
  // 工具栏
  @Builder NavigatorToolbar(){
      Flex({space:{main:{value:10,unit:LengthUnit.VP}},alignItems:ItemAlign.Center,justifyContent:FlexAlign.Start}){
        Row({space:10}){
          Row(){
            if(this.fileDownLoading){
              Progress({ value: 20,total:100, type: ProgressType.Ring }).width(25).color(Color.White).style({status:ProgressStatus.LOADING})
            }else{
              Image($rawfile('FileLIcon/TerminalDownLoad.png')).width(40).onClick(()=>{
                if(this.selectedFile && !this.selectedFile.isDirectory){
                  if (this.fileUpLoading) {
                    this.getUIContext().getPromptAction().showToast({
                      message: $r('app.string.UPLOAD_IN_PROGRESS'),
                      showMode: promptAction.ToastShowMode.DEFAULT,
                      textColor: $r('app.color.warning_textColor'),
                      duration: 4000
                    });
                    return;
                  }else {
                    this.downLoadFile(this.selectedFile)
                  }
                }else{
                  this.getUIContext().getPromptAction().showToast({
                    message: this.selectedFile?.isDirectory?$r('app.string.FOLDER_DOWNLOAD_NOT_SUPPORTED'):$r('app.string.SELECT_FILE_TO_DOWNLOAD'),
                    showMode: promptAction.ToastShowMode.DEFAULT,
                    textColor: $r('app.color.warning_textColor'),
                    duration: 4000
                  });
                }
              })
            }
          }
          .justifyContent(FlexAlign.Center)
          .width(40)
          Row(){
            if(this.fileUpLoading){
              // Progress({ value: this.fileUpProgress, type: ProgressType.Ring }).width(100).width(40).color(Color.White)
              Progress({ value: 20,total:100, type: ProgressType.Ring }).width(25).color(Color.White).style({status:ProgressStatus.LOADING})
            }else{
              Image($rawfile('FileLIcon/TerminalUpLoad.png')).width(40).onClick(()=>{
                if (this.fileDownLoading) {
                  this.getUIContext().getPromptAction().showToast({
                    message: $r('app.string.DOWNLOAD_IN_PROGRESS'),
                    showMode: promptAction.ToastShowMode.DEFAULT,
                    textColor: $r('app.color.warning_textColor'),
                    duration: 4000
                  });
                  return;
                }else{
                  this.uploadFile()}
              }
              )
            }
          }
          .justifyContent(FlexAlign.Center)
          .width(40)
          Image($rawfile('FileLIcon/TerminalAdd.png')).width(40).onClick(()=>{
            this.openDialog('addDirectory')
          })
          Image($rawfile('FileLIcon/TerminalRename.png')).width(40).onClick(()=>{
            if (this.selectedFile) {
              this.openDialog('rename')
            }else{
              this.getUIContext().getPromptAction().showToast({
                message:$r('app.string.SELECT_TO_RENAME'),
                showMode:promptAction.ToastShowMode.DEFAULT,
                // textColor:Color.Orange,
                textColor:$r('app.color.warning_textColor'),
                duration:4000
              })
            }
          })
          Image($rawfile('FileLIcon/TerminalDelete.png')).width(40).onClick(()=>{
            if(this.selectedFile&&this.delConfirm){
              this.delConfirm.open()
            }else{
              this.getUIContext().getPromptAction().showToast({
                message:$r('app.string.SELECT_TO_DELETE'),
                showMode:promptAction.ToastShowMode.DEFAULT,
                textColor:$r('app.color.warning_textColor'),
                duration:4000
              })
            }
          })
          Image($rawfile('FileLIcon/port.png')).width(40).onClick(()=>{
            this.getPortForwardings()
          })
        }
        .width(290)
          Scroll(this.navScroller){
            Row(){
              ForEach(this.pathNodeObj,(fileNode:fileNodeType,INDEX:number)=>{
                this.NavigatorToolbarItem(fileNode,INDEX)
                // 如果当前节点不为最后一个时，加上>
                if(INDEX !== this.pathNodeObj.length -1){
                  Image($rawfile('FileLIcon/Next.png')).margin({left:5,right:5}).width(22)
                }
              },(fileNode:fileNodeType)=>JSON.stringify(fileNode))
            }
          }
          .width("calc(100% - 370vp)")
          .height('100%')
          .scrollable(ScrollDirection.Horizontal)
          .scrollBar(BarState.Off)
          .align(Alignment.Start)
          .border({width:1,color:'#303030'})
          .borderRadius(12)
          .padding({left:10,right:10})

        if(this.uploadProgress || this.downProgress){
          Progress({ value: this.uploadProgress?.progress || this.downProgress?.progress, total: 100, type: ProgressType.Capsule})
            .color(this.upOrDownType === 1?Color.Green:this.upOrDownType === 0?Color.Red:Color.Grey)
            .borderColor(Color.Grey)
            .value(this.uploadProgress?.progress|| this.downProgress?.progress)
            .width(70)
            .height(25)
            .style({
              content:`${this.uploadProgress?.progress|| this.downProgress?.progress || 0}%`,
              font:{size:14,weight:600,style: FontStyle.Italic},
              // fontColor:this.upOrDownType !== -1?Color.White:'#000000',
              fontColor:Color.White,
              borderColor:Color.Black,
            })
            .onClick(()=>{
              this.showNotification = !this.showNotification
            })
        }
      }
      .width('100%')
      .height(50)
  }
  // 工具栏文件节点
  @Builder NavigatorToolbarItem(fileNode:fileNodeType,INDEX:number){
    Row(){
      // 文件夹的symbol
      Text(fileNode.isDirectory?'󰃅':'󰂼')
        .fontSize(20)
        .fontColor(this.selectedFile && INDEX===this.pathNodeObj.length -1?'#fff':'#555555')
      Text(fileNode.name)
        .fontSize(20)
        .fontColor(this.selectedFile && INDEX===this.pathNodeObj.length -1?'#fff':'#555555')
        .margin({left:10})
    }
    .onClick(()=>{
      if(this.pathNodeObj.length-1 !== INDEX){
        this.clickNavFileNode(fileNode,INDEX)
      }
    })
  }

  build() {
      Column(){
        Notification({title:$r('app.string.FILE_TRANSFER'),show:this.showNotification,customPosition:{top:90,right:5}}){
          Column({space:14}){
            Flex({justifyContent:FlexAlign.SpaceBetween}){
              Row(){
                Text(this.uploadProgress?.remotePath.split('/').slice(-1)[0] || this.downProgress?.remotePath.split('/').slice(-1)[0] || $r('app.string.UNKNOWN_FILE'))
                  .fontSize(14)
                  .fontColor('#666666')
                  .width('50%')
                  .textOverflow({overflow:TextOverflow.Ellipsis})
                  .ellipsisMode(EllipsisMode.END)
                  .maxLines(1)
                if(this.uploadProgress){
                  Text((this.uploadProgress.completedFiles||0)+1 + '/' + this.uploadProgress.totalFiles).margin({left:16}).fontSize(14).fontColor('#666666')
                }
              }
              Text(`${this.uploadProgress?.progress || this.downProgress?.progress || 0}%`).fontSize(14).fontColor('#666666')
            }
            Flex({justifyContent:FlexAlign.SpaceBetween}){
              Text(((this.uploadProgress?.currentFileSize || this.downProgress?.fileSize || 0) / (1024*1024)).toFixed(2) + 'MB').fontSize(12).fontColor('#999999')
              if(this.uploadProgress){
                Text(this.uploadProgress?.speed.toFixed(2)+'MB/s').fontSize(12).fontColor('#999999')
              }else{
                Text(this.downProgress?.speed.toFixed(2)+'MB/s').fontSize(12).fontColor('#999999')
              }
            }
            Progress({value:(this.uploadProgress?.progress || this.downProgress?.progress || 0)}).color(((this.uploadProgress || this.downProgress)&&(this.upOrDownType === 1))?Color.Green:'#0A59F7')
            Button(this.upOrDownType===-1?$r('app.string.CANCEL_TRANSFER'):this.upOrDownType===0?$r('app.string.TRANSFER_CANCELLED'):$r('app.string.TRANSFER_SUCCESSFUL')).enabled(this.upOrDownType ===-1).fontColor('#0A59F7').backgroundColor('#F1F1F1').fontWeight(500).fontSize(14).width('100%').onClick(()=>{
              if (this.nowAllConfigData?.Session?.connectionType === 'SSH') {
                this.closeTransmission()
              }
            })
          }

        }
        Row() {
          Column(){
            Row({space:10}){
              Image($rawfile('FileLIcon/Terminal.png')).width(40)
              Text('CrossShellNext').fontColor(Color.White).fontSize(20)
            }
            .justifyContent(FlexAlign.Start)
            .padding({top:15,bottom:20,left:15})
            FileList({
              clickListItem:this.clickListItem,
              doubleClick:this.doubleClick,
              dataSource:this.dataSource,
              filePath:this.filePath,
              selectedFileName:this.selectedFileName
            })
              .width('100%')
              .height('calc(100% - 35vp)')
          }
          .onDrop((event: DragEvent, extraParams?: string | undefined)=>{
            this.dropFileIn(event)
          })
          .constraintSize({
            minWidth:220,
            maxWidth:260,
          })
          .width('20%')
          .height('100%')
          .border({color:'#303030',width:1})
          .backgroundColor('#1C1C1C')
          .visibility(this.showFlag ? Visibility.None : Visibility.Visible)
          Column(){
            Column(){
              this.NavigatorToolbar()
            }
            .onDrop((event: DragEvent, extraParams?: string | undefined)=>{
              this.dropFileIn(event)
            })
            .width('100%')
            .focusable(true)
            .enabled(!this.fileListLoading)
            .justifyContent(FlexAlign.Center)
            .alignItems(HorizontalAlign.Center)
            .border({width:1,color:'#303030'})
            .borderRadius(0)
            .height(90)
            .padding({left:16,right:16})
            .backgroundColor('#1C1C1C')
            .visibility(this.showFlag ? Visibility.None : Visibility.Visible)
            // 仅在拿到有效 sessionId 后再挂载 Terminal，避免以 -1 键登记端口
            if (this.sessionId >= 0 && this.nowAllConfigData?.Session.connectionType !== 'Serial') {
              Terminal({configDataSource:this.nowAllConfigData,sessionId:this.sessionId})
                .width('100%')
                .backgroundColor("#181818")
                .height(this.showFlag ? "100%" : 'calc(100% - 110vp)')
                .focusable(true)
            } else if(this.nowAllConfigData?.Session.connectionType === 'Serial'){
              SerialTerminal({SerialDataSource:this.nowAllConfigData.Serial})
                .width('100%')
                .backgroundColor($r('sys.color.comp_background_list_card'))
                .height(this.showFlag ? "100%" : 'calc(100% - 110vp)')
                .focusable(true)
            } else {
              Text($r('app.string.terminal_init'))
                .fontColor('#888')
                .fontSize(14)
                .width('100%')
                .backgroundColor("#181818")
                .height(this.showFlag ? "100%" : 'calc(100% - 110vp)')
                .focusable(true)
            }
          }
          .focusable(true)
          .alignItems(HorizontalAlign.Start)
          .height('100%')
          .width(this.showFlag ? '100%' : '80%')
          .constraintSize({
            minWidth: this.showFlag ? '100%' : 'calc(100% - 260vp)',
            maxWidth: this.showFlag ? '100%' : 'calc(100% - 220vp)',
          })
        }
        .focusable(true)
        .width('100%')
        .height('100%')
      }
      .focusable(true)

      .focusable(true)
      .width('100%')
      .height('100%')
      .backgroundColor("#181818")
    }
}

