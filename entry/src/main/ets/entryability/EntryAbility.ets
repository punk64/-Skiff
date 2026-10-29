import {
  AbilityConstant, ConfigurationConstant, UIAbility, Want, errorManager } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import {permissionManager} from 'ets/common/utils/repPermissionsFromUser'
import { KvManager } from '../common/utils/KVManager';
import { distributedKVStore, preferences } from '@kit.ArkData';
import { pageDefaultData } from '../pages/entryPage/pageDefaultData';
import { emitter } from '@kit.BasicServicesKit';
import dataPreferences from '@ohos.data.preferences';
import data_distributedKVStore from '@ohos.data.distributedKVStore';
import { preferencesUtil } from '../common/utils/preferenceutils';
import { windowSizeUtils } from '../common/utils/WindowSizeUtils';
import { i18n } from '@kit.LocalizationKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { warmupProxyDns } from 'libentry.so';
import { decryptProxyPassword, ensureProxyHuskKey } from '../common/utils/ProxyCredentialUtil';
import { ProxyType } from '../common/types/dataType';
import { loadUiPrefsSync, UiPrefs } from '../common/Store';

let LanguageMode:string
export default class EntryAbility extends UIAbility {
  // 所有数据的Store实例
  private allDataKvStore:distributedKVStore.SingleKVStore | undefined =undefined

  para: Record<string, string> = {
    'saveSession': ''
  };
  storage: LocalStorage = new LocalStorage(this.para);
  // App interface language: internal tag (zh/en/default) -> BCP-47; default follows system
  private applySavedLanguage(lang: string): void {
    try {
      let tag = 'zh-CN'
      if (lang === 'en') {
        tag = 'en-US'
      } else if (lang === 'default') {
        // auto: follow the system language (BCP-47)
        const sysLang = i18n.System.getSystemLanguage()
        tag = (sysLang && sysLang.toLowerCase().startsWith('en')) ? 'en-US' : 'zh-CN'
      }
      i18n.System.setAppPreferredLanguage(tag)
      AppStorage.SetOrCreate('appLanguage', lang)
      console.log('app language applied:', lang, '->', tag)
    } catch (e) {
      console.error('apply app language failed', e)
    }
  }
   async  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): Promise<void>{
     preferencesUtil.init(this.context)
     // 启动时应用已保存的界面语言（zh/en/default，default 回退中文）
     try {
       const savedLanguage = await preferencesUtil.getPreference('language', 'Appear')
       if (savedLanguage !== 'DATA NOT FOUND' && savedLanguage) {
         this.applySavedLanguage(savedLanguage)
       }
     } catch (e) {
       console.error('读取并应用语言失败', e)
     }
     let KVManager:KvManager = new KvManager({
       context: this.context,
       bundleName:this.context.resourceManager.getStringSync($r('app.string.App_bundleName').id),//您自己的App对应的包名
     })
// 首次启动时初始化 KVStore 默认值；已存在的 key 保留用户配置（避免退出重开后配置丢失）
      KVManager.getStore('allData').then(async (allDataKVManagerStore)=>{
        const entries: Array<[string, object]> = Object.entries(pageDefaultData)
        for (let i = 0; i < entries.length; i++) {
          const key = entries[i][0] as string
          try {
            if (key === 'Proxy') {
              await allDataKVManagerStore.put('Proxy', JSON.stringify(pageDefaultData.Proxy))
              continue
            }
            let existing: string | number | boolean | Uint8Array | null = null
            try {
              existing = await allDataKVManagerStore.get(key) as string | number | boolean | Uint8Array
            } catch (getErr) {
              // 对不存在的 key，SingleKVStore.get 会抛 "Not found" 异常，视为不存在 → 写入默认值
              existing = null
            }
            if (existing === undefined || existing === null || existing === '') {
              await allDataKVManagerStore.put(key, JSON.stringify(entries[i][1] as object))
            }
          } catch (e) {
            console.error('初始化 KVStore key 失败: ' + key, e)
          }
        }
        // 启动预热：代理配置加载完成后预热 HUKS，缩短首次建连耗时
        this.warmupProxy()
      })
    AppStorage.setOrCreate('TerminalType',false)
    AppStorage.setOrCreate('currentColorMode', this.context.config.colorMode);

    // 当entryAbility创建完成的时候,创建globalPorts
    AppStorage.setOrCreate('globalPorts',new Map())
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onCreate');

    try {
      // 检查剪贴板权限
      const hasClipboardPermission = permissionManager.checkPermissions(['ohos.permission.READ_PASTEBOARD']);
      console.log('剪贴板权限状态:', hasClipboardPermission);
      
      if(!hasClipboardPermission){
        // 请求剪贴板权限
        try {
          const granted = await permissionManager.requestPermissions(['ohos.permission.READ_PASTEBOARD']);
          console.log('剪贴板权限请求结果:', granted);
          
          if(!granted){
            console.warn('用户拒绝了剪贴板权限，某些功能可能受限');
            // 设置一个标志，表示应用应当在没有剪贴板权限的情况下运行
            AppStorage.setOrCreate('clipboardPermissionGranted', false);
          } else {
            AppStorage.setOrCreate('clipboardPermissionGranted', true);
          }
        } catch (permError) {
          console.error('请求剪贴板权限时出错:', permError);
          AppStorage.setOrCreate('clipboardPermissionGranted', false);
        }
      } else {
        AppStorage.setOrCreate('clipboardPermissionGranted', true);
      }
    } catch (error) {
      console.error('处理剪贴板权限时发生错误:', error);
      // 默认为false，防止在后续流程中出现异常
      AppStorage.setOrCreate('clipboardPermissionGranted', false);
    }
    this.installErrorGuard()
  }

  private installErrorGuard(): void {
    try {
      const observer: errorManager.ErrorObserver = {
        onUnhandledException: (errMsg: string): void => {
          hilog.error(0x0000, 'SkiffCrash', 'unhandled: %{public}s', errMsg)
          AppStorage.setOrCreate('sk_lastCrash', errMsg.length > 400 ? errMsg.substring(0, 400) : errMsg)
        },
        onException: (errObject: Error): void => {
          const msg: string = errObject.message === undefined ? '' : errObject.message
          hilog.error(0x0000, 'SkiffCrash', 'exception: %{public}s', msg)
          AppStorage.setOrCreate('sk_lastCrash', msg.length > 400 ? msg.substring(0, 400) : msg)
        }
      }
      errorManager.on('error', observer)
    } catch (e) {
      hilog.error(0x0000, 'SkiffCrash', 'install error guard failed')
    }
  }

  onDestroy(): void {
    emitter.emit('EntryOnDestroy')
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onDestroy');
    // 退出时关闭代理并清空代理配置：type=None、enabled=false、host/port/username/password 全部置空，
    // 避免未保存的代理修改残留到下一次启动
    try {
      const km = new KvManager({
        context: this.context,
        bundleName: this.context.resourceManager.getStringSync($r('app.string.App_bundleName').id),
      })
      km.getStore('allData').then((store: distributedKVStore.SingleKVStore) => {
        store.put('Proxy', JSON.stringify(pageDefaultData.Proxy)).catch((e: BusinessError) => {
          console.error('onDestroy clear proxy failed', e.message ? e.message : String(e))
        })
      }).catch((e: BusinessError) => {
        console.error('onDestroy get store failed', e.message ? e.message : String(e))
      })
    } catch (e) {
      console.error('onDestroy clear proxy exception', e)
    }
  }

  // 启动预热：确保 HUKS 密钥就绪
  private async warmupProxy(): Promise<void> {
    try {
      await ensureProxyHuskKey()
      console.log('代理 HUKS 启动预热完成')
    } catch (e) {
      console.error('代理 HUKS 启动预热失败:', e)
    }
  }

  private applySystemBar(win: window.Window): void {
    try {
      const dark: boolean = AppStorage.get<boolean>('csx_themeDark') ?? true
      win.setWindowSystemBarProperties({
        statusBarColor: dark ? '#0A0C10' : '#F1F3F7',
        statusBarContentColor: dark ? '#E9EFF7' : '#10151C',
        navigationBarColor: dark ? '#0A0C10' : '#F1F3F7',
        navigationBarContentColor: dark ? '#59647A' : '#98A2B3'
      })
      const sys = win.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM)
      const nav = win.getWindowAvoidArea(window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR)
      AppStorage.setOrCreate('topSafePx', sys.topRect.height)
      AppStorage.setOrCreate('bottomSafePx', nav.bottomRect.height)
    } catch (e) {
      const err = e as BusinessError
      console.error('设置系统栏样式失败:', err.message)
      AppStorage.setOrCreate('topSafePx', 0)
      AppStorage.setOrCreate('bottomSafePx', 0)
    }
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onWindowStageCreate');
    AppStorage.setOrCreate('TerminalType',false)
    const uiPrefs: UiPrefs = loadUiPrefsSync(this.context)
    AppStorage.setOrCreate('csx_themeDark', uiPrefs.themeDark)
    AppStorage.setOrCreate('csx_lang', uiPrefs.lang)
    AppStorage.setOrCreate('csx_termFont', uiPrefs.termFont)
    AppStorage.setOrCreate('sk_keepAlive', uiPrefs.keepAlive)
    AppStorage.setOrCreate('sk_bgTunnel', uiPrefs.bgTunnel)
    AppStorage.setOrCreate('sk_autoReconnect', uiPrefs.autoReconnect)
    AppStorage.setOrCreate('sk_logOn', uiPrefs.logOn)
    AppStorage.setOrCreate('sk_logType', uiPrefs.logType)
    AppStorage.setOrCreate('sk_proxyOn', uiPrefs.proxyOn)
    AppStorage.setOrCreate('sk_proxyType', uiPrefs.proxyType)
    AppStorage.setOrCreate('sk_proxyHost', uiPrefs.proxyHost)
    AppStorage.setOrCreate('sk_proxyPort', uiPrefs.proxyPort)
    this.context.getApplicationContext().setColorMode(uiPrefs.themeDark
      ? ConfigurationConstant.ColorMode.COLOR_MODE_DARK
      : ConfigurationConstant.ColorMode.COLOR_MODE_LIGHT)
    windowStage.loadContent('pages/mobile/Index', (err) => {
      if (err.code) {
        hilog.error(0x0000, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err) ?? '');
        return;
      }
      hilog.info(0x0000, 'testTag', 'Succeeded in loading the content.');

    // 在内容加载完成后请求剪切板权限
    permissionManager.requestPermissions(['ohos.permission.READ_PASTEBOARD']).then(granted => {
      console.log('应用加载后申请剪贴板权限结果:', granted);
      AppStorage.setOrCreate('clipboardPermissionGranted', granted);
    }).catch((error:Error) => {
      console.error('申请剪贴板权限出错:', error);
      AppStorage.setOrCreate('clipboardPermissionGranted', false);
    });
  });
  
    // 动态授权
    // permissionManager.requestPermissions(['ohos.permission.SYSTEM_FLOAT_WINDOW'])
    // 窗口
    windowStage.getMainWindow().then((data: window.Window) => {
      if (canIUse('SystemCapability.Window.SessionManager')) {
        data.setWindowDecorVisible(true);
        data.setWindowDecorHeight(56);
      } else {
        hilog.info(0x0000, 'Simple', 'can not use SessionManager syscap');
      }
      this.applySystemBar(data);
    });
    AppStorage.setOrCreate('windowStage', windowStage);
  }


  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onForeground');
    const prevTick: number | undefined = AppStorage.get<number>('sk_fgTick')
    AppStorage.setOrCreate('sk_fgTick', (prevTick === undefined ? 0 : prevTick) + 1)
     // 每次回到前台时检查权限
  const hasClipboardPermission = permissionManager.checkPermissions(['ohos.permission.READ_PASTEBOARD']);
  if (!hasClipboardPermission) {
    permissionManager.requestPermissions(['ohos.permission.READ_PASTEBOARD']).then(granted => {
      console.log('前台运行时申请剪贴板权限结果:', granted);
      AppStorage.setOrCreate('clipboardPermissionGranted', granted);
    }).catch((error:Error) => {
      console.error('申请剪贴板权限出错:', error);
      AppStorage.setOrCreate('clipboardPermissionGranted', false);
    });
  } else {
    AppStorage.setOrCreate('clipboardPermissionGranted', true);
  }
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onBackground');
  }
};

