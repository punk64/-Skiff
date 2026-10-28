import SelectList from '../../../components/SelectList/index'
import { itemType } from '../../../components/SelectList/type';
import deepClone from '../../../common/tools/deepClone'
import { encryptProxyPassword, decryptProxyPassword } from '../../../common/utils/ProxyCredentialUtil'
import { common, Want } from '@kit.AbilityKit';
import { BusinessError, emitter } from '@kit.BasicServicesKit';
import { distributedKVStore } from '@kit.ArkData';
import { KvManager } from '../../../common/utils/KVManager'
import DSerials from 'ets/pages/entryPage/Connection/Serial/index'
import {
  SessionType,
  TerminalType,
  ConnectionType,
  allDataType,
  SerialType,
  TelnetType,
  TunnelsType,
  WindowSizeType,
  CharacterType,
  CursorType,
  ColorType,
  LoggingType,
  SelectType,
  KeyGenType,
  AppearType,
  ProxyType,
  CloseWindowMode
} from '../../../common/types/dataType'
import { pageDefaultData } from '../pageDefaultData';
import { fileIo as fs, picker } from '@kit.CoreFileKit';
import { pointer } from '@kit.InputKit';
import { LengthMetrics, LengthUnit, promptAction } from '@kit.ArkUI';
// import DSerials from '../Connection/Serial';
// import DTelnet from '../Connection/Telnet';
import json from '@ohos.util.json';

// import { CloseWindowMode } from '../layout/BottomButton';

interface itemParamsType {
  key?: string;
  data?: allDataType
}

export const connectionType: string = 'connectionType'

@Component
export default struct DSessionIndex {
  @State @Watch('nowSessionDataChange') nowData: SessionType = deepClone(pageDefaultData.Session)
  @StorageProp('currentColorMode') @Watch('onColorModeChange') currentColorMode: number = -1; // 监听当前颜色模式
  @State listSelected: itemType<itemParamsType> = {
    label: []
  } //当前选中的数据
  @State selectIndex: number = -1 //当前选中的列表索引
  // @State selectList:itemType<itemParamsType>[]=[{label:['default Settings'],params:{key:'default',data:pageDefaultData}}] //选中列表数据源
  @State selectList: itemType<itemParamsType>[] = [] //选中列表数据源
  @State privateKeyFile: fs.File | null = null
  private context = getContext(this) as common.UIAbilityContext
  @LocalStorageLink('saveSession') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData: string = ''
  // KVManager实例
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined
  // 列表数据的Store实例
  private listKvStore: distributedKVStore.SingleKVStore | undefined = undefined
  //连接类型
  @LocalStorageLink(connectionType) connectionType: 'SSH' | 'Telnet' | 'Serial' = 'SSH'

  @Styles
  inputTextStyle(){
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }

  @Styles
  ButtonStyles(){
    .border({ width: 1, style: BorderStyle.Solid })
    .borderColor(0xDCDCDC)
    .borderRadius(5)
    // .width('15%')
    .height(30)
    .margin({
      left: 10
    })
  }

  @Styles
  hoverStyles() {
    .hoverEffect(HoverEffect.Highlight).onHover((isHover, event) => {
      if (isHover) {
        this.getUIContext().getCursorController().setCursor(pointer.PointerStyle.HAND_POINTING)
      } else {
        this.getUIContext().getCursorController().restoreDefault()
      }
    })
  }

  // 更新数据并强制UI刷新
  updateDataAndRefreshUI() {
    this.saveStore();

    // 创建一个新对象，强制UI刷新
    let clonedData = deepClone(this.nowData);
    this.nowData = clonedData;
    // 不打印含会话凭据的整包日志
  }

  //选择退出时关闭窗口模式
  selectCloseWindowMode(mode: CloseWindowMode) {
    console.log('sx 切换模式：' + mode)
    this.nowData.closeWindowMode = mode
  }

  radioChange(value: string) {
    console.log('切换连接类型为:', value);
    this.nowData.connectionType = value

    // 确保在切换到Telnet时进行初始化
    if (value === 'Telnet' && !this.nowData.Telnet) {
      this.nowData.Telnet = deepClone(pageDefaultData.Telnet)
      // 不打印 Telnet 凭据
    }

    // 更新数据并刷新UI
    this.updateDataAndRefreshUI();
  }

  aboutToAppear() {
    // 当页面挂载之前，初始化页面
    console.log('aboutToAppear')
    this.init()
  }

  // 每次页面显示时调用，确保UI状态正确
  onPageShow() {
    console.log('onPageShow被调用，当前颜色模式:', this.currentColorMode);

    // 如果当前连接类型是Telnet，确保加载正确的Telnet数据
    if (this.nowData.connectionType === 'Telnet') {
      this.reloadTelnetData();
    }
  }

  // 重新加载Telnet数据的方法
  reloadTelnetData() {
    // 从KVStore获取最新的Telnet数据
    this.allDataKvStore?.get('Telnet').then((telnetData) => {
      if (telnetData) {
        try {
          const parsedTelnetData = JSON.parse(telnetData as string) as TelnetType;
          // 不打印 Telnet 凭据

          // 确保nowData.Telnet存在
          if (!this.nowData.Telnet) {
            this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
          }

          // 合并数据
          this.nowData.Telnet.ambiguity = parsedTelnetData.ambiguity || this.nowData.Telnet.ambiguity;
          this.nowData.Telnet.mode = parsedTelnetData.mode || this.nowData.Telnet.mode;
          this.nowData.Telnet.keyboard = parsedTelnetData.keyboard;
          this.nowData.Telnet.telnetNewLine = parsedTelnetData.telnetNewLine;
          this.nowData.Telnet.hostName = parsedTelnetData.hostName || this.nowData.Telnet.hostName;
          this.nowData.Telnet.port = parsedTelnetData.port || this.nowData.Telnet.port;

          // 强制刷新UI
          this.updateDataAndRefreshUI();
        } catch (e) {
          console.error('解析Telnet数据失败', e);
        }
      }
    }).catch((error: Error) => {
      console.error('获取Telnet数据失败:', error);
    });
  }

  // 颜色模式变化回调函数
  onColorModeChange() {
    console.log('颜色模式已变更, 新的颜色模式:', this.currentColorMode)

    // 确保在颜色模式改变时保留Telnet设置
    // 首先从KVStore获取最新的Telnet数据
    if (this.nowData.connectionType === 'Telnet') {
      this.reloadTelnetData();
    } else {
      // 其他连接类型，直接刷新UI
      this.updateDataAndRefreshUI();
    }
  }

  aboutToDisappear(): void {
    console.log('销毁')
    if (this.nowData.privateKeyFd > -1) {
      try {
        fs.close(this.nowData.privateKeyFd)
      } catch (e) {
        console.log('关闭文件失败：' + e.message)
      }
    }
    try {
      this.allDataKvStore?.off('dataChange')
    } catch (e) {
      console.log('移除数据监听失败：' + e.message)
    }
  }

  // 页面初始化
  async init(): Promise<void> {
    this.listKvStore = await this.KVManager.getStore('SessionList')
    this.allDataKvStore = await this.KVManager.getStore('allData')
    const list = await this.listKvStore.get('SessionList') as string
    this.selectList = JSON.parse(list)
    this.nowData = deepClone(pageDefaultData.Session)
    this.defaultData = JSON.stringify(pageDefaultData)
    this.saveSession = 'default settings'

    // 确保AppStorage中的currentColorMode值已设置
    if (AppStorage.Has('currentColorMode')) {
      let colorMode: number = AppStorage.Get('currentColorMode') as number;
      console.log('初始化时从AppStorage获取的颜色模式:', colorMode);
    } else {
      // 如果AppStorage中没有，则设置一个默认值
      console.log('AppStorage中没有颜色模式，设置默认值');
      AppStorage.SetOrCreate('currentColorMode', -1);
    }

    // 监听Serial的数据变化
    this.allDataKvStore.on('dataChange', distributedKVStore.SubscribeType.SUBSCRIBE_TYPE_LOCAL, async (data) => {
      console.log(JSON.stringify(data.updateEntries))
      data.updateEntries.map(item => {
        if (item.key === 'Serial') {
          this.nowData.speed = (JSON.parse(item.value.value as string) as SerialType).speed
          // this.nowData.serialLine = (JSON.parse(item.value.value as string) as SerialType).serialLine
        }
      })
    })
    this.saveStore()
  }

  // 持久化数据
  async saveStore() {
    // 不打印含会话凭据的整包日志
    
    // 保存Session数据（会话密码经 HUKS 加密后落 KV；加密失败则保留原文兜底，避免丢密码）
    const toStore = deepClone(this.nowData)
    try {
      toStore.password = await encryptProxyPassword(this.nowData.password)
    } catch (e) {
      console.error('会话密码加密失败，按明文保存', e)
    }
    this.allDataKvStore && this.allDataKvStore.put('Session', JSON.stringify(toStore));
    
    // 如果是Telnet类型，同时更新Telnet数据
    if (this.nowData.connectionType === 'Telnet' && this.nowData.Telnet) {
      // 不打印 Telnet 凭据
      this.allDataKvStore && this.allDataKvStore.put('Telnet', JSON.stringify(this.nowData.Telnet));
    }
  }

  // nowData变更时触发
  nowSessionDataChange() {
    this.saveStore()
  }

  // 双击列表Item事件
  doubleItemClick(item: itemType<object>) {
    // 将Context对象传递过去
    let wantInfo: Want = {
      bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
      abilityName: 'shellWindowAbility', //您自己新建的UIability
      parameters: {
        // 自定义信息
        info: item
      },
    }
    if (this.context) {
      // context为调用方UIAbility的UIAbilityContext
      this.context.startAbility(wantInfo).then(() => {
        console.log('startAbility success.');
      }).catch((error: BusinessError) => {
        console.log('startAbility failed.', error);
      });
    }
  }

  // 选择密钥文件
  async selectKeyFile() {
    if (this.nowData.privateKeyFd) {
      try {
        // 重新选择时关闭已打开的密钥文件
        fs.closeSync(this.nowData.privateKeyFd)
      } catch (e) {
        console.log('关闭文件失败' + e.code + e.message)
      }
    }
    const documentSelectOptions = new picker.DocumentSelectOptions();
    documentSelectOptions.fileSuffixFilters = ['.pem']
    documentSelectOptions.maxSelectNumber = 1
    // 创建DocumentViewPicker实例
    let documentPicker = new picker.DocumentViewPicker(this.context)
    // 调用 select 方法让用户选择保存位置（不让用户自定义文件名）
    const file = await documentPicker.select(documentSelectOptions)
    if (file && file.length > 0) {
      // 验证文件后缀名是否为.pem
      if (!file[0].toLowerCase().endsWith('.pem')) {
        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.Session_Key_Select_Error_Toast'),
          duration: 3000
        })
        return
      }

      const selectFile = fs.openSync(file[0], fs.OpenMode.READ_WRITE)
      // 公钥文件句柄
      this.nowData.privateKeyFd = selectFile.fd
      this.nowData.privateKeyName = selectFile.name
      this.privateKeyFile = selectFile
      // 保存密钥文件至沙箱中
      this.nowData.privateKeyPath = await this.saveProvideKey(this.privateKeyFile)
      this.saveStore()
    }
  }

  // 将密钥文件保存,返回存储的文件路径
  async saveProvideKey(file: fs.File): Promise<string> {
    const resFilePath = this.context.filesDir
    const newFileName: string = file.name
    try {
      await fs.copyFile(file.fd, resFilePath + '/' + newFileName);
      const fileList: string[] = await fs.listFile(resFilePath)
      console.log('copyType', fileList, newFileName)
    } catch (e) {
      console.error('saveProvideKey', e)
    }
    return resFilePath + '/' + newFileName
  }

  // 将密钥文件读取出来
  async readProvideKey(fileName: string): Promise<fs.File> {
    const resFile = this.context.filesDir
    const list: string[] = await fs.listFile(resFile)
    console.log('filePath----------->', list)
    const file: fs.File = await fs.open(resFile + `/${fileName}`)
    return file
  }

  // load按钮点击回调
  async load() {
    if (this.nowData.privateKeyPath) {
      try {
        console.log('关闭已经打开的文件')
        const openFile: fs.File = await fs.open(this.nowData.privateKeyPath)
        fs.closeSync(openFile.fd)
      } catch (e) {
        console.error(e)
      }
    }
    // 不打印含全量会话凭据的 selectList
    // 不打印含会话凭据的 listSelected
    // 将选中的item中的值深拷贝给nowData
    if (this.listSelected.params && this.listSelected.params.data) {
      this.nowData = deepClone(this.listSelected.params.data.Session)
      // 兼容历史会话：proxyPolicy 缺失时补默认值
      if (this.nowData.proxyPolicy === undefined || this.nowData.proxyPolicy === null) {
        this.nowData.proxyPolicy = 'global'
      }
      // 加载会话：若密码为 H1: 密文则解密为明文，供密码框正确显示
      try {
        if (this.nowData.password && this.nowData.password.startsWith('H1:')) {
          this.nowData.password = await decryptProxyPassword(this.nowData.password)
        }
      } catch (e) {
        console.error('会话密码解密失败', e)
      }
      // 不打印含会话凭据的 listSelected
      this.defaultData = JSON.stringify(this.listSelected.params.data)
      // const findIndex =
      //   this.selectList.findIndex((value: itemType<itemParamsType>) => value.label[0] == this.nowData.savedSessions)
      // if(findIndex>=0){
      //   if(this.selectList[findIndex].params){
      //     this.defaultData = JSON.stringify(this.selectList[findIndex].params?.data)
      //   }else{
      //     this.defaultData = JSON.stringify(this.listSelected.params.data)
      //   }
      //   // console.log('selectList   ',JSON.stringify(this.selectList[existIndex]))
      // }

      this.saveSession = this.nowData.savedSessions + Date.now()
      console.log('zfr saveSession',this.saveSession)
      // 不打印含会话凭据的 nowData
      
      // 加载Telnet数据
      if (this.nowData.connectionType === 'Telnet') {
        // 如果有直接保存在Session中的Telnet数据，使用它
        if (!this.nowData.Telnet && this.listSelected.params.data.Telnet) {
          this.nowData.Telnet = deepClone(this.listSelected.params.data.Telnet)
          // 不打印 Telnet 凭据
        }
      }
      
      if (this.nowData.privateKeyName) {
        this.privateKeyFile = await this.readProvideKey(this.nowData.privateKeyName)
        this.nowData.privateKeyFd = this.privateKeyFile.fd
        this.nowData.privateKeyName = this.privateKeyFile.name
      }
      this.saveStore()
    }
  }

  // reset按钮点击回调
  reset() {
    try {
      // 重置kvManager中的值为默认值
      Object.entries(pageDefaultData).forEach((item: Array<object | string>, index) => {
        this.allDataKvStore?.put(item[0] as string, JSON.stringify(item[1]))
      })

      console.log('reset 时pageDefaultData', json.stringify(pageDefaultData.Cursor))
      this.nowData = deepClone(pageDefaultData.Session)
      this.defaultData = JSON.stringify(pageDefaultData)
      this.saveSession = 'default Settings' + new Date().getTime()
      console.log('this.nowData', JSON.stringify(pageDefaultData.Session))
    } catch (e) {
      console.error('重置错误', e)
    }
  }

  // save按钮点击回调
  //todo 保存
  async save() {
     if (this.nowData.savedSessions.trim().length <= 0){
       this.getUIContext().getPromptAction().showToast({
         message:$r('app.string.Session_Save_Error_Toast'),
         duration:2000,
       })
       return
     }
    const Session = await this.allDataKvStore?.get('Session')
    const Logging = await this.allDataKvStore?.get('Logging')
    const Terminal = await this.allDataKvStore?.get('Terminal')
    const Connection = await this.allDataKvStore?.get('Connection')
    const Serial = await this.allDataKvStore?.get('Serial')
    const Telnet = await this.allDataKvStore?.get('Telnet')
    const Tunnels = await this.allDataKvStore?.get('Tunnels')
    const WindowSize = await this.allDataKvStore?.get('WindowSize')
    const Character = await this.allDataKvStore?.get('Character')
    const Cursor = await this.allDataKvStore?.get('Cursor')
    const Color = await this.allDataKvStore?.get('Color')
    const Select = await this.allDataKvStore?.get('Select')
    const keyGen = await this.allDataKvStore?.get('KeyGen')
    const Appear = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Appear')
    // 保存会话前先把代理面板当前值同步到 KV（面板值可能尚未落盘），确保会话快照携带最新代理配置
    const preProxy = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Proxy')
    AppStorage.SetOrCreate('saveProxy', 'save' + Date.now())
    let Proxy = preProxy
    const proxyPoll = Date.now() + 800
    while (Date.now() < proxyPoll) {
      await new Promise<void>((resolve) => setTimeout(() => resolve(), 60))
      const cur = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Proxy')
      if (JSON.stringify(cur) !== JSON.stringify(preProxy)) {
        Proxy = cur
        break
      }
    }


    // 确保内嵌的Telnet数据被正确保存到KVStore
    if (this.nowData.connectionType === 'Telnet' && this.nowData.Telnet) {
      await this.allDataKvStore?.put('Telnet', JSON.stringify(this.nowData.Telnet))
      // 不打印 Telnet 凭据
    }

    // 不打印含会话凭据的 nowData
    const data: allDataType = {
      Session: JSON.parse(Session as string) as SessionType,
      Logging: JSON.parse(Logging as string) as LoggingType,
      Terminal: JSON.parse(Terminal as string) as TerminalType,
      Connection: JSON.parse(Connection as string) as ConnectionType,
      Serial: JSON.parse(Serial as string) as SerialType,
      Telnet: JSON.parse(Telnet as string) as TelnetType,
      Tunnels: JSON.parse(Tunnels as string) as TunnelsType,
      WindowSize: JSON.parse(WindowSize as string) as WindowSizeType,
      Character: JSON.parse(Character as string) as CharacterType,
      Cursor: JSON.parse(Cursor as string) as CursorType,
      Color: JSON.parse(Color as string) as ColorType,
      Select: JSON.parse(Select as string) as SelectType,
      KeyGen: JSON.parse(keyGen as string) as KeyGenType,
      Appear: JSON.parse(Appear as string) as AppearType,
      Proxy: JSON.parse(Proxy as string) as ProxyType
    }
    // 不打印含全量会话凭据的 allData
    // 将页面的值包装成item对象
    const itemObj: itemType<itemParamsType> = {
      label: [this.nowData.savedSessions],
      params: {
        data
      }
    }
      // 查找列表中是否存在当前的sessions名称
      const existIndex =
        this.selectList.findIndex((value: itemType<itemParamsType>) => value.label[0] == this.nowData.savedSessions)
      if (existIndex > -1) {
        this.selectList[existIndex] = itemObj
        this.selectIndex = existIndex
      } else {
        this.selectList.push(itemObj)
      }
// 不打印含全量会话凭据的 selectList
      if (this.listKvStore !== undefined) {
        this.listKvStore.put('SessionList', JSON.stringify(this.selectList))
        // 不打印含全量会话凭据的 selectList
      }

    this.selectList = deepClone(this.selectList)
    this.getUIContext().getPromptAction().showToast({
      message:$r('app.string.Save_Success_Toast'),
      duration:2000,
    })
    // 不打印含全量会话凭据的 allData
  }

  // delete按钮点击回调
  delete() {
    if (this.listSelected && this.listSelected.params && this.listSelected.params.key !== 'default' &&
      this.listKvStore !== undefined) {
      this.selectList.splice(this.selectIndex, 1)
      this.selectIndex = -1
      this.listKvStore.put('SessionList', JSON.stringify(this.selectList))
      // 不打印含全量会话凭据的 selectList
    }
  }

  async radioChangeLoginMethod(loginType: 'password' | 'publickey') {
    this.nowData.authType = loginType
  }

  build() {
    Column({ space: 20 }) {
      Text($r('app.string.session_title')).fontSize(14).textAlign(TextAlign.Start).fontColor('#999')
        .margin({
          top: 5
        })
      Column({ space: 10 }) {
        // TitleCard({ titleString: $r('app.string.connect_inter_version')}) {
        Text($r('app.string.connection_type')).textAlign(TextAlign.Start).width('100%')
        Row({ space: 32 }) {
          Row({ space: 8 }) {
            Radio({ value: 'SSH', group: 'TypeRadio' })
              .checked(this.nowData.connectionType === 'SSH')
              .onChange((isChecked:boolean)=>{
                if(isChecked){
                  this.radioChange('SSH')
                  this.connectionType = "SSH"
                }
              })
            Text('SSH')
              .fontColor(this.nowData.connectionType === 'SSH' ? $r('app.color.light_Select') :
              $r('app.color.light_noSelect'))
          }

          Row({ space: 8 }) {
            Radio({ value: 'Telnet', group: 'TypeRadio' })
              .checked(this.nowData.connectionType == 'Telnet')
              .onChange((isChecked:boolean)=>{
                if(isChecked){
                  this.radioChange('Telnet')
                  this.connectionType = "Telnet"
                }
              })
            Text('Telnet')
              .fontColor(this.nowData.connectionType == 'Telnet' ? $r('app.color.light_Select') :
              $r('app.color.light_noSelect'))
          }

          Row({ space: 8 }) {
            Radio({ value: 'Serial', group: 'TypeRadio' })
              .checked(this.nowData.connectionType == 'Serial')
              .onChange((isChecked:boolean)=>{
                if(isChecked){
                  this.radioChange('Serial')
                  this.connectionType = "Serial"
                }
              })
            Text('Serial')
              .fontColor(this.nowData.connectionType == 'Serial' ? $r('app.color.light_Select') :
              $r('app.color.light_noSelect'))
          }

        }.justifyContent(FlexAlign.Start)
        .width('100%')

        DSerials().visibility(this.nowData.connectionType == 'Serial' ? Visibility.Visible : Visibility.None)
        // DTelnet().visibility(this.nowData.connectionType == 'Telnet' ? Visibility.Visible : Visibility.None)



        // 直接集成Telnet功能
        if (this.nowData.connectionType == 'Telnet') {
          Column({ space: 20 }) {
            Flex({ direction: FlexDirection.Row, justifyContent: FlexAlign.SpaceBetween }) {
              Column() {
                Text($r('app.string.hostname'))
                  .textAlign(TextAlign.Start)
                  .width('100%')
                  .textOverflow({ overflow: TextOverflow.MARQUEE })
                  .margin({ bottom: 5 })
                TextInput({ text: this.nowData.Telnet ? this.nowData.Telnet.hostName : '' })
                  .inputTextStyle()
                  .hoverStyles()
                  .width('100%')
                  .maxLength(100)
                  .onChange((value: string) => {
                    console.log('Telnet hostname changed to:', value);
                    if (!this.nowData.Telnet) {
                      this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                    }
                    this.nowData.Telnet.hostName = value;
                    // 更新数据并刷新UI
                    this.updateDataAndRefreshUI();
                  })
              }.width('45%')

              Column() {
                Text($r('app.string.Port'))
                  .textAlign(TextAlign.Start)
                  .width('100%')
                  .textOverflow({ overflow: TextOverflow.MARQUEE })
                  .margin({ bottom: 5 })
                TextInput({ text: this.nowData.Telnet ? this.nowData.Telnet.port : '' })
                  .inputTextStyle()
                  .hoverStyles()
                  .width('100%')
                  .onChange((value: string) => {
                    console.log('Telnet port changed to:', value);
                    if (!this.nowData.Telnet) {
                      this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                    }
                    this.nowData.Telnet.port = value;
                    // 更新数据并刷新UI
                    this.updateDataAndRefreshUI();
                  })
              }.width('45%')
            }.width('100%')

            Column({ space: 16 }) { // 调整垂直间距
              Text($r('app.string.ambiguity'))
                .fontSize(16)
                .fontWeight(FontWeight.Regular)

              Flex({ justifyContent: FlexAlign.SpaceBetween }) {
                Row({ space: 5 }) {
                  Radio({ value: 'BSD', group: 'TelnetAmbiguityRadioGroup' })
                    .checked(this.nowData.Telnet?.ambiguity === 'BSD')
                    .onChange((isChecked: boolean) => {
                      if (isChecked) {
                        console.log('Changing ambiguity to BSD');
                        if (!this.nowData.Telnet) {
                          this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                        }
                        this.nowData.Telnet.ambiguity = 'BSD';
                        // 更新数据并刷新UI
                        this.updateDataAndRefreshUI();
                      }
                    })
                  Text($r('app.string.BSD'))
                    .fontSize(14)
                    .fontColor(this.nowData.Telnet?.ambiguity === 'BSD' ? $r('app.color.light_Select') :
                    $r('app.color.light_noSelect'))
                    .margin({ left: 8 })
                }.width('50%').onClick(() => {
                  console.log('BSD row clicked');
                  if (!this.nowData.Telnet) {
                    this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                  }
                  this.nowData.Telnet.ambiguity = 'BSD';
                  // 更新数据并刷新UI
                  this.updateDataAndRefreshUI();
                })

                Row({ space: 5 }) {
                  Radio({ value: 'RFC', group: 'TelnetAmbiguityRadioGroup' })
                    .checked(this.nowData.Telnet?.ambiguity === 'RFC')
                    .onChange((isChecked: boolean) => {
                      if (isChecked) {
                        console.log('Changing ambiguity to RFC');
                        if (!this.nowData.Telnet) {
                          this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                        }
                        this.nowData.Telnet.ambiguity = 'RFC';
                        // 强制更新数据并刷新UI
                        this.updateDataAndRefreshUI();
                      }
                    })
                  Text($r('app.string.RFC'))
                    .fontSize(14)
                    .fontColor(this.nowData.Telnet?.ambiguity === 'RFC' ? $r('app.color.light_Select') :
                    $r('app.color.light_noSelect'))
                    .margin({ left: 8 })
                }.width('50%').onClick(() => {
                  console.log('RFC row clicked');
                  if (!this.nowData.Telnet) {
                    this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                  }
                  this.nowData.Telnet.ambiguity = 'RFC';
                  // 更新数据并刷新UI
                  this.updateDataAndRefreshUI();
                })
              }

              Text($r('app.string.Telnetnegotiationmode'))
                .fontSize(16)
                .fontWeight(FontWeight.Regular)
                .margin({ top: 8 })

              Flex({ justifyContent: FlexAlign.SpaceBetween }) {
                Row({ space: 5 }) {
                  Radio({ value: 'Passive', group: 'TelnetModeRadioGroup' })
                    .checked(this.nowData.Telnet?.mode === 'Passive')
                    .onChange((isChecked: boolean) => {
                      if (isChecked) {
                        console.log('Changing mode to Passive');
                        if (!this.nowData.Telnet) {
                          this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                        }
                        this.nowData.Telnet.mode = 'Passive';
                        // 更新数据并刷新UI
                        this.updateDataAndRefreshUI();
                      }
                    })
                  Text($r('app.string.Passive'))
                    .fontSize(14)
                    .fontColor(this.nowData.Telnet?.mode === 'Passive' ? $r('app.color.light_Select') :
                    $r('app.color.light_noSelect'))
                    .margin({ left: 8 })
                }.width('50%').onClick(() => {
                  console.log('Passive row clicked');
                  if (!this.nowData.Telnet) {
                    this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                  }
                  this.nowData.Telnet.mode = 'Passive';
                  // 更新数据并刷新UI
                  this.updateDataAndRefreshUI();
                })

                Row() {
                  Radio({ value: 'Active', group: 'TelnetModeRadioGroup' })
                    .checked(this.nowData.Telnet?.mode === 'Active')
                    .onChange((isChecked: boolean) => {
                      if (isChecked) {
                        console.log('Changing mode to Active');
                        if (!this.nowData.Telnet) {
                          this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                        }
                        this.nowData.Telnet.mode = 'Active';
                        // 更新数据并刷新UI
                        this.updateDataAndRefreshUI();
                      }
                    })
                  Text($r('app.string.Active'))
                    .fontSize(14)
                    .fontColor(this.nowData.Telnet?.mode === 'Active' ? $r('app.color.light_Select') :
                    $r('app.color.light_noSelect'))
                    .margin({ left: 8 })
                }.width('50%').onClick(() => {
                  console.log('Active row clicked');
                  if (!this.nowData.Telnet) {
                    this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                  }
                  this.nowData.Telnet.mode = 'Active';
                  // 更新数据并刷新UI
                  this.updateDataAndRefreshUI();
                })
              }

              Row({ space: 5 }) {
                Checkbox({ name: 'keyboardSends', group: 'TelnetKeyboardGroup' })
                  .select(this.nowData.Telnet?.keyboard ?? false)
                  .shape(CheckBoxShape.ROUNDED_SQUARE)
                  .onChange((value: boolean) => {
                    console.log('Keyboard checkbox changed to:', value);
                    if (!this.nowData.Telnet) {
                      this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                    }
                    this.nowData.Telnet.keyboard = value;
                    // 更新数据并刷新UI
                    this.updateDataAndRefreshUI();
                  })
                Text($r('app.string.Telnetspecialcommands'))
                  .fontSize(14)
                  .fontColor(this.nowData.Telnet?.keyboard ? $r('app.color.light_Select') :
                  $r('app.color.light_noSelect'))
                  .margin({ left: 8 })
              }.width('100%')

              Row({ space: 5 }) {
                Checkbox({ name: 'telnetNewLine', group: 'TelnetKeyboardGroup' })
                  .select(this.nowData.Telnet?.telnetNewLine ?? false)
                  .shape(CheckBoxShape.ROUNDED_SQUARE)
                  .onChange((value: boolean) => {
                    console.log('NewLine checkbox changed to:', value);
                    if (!this.nowData.Telnet) {
                      this.nowData.Telnet = deepClone(pageDefaultData.Telnet);
                    }
                    this.nowData.Telnet.telnetNewLine = value;
                    // 更新数据并刷新UI
                    this.updateDataAndRefreshUI();
                  })
                Text($r('app.string.TelnetNewLine'))
                  .fontSize(14)
                  .fontColor(this.nowData.Telnet?.telnetNewLine ? $r('app.color.light_Select') :
                  $r('app.color.light_noSelect'))
                  .margin({ left: 8 })
              }.width('100%')
            }.alignItems(HorizontalAlign.Start)
          }
        }

        // SSH功能
        Column() {
          Text($r('app.string.session_select')).textAlign(TextAlign.Start).width('100%')
          Flex({ direction: FlexDirection.Row }) {
            Row() {
              Radio({ value: 'SSH', group: 'radioGroup' })
                .focusOnTouch(true)
                .checked(this.nowData.authType === 'password')
                .onClick((): Promise<void> => this.radioChangeLoginMethod('password'))
              Text($r('app.string.hostname'))
                .textAlign(TextAlign.Start)
                .width('75%')
                .textOverflow({ overflow: TextOverflow.MARQUEE })
                .fontColor(this.nowData.authType == 'password' ? $r('app.color.light_Select') :
                $r('app.color.light_noSelect'))
            }.width('65%').hoverStyles().onClick((): Promise<void> => this.radioChangeLoginMethod('password'))

            Row() {
              Radio({ value: 'Serial', group: 'radioGroup' })
                .focusOnTouch(true)
                .checked(this.nowData.authType === 'publickey')
                .onClick((): Promise<void> => this.radioChangeLoginMethod('publickey'))
              Text($r('app.string.Keylogin'))
                .fontColor(this.nowData.authType == 'publickey' ? $r('app.color.light_Select') :
                $r('app.color.light_noSelect'))
            }
            .width('35%')
            .hoverStyles()
            .onClick((): Promise<void> => this.radioChangeLoginMethod('publickey'))
          }.width('100%').margin({ top: 5, bottom: 5 })

          Column() {
            Flex({ direction: FlexDirection.Row, justifyContent: FlexAlign.SpaceBetween }) {
              Column() {
                Text($r('app.string.hostname'))
                  .textAlign(TextAlign.Start)
                  .width('100%')
                  .textOverflow({ overflow: TextOverflow.MARQUEE })
                  .margin({ bottom: 5 })
                TextInput({ text: $$this.nowData.hostName }).inputTextStyle().hoverStyles().width('100%').maxLength(100)
              }.width('30%')

              Column() {
                Text($r('app.string.UserName'))
                  .textAlign(TextAlign.Start)
                  .width('100%')
                  .textOverflow({ overflow: TextOverflow.MARQUEE })
                  .margin({ bottom: 5 })
                TextInput({ text: $$this.nowData.userName }).inputTextStyle().hoverStyles().width('100%').maxLength(50)
              }.width('30%')

              Column() {
                Text($r('app.string.Port'))
                  .textAlign(TextAlign.Start)
                  .width('100%')
                  .textOverflow({ overflow: TextOverflow.MARQUEE })
                  .margin({ bottom: 5 })
                TextInput({ text: $$this.nowData.port }).inputTextStyle().hoverStyles().width('100%').maxLength(50)
              }.width('30%')
            }.width('100%')

            Column() {
              Text($r('app.string.Password')).textAlign(TextAlign.Start).width('100%').margin({ top: 5, bottom: 5 })
              TextInput({ text: $$this.nowData.password })
                .inputTextStyle()
                .hoverStyles()
                .type(InputType.Password)
                .width('100%')
                .maxLength(50)
            }.width('100%').visibility(this.nowData.authType === 'password' ? Visibility.Visible : Visibility.None)

            Column() {
              Text($r('app.string.keyauthentication'))
                .textOverflow({ overflow: TextOverflow.MARQUEE })
                .textAlign(TextAlign.Start)
                .width('100%')
                .margin({ top: 5, bottom: 5 })
              Flex({
                direction: FlexDirection.Row,
                justifyContent: FlexAlign.SpaceBetween,
                alignItems: ItemAlign.Center
              }) {
                TextInput({ text: `${this.nowData.privateKeyName || ' '}` })
                  .inputTextStyle()
                  .hoverStyles()
                  .width('80%')
                  .enabled(false)
                Button($r('app.string.browse'), { type: ButtonType.Normal })
                  .buttonStyle(ButtonStyleMode.NORMAL)
                  .ButtonStyles()
                  .width('18%')
                  .fontColor($r('app.color.font_color'))
                  .fontSize(14)
                  .fontWeight(400)
                  .onClick(() => this.selectKeyFile())
              }
            }.width('100%').visibility(this.nowData.authType === 'publickey' ? Visibility.Visible : Visibility.None)
          }
        }.visibility(this.nowData.connectionType == 'SSH' ? Visibility.Visible : Visibility.None)
      }.width('100%').backgroundColor($r('app.color.titleCar_color')).borderRadius(10).padding(20)

      Text($r('app.string.session_load_save')).fontSize(14).textAlign(TextAlign.Start).fontColor('#999')
        .margin({
          top: 5,
        })
      Column({ space: 10 }) {
        Text($r('app.string.session_save')).textAlign(TextAlign.Start).width('100%')
        Flex({
          direction: FlexDirection.Row,
          justifyContent: FlexAlign.SpaceBetween,
          space: { main: { value: 5, unit: LengthUnit.VP } }
        }) {
          TextInput({ text: $$this.nowData.savedSessions })
            .inputTextStyle()
            .width('calc(100% - 340vp)')
            .hoverStyles()
            .maxLength(50)
          Button($r('app.string.load'), { type: ButtonType.Normal })
            .width(80)
            .buttonStyle(ButtonStyleMode.NORMAL)
            .ButtonStyles()
            .fontColor($r('app.color.font_color'))
            .fontSize(14)
            .fontWeight(400)
            .hoverStyles()
            .onClick(() => this.load())
          Button($r('app.string.reset'), { type: ButtonType.Normal })
            .width(80)
            .buttonStyle(ButtonStyleMode.NORMAL)
            .ButtonStyles()
            .fontColor($r('app.color.font_color'))
            .fontSize(14)
            .fontWeight(400)
            .hoverStyles()
            .onClick(() => this.reset())
          Button($r('app.string.Save'), { type: ButtonType.Normal })
            .width(80)
            .buttonStyle(ButtonStyleMode.NORMAL)
            .ButtonStyles()
            .fontColor($r('app.color.font_color'))
            .fontSize(14)
            .fontWeight(400)
            .hoverStyles()
            .onClick(() => this.save())
          Button($r('app.string.Delete'), { type: ButtonType.Normal })
            .width(80)
            .buttonStyle(ButtonStyleMode.NORMAL)
            .ButtonStyles()
            .fontColor($r('app.color.font_color'))
            .fontSize(14)
            .fontWeight(400)
            .hoverStyles()
            .onClick(() => this.delete())
        }

        Column() {
          SelectList({
            listData: this.selectList,
            selected: this.listSelected,
            selectIndex: this.selectIndex,
            defaultRow: 5,
            onDoubleClick:()=>{
              this.load()
              emitter.emit('openTerminal')
            }
          })
        }
        .width('100%')
        .alignItems(HorizontalAlign.Start)
      }.width('100%').backgroundColor($r('app.color.titleCar_color')).borderRadius(10).padding(20)

      //设置关闭模式
      Column() {
        Text($r('app.string.CloseWindowOnExit')).alignSelf(ItemAlign.Start)
        Flex({space:{main:LengthMetrics.vp(80)},wrap:FlexWrap.Wrap}) {
          Row({ space: 2 }) {
            Radio({ value: 'Always', group: 'CloseWindowMode' })
              .checked(this.nowData.closeWindowMode == CloseWindowMode.Always)
              .onChange((isChecked: boolean) => {
                console.log('sx 点击always')
                isChecked && this.selectCloseWindowMode(CloseWindowMode.Always)
              })
            Text($r('app.string.Always'))
          }

          Row({ space: 2 }) {
            Radio({ value: 'Never', group: 'CloseWindowMode' })
              .checked(this.nowData.closeWindowMode == CloseWindowMode.Never)
              .onChange((isChecked: boolean) => {
                console.log('sx 点击never')
                isChecked && this.selectCloseWindowMode(CloseWindowMode.Never)
              })
            Text($r('app.string.Never'))
          }

          Row({ space: 2 }) {
            Radio({ value: 'OnlyOnCleanExit', group: 'CloseWindowMode' })
              .checked(this.nowData.closeWindowMode == CloseWindowMode.OnlyOnCleanExit)
              .onChange((isChecked: boolean) => {
                console.log('sx 点击only')
                isChecked && this.selectCloseWindowMode(CloseWindowMode.OnlyOnCleanExit)
              })
            Text($r('app.string.OnlyOnCleanExit'))
          }
        }
        .width('100%')
      }.width('100%').backgroundColor($r('app.color.titleCar_color')).borderRadius(10).padding(20)
    }
    .alignItems(HorizontalAlign.Start).padding({ bottom: 10 })
  }
}

