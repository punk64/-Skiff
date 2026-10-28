import { distributedKVStore } from '@kit.ArkData';
import { ConnectionType, LoggingType, SessionType } from '../../../../common/types/dataType'
import { pageDefaultData } from '../../pageDefaultData';
import { KvManager } from '../../../../common/utils/KVManager'
import { permissionManager } from '../../../../common/utils/PremissisonsManager';
import { Environment, fileShare, picker } from '@kit.CoreFileKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { pointer } from '@kit.InputKit';
import deepClone from '../../../../common/tools/deepClone';
import { BusinessError } from '@ohos.base';
import { common, Permissions } from '@kit.AbilityKit';
import { fileUri } from '@kit.CoreFileKit';
import { text } from '@kit.ArkGraphics2D';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { promptAction } from '@kit.ArkUI';
import { connectionType } from '..';

interface A {
  test: () => void
}

let api: A = {
  test() {
  }
}
api.test()

@Component
struct Logging {
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

  @State logTempFileName: string = ''
  @State @Watch('nowSessionDataChange') nowData: LoggingType = pageDefaultData.Logging
  @State sessionData: SessionType = pageDefaultData.Session
  @Provide logFileUri: string = '';
  @Provide fileFd: number = -1;
  // KVManager实例
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  @LocalStorageLink('defaultData') defaultData: string = ''
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @State LogQuanUrl: string = ''
  @LocalStorageLink(connectionType) @Watch('connectionTypeChange') connectionType: 'SSH' | 'Telnet' = 'SSH'

  connectionTypeChange(){
    console.log('sx connectionTypeChange 改变了'+this.connectionType)
    if((this.connectionType == 'Telnet')&&(this.nowData.logType == 3 || this.nowData.logType == 4)){
      this.nowData.logType = 0
    }
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

  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  aboutToAppear() {
    // 当页面挂载之前，初始化页面
    console.log('aboutToAppear')
    const permission: Permissions[] = [
      'ohos.permission.READ_WRITE_DOCUMENTS_DIRECTORY',
      'ohos.permission.FILE_ACCESS_PERSIST',
      'ohos.permission.READ_WRITE_DOWNLOAD_DIRECTORY',
    // 'ohos.permission.READ_IMAGEVIDEO',
    // 'ohos.permission.WRITE_IMAGEVIDEO'
    ]
    if (!permissionManager.checkPermissions(permission)) {
      permissionManager.requestPermissions(permission).then(res => {
        if (res) {
          console.log("获取权限成功")
          this.initFileBaseUri();
        } else {
          permissionManager.openPermissionSettingsPage()
          return;
        }
      })
    }
    this.init()
  }

  aboutToDisappear(): void {
    this.allDataKvStore?.off('dataChange')
    //还原数据
    // this.allDataKvStore?.put(loggingMultiSelConfig, JSON.stringify({}))
  }

  // 页面初始化
  async init(): Promise<void> {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    this.nowData = deepClone(pageDefaultData.Logging)
    console.log('页面初始化时 nowdata内容' + JSON.stringify(this.nowData))
    this.LogQuanUrl =
      this.nowData.logFileBaseUri + (this.nowData.logFileName.length > 0 ? "/" + this.nowData.logFileName : '')
    this.saveStore()
    this.sessionData = JSON.parse(await this.allDataKvStore?.get('Session') as string) as SessionType
    //初始化可选项
    // this.changeLoggingMultiSelConfig()
  }

  // 持久化数据
  saveStore() {
    this.allDataKvStore && this.allDataKvStore.put('Logging', JSON.stringify(this.nowData))
    console.log('log 数据 进行保存数据库', JSON.stringify(this.nowData));
  }

  // nowData变更时触发
  nowSessionDataChange() {
    // this.initLogFileUri()
    this.saveStore()
    console.log('log 数据进行更改时保存', JSON.stringify(this.nowData));
  }

  async saveSessionChange() {
    try {
      console.log('saveSession--------->', JSON.stringify(JSON.parse(this.defaultData).Logging))
      this.nowData = deepClone(JSON.parse(this.defaultData).Logging as LoggingType)
      console.log('加载时获取到的log数据是' + JSON.stringify(this.nowData))
      this.LogQuanUrl =
        this.nowData.logFileBaseUri + (this.nowData.logFileName.length > 0 ? "/" + this.nowData.logFileName : '')
      if (canIUse('SystemCapability.FileManagement.AppFileService.FolderAuthorization') &&
        this.nowData.policiesArray.length > 0) {
        fileShare.activatePermission(this.nowData.policiesArray).then(res => {
          console.log('激活持久化文件成功', res)
        }).catch((err: Error) => {
          console.error('激活持久化文件失败', err)
        })
      }
      this.sessionData = JSON.parse(await this.allDataKvStore?.get('Session') as string) as SessionType
    } catch (e) {
      console.error(e)
    }
  }

  initFileBaseUri() {
    console.log('初始化路径开始')
    try {
      try {
        // 获取 document/CrossShellNext 目录
        const documentPath = Environment.getUserDocumentDir();
        let docFilenames = fs.listFileSync(`${documentPath}`);
        //确保公告文件目录中存在CrossShellNext目录
        let index = docFilenames.findIndex((e) => e === 'CrossShellNext');
        if (index === -1) {
          fs.mkdirSync(`${documentPath}/CrossShellNext`, true);
          console.log("不存在直接创建")
        } else {
          let isDirectory = fs.statSync(`${documentPath}/CrossShellNext`).isDirectory();
          if (!isDirectory) {
            fs.mkdirSync(`${documentPath}/CrossShellNext`, true);
            console.log("不存在同名文件夹直接创建")
          }
        }
        this.nowData.logFileBaseUri = `${documentPath}/CrossShellNext`;
      } catch (e) {
        console.log('日志路径初始化错误' + e.code + e.message)
      }

      // let filenames  = fs.listFileSync(`${documentPath}/Putty`);
      // console.log('filenames',JSON.stringify(filenames))

      console.log('初始化路径结束')

    } catch (error) {
      console.log('err', error)
    }
  }

  // 格式化文件名中的特殊占位符
  formatFileName(fileName: string): string {
    console.log('格式化 日志名执行了' + fileName)
    if (!fileName) {
      return '';
    }

    const now = new Date();
    const year = now.getFullYear().toString();
    const month = (now.getMonth() + 1).toString().padStart(2, '0');
    const day = now.getDate().toString().padStart(2, '0');
    const hours = now.getHours().toString().padStart(2, '0');
    const minutes = now.getMinutes().toString().padStart(2, '0');
    const seconds = now.getSeconds().toString().padStart(2, '0');

    let formattedName = fileName;
    formattedName = formattedName.replace(/&Y/g, year);
    formattedName = formattedName.replace(/&M/g, month);
    formattedName = formattedName.replace(/&D/g, day);
    formattedName = formattedName.replace(/&T/g, `${hours}${minutes}${seconds}`);

    // 如果需要添加主机名和端口号的替换
    formattedName = formattedName.replace(/&H/g, this.sessionData.hostName || '');
    formattedName = formattedName.replace(/&P/g, this.sessionData.port?.toString() || '');

    return formattedName;
  }

  decodeUriPath(uri: string): string {
    try {
      return decodeURIComponent(uri);
    } catch (error) {
      console.error('URI解码失败:', error);
      return uri;
    }
  }

  build() {
    Column({ space: 10 }) {
      Text($r('app.string.Sessionlogging')).width('100%').textAlign(TextAlign.Start)
      Flex({ wrap: FlexWrap.Wrap }) {
        Row() {
          Radio({ value: 'SSH', group: 'SessionLogging_radioGroup' })
            .checked(this.nowData.logType === 0)
            .onClick(() => {
              this.nowData.logType = 0
              // 重置  SSH数据包日志记录的特定选项
              this.nowData.omitKnownPassword = false
              this.nowData.omitSessionData = false
            })
          Text($r('app.string.None'))
            .textAlign(TextAlign.Start)
            .width('75%')
            .textOverflow({ overflow: TextOverflow.MARQUEE })
            .fontColor(this.nowData.logType == 0 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
        }.width('33%').hoverStyles()

        Row() {
          Radio({ value: 'SSH', group: 'SessionLogging_radioGroup' })
            .checked(this.nowData.logType === 1)
            .onClick(() => {
              this.nowData.logType = 1
              // 重置  SSH数据包日志记录的特定选项
              this.nowData.omitKnownPassword = false
              this.nowData.omitSessionData = false
            })
          Text($r('app.string.Printableoutput'))
            .textAlign(TextAlign.Start)
            .width('75%')
            .fontColor(this.nowData.logType == 1 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            .textOverflow({ overflow: TextOverflow.MARQUEE })
        }.width('33%').hoverStyles()

        Row() {
          Radio({ value: 'SSH', group: 'SessionLogging_radioGroup' })
            .checked(this.nowData.logType === 2)
            .onClick(() => {
              this.nowData.logType = 2
              // 重置  SSH数据包日志记录的特定选项
              this.nowData.omitKnownPassword = false
              this.nowData.omitSessionData = false
            })
          Text($r('app.string.Allsessionoutput'))
            .textAlign(TextAlign.Start)
            .width('75%')
            .fontColor(this.nowData.logType == 2 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            .textOverflow({ overflow: TextOverflow.MARQUEE })
        }.width('33%').hoverStyles()

        //-------------------------------------
        //当选中ssh时显示
        //todo  多选一
        if (this.connectionType == 'SSH') {
          Row() {
            Radio({ value: 'SSH', group: 'SessionLogging_radioGroup' })
              .checked(this.nowData.logType === 3)
              .onClick(() => {
                this.nowData.logType = 3
              })
            Text($r('app.string.SSHpackets'))
              .textAlign(TextAlign.Start)
              .width('75%')
              .fontColor(this.nowData.logType == 3 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
              .textOverflow({ overflow: TextOverflow.MARQUEE })
          }.width('33%').hoverStyles()

          Row() {
            Radio({ value: 'SSH', group: 'SessionLogging_radioGroup' })
              .checked(this.nowData.logType === 4)
              .onClick(() => {
                this.nowData.logType = 4
              })
            Text($r('app.string.SSHpacketsandrawdata'))
              .textAlign(TextAlign.Start)
              .width('75%')
              .fontColor(this.nowData.logType == 4 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
              .textOverflow({ overflow: TextOverflow.MARQUEE })
          }.width('33%').hoverStyles()
        }


        //-------------------------------------------
      }.width('100%')

      Text($r('app.string.Logfilename')).width('100%').textAlign(TextAlign.Start)
      Flex({ direction: FlexDirection.Row, alignItems: ItemAlign.Center, justifyContent: FlexAlign.SpaceBetween }) {
        Text(`${this.LogQuanUrl}`)
          .borderRadius(0)
          .padding(2)
          .textOverflow({ overflow: TextOverflow.MARQUEE })
          .border({ width: 1, style: BorderStyle.Solid })
          .backgroundColor(Color.Transparent)
          .width('90%')// .maxLength(50)
          // .onChange((value: string) => {
          //   this.LogQuanUrl = value
          // })
          .onBlur(() => {
            // 判断当前路径是否存在 | 合法
            // try{
            //   const stateFile:boolean = fs.accessSync((new fileUri.FileUri(this.LogQuanUrl)).path,fs.AccessModeType.READ_WRITE,fs.AccessFlagType.LOCAL)
            //   console.log('文件是否合法1',this.LogQuanUrl,stateFile)
            //
            //   // 当前文件或文件夹存在
            //   if( (stateFile)){
            //     // 判断其为文件还是文件夹
            //     if( /\.\w+$/.test(this.LogQuanUrl) && (this.LogQuanUrl.endsWith('.log') || this.LogQuanUrl.endsWith('.txt')) ){
            //       const parts = this.LogQuanUrl.split("/");
            //       this.logTempFileName = parts[parts.length - 1];
            //       this.nowData.logFileName  = this.formatFileName(this.logTempFileName);
            //       parts.pop()
            //       this.nowData.logFileBaseUri = parts.join('/')
            //       return;
            //     }else if(!/\.\w+$/.test(this.LogQuanUrl)){
            //       this.getUIContext().getPromptAction().showToast({
            //         message:'当前路径为文件夹路径，已自动生成日志文件名',
            //         duration:2000,
            //         showMode:promptAction.ToastShowMode.DEFAULT,
            //         bottom:85
            //       })
            //     }else{
            //       this.getUIContext().getPromptAction().showToast({
            //         message:'当前路径文件后缀不合法',
            //         duration:2000,
            //         showMode:promptAction.ToastShowMode.DEFAULT,
            //         bottom:85
            //       })
            //     }
            //     // const date = new Date();
            //     // const parts = this.LogQuanUrl.split("/");
            //     // this.nowData.logFileBaseUri = parts.join('/')
            //     // this.nowData.logFileName  = this.formatFileName(`CrossShellNext_${date.getFullYear()}${date.getMonth()>8?(date.getMonth()+1):'0'+(date.getMonth()+1)}${date.getDate()>9?date.getDate():'0'+date.getDate()}.log`);
            //     // this.LogQuanUrl = this.nowData.logFileBaseUri + "/" + this.nowData.logFileName
            //   }else{
            //     this.getUIContext().getPromptAction().showToast({
            //       message:'当前路径不合法',
            //       duration:2000,
            //       showMode:promptAction.ToastShowMode.DEFAULT,
            //       bottom:85
            //     })
            //     // 当前文件或文件夹不存在，重置为默认路径
            //     this.LogQuanUrl = this.nowData.logFileBaseUri +( this.nowData.logFileName.length>0? "/" + this.nowData.logFileName:'')
            //   }
            //   console.log("格式化 后的名字是" + this.nowData.logFileName )
            //   console.log("格式化 后的全路径是" + this.LogQuanUrl)
            // }catch (e) {
            //   console.error('输入框失焦逻辑异常',e)
            // }
          })
          // .hoverStyles()
          .inputTextStyle()

        Button($r('app.string.browse'), { type: ButtonType.Normal })
          .buttonStyle(ButtonStyleMode.NORMAL)
          .ButtonStyles()
          .width('14%')
          .fontColor($r('app.color.font_color'))
          .fontSize(14)
          .fontWeight(400)
          .onClick(() => {
            let context = getContext(this) as common.Context; // 请确保getContext(this)返回结果为UIAbilityContext
            let documentSelectOptions = new picker.DocumentSelectOptions();
            if (canIUse('SystemCapability.FileManagement.UserFileService.FolderSelection')) {
              documentSelectOptions.selectMode = picker.DocumentSelectMode.MIXED
              documentSelectOptions.mergeMode = picker.MergeTypeMode.DOCUMENT
              // documentSelectOptions.authMode = true; // 需在2in1设备启用授权模式
            }

            documentSelectOptions.defaultFilePathUri =
              fileUri.getUriFromPath(Environment.getUserDocumentDir() + '/CrossShellNext')
            documentSelectOptions.fileSuffixFilters =
              ['文字/Word格式( *.txt *.log)|*.txt,*.log', '.txt', '.log']

            let documentPicker = new picker.DocumentViewPicker(context);
            documentPicker.select(documentSelectOptions).then((documentSaveResult: Array<string>) => {
              console.info('DocumentViewPicker.save successfully, documentSaveResult uri: ' +
              JSON.stringify(documentSaveResult));
              if (documentSaveResult[0] != undefined) {
                const pathToUri = new fileUri.FileUri(documentSaveResult[0])
                // 授权持久化
                let policyInfo: fileShare.PolicyInfo = {
                  uri: documentSaveResult[0],
                  operationMode: fileShare.OperationMode.READ_MODE | fileShare.OperationMode.WRITE_MODE,
                };
                console.log('policyInfo', JSON.stringify(policyInfo))
                fileShare.persistPermission([policyInfo]).then(() => {
                  console.info("persistPermission successfully");
                  this.nowData.policiesArray = [policyInfo]
                }).catch((err: BusinessError<Array<fileShare.PolicyErrorResult>>) => {
                  console.error("persistPermission failed with error message: " + err.message + ", error code: " +
                  err.code);
                });
                let isDirectory: boolean
                try {
                  isDirectory = fs.statSync(pathToUri.path).isDirectory()
                  console.log('选择了文件夹', isDirectory, pathToUri.path)
                  if (isDirectory) {
                    const date = new Date()
                    this.nowData.logFileBaseUri = this.decodeUriPath(pathToUri.path)
                    this.nowData.logFileName =
                      `CrossShellNext_${date.getFullYear()}${date.getMonth() > 8 ? (date.getMonth() + 1) :
                        '0' + (date.getMonth() + 1)}${date.getDate() > 9 ? date.getDate() : '0' + date.getDate()}.log`
                  } else {
                    const pathArr = pathToUri.path.split('/')
                    this.nowData.logFileName = pathArr[pathArr.length-1]
                    pathArr.pop()
                    this.nowData.logFileBaseUri = this.decodeUriPath(pathArr.join('/'))
                  }

                  console.log('this.nowData.logFileBaseUri', this.nowData.logFileBaseUri)
                  console.log('this.nowData.logFileName', this.nowData.logFileName)
                } catch (e) {
                  console.log('选择出错', e)
                }

                this.LogQuanUrl = this.nowData.logFileBaseUri +
                  (this.nowData.logFileName.length > 0 ? "/" + this.nowData.logFileName : '')
                console.log('日志选择 的全路径' + this.LogQuanUrl)
                console.log('日志选择 的base路径' + this.nowData.logFileBaseUri)
                console.log('日志选择 的文件名' + this.nowData.logFileName)
              }
              hilog.debug(0, '', `日志选择 的全路径:${this.nowData.logFileBaseUri}`)
            }).catch((err: BusinessError) => {
              console.error('DocumentViewPicker.save failed with err: ' + JSON.stringify(err));
            });
          })
      }.height('10%')

      // Text($r('app.string.Logtip'))
      //   .fontColor('#aaa').width('100%').textAlign(TextAlign.Start)
      Text($r('app.string.Logsave'))
        .width('100%')
        .textAlign(TextAlign.Start)
      Flex({ direction: FlexDirection.Column }) {
        Row() {
          Radio({ value: 'Always overwrite it', group: 'existOperation_radioGroup' })
            .checked(this.nowData.existOperation === 0)
            .onClick(() => {
              this.nowData.existOperation = 0
            })
          Text($r('app.string.overwrite'))
            .textAlign(TextAlign.Start)
            .width('75%')
            .fontColor(this.nowData.existOperation == 0 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            .textOverflow({ overflow: TextOverflow.MARQUEE })
        }.width('40%').hoverStyles().onClick(() => {
          this.nowData.existOperation = 0
        })

        Row() {
          Radio({ value: 'Always append to the end of it', group: 'existOperation_radioGroup' })
            .checked(this.nowData.existOperation === 1)
            .onClick(() => {
              this.nowData.existOperation = 1
            })
          Text($r('app.string.append'))
            .textAlign(TextAlign.Start)
            .width('75%')
            .fontColor(this.nowData.existOperation == 1 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            .textOverflow({ overflow: TextOverflow.MARQUEE })
        }.width('60%').hoverStyles().onClick(() => {
          this.nowData.existOperation = 1
        })

        //---------------------------------------------
        //todo 可选
        Row() {
          Checkbox()
            .select(this.nowData.includeHeader)
            .shape(CheckBoxShape.ROUNDED_SQUARE)
            .onChange((isChecked) => {
              this.nowData.includeHeader = isChecked
            })
          Text($r('app.string.Includeheader'))
            .textAlign(TextAlign.Start)
            .width('75%')
            // .fontColor(this.nowData.existOperation == 1 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
            .textOverflow({ overflow: TextOverflow.MARQUEE })
        }.width('60%').hoverStyles()

        //---------------------------------------------
      }.width('100%')

      //-------------------------------------------------
      if([3,4].indexOf(this.nowData.logType)>=0){
        Column() {
          Text($r('app.string.Options_specific_to_SSH_packet_logging'))
          Row() {
            Checkbox().shape(CheckBoxShape.ROUNDED_SQUARE)
              .select(this.nowData.omitKnownPassword)
              .onChange((isChecked) => {
                this.nowData.omitKnownPassword = isChecked
              })
            Text($r('app.string.Omit_known_password_fields'))
              .textAlign(TextAlign.Start)
              .width('75%')
              // .fontColor(this.nowData.existOperation == 1 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
              .textOverflow({ overflow: TextOverflow.MARQUEE })
          }.width('60%').hoverStyles()

          Row() {
            Checkbox().shape(CheckBoxShape.ROUNDED_SQUARE)
              .select(this.nowData.omitSessionData)
              .onChange((isChecked) => {
                this.nowData.omitSessionData = isChecked
              })
            Text($r('app.string.Omit_session_data'))
              .textAlign(TextAlign.Start)
              .width('75%')
              // .fontColor(this.nowData.existOperation == 1 ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
              .textOverflow({ overflow: TextOverflow.MARQUEE })
          }.width('60%').hoverStyles()
        }.width('100%').alignItems(HorizontalAlign.Start)
      }
      //-------------------------------------------------
      // Button('addText').onClick(() => {this.addText()})
    }
    .width('100%')
    .backgroundColor($r('app.color.titleCar_color'))
    .padding(10)
    .margin({ bottom: 10 })
    .borderRadius(3)
  }
}

export default Logging