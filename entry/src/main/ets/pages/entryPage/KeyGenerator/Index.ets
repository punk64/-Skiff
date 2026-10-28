import TitleCard from '../../../components/TitleCard'
import { pointer } from '@kit.InputKit'
import { common } from '@kit.AbilityKit'
import { fileIo as fs, picker } from '@kit.CoreFileKit'
import promptAction from '@ohos.promptAction'
import { generateKeyPair } from 'libentry.so'
import { pageDefaultData } from '../pageDefaultData'
import { KeyGenType } from '../../../common/types/dataType'
import deepClone from '../../../common/tools/deepClone'
import { distributedKVStore } from '@kit.ArkData'
import { KvManager } from '../../../common/utils/KVManager'

interface KeyGeneratorData {
  keyType: string,
  keyFilePath: string,
  keyPassword: string,
  confirmPassword: string,
  publicKey: string,
  keyFile?: fs.File,
  publicKeyFile?: fs.File
}

@Component
export default struct KeyGenerator {
  @State @Watch('nowDataChange') nowData1: KeyGenType = pageDefaultData.KeyGen
  @State nowData: KeyGeneratorData = {
    keyType: 'RSA',
    keyFilePath: '',
    keyPassword: '',
    confirmPassword: '',
    publicKey: ''
  }
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined
  private context = getContext(this) as common.UIAbilityContext
  @State publicKeyUrl: string = ''
  // KVManager实例
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })

  @State fileTemp:string=''

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
    .height(30)
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

  async aboutToAppear(): Promise<void> {
    this.allDataKvStore = await this.KVManager.getStore('allData')
  }

  nowDataChange() {
    this.saveStore()
  }

  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('KeyGen', JSON.stringify(this.nowData1))
  }

  // 工具函数：生成文件名
  generateAutoFileName(): string {
    const now = new Date()
    const pad = (n: number) => n.toString().padStart(2, '0')
    const yyyy = now.getFullYear()
    const MM = pad(now.getMonth() + 1)
    const dd = pad(now.getDate())
    const HH = pad(now.getHours())
    const mm = pad(now.getMinutes())
    const ss = pad(now.getSeconds())
    return `${this.nowData.keyType}_${yyyy}${MM}${dd}_${HH}${mm}${ss}.pem`
  }

  // 浏览文件按钮点击回调，调出系统文件选择器
  async browseFiles() {
    try {
      // 创建文件管理器选项实例
      const documentSelectOptions = new picker.DocumentSelectOptions();
      documentSelectOptions.selectMode = picker.DocumentSelectMode.FOLDER
      documentSelectOptions.maxSelectNumber = 1
      documentSelectOptions.defaultFilePathUri='storage/Users/currentUser'
      // 自动生成文件名
      const autoFileName = this.generateAutoFileName()
      // 创建DocumentViewPicker实例
      let documentPicker = new picker.DocumentViewPicker(this.context)
      // 调用 select 方法让用户选择保存位置（不让用户自定义文件名）
      const files = await documentPicker.select(documentSelectOptions)
      console.log('选择的文件路劲是' + files)
      this.fileTemp=files[0]
      if (files[0] != '') {
        let temp = this.extractPath(files[0])
        // 解码路径中的URI编码字符（如中文）
        temp = this.decodeUriPath(temp)
        files[0] = this.decodeUriPath(files[0])
        console.log('密钥文件全路径是'+temp+"/"+autoFileName)
        const selectedFile = fs.openSync(temp+"/"+autoFileName, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE)
        this.nowData.keyFilePath = files[0]+"/"+autoFileName
        this.nowData1.keyFilePath = files[0]+"/"+autoFileName
        this.nowData.keyFile = selectedFile
        console.log('选择的文件：', selectedFile.name, '路径：', files[0])
      }
    } catch (error) {
      console.error('文件选择失败：', error)
    }
  }

  extractPath(input: string): string {
    // 检查输入是否以 "file://docs" 开头
    if (input.startsWith("file://docs")) {
      // 截取掉 "file://docs" 部分
      return input.substring(11); // "file://docs/" 的长度是 11
    } else {
      return ''
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

  // 生成按钮点击回调
  async generateKey() {
    // 私钥口令不能为空：避免生成未加密私钥（安全项）
    if (!this.nowData.keyPassword || this.nowData.keyPassword.length === 0) {
      this.getUIContext().getPromptAction().showToast({
        message: $r('app.string.KeyGenerator_Password_Error_Toast'),
        duration: 3000
      })
      return;
    }
    if(this.nowData.confirmPassword !== this.nowData.keyPassword){
      this.getUIContext().getPromptAction().showToast({
        message:$r('app.string.Passwords_do_not_match'),
        textColor:$r('app.color.warning_textColor'),
        duration:4000
      })
      return;
    }
    try {
        // 调用browseFiles方法
        await this.browseFiles()
        // 如果用户取消了选择，则中止操作
        if ( !this.fileTemp) {
          console.log('秘钥生成取消')
          return
        }

      // 检查密码是否匹配
      if (this.nowData.keyPassword !== this.nowData.confirmPassword) {
        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.KeyGenerator_Password_Error_Toast'),
          duration: 3000
        })
        return
      }
      this.publicKeyUrl = this.extractPath(this.nowData.keyFilePath)
      // fs.mkdirSync(this.nowData.keyFilePath+'.pub')
      // 创建临时文件用于存储公钥
      const tempPublicKeyFile = this.publicKeyUrl + '.pub'
      console.log('公钥文件url' + tempPublicKeyFile)
      const publicKeyFile = await fs.open(tempPublicKeyFile, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE)
      this.nowData.publicKeyFile = publicKeyFile

      // 调用生成密钥对函数
      const result = await generateKeyPair(
        this.nowData.keyType,
        this.nowData.keyPassword,
        this.nowData.keyFile?.fd,
        publicKeyFile.fd
      )

      if (result) {
        // 读取生成的公钥
        const publicKeyContent = await fs.readText(tempPublicKeyFile)
        console.log(publicKeyContent)
        // 更新UI显示公钥内容
        this.nowData.publicKey = publicKeyContent

        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.KeyGenerator_Successful_Toast'),
          duration: 3000
        })
      } else {
        this.getUIContext().getPromptAction().showToast({
          message: $r('app.string.KeyGenerator_Error_Toast'),
          duration: 3000
        })
      }
    } catch (error) {
      console.error('生成密钥失败：', error)
      promptAction.showToast({
        message: $r('app.string.KeyGenerator_Error_Toast'),
        duration: 3000
      })
    }
  }

  // 页面销毁时关闭打开的文件
  aboutToDisappear() {
    // 关闭密钥文件
    if (this.nowData.keyFile) {
      try {
        fs.close(this.nowData.keyFile.fd)
        console.log('密钥文件已关闭')
      } catch (error) {
        console.error('关闭密钥文件失败：', error)
      }
    }

    // 关闭公钥文件
    if (this.nowData.publicKeyFile) {
      try {
        fs.close(this.nowData.publicKeyFile.fd)
        console.log('公钥文件已关闭')
      } catch (error) {
        console.error('关闭公钥文件失败：', error)
      }
    }
  }

  build() {
    Column() {
      TitleCard({ titleString: $r('app.string.KeyGeneratorTitle') }) {
        Scroll() {
          Column({ space: 16 }) {
            // 密钥类型选择
            Text($r('app.string.KeyGeneratortype')).textAlign(TextAlign.Start).width('100%')
            Column({ space: 10 }) {
              Flex({ direction: FlexDirection.Row, justifyContent: FlexAlign.Start, wrap: FlexWrap.Wrap }) {
                Row({ space: 5 }) {
                  Radio({ value: 'RSA', group: 'keyTypeGroup' })
                    .checked(this.nowData.keyType === 'RSA')
                    .onChange((isChecked: boolean) => {
                      if (isChecked) {
                        this.nowData.keyType = 'RSA'
                      }
                    })
                  Text('RSA')
                    .fontColor(this.nowData.keyType == 'RSA' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                }
                .hoverStyles()
                .margin({ right: 20 })


                Row({ space: 5 }) {
                  Radio({ value: 'ECDSA', group: 'keyTypeGroup' })
                    .checked(this.nowData.keyType === 'ECDSA')
                    .onChange((isChecked: boolean) => {
                      if (isChecked) {
                        this.nowData.keyType = 'ECDSA'
                      }
                    })
                  Text('ECDSA')
                    .fontColor(this.nowData.keyType == 'ECDSA' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                }
                .hoverStyles()
                .margin({ right: 20 })

                Row({ space: 5 }) {
                  Radio({ value: 'ED25519', group: 'keyTypeGroup' })
                    .checked(this.nowData.keyType === 'ED25519')
                    .onChange((isChecked: boolean) => {
                      if (isChecked) {
                        this.nowData.keyType = 'ED25519'
                      }
                    })
                  Text('ED25519')
                    .fontColor(this.nowData.keyType == 'ED25519' ? $r('app.color.light_Select'): $r('app.color.light_noSelect'))
                }
                .hoverStyles()
                .margin({ right: 20 })

              }
            }
            .alignItems(HorizontalAlign.Start)
            .width('100%')

            // 密钥文件路径
            Text($r('app.string.KeyGeneratorPath')).textAlign(TextAlign.Start).width('100%')
            Column({ space: 10 }) {
              Flex({ justifyContent: FlexAlign.SpaceBetween }) {
                TextInput({ text: this.nowData.keyFilePath ,placeholder: $r('app.string.Click_Generate_to_select_the_path') })
                  .inputTextStyle()
                  .width('100%')
                  .hoverStyles()
                  .enabled(false)
                }
                .width('100%')

                // 密钥密码
                  Text($r('app.string.KeyPassword')).textAlign(TextAlign.Start).width('100%')
                  Flex({ direction: FlexDirection.Column, alignItems: ItemAlign.Start }) {
                    TextInput({ text: this.nowData.keyPassword })
                      .inputTextStyle()
                      .width('100%')
                      .type(InputType.Password)
                      .hoverStyles()
                      .onChange((value: string) => {
                        this.nowData.keyPassword = value
                      })
                  }
                  .width('100%')

                // 确认密码
                  Text($r('app.string.ConfirmPassword')).textAlign(TextAlign.Start).width('100%')
                  Flex({ direction: FlexDirection.Column, alignItems: ItemAlign.Start }) {
                    TextInput({ text: this.nowData.confirmPassword })
                      .inputTextStyle()
                      .width('100%')
                      .type(InputType.Password)
                      .hoverStyles()
                      .onChange((value: string) => {
                        this.nowData.confirmPassword = value
                      })
                  }
                  .width('100%')

                // 公钥
                  Text($r('app.string.PublicKey')).textAlign(TextAlign.Start).width('100%')
                  Flex({ direction: FlexDirection.Column, alignItems: ItemAlign.Start }) {
                    TextArea({ text: this.nowData.publicKey })
                      .inputTextStyle()
                      .width('100%')
                      .height(120)
                      .hoverStyles()
                  }
                  .width('100%')

                // 生成按钮
                Flex({ justifyContent: FlexAlign.End }) {
                  Button($r('app.string.Generate'), { type: ButtonType.Normal })
                    .buttonStyle(ButtonStyleMode.NORMAL)
                    .fontColor($r('app.color.font_color'))
                    .fontSize(16)
                    .fontWeight(500)
                    .width(160)
                    .ButtonStyles()
                    .onClick(() => this.generateKey())
                }
                .width('100%')
                .padding(20)
              }
              .padding({right: 20})
              .width('100%')
            }
          }.scrollBar(BarState.Off)
        }
      }.width('100%').height('100%')
    }
  }




