import { webview } from '@kit.ArkWeb';
import {
  allDataType,
  CloseWindowMode,
  SessionType,
  TerminalType,
  WindowSizeType,
  xtermStyle
} from 'ets/common/types/dataType';
import { registerDataCallback, sendTerminalCommand, resizeTerminal, closeProtocol } from 'libentry.so'
import json from '@ohos.util.json';
import { JSON } from '@kit.ArkTS';
import { it } from '@ohos/hypium';
import { BusinessError, emitter } from '@kit.BasicServicesKit';
import { pageDefaultData } from '../../pages/entryPage/pageDefaultData';
import { window } from '@kit.ArkUI';
import { KvManager } from '../../common/utils/KVManager';
import { distributedKVStore } from '@kit.ArkData';
import { AlertDialog, ConfirmDialog, Theme } from '@kit.ArkUI';
import { common } from '@kit.AbilityKit';

interface GeneratedTypeLiteralInterface_1 {
  result: string;
  status: boolean;
}

const pendingTerminalData: Map<number, GeneratedTypeLiteralInterface_1 []> = new Map();

@Component
export default struct Terminal {
  @State webController: webview.WebviewController | null = null
  @Prop @Watch('dataSourceChange') configDataSource: allDataType; // 所有配置的数据源
  @State nativePorts: webview.WebMessagePort | null = null;
  @State ports: webview.WebMessagePort[] = []; // 消息端口
  @State info: xtermStyle = { cursorAppear: 'block', cursorBlinks: true, foreground: 'ffffff' };
  @Prop @Watch('onSessionIdChanged') sessionId: number = -1
  @State closeWindowMode: CloseWindowMode = CloseWindowMode.Always
  private isFinishSession: boolean = false //是否结束本次会话
  // 端口/终端就绪标记：只有在两者就绪后才转发首包，避免时序竞态
  @State private portMappedReady: boolean = false // globalPorts 已登记且 sessionId 有效
  @State private h5TerminalReady: boolean = false // H5 端 xterm 已 open 并发回 ready 信号
  //异常退出弹窗
  //todo 异常退出弹窗
  abNormalExitDialog: CustomDialogController = new CustomDialogController({
    builder: AlertDialog({
      theme: AppStorage.get('currentColorMode'),
      primaryTitle: $r('app.string.abNormalExitTitle'),
      content: $r('app.string.abNormalExitContent'),
      primaryButton: {
        value: $r('app.string.abNormalExitConfirm'),
        action: () => {
          this.abNormalExitCallBack()
        }
      },

    }),
    cancel:()=>{
      this.abNormalExitCallBack()
    },
    autoCancel:true
  })

  abNormalExitCallBack() {
    console.log('sx 点击异常退出弹窗' + this.closeWindowMode)
    if (this.closeWindowMode == CloseWindowMode.Always) {
      console.log('sx 关闭终端')
      const eventData: emitter.EventData = {
        data: {
          sessionId: this.sessionId
        }
      };
      emitter.emit('session_logout_request', eventData); // 发出带有 EventData 的事件
    }
  }

  // KVManager实例
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  // 添加这个方法处理终端大小变更
  async resizeTerminalToSize(width: number, height: number): Promise<void> {
    console.info(`调整终端大小: ${width}x${height}`);
    if (!width || !height || width < 1 || height < 1) {
      console.error(`终端大小无效: ${width}x${height}`);
      return;
    }

    try {
      const success = await resizeTerminal(this.sessionId, width, height);
      console.info(`终端大小调整${success ? '成功' : '失败'}: ${width}x${height}`);
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : String(err);
      console.error(`调整终端大小错误: ${errorMsg}`);
    }
  }

  async windowResizedIdChange() {
    let windowSize = ''
    let id = ''
    try {
      windowSize = await this.allDataKvStore?.get('WindowSize') as string
      id = (JSON.parse(windowSize) as WindowSizeType).windowResizedId
    } catch (e) {
      id = pageDefaultData.WindowSize.windowResizedId
    }
    if (id === '3') {
      (AppStorage.get('windowStage') as window.WindowStage).getSubWindow().then((data) => {
        data.forEach(item => {
          item.setResizeByDragEnabled(false)
        })
        console.info('Succeeded in obtaining the subwindow. Data: ' + JSON.stringify(data));
      }).catch((err: BusinessError) => {
        console.error(`Failed to obtain the subwindow. Cause code: ${err.code}, message: ${err.message}`);
      });
      (AppStorage.get('windowStage') as window.WindowStage).getMainWindowSync().setResizeByDragEnabled(false)
    } else {
      (AppStorage.get('windowStage') as window.WindowStage).getSubWindow().then((data) => {
        data.forEach(item => {
          item.setResizeByDragEnabled(true)
        })
        console.info('Succeeded in obtaining the subwindow. Data: ' + JSON.stringify(data));
      }).catch((err: BusinessError) => {
        console.error(`Failed to obtain the subwindow. Cause code: ${err.code}, message: ${err.message}`);
      });
      (AppStorage.get('windowStage') as window.WindowStage)
    }
  }

  aboutToAppear() {
    // console.log('sx 进入终端 当前关闭状态：' + this.closeWindowMode)
    this.webController = new webview.WebviewController()
    this.initConfig();
    emitter.on('windowResizedId', () => {
      if (this.webController) {
        this.webController.runJavaScriptExt('getWindowResizedId()')
      }
    })
    this.windowSizeMax()
    this.KVManager.getStore('allData').then(async (data) => {
      this.allDataKvStore = data
      this.closeWindowMode =
        (JSON.parse((await this.allDataKvStore.get("Session") as string)) as SessionType).closeWindowMode
      console.log('sx 到终端了，关闭模式：' + this.closeWindowMode)
    })
  }

  windowSizeMax() {
    const windowClass = (this.getUIContext().getHostContext() as common.UIAbilityContext).windowStage
    windowClass.getMainWindowSync().on('windowStatusChange', (status: window.WindowStatusType) => {
      if (!this.webController) {
        return
      }
      if (status === window.WindowStatusType.MAXIMIZE || status === window.WindowStatusType.FULL_SCREEN) {
        this.webController.runJavaScriptExt('windowToMax()')
        console.log('窗口最大化')
      } else {
        this.webController.runJavaScriptExt('exitWindowMax()')
        console.log('窗口最小化')
      }
    });
  }

  dataSourceChange() {
    this.initConfig();
  }

  initConfig() {
    if (!this.configDataSource || !this.configDataSource.Cursor) {
      return; // 如果configDataSource或Cursor不存在，直接返回，保留默认值
    }
    this.info.cursorAppear = this.configDataSource.Cursor.cursorAppear
    this.info.cursorBlinks = this.configDataSource.Cursor.cursorBlinks
    if (this.configDataSource.Color) {
      this.info.foreground = this.configDataSource.Color.fontColor
    }
  }

  aboutToDisappear(): void {
    // 先关闭消息端口，再终止渲染进程，避免底层双重 close 导致 chromium ScopedFD EBADF 崩溃
    try { this.ports[0]?.close() } catch (e) { console.error('关闭 ports[0] 失败', JSON.stringify(e)) }
    try { this.nativePorts?.close() } catch (e) { console.error('关闭 nativePorts 失败', JSON.stringify(e)) }
    // 销毁当前 webview 的渲染进程（放在端口关闭之后）
    try { this.webController?.terminateRenderProcess() } catch (e) { console.error('terminateRenderProcess 失败', JSON.stringify(e)) }
    // 清理全局端口映射
    let nowGlobalPorts = AppStorage.get('globalPorts') as Map<number, webview.WebMessagePort>;
    if (nowGlobalPorts) {
      nowGlobalPorts.delete(this.sessionId);
      AppStorage.set('globalPorts', nowGlobalPorts);
    }
    emitter.off("port" + this.sessionId)
    emitter.off('windowResizedId');
    const windowClass = (this.getUIContext().getHostContext() as common.UIAbilityContext).windowStage
    try {
      const data =  windowClass.getMainWindowSync()
      data.off('windowStatusChange')
      console.log('Terminal注销监听窗口大小状态成功')
    }catch (e) {
      console.error('Terminal注销监听窗口大小状态失败',JSON.stringify(e))
    }
  }

  // 通过MessagePort向web侧发送数据
  postMessage(msg: webview.WebMessageExt): void {
    this.nativePorts?.postMessageEventExt(msg)
  }

  // 将服务器数据发送到Web端
  globalDataDispatcher = async (sessionId: number, result: string, status: boolean): Promise<void> => {
    console.log('dyxSessionId 当前的SessionId',this.sessionId)

    // --- 添加日志：打印原始服务器返回数据 ---
    console.log(`globalDataDispatcher raw data for sessionId ${sessionId}: status=${status}, result=${JSON.stringify(result)}`);
    // --- 日志结束 ---

    // --- 增强的退出指令检测 ---
    // 1. 移除 ANSI escape sequences
    const ansiRegex = /\x1b\[[?0-9;]*[a-zA-Z]/g; // Regex to match common ANSI sequences
    let cleanedResult = result.replace(ansiRegex, '');
    // 2. 移除 Null characters (\u0000)
    cleanedResult = cleanedResult.replace(/\u0000/g, '');
    // 3. 移除 Carriage Return characters (\r)
    cleanedResult = cleanedResult.replace(/\r/g, '');
    // 4. 去除首尾空格并转为小写
    const trimmedResult = cleanedResult.trim().toLowerCase();
    console.log(`globalDataDispatcher cleaned & trimmed result: ${JSON.stringify(trimmedResult)}`); // Log cleaned result

    // 5. 检查是否匹配常见的退出指令
    // 用户主动退出指令
    const userExitCommands = ['logout', '注销', 'exit', 'quit', ':q'];
    //异常退出指令
    const abnormalExitCommands = ['connection closed or error occurred in receive thread'];
    if (userExitCommands.includes(trimmedResult)) {
      this.isFinishSession = true
      // 检测到退出指令 (flexible match)
      console.log(`检测到退出指令 (flexible match)， sessionId: ${sessionId}, trimmedResult: ${trimmedResult}`);
      //通过用户选择决定弹窗是否关闭
      console.log('sx 检测到退出 当前模式：' + this.closeWindowMode)
      if (this.closeWindowMode == CloseWindowMode.Always || this.closeWindowMode == CloseWindowMode.OnlyOnCleanExit) {
        console.log('sx 关闭终端')
        const eventData: emitter.EventData = {
          data: {
            sessionId: sessionId,
            status:true
          }
        };
        emitter.emit('session_logout_request', eventData); // 发出带有 EventData 的事件
        return
      }
    } else if (abnormalExitCommands.includes(trimmedResult)) {
      if (this.isFinishSession) {
        return
      }
      console.log('sx 检测到异常退出')
      const eventData: emitter.EventData = {
        data: {
          sessionId: sessionId,
          status:false
        }
      };
      emitter.emit('session_logout_request', eventData);
      // this.abNormalExitDialog.open()
    }
    // --- 退出指令检测结束 ---

    console.log(`globalDataDispatcher: Received data for sessionId ${sessionId}`);
    const activeTerminalPorts = AppStorage.get('globalPorts') as Map<number, webview.WebMessagePort>

    // 添加安全检查，确保activeTerminalPorts已定义
    if (!activeTerminalPorts) {
      console.error(`globalDataDispatcher: activeTerminalPorts is undefined. Cannot process data for sessionId ${sessionId}.`);
      return;
    }
    const targetPort = activeTerminalPorts.get(sessionId);
    // 端口映射或 H5 终端未就绪时，直接缓存等待，避免首包在解析器未就绪时渲染
    if (!targetPort || !this.portMappedReady || !this.h5TerminalReady) {
      if (!pendingTerminalData.has(sessionId)) {
        pendingTerminalData.set(sessionId, []);
      }
      pendingTerminalData.get(sessionId)!.push({ result, status });
      console.log(`globalDataDispatcher: Port/Terminal not ready. Cached for sessionId ${sessionId}.`);
      return;
    }
    console.log('服务器终端返回的数据', JSON.stringify(result), status)
    console.log('服务器终端返回的数据 targetPort', targetPort, sessionId, this.sessionId)

    if (this.allDataKvStore &&
      (JSON.parse(await this.allDataKvStore.get('Terminal') as string) as TerminalType).variousCheck.indexOf('4') >=
        0) {
      result = result.replace(/\r\n/g, '\r\n\n');
    }

    // 完整的ANSI转义序列处理方案
    if (targetPort != undefined) {
      console.log(`globalDataDispatcher: Found target port for sessionId ${sessionId}. Posting data...`);
      const strExt: webview.WebMessageExt = new webview.WebMessageExt();
      strExt.setType(webview.WebMessageType.ARRAY);
      strExt.setArray([result, status + '']);
      console.log('zfr服务器终端返回的数据', JSON.stringify(strExt.getArray()))
      try {
        targetPort.postMessageEventExt(strExt);
        console.log(`globalDataDispatcher: Data posted successfully to port for sessionId ${sessionId}.`);
      } catch (e) {
        console.error(`globalDataDispatcher: Error posting message to port for sessionId ${sessionId}:`, e);
      }
    } else {
      // 理论上不会进入（已在前置分支 return），保底缓存
      if (!pendingTerminalData.has(sessionId)) {
        pendingTerminalData.set(sessionId, []);
      }
      pendingTerminalData.get(sessionId)!.push({ result, status });
      console.log(`globalDataDispatcher: Fallback cache for sessionId ${sessionId}.`);
    }
  }

  pendingTerminalData() {
    // 补发缓存数据
    if (pendingTerminalData.has(this.sessionId)) {
      console.log('pendingTerminalData----->', JSON.stringify(pendingTerminalData.get(this.sessionId)))
      const pendingList = pendingTerminalData.get(this.sessionId)!
      console.log('pendingList----->', JSON.stringify(pendingList))
      pendingTerminalData.delete(this.sessionId)
      console.log('pendingTerminalData----->', JSON.stringify(pendingTerminalData))
      pendingList.forEach(item => {
        console.log('item result' + item.result + "item status" + item.status)
        const strExt: webview.WebMessageExt = new webview.WebMessageExt();
        strExt.setType(webview.WebMessageType.ARRAY);
        strExt.setArray([item.result, item.status + '']);
        try {
          console.log('this.nativePorts', this.nativePorts)
          if (this.nativePorts != null) {
            this.nativePorts.postMessageEventExt(strExt);
            console.log('本地端口补发数据发送成功')
          } else {
            console.error('本地端口补发数据发送失败，因为本地端口为null')
          }
        } catch (e) {
          console.log('补发的传递失败')
        }
        console.log(`webPageEnd: 补发缓存数据到端口 sessionId ${this.sessionId}`);
      })
    }
  }

  // 发送终端配置信息到Web端
  sendInfo() {
    const strExt: webview.WebMessageExt = new webview.WebMessageExt()
    strExt.setType(webview.WebMessageType.STRING)
    strExt.setString(json.stringify(this.info))
    this.postMessage(strExt)
  }

  // 发送命令到SSH (修正版)
  async sendCommand(command: string): Promise<void> { // 返回 Promise<void>
    console.info("发送命令到SSH:", command, "sessionId:", this.sessionId);
    if (command === undefined || command === null) {
      const errorMsg = "命令不能为空";
      console.error(errorMsg);
    }
    try {
      await sendTerminalCommand(this.sessionId, command);
      // 如果 await 没有抛出错误，说明命令已成功发送 (C++ resolve 了)
      console.log(`命令 '${command}' 发送成功 (Promise resolved),webId'${this.webController?.getWebId()}'发送了消息`);
      // 函数正常结束，隐式返回一个 resolved 的 Promise<void>
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : String(err);
      console.error(`终端命令处理错误: ${errorMsg}`);
    }
  }

  // Web页面加载完成后初始化消息端口
  webPageEnd() {
    console.log('dyx this.webController',this.webController)
    if (!this.webController) {
      return;
    }
    // 创建消息端口数组
    this.ports = this.webController.createWebMessagePorts(true);
    this.nativePorts = this.ports[1]
    console.log('webPageEnd 创建消息端口后 nativePorts 赋值成功' + this.nativePorts)

    if (this.nativePorts != null) {
      emitter.emit("port" + this.sessionId);
      console.log('emitter 执行发送')
    }

    // 仅在拿到有效 sessionId 时登记端口，避免 -1 作为键
    if (this.sessionId >= 0) {
      let nowGlobalPorts = AppStorage.get('globalPorts') as Map<number, webview.WebMessagePort>;
      if (!nowGlobalPorts) {
        console.log("获取全局 Map，没有就初始化")
        nowGlobalPorts = new Map<number, webview.WebMessagePort>();
      }
      nowGlobalPorts.set(this.sessionId, this.nativePorts as webview.WebMessagePort);
      AppStorage.set('globalPorts', nowGlobalPorts);
      this.portMappedReady = true;
    } else {
      this.portMappedReady = false; // 等 sessionId 变化时在 onSessionIdChanged 中登记
    }

    // 注册Web端消息处理函数
    this.nativePorts?.onMessageEventExt(async (result) => {
      try {
        const type = result.getType();
        if (type === webview.WebMessageType.STRING) {
          const command = result.getString();
          if (command === "__terminal_ready__") {
            // H5 确认 xterm 已打开，可安全写入
            this.h5TerminalReady = true;
            if (this.portMappedReady) {
              this.pendingTerminalData();
            }
            return;
          }
          // 接收"ready"消息时发送终端配置
          if (command === "ready") {
            console.info("Web端准备就绪，发送配置");
            this.sendInfo();
            return;
          }
          // 处理终端大小变更消息
          if (command.startsWith("resize:")) {
            const parts = command.split(":");
            if (parts.length === 2) {
              const sizeInfo = parts[1];
              try {
                const sizeParts = sizeInfo.split(",");
                if (sizeParts.length === 2) {
                  const cols = Number(sizeParts[0]);
                  const rows = Number(sizeParts[1]);
                  if (cols && rows) {
                    await this.resizeTerminalToSize(cols, rows);
                  }
                }
              } catch (e) {
                console.error("解析终端大小信息错误:", e);
              }
            }
            return;
          }
          // 收到Web端发来的命令，发送到SSH
          console.info("从Web端收到命令:", command);
          await this.sendCommand(command);
        }
      } catch (err) {
        console.error('终端命令处理错误:', err);
      }
    });
    // 发送端口到HTML5
    this.webController.postMessage('__init_port__', [this.ports[0]], "*");

    // 注册数据回调，用于接收SSH的响应
    registerDataCallback(this.globalDataDispatcher);

    this.pendingTerminalData();
  }

  // sessionId 变更后（通常从 -1 变为有效），补登记 globalPorts 并触发补发
  onSessionIdChanged() {
    try {
      if (this.sessionId < 0 || !this.nativePorts) {
        this.portMappedReady = false;
        return;
      }
      let nowGlobalPorts = AppStorage.get('globalPorts') as Map<number, webview.WebMessagePort>;
      if (!nowGlobalPorts) {
        nowGlobalPorts = new Map<number, webview.WebMessagePort>();
      }
      // 如存在旧键 -1，对应本端口则清理
      const invalidKey: number = -1;
      if (nowGlobalPorts.get(invalidKey) === this.nativePorts) {
        nowGlobalPorts.delete(invalidKey);
      }
      nowGlobalPorts.set(this.sessionId, this.nativePorts);
      AppStorage.set('globalPorts', nowGlobalPorts);
      this.portMappedReady = true;
      if (this.h5TerminalReady) {
        this.pendingTerminalData();
      }
    } catch (e) {
      console.error('onSessionIdChanged error:', e);
    }
  }

  build() {
    Column() {
      Web({
        src: $rawfile('xterm/xterm.html'),
        controller: this.webController,
        renderMode: RenderMode.SYNC_RENDER,
      })
        .allowDrop(null)
        .borderRadius(0)
        .width('100%')
        .height('100%')
        .onPageBegin(() => {
          emitter.on("port" + this.sessionId, () => {
            console.log('emitter 接受到了')
            this.pendingTerminalData()
          })
        })
        .onPageVisible(() => {
          this.webPageEnd()
        })
        .onAppear(() => {
          if (this.webController) {
            this.webController.registerJavaScriptProxy({
              getWindowResizedId: async () => {
                let windowSize = ''
                let id = ''
                try {
                  windowSize = await this.allDataKvStore?.get('WindowSize') as string
                  id = (JSON.parse(windowSize) as WindowSizeType).windowResizedId
                } catch (e) {
                  id = pageDefaultData.WindowSize.windowResizedId
                }
                return id
              }
            }, 'arkTS', ['getWindowResizedId'])
            this.webController.runJavaScriptExt('getWindowResizedId()')
          }
        })
        .textAutosizing(true)
        .backgroundColor("#181818")
        .javaScriptAccess(true)
        .zoomAccess(false)
    }
    .borderRadius(0)
    .backgroundColor("#181818")
    .width('100%')
    .height('100%')
  }
}
