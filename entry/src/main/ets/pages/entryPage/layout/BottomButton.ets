import { AbilityLifecycleCallback, common, Want } from '@kit.AbilityKit';
import { BusinessError, emitter, serialManager } from '@kit.BasicServicesKit';
import { distributedKVStore } from '@kit.ArkData';
import { KvManager } from '../../../common/utils/KVManager';
import {
  ConnectionType,
  KeyGenType,
  LoggingType,
  ProxyType,
  SerialType,
  SessionType,
  TelnetType,
  TunnelsType
} from '../../../common/types/dataType';
import { AlertDialog, promptAction, window } from '@kit.ArkUI';
import { fileIo as fs, fileShare, fileUri } from '@kit.CoreFileKit';
import {
  decryptPrivateKey,
  openProtocol,
  setLogging,
  startLocalPortForwarding,
  startRemotePortForwarding,
  startDynamicPortForwarding,
  closeProtocol,
  LoggingConfig
} from 'libentry.so';
import bundleManager from '@ohos.bundle.bundleManager';
import { pageDefaultData } from '../pageDefaultData';
import deepClone from '../../../common/tools/deepClone';
import { encryptProxyPassword, decryptProxyPassword } from '../../../common/utils/ProxyCredentialUtil';

import { buffer } from '@kit.ArkTS';
import Logging from '../Session/Logging';
import Session, { connectionType,  } from '../Session';
import { itemType } from '../../../components/SelectList/type';
import SerialClass from '../Connection/Serial/SerialClass';


// 定义端口转发项的参数类型
interface PortForwardParams {
  type: string;
  sourcePort: string;
  destination: string;
}

let windowStage: window.WindowStage | undefined;
let subWindow: window.Window;
let loginFlag: boolean

interface openSSHType {
  Session: SessionType;
  Connection: ConnectionType;
  Telnet: TelnetType;
  Proxy: ProxyType;
}

// 在文件顶部添加类型定义
interface SystemInfo {
  deviceType: string;
  osFullName: string;
  osVersion: string;
}

@Concurrent
async function openSSH(data: openSSHType,openProtocol:(config: string)=>Promise<number>) { // Return Promise<boolean>
  // 安全：data 含明文代理密码与 SSH 凭据，禁止序列化打印
  console.log('调用登录');
  try {
    // Await the Promise returned by the N-API function
    let resultBool = await openProtocol(JSON.stringify(data));
    if (resultBool != -1) {
      console.log('登录成功 (from NAPI)');
      return resultBool;
    } else {
      // This case might be less common now if errors cause rejection
      console.log('登录失败 (from NAPI resolve(false))');
      return -1;
    }
    // Return the boolean result
  } catch (error) {
    // Handle Promise rejection (connection failed with an error)
    console.error('登录失败 (NAPI Promise rejected):', JSON.stringify(error));
    // 透传错误给 UI 层展示（代理错误提示映射）
    const errMsg = (error instanceof Error) ? error.message : String(error);
    throw new Error(errMsg);
  }
}

@CustomDialog
struct AboutDialog {
  // 添加初始化值
  private bundleInfo: bundleManager.BundleInfo = {} as bundleManager.BundleInfo
  controller: CustomDialogController
  urlString: UrlStyle = new UrlStyle("https://gitcode.com/OpenHarmonyPCDeveloper/CrossShellNext");
  TextController: TextController = new TextController();
  mutableStyledString: MutableStyledString = new MutableStyledString("OpenHarmony_CrossShellNext", [{
    start: 0,
    length: "OpenHarmony_CrossShellNext".length,
    styledKey: StyledStringKey.URL,
    styledValue: this.urlString
  }]);
  aboutToAppear(): void {
    this.TextController.setStyledString(this.mutableStyledString)
  }

  build() {
    Column() {
      Text($r('app.string.About'))
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .margin({ top: 20, bottom: 16 })
        .width('100%')// 添加宽度
        .textAlign(TextAlign.Center) // 添加文本居中对齐

      Row({ space: 10 }) {
        Image($r('app.media.Terminal')).width(100).height(100)
        Column({ space: 16 }) {
          Column({ space: 6 }) {
            Text() {
              Span($r('app.string.App_Name'));
              Span(': ')
            }
            .fontColor($r('app.color.font_color'))
            .fontSize(16)
            .fontWeight(500)
            .textAlign(TextAlign.Start) // 添加文本左对齐
            Text('CrossShellNext')
              .fontSize(16)
              .textAlign(TextAlign.Start) // 添加文本左对齐
          }.alignItems(HorizontalAlign.Start)

          Column({ space: 6 }) {
            Text() {
              Span($r('app.string.Version'));
              Span(': ')
            }
            .fontColor($r('app.color.font_color'))
            .fontSize(16)
            .fontWeight(500)
            .textAlign(TextAlign.Start) // 添加文本左对齐
            Text(`${this.bundleInfo.versionName}`)
              .fontColor($r('app.color.font_color'))
              .fontSize(16)
              .textAlign(TextAlign.Start) // 添加文本左对齐
          }.alignItems(HorizontalAlign.Start)

          Column({ space: 6 }) {
            Text($r('app.string.Introduction')) {
              Span($r('app.string.Introduction'));
              Span(': ')
            }
            .fontColor($r('app.color.font_color'))
            .fontSize(16)
            .fontWeight(500)
            .textAlign(TextAlign.Start) // 添加文本左对齐
            Text($r('app.string.aboutUsIntroduction'))
              .fontColor($r('app.color.font_color'))
              .fontSize(16)
              .textAlign(TextAlign.Start) // 添加文本左对齐
              .lineHeight(24)
          }.alignItems(HorizontalAlign.Start)

          Column({ space: 6 }) {
            Text($r('app.string.OpenSourceAddress')) {
              Span($r('app.string.OpenSourceAddress'));
              Span(': ')
            }
            .fontColor($r('app.color.font_color'))
            .fontSize(16)
            .fontWeight(500)
            .textAlign(TextAlign.Start) // 添加文本左对齐
            Text(undefined, { controller: this.TextController })
              .key('mutableStyledString')
              .textAlign(TextAlign.Start) // 添加文本左对齐
              .fontColor($r('app.color.font_color'))
          }.alignItems(HorizontalAlign.Start)
        }
        .padding({ left: 12, right: 12 })
        .width('calc(100% - 110vp)') // 确保内容列占满宽度
        .alignItems(HorizontalAlign.Start)
      }
      .padding(10)
      .alignItems(VerticalAlign.Top)

      Button($r('app.string.ok'))
        .width('calc(100% - 48vp)')
        .height(32)
        .fontSize(16)
        .fontColor('#0A59F7')
        .backgroundColor(Color.Transparent)
        .margin({ top: 22, bottom: 16 })
        .onClick(() => {
          this.controller.close()
        })
    }
    .width(500)
    .backgroundColor($r('app.color.serial_message_list_background'))
    .borderRadius(24)
  }
}

let logfile: fs.File

@Component
export default struct BottomButton {
  private context = getContext(this) as common.UIAbilityContext
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined
  @State nowData: LoggingType = deepClone(pageDefaultData.Logging)
  @Provide fileFd: number = -1;
  @Provide logFileUri: string = '';
  // KVManager实例
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: this.context.resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  // open点击状态
  @LocalStorageLink('load') openLoading: boolean = false
  // loading文本
  @LocalStorageLink('loadText') loadingText: Resource = $r('app.string.loading')
  @Consume('abilityCallback') abilityCallback: AbilityLifecycleCallback
  @State textValue: string = ''
  @LocalStorageLink('sessionId') sessionId: number = -1
  /**
   * 删除二次确认弹窗
   */
  private delConfirm: CustomDialogController | null = new CustomDialogController({
    builder: AlertDialog({
      primaryTitle: $r('app.string.SYSTEM_PROMPT'),
      content: $r('app.string.CONFIRM_CLOSE_TERMINAL'),
      primaryButton: {
        value: $r('app.string.CANCEL'),
        action: () => {
          this.delConfirm?.close();
        },
        fontColor: '#0A59F7',
        background: '#fff'
      },
      secondaryButton: {
        value: $r('app.string.abNormalExitConfirm'),
        action: () => {
          try {
            // 避免重复关闭连接

            closeProtocol(this.sessionId);
            console.log('连接已关闭');
          } catch (error) {
            console.error('关闭连接出错:', error);
          }
          setTimeout(() => {
            this.openTerminal().then(() => {
              if (loginFlag) {
                // this.initLogFileUri()
                console.log('77---->生成日志文件')
              } else {
                console.log('77---->不生成日志文件')
              }
            })
          }, 500);
        },
        fontColor: '#fff',
        background: '#0A59F7'
      }
    }),
  })

  closeFile() {
    fs.closeSync(this.fileFd);
    this.fileFd = -1;
  }

  nowSessionDataChange() {
    // this.initLogFileUri()
    console.log('log 数据进行更改时保存', JSON.stringify(this.nowData));
  }

  // Store实例
  @Styles
  ButtonStyles(){
    .border({ width: 1, style: BorderStyle.Solid })
    .borderColor(0xDCDCDC)
    .borderRadius(5)
    .width('20%')
    .height(30)
  }

  onCancel(): void {
    this.passwordDialogController?.close();
    // 用户取消输入密码，终止登录流程
    this.openLoading = false;
  }

  async onAccept() {
    // 安全：textValue 为用户输入的明文口令，禁止打印
    // 尝试解密私钥
    const Session: SessionType = JSON.parse(await this.allDataKvStore?.get('Session') as string) as SessionType;
    // 将输入的密码赋值给Session对象
    Session.password = this.textValue;
    await this.allDataKvStore?.put('Session', JSON.stringify(await this.encryptSessionPasswordWrapper(Session)));
    // 不能重复调用解密接口
    // let decryptResult = await decryptPrivateKey(Session.privateKeyFd, this.textValue);
    // console.log('私钥解密结果：', decryptResult ? '成功' : '失败');
    // if (!decryptResult) {
    //   promptAction.showToast({
    //     message: '密钥密码错误',
    //     duration: 3000
    //   });
    //   this.openLoading = false;
    //   return;
    // }
    // 如果解密成功，继续后续流程
    await this.openTerminal(); // Await the async function if needed
    this.textValue = '';
  }

  // 创建自定义弹窗
  passwordDialogController: CustomDialogController = new CustomDialogController({
    builder: CustomDialogExample({
      textValue: $textValue,
      cancel: () => {
        this.onCancel()
      },
      confirm: () => {
        this.onAccept()
      },
    }),

    autoCancel: false,
    alignment: DialogAlignment.Center,
    customStyle: true
  });

  async aboutToAppear(): Promise<void> {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    this.nowData = deepClone(pageDefaultData.Logging)
    console.log('页面初始化时 nowdata内容' + JSON.stringify(this.nowData))
    emitter.on('shellLoggingEntryOnDestroy', () => {
      if (this.context) {
        try {
          fs.closeSync(logfile.fd)
          console.log('终端关闭了，日志停止')
        } catch (err) {
          // 捕获同步的参数错误
          let code = (err as BusinessError).code;
          let message = (err as BusinessError).message;
          console.error(`日志 failed, code is ${code}, message is ${message}`);
        }
      }
    })
    emitter.on('openTerminal', () => {
      this.open()
    })
  }

  aboutToDisappear(): void {
    emitter.off('shellLoggingEntryOnDestroy')
    emitter.off('openTerminal')
  }

  dialogController: CustomDialogController = new CustomDialogController({
    builder: AboutDialog({ bundleInfo: {} as bundleManager.BundleInfo }),
    alignment: DialogAlignment.Center,
    customStyle: true,
    autoCancel: false
  })

  // 点击About按钮
  async about() {
    try {
      // 获取当前应用的包信息
      const bundleFlags = bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION;
      const bundleInfo = await bundleManager.getBundleInfoForSelf(bundleFlags);

      // 更新对话框控制器
      this.dialogController = new CustomDialogController({
        builder: AboutDialog({ bundleInfo: bundleInfo }),
        alignment: DialogAlignment.Center,
        customStyle: true,
        autoCancel: false
      })

      // 显示对话框
      this.dialogController.open()
    } catch (error) {
      console.error('Failed to get bundle info:', error);
    }
  }

  // 将密钥文件读取出来
  async readProvideKey(fileName: string, filePath: string): Promise<number> {
    const resFile = this.context.filesDir
    const list: string[] = await fs.listFile(resFile)
    console.log('filePath----------->', list)
    if (list.indexOf(fileName) > -1) {
      // 如果文件目录存在当前文件，则直接打开，否则返回当前已打开的文件句柄对象
      console.log('filetest----已有文件----------》')
      const file: fs.File = await fs.open(resFile + `/${fileName}`)
      return file.fd
    } else {
      console.log('c----无文件----------》', filePath)
      const file: fs.File = await fs.open(filePath)
      return file.fd
    }

  }

  // 解码URI编码的路径
  decodeUriPath(uri: string): string {
    try {
      return decodeURIComponent(uri);
    } catch (error) {
      console.error('URI解码失败:', error);
      return uri;
    }
  }

  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    let loggingStr = await this.allDataKvStore?.get('Logging') as string
    let logging: LoggingType = JSON.parse(loggingStr);
    // this.nowData=JSON.parse(loggingStr as string) as LoggingType
    console.log('sx loggingStr', loggingStr)
    if (!logging.logType) {
      console.log('sx !this.nowData.logType' + this.nowData.logType)
      return
    }
    if (!logging.logFileBaseUri || !logging.logFileName) {
      console.log('sx !this.nowData.logFileBaseUri || !this.nowData.logFileName')
      return
    }

    let logFileUri = logging.logFileBaseUri + "/" + logging.logFileName
    console.log('sx 11111loggingStr' + logFileUri)
    const path = (new fileUri.FileUri(logFileUri)).path

    let tempLogFileUri = path
    // let tempLogFileUri =this.decodeUriPath(path)
    console.log('sx 修复日志中文路劲' + tempLogFileUri, fileUri.getUriFromPath(tempLogFileUri))
    try {
      logfile = fs.openSync(tempLogFileUri, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE);
    } catch (e) {
      console.error('sx 文件打开或者创建异常', e)
    }

    if (logging.existOperation === 0) {
      console.log('sx 执行覆盖')
      try {
        let flag = fs.accessSync(tempLogFileUri)
        if (flag) {
          // 截断文件
          fs.truncateSync(logfile.fd, 0)
        }
      } catch (e) {
        console.error('sx 检查文件是否存在错误', e)
      }
    } else {
      console.log('sx 执行追加')
      try {
        logfile = fs.openSync(tempLogFileUri, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.APPEND);
      } catch (e) {
        console.error('sx 执行追加-打开错误', e)
      }
    }
    let fileFd = logfile.fd;
    console.log('sx loggingStr', fileFd)
    console.log('sx bottom button sessionId', this.sessionId)
    // setLogging(this.sessionId,fileFd, logging.logType, logging.existOperation, (success, error) => {
    //   console.log('loggingStr success', success);
    //   console.log('loggingStr error', error)
    // })
    try {
      console.log('sx 1111')
      // let configStr:string = (await this.allDataKvStore?.get(loggingMultiSelConfig)) as string
      console.log('sx 2222')
      // let multiSelConfig:LoggingMultiSelConfig = JSON.parse(configStr)
      let config: LoggingConfig = {
        fd: fileFd,
        logType: logging.logType,
        existOperation: logging.existOperation,
        includeHeader:logging.includeHeader,
        omitKnownPassword:logging.omitKnownPassword,
        omitSessionData:logging.omitSessionData
      }
      console.log('sx 点击open 配置项： config:'+JSON.stringify(config))
      //todo setLogging
      setLogging(this.sessionId,config,(success, error) => {
        console.log('sx loggingStr success', success);
        console.log('sx loggingStr error', error)
      })
    }catch (e) {
      console.log('sx err'+JSON.stringify(e))
    }
  }

  // --- Refactor openTerminal ---
  async openTerminal() {
    this.openLoading = true;
    this.loadingText = $r('app.string.logging_loading');
    let loginSuccess = -1; // Track overall success

    try {
      // 1. Prepare parameters（用 safeGet 容错读取，key 缺失不抛 Not found）
      let Session: SessionType = deepClone(pageDefaultData.Session)
      let KeyGen: KeyGenType = deepClone(pageDefaultData.KeyGen)
      let Telnet: TelnetType = deepClone(pageDefaultData.Telnet)
      let Connection: ConnectionType = deepClone(pageDefaultData.Connection)
      try {
        const s = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Session')
        if (s) {
          Session = await this.decryptSessionPasswordWrapper(JSON.parse(s) as SessionType)
        }
      } catch (e) {
        console.error('读取 Session 配置失败', e)
      }
      try {
        const kg = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'KeyGen')
        if (kg) {
          KeyGen = JSON.parse(kg) as KeyGenType
        }
      } catch (e) {
        // KeyGen 仅用于展示，读不到不影响 SSH 连接
      }
      try {
        const t = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Telnet')
        if (t) {
          Telnet = JSON.parse(t) as TelnetType
        }
      } catch (e) {
        console.error('读取 Telnet 配置失败', e)
      }
      try {
        const c = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Connection')
        if (c) {
          Connection = JSON.parse(c) as ConnectionType
        }
      } catch (e) {
        console.error('读取 Connection 配置失败', e)
      }
      // 安全：Session/Telnet 含明文凭据，禁止序列化打印
      console.log("connection===================", JSON.stringify(Connection));

      // --- 组装代理配置 ---
      // 连接前先把代理面板当前值同步到 KV（面板值可能尚未落盘），避免读到过期/默认代理配置
      const preProxyStr = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Proxy')
      AppStorage.SetOrCreate('saveProxy', 'conn' + Date.now())
      let proxyStr = preProxyStr
      const pollDeadline = Date.now() + 800
      while (Date.now() < pollDeadline) {
        await new Promise<void>((resolve) => setTimeout(() => resolve(), 60))
        const cur = await this.KVManager.safeGet(this.allDataKvStore as distributedKVStore.SingleKVStore, 'Proxy')
        if (JSON.stringify(cur) !== JSON.stringify(preProxyStr)) {
          proxyStr = cur
          break
        }
      }
      let proxyConfig: ProxyType = deepClone(pageDefaultData.Proxy)
      if (proxyStr) {
        try {
          const parsedProxy = JSON.parse(proxyStr) as ProxyType
          proxyConfig = {
            type: (parsedProxy.type ?? proxyConfig.type) as 'None' | 'SOCKS5' | 'HTTP',
            host: parsedProxy.host ?? proxyConfig.host,
            port: parsedProxy.port ?? proxyConfig.port,
            username: parsedProxy.username ?? proxyConfig.username,
            password: parsedProxy.password ?? proxyConfig.password,
            enabled: parsedProxy.enabled ?? proxyConfig.enabled
          }
        } catch (e) {
          console.error('解析 Proxy 配置失败', e)
        }
      }
      // 会话代理策略：off=禁用，custom=使用本配置，global=跟随全局开关
      const policy: 'global' | 'custom' | 'off' = (Session.proxyPolicy ?? 'global')
      if (policy === 'off' || (policy === 'global' && !proxyConfig.enabled)) {
        proxyConfig = {
          type: 'None',
          host: proxyConfig.host,
          port: proxyConfig.port,
          username: proxyConfig.username,
          password: proxyConfig.password,
          enabled: false
        }
      }
      // 代理密码解密（HUKS 密文 → 明文，仅供本次握手使用）
      let proxyPassword = proxyConfig.password ?? ''
      if (proxyPassword.startsWith('H1:')) {
        try {
          proxyPassword = await decryptProxyPassword(proxyPassword)
        } catch (e) {
          console.error('代理密码解密失败', e)
          proxyPassword = ''
        }
      }
      proxyConfig = {
        type: proxyConfig.type,
        host: proxyConfig.host,
        port: proxyConfig.port,
        username: proxyConfig.username,
        password: proxyPassword,
        enabled: proxyConfig.enabled
      }
      console.log('proxy 策略:', policy, '代理类型:', proxyConfig.type)

      let params: openSSHType = {
        Session,
        Connection,
        Telnet,
        Proxy: proxyConfig
      };
      // 2. Call the async openSSH wrapper (or directly await openProtocol)
      // 安全：params 含明文代理密码与 SSH 凭据，禁止序列化打印
      console.log('Calling async openSSH wrapper...');
      // No need for taskpool here, openSSH awaits the async NAPI call
      loginSuccess = await openSSH(params,openProtocol);
      console.log(`openSSH wrapper completed. Success: ${loginSuccess}`);
      this.sessionId = loginSuccess
      if (loginSuccess != -1) {
        // 初始化日志
        this.init()
      }
      // 3. Check result and proceed
      if (loginSuccess != -1) {
        loginFlag = true; // Keep global flag if necessary
        const WindowSize: object = JSON.parse((await this.allDataKvStore?.get('WindowSize')) as string);
        console.log('SSH and SFTP ready. Starting Terminal Ability...');
        let wantInfo: Want = {
          bundleName: this.context.resourceManager.getStringSync($r('app.string.App_bundleName').id),
          abilityName: 'CrossShellNext_Terminal',
          parameters: {
            info: JSON.stringify(WindowSize),
            sessionId: this.sessionId
          },
        };
        try {
          await this.context.startAbility(wantInfo);
          console.log('startAbility success.')
        } catch (e) {
          console.error('startAbility失败', e)
        }
      } else {
        // Handle connection failure (already logged in openSSH or caught below)
        loginFlag = false;
        // Don't show redundant toast if openSSH already logged error
        if (loginSuccess == -1) { // Only show toast if it wasn't a caught exception below
          this.getUIContext().getPromptAction().showToast({
            message: $r('app.string.Login_Failed'),
            showMode: promptAction.ToastShowMode.DEFAULT,
            textColor: Color.Red
          });
        }
      }
    } catch (e) {
      // Catch errors from parameter prep or if openSSH re-throws
      loginFlag = false;
      loginSuccess = -1;
      console.error('Error during openTerminal:', JSON.stringify(e));
      this.getUIContext().getPromptAction().showToast({
        message: this.localizeConnectError(e.message) || $r('app.string.Link_Error'),
        showMode: promptAction.ToastShowMode.DEFAULT,
        textColor: Color.Red
      });
    } finally {
      // 4. Always hide loading indicator
      this.openLoading = false;
      this.loadingText = $r('app.string.loading');
      console.log('openTerminal finished.');
    }
  }

  // 连接错误本地化：仅代理错误（二期新增机制）显示专一细分提示，
  // 其余所有连接/登录类失败统一回一期「登录失败，请检查连接信息或网络」，不覆盖一期。
  localizeConnectError(msg: string): string {
    const rawMsg = msg ?? ''
    const rm = this.getUIContext().getHostContext()?.resourceManager
    const loginFailed = (): string => rm ? rm.getStringSync($r('app.string.Login_Failed').id) : rawMsg
    const pm = /^PROXY_ERR:(\d+)$/.exec(rawMsg)
    if (pm) {
      const n = Number(pm[1])
      const ids: Resource[] = [
        $r('app.string.proxy_err_1'), $r('app.string.proxy_err_2'), $r('app.string.proxy_err_3'),
        $r('app.string.proxy_err_4'), $r('app.string.proxy_err_5'), $r('app.string.proxy_err_6'),
        $r('app.string.proxy_err_7'), $r('app.string.proxy_err_8')
      ]
      if (n >= 1 && n <= ids.length) {
        return rm ? rm.getStringSync(ids[n - 1].id) : rawMsg
      }
      return rawMsg
    }
    if (rawMsg !== '') {
      // CONNECT_ERR 全码、legacy native code、未识别 → 一期「登录失败」
      return loginFailed()
    }
    return rawMsg
  }


  private async encryptSessionPasswordWrapper(s: SessionType): Promise<SessionType> {
    const c: SessionType = deepClone(s)
    try {
      c.password = await encryptProxyPassword(s.password)
    } catch (e) {
      console.error('会话密码加密失败，按明文保存', e)
    }
    return c
  }

  private async decryptSessionPasswordWrapper(s: SessionType): Promise<SessionType> {
    const c: SessionType = deepClone(s)
    try {
      c.password = await decryptProxyPassword(s.password)
    } catch (e) {
      console.error('会话密码解密失败，保留原值', e)
    }
    return c
  }

  // 打开串口终端窗口
  async openSerialTerminal(){
    if(canIUse('SystemCapability.USB.USBManager.Serial')){
      const serialDeviceIdList = AppStorage.get('SerialList') as number[] || []
      try{
        const Serial: SerialType = JSON.parse(await this.allDataKvStore?.get('Serial') as string) as SerialType
        const serial:SerialClass = new SerialClass(this.getUIContext(),Serial.readEncoding)
        try{
          await serial.getSerialDevice()
        } catch (e) {
          const _em = (e instanceof Error) ? e.message : JSON.stringify(e)
          this.getUIContext().getPromptAction().showToast({
            message: _em || '',
            duration:4000,
            textColor:Color.Red
          })
        }
        console.log('dyx serialDeviceIdList',JSON.stringify(serialDeviceIdList))
        if(serialDeviceIdList.indexOf(serial.serialId)>-1){
          this.getUIContext().getPromptAction().showToast({
            message:$r('app.string.serial_device_open_warn'),
            duration:4000,
            textColor:Color.Red
          })
          return
        }
        console.log('dyx serialId',serial.serialId)
        const defaultStatus:boolean =  await serial.serialDefault() // 获取权限状态
        this.getUIContext().getPromptAction().showToast({
          message:defaultStatus?$r('app.string.open_success'):$r('app.string.failed_open'),
          textColor:defaultStatus?Color.Green:Color.Red,
          duration:4000
        })
        if(defaultStatus){
          const Serial: SerialType = JSON.parse(await this.allDataKvStore?.get('Serial') as string) as SerialType
          console.log('dyx SerialData',JSON.stringify(Serial))
          let attribute: serialManager.SerialAttribute = {
            baudRate: new Number(Serial.speed).valueOf() as serialManager.BaudRates,
            dataBits: new Number(Serial.dataBits).valueOf() as serialManager.DataBits,
            parity: new Number(serialManager.Parity['PARITY_'+Serial.parity]).valueOf() as serialManager.Parity,
            stopBits: new Number(Serial.stopBits).valueOf() as serialManager.StopBits
          }
          // 设置串口配置
          serial.openSerialDevice(attribute)
          console.log('dyx serial attribute',JSON.stringify(serialManager.getAttribute(serial.serialId)))
          this.openLoading = true;
          this.loadingText = $r('app.string.serial_open_loading');
          // const Serial: SerialType = JSON.parse(await this.allDataKvStore?.get('Serial') as string) as SerialType;
          const WindowSize: object = JSON.parse((await this.allDataKvStore?.get('WindowSize')) as string);
          console.log('SSH and SFTP ready. Starting Terminal Ability...');
          let wantInfo: Want = {
            bundleName: this.context.resourceManager.getStringSync($r('app.string.App_bundleName').id),
            abilityName: 'CrossShellNext_Terminal',
            parameters: {
              info: JSON.stringify(WindowSize),
              sessionId: this.sessionId,
              serialId:serial.serialId
            },
          };
          try {
            await this.context.startAbility(wantInfo);
            this.openLoading = false;
            console.log('startAbility success.')
          } catch (e) {
            console.error('startAbility失败', e)
          }
        }
      }catch (e) {
        console.log('dyx 打开串口传输窗口错误',e)
        this.getUIContext().getPromptAction().showToast({
          message:$r('app.string.failed_open'),
          textColor:Color.Red,
          duration:4000
        })
      }
    }else{
      this.getUIContext().getPromptAction().showToast({
        message:$r('app.string.serial_open_capability'),
        showMode:promptAction.ToastShowMode.DEFAULT,
        textColor:$r('app.color.warning_textColor'),
        duration:4000
      })
    }


  }

  // 点击Open按钮
  //todo 点击open按钮
  async open() {
    const Session: SessionType = JSON.parse(await this.allDataKvStore?.get('Session') as string) as SessionType;

    // 增加一个标记，表示 fd 是否已经被用过
    if (Session.privateKeyFd != -1) {
      try {
        console.log('第二次点击open执行关闭秘钥文件fd')
        fs.close(Session.privateKeyFd)
        Session.privateKeyFd = -1
        await this.allDataKvStore?.put('Session', JSON.stringify(await this.encryptSessionPasswordWrapper(Session)))
        console.log("yu----->>>>Session 已保存")
        let fdTemp = fs.openSync(Session.privateKeyPath, fs.OpenMode.READ_WRITE)
        Session.privateKeyFd = fdTemp.fd
        await this.allDataKvStore?.put('Session', JSON.stringify(await this.encryptSessionPasswordWrapper(Session)))

      } catch (e) {
        console.log('关闭失败', e)
      }
    }

    if (Session.privateKeyPath != '' && Session.privateKeyPath != undefined
      && Session.authType == 'publickey' && Session.connectionType == 'SSH') {
      // 获取文件行的迭代器
      let iter = fs.readLinesSync(Session.privateKeyPath, { encoding: 'utf-8' });
      // 读取第一行
      let firstLine = iter.next().value;
      // 识别加密私钥：PEM(ENCRYPTED) 与 OpenSSH(首行无 ENCRYPTED 标记，无论是否加密均需口令框)
      if (firstLine.includes("ENCRYPTED") || firstLine.includes("OPENSSH PRIVATE KEY")) {
        // 创建密钥密码输入弹窗（用户可输入口令；无私钥口令可空确认）
        try {
          this.passwordDialogController.open()
        } catch (error) {
          console.error('创建密码弹窗失败：', error);
          this.openLoading = false;
        }
      } else {
        await this.openTerminal(); // Await the async function if needed
      }
    } else if (Session.connectionType == 'Serial') {
      //判断Terminal是否打开
      //  if (AppStorage.get('TerminalType')) {
      //    this.delConfirm?.open()
      //  } else {
      this.openSerialTerminal()
      //  }
    } else{
      await this.openTerminal(); // Await the async function if needed
    }

  }

  // 点击Cancel
  cancel() {
    if (this.context) {
      try {
        this.context.terminateSelf((err: BusinessError) => {
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
  }

  build() {
    Flex({ alignItems: ItemAlign.Center, justifyContent: FlexAlign.SpaceBetween }) {
      Row({ space: 20 }) {
        Button($r('app.string.About'), { type: ButtonType.Normal })
          .buttonStyle(ButtonStyleMode.NORMAL)
          .ButtonStyles()
          .fontColor('#0A59F7')  // 添加这行，设置字体颜色
          .onClick(() => this.about())
      }

      Row({ space: 20 }) {
        Button($r('app.string.Cancel'), { type: ButtonType.Normal })
          .buttonStyle(ButtonStyleMode.NORMAL)
          .ButtonStyles()
          .fontColor('#0A59F7')
          .onClick(() => this.cancel())
        Button($r('app.string.Open'), { type: ButtonType.Normal })
          .buttonStyle(ButtonStyleMode.NORMAL)
          .ButtonStyles()
          .fontColor('#0A59F7')
          .onClick(() => this.open())
      }
    }.height('100%')
    .padding({
      top: 10,
      left: 10,
      right: 20,
      bottom: 10
    })
    .border({
      width: {
        top: '2px'
      },
      style: BorderStyle.Solid,
      color: '#ccc'
    })
  }
}

// 自定义弹窗组件
@CustomDialog
struct CustomDialogExample {
  @Link textValue: string
  private controller?: CustomDialogController;
  cancel: () => void = () => {
  }
  confirm: () => void = () => {
  }

  build() {
    Column() {
      Text($r('app.string.ENTER_KEY_PASSWORD'))
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .margin({ top: 20, bottom: 10 })

      TextInput({ placeholder: $r('app.string.ENTER_PASSWORD') })
        .type(InputType.Password)
        .width('80%')
        .height(40)
        .margin({ top: 10, bottom: 20 })
        .onChange((value: string) => {
          this.textValue = value;
        })

      Flex({ justifyContent: FlexAlign.SpaceAround }) {
        Button($r('app.string.CANCEL'))
          .onClick(() => {
            this.cancel();
          })
          .width(100)

        Button($r('app.string.abNormalExitConfirm'))
          .onClick(() => {
            this.confirm()
            this.controller?.close()
          })
          .width(100)
      }
      .width('90%')
      .margin({ bottom: 20 })
    }
    .width(300)
    .padding(10)
    .backgroundColor($r('app.color.Key_prompt_window'))
    .borderRadius(10)
  }
}
