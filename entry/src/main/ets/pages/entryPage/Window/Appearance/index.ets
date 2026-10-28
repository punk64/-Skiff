import { AppearType, CursorType } from '../../../../common/types/dataType';
import { KvManager } from '../../../../common/utils/KVManager';
import TitleCard from '../../../../components/TitleCard';
import { pageDefaultData } from '../../pageDefaultData';
import { distributedKVStore } from '@kit.ArkData';
import deepClone from '../../../../common/tools/deepClone';
import { i18n } from '@kit.LocalizationKit';
import { preferencesUtil } from '../../../../common/utils/preferenceutils';

let applicationContext = getContext().getApplicationContext();
@Component
export default struct Appearance {
  @State @Watch('watchNowDataChange') nowData: CursorType = deepClone(pageDefaultData.Cursor)
  @State @Watch('saveAppearStore') appearData: AppearType = deepClone(pageDefaultData.Appear)
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  @LocalStorageLink('defaultData') defaultData: string = ''
  @State op1: string = '#ECECEC'
  @State op: string = '#ECECEC'
  @StorageProp('currentColorMode') currentMode: number = -1;
  //测试用
  @State message: string | Resource = $r('app.string.test_content');

  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  // 监听 nowData 改变时触发
  watchNowDataChange() {
    this.saveStore()
  }

  async saveStore() {
    this.allDataKvStore && await this.allDataKvStore.put('Cursor', JSON.stringify(this.nowData))
  }

  async saveAppearStore() {
    this.allDataKvStore && await this.allDataKvStore.put('Appear', JSON.stringify(this.appearData))
    console.log('保存到数据库的配置页样式信息是',JSON.stringify(this.appearData))

    // 设置应用颜色模式
    applicationContext.setColorMode(this.appearData.mode);

    // 同时更新AppStorage中的currentColorMode值，使其他页面能够检测到变化
    AppStorage.Set('currentColorMode', this.appearData.mode);
    console.log('更新AppStorage中的currentColorMode值为:', this.appearData.mode);

    // 保存首选项
    preferencesUtil.putPreference("language",this.appearData.language,"Appear",true)
    preferencesUtil.putPreference("mode",this.appearData.mode,"Appear",true)
  }

  // 初始化
  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    // 获取Cursor数据
    let CursorData = await this.allDataKvStore?.get('Cursor')
    if (CursorData) {
      try {
        let cursorTemp = JSON.parse(CursorData as string) as CursorType
        this.nowData = cursorTemp
      } catch (e) {
        console.error("解析Cursor数据失败", e)
        this.nowData = deepClone(pageDefaultData.Cursor)
      }
    } else {
      this.nowData = deepClone(pageDefaultData.Cursor)
    }

   try {
     let language=await preferencesUtil.getPreference("language","Appear")
     let mode=await preferencesUtil.getPreference("mode","Appear")

     console.log('zfr  从首选项中读取 的语言数据是',language,mode)
     // 应用配置
     if (language!='DATA NOT FOUND'){
       this.appearData.language=language
     }
     // 应用语言：zh/en 映射为 BCP-47 标签，default 回退中文
     this.applyLanguage(this.appearData.language)

     if (mode!='DATA NOT FOUND'){
       this.appearData.mode=Number(mode)
       applicationContext.setColorMode(this.appearData.mode);
     }else {
       applicationContext.setColorMode(this.appearData.mode);
       console.log('深色浅色设置默认值', this.appearData.mode)
     }
   }catch (e){
     console.log("设置失败")
   }

  }

  aboutToAppear(): void {
    this.init()
  }



  async saveSessionChange() {
    try {
      this.nowData = deepClone(JSON.parse(this.defaultData).Cursor as CursorType)
      this.saveStore()
    } catch (e) {
      console.error(e)
    }
  }

  cursorRadio(type: string) {
    this.nowData.cursorAppear = type
  }

  // Map the internal language tag (zh/en/default) to a BCP-47 tag
  resolveLanguage(lang: string): string {
    if (lang === 'en') {
      return 'en-US'
    }
    if (lang === 'default') {
      // auto: follow the system language (BCP-47)
      const sysLang = i18n.System.getSystemLanguage()
      return (sysLang && sysLang.toLowerCase().startsWith('en')) ? 'en-US' : 'zh-CN'
    }
    // zh and anything else -> Chinese
    return 'zh-CN'
  }

  // 应用界面语言
  applyLanguage(lang: string): void {
    try {
      const tag = this.resolveLanguage(lang)
      i18n.System.setAppPreferredLanguage(tag)
      AppStorage.SetOrCreate('appLanguage', lang)
    } catch (e) {
      console.error('设置应用语言失败', e)
    }
  }

  languageRadio(type: string) {
    if (this.appearData.language === type) return
    this.appearData.language = type
    this.applyLanguage(type)
    // 仅"自动"需要重启提示语；简体中文/English 选择时不提示
    if (type === 'default') {
      this.getUIContext().getPromptAction().showToast({
        message: $r('app.string.Set_language_prompt_to_restart'),
        duration: 3000
      })
    }
  }

  checkChange(isCheck: boolean) {
    this.nowData.cursorBlinks = isCheck
  }

  build() {
    Column() {
      Scroll() {
        Column({ space: 10 }) {
          TitleCard({ titleString: $r('app.string.cursor_set') }) {
            Column({ space: 8 }) {
              Text($r('app.string.cursor_appear'))
              Flex({ justifyContent: FlexAlign.SpaceBetween }) {
                Row({ space: 5 }) {
                  Radio({ value: 'block', group: 'cursorRadio' })
                    .checked(this.nowData.cursorAppear == 'block')
                    .onClick(() => this.cursorRadio('block'))
                  Text($r('app.string.cursor_block'))
                    .fontColor(this.nowData.cursorAppear === 'block' ? $r('app.color.light_Select') :
                    $r('app.color.light_noSelect'))
                }

                Row({ space: 5 }) {
                  Radio({ value: 'underline', group: 'cursorRadio' })
                    .checked(this.nowData.cursorAppear === 'underline')
                    .onClick(() => this.cursorRadio('underline'))
                  Text($r('app.string.cursor_Underline'))
                    .fontColor(this.nowData.cursorAppear === 'underline' ? $r('app.color.light_Select') :
                    $r('app.color.light_noSelect'))
                }

                Row({ space: 5 }) {
                  Radio({ value: 'bar', group: 'cursorRadio' })
                    .checked(this.nowData.cursorAppear === 'bar')
                    .onClick(() => this.cursorRadio('bar'))
                  Text($r('app.string.cursor_VerticalLine'))
                    .fontColor(this.nowData.cursorAppear === 'bar' ? $r('app.color.light_Select') :
                    $r('app.color.light_noSelect'))
                }
              }

              Row({ space: 5 }) {
                Checkbox({ name: '1', group: 'tcpGroup' })
                  .select(this.nowData.cursorBlinks)
                  .shape(CheckBoxShape.ROUNDED_SQUARE)
                  .onChange((isCheck: boolean) => this.checkChange(isCheck))

                Text($r('app.string.cursor_Blink'))
                  .fontColor(this.nowData.cursorBlinks ? $r('app.color.light_Select') : $r('app.color.light_noSelect'))
              }

            }.alignItems(HorizontalAlign.Start)
          }

          TitleCard({ titleString: $r('app.string.language_title') }) {
            Flex() {
              Column({ space: 8 }) {
                Text($r('app.string.language_switch'))
                Flex({ justifyContent: FlexAlign.SpaceBetween }) {
                  Row({ space: 5 }) {
                    Radio({ value: 'EN', group: 'languageRadio' })
                      .checked(this.appearData.language == 'en')
                      .onClick(() => this.languageRadio('en'))
                    Text("English")
                      .fontColor(this.appearData.language === 'en' ? $r('app.color.light_Select') :
                      $r('app.color.light_noSelect'))
                  }

                  Row({ space: 5 }) {
                    Radio({ value: 'ZH', group: 'languageRadio' })
                      .checked(this.appearData.language === 'zh')
                      .onClick(() => this.languageRadio('zh'))
                    Text('简体中文')
                      .fontColor(this.appearData.language === 'zh' ? $r('app.color.light_Select') :
                      $r('app.color.light_noSelect'))
                  }


                  Row({ space: 5 }) {
                    Radio({ value: 'default', group: 'languageRadio' })
                      .checked(this.appearData.language === 'default')
                      .onClick(() => this.languageRadio('default'))
                    Text($r('app.string.Auto'))
                      .fontColor(this.appearData.language === 'default' ? $r('app.color.light_Select') :
                      $r('app.color.light_noSelect'))
                  }
                }
              }.alignItems(HorizontalAlign.Start)
            }
          }

          TitleCard({ titleString: $r('app.string.Displaymode') }) {
            Flex() {
              Column({ space: 8 }) {
                Text($r('app.string.mode_switch'))
                Flex({ justifyContent: FlexAlign.SpaceBetween }) {

                  Row({ space: 5 }) {
                    Radio({ value: 'Light', group: 'modeRadio' })
                      .checked(this.appearData.mode == 1)
                      .onClick(() => {
                        this.appearData.mode = 1
                      })
                    Text($r('app.string.Light'))
                      .fontColor(this.appearData.mode === 1 ? $r('app.color.light_Select') :
                      $r('app.color.light_noSelect'))
                  }

                  Row({ space: 5 }) {
                    Radio({ value: 'Dark', group: 'modeRadio' })
                      .checked(this.appearData.mode === 0)
                      .onClick(() => {
                        this.appearData.mode = 0
                      })
                    Text($r('app.string.Dark'))
                      .fontColor(this.appearData.mode === 0 ? $r('app.color.light_Select') :
                      $r('app.color.light_noSelect'))
                  }

                  Row({ space: 5 }) {
                    Radio({ value: 'auto', group: 'modeRadio' })
                      .checked(this.appearData.mode === -1)
                      .onClick(() => {
                        this.appearData.mode = -1
                      })
                    Text($r('app.string.Auto'))
                      .fontColor(this.appearData.mode === -1 ? $r('app.color.light_Select') :
                        $r('app.color.light_noSelect'))
                  }

                }
              }.alignItems(HorizontalAlign.Start)
            }
          }
        }
      }.scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
    .padding(20)
  }
}
