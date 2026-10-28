import TitleCard from '../../../../components/TitleCard'
import { pageDefaultData } from '../../../../pages/entryPage/pageDefaultData'
import { ProxyType } from '../../../../common/types/dataType'
import { KvManager } from '../../../../common/utils/KVManager'
import { distributedKVStore } from '@kit.ArkData'
import deepClone from '../../../../common/tools/deepClone'
import { encryptProxyPassword, decryptProxyPassword } from '../../../../common/utils/ProxyCredentialUtil'

// 代理配置面板（SOCKS5 / HTTP CONNECT）
@Component
export default struct DProxy {

  @State proxyData: ProxyType = deepClone(pageDefaultData.Proxy)
  @LocalStorageLink('saveSession') @Watch('saveSessionChange') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData: string = ''
  // 连接/保存会话前触发：把代理面板当前值持久化到 KV（面板值可能尚未落盘）
  @StorageLink('saveProxy') @Watch('saveProxyChange') saveProxy: string = ''

  // KVManager实例
  @State KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id),
  })
  // 所有数据的Store实例
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  @Styles
  inputTextStyle() {
    .padding(2)
    .border({ width: 1, style: BorderStyle.Solid })
    .backgroundColor(Color.Transparent)
    .borderRadius(8)
    .borderColor('#ECECEC')
  }

  // 监听saveSession变化（会话加载时联动）
  saveSessionChange() {
    try {
      const parsed = JSON.parse(this.defaultData) as Record<string, object>
      if (parsed && parsed.Proxy) {
        this.proxyData = deepClone(parsed.Proxy as ProxyType)
      }
    } catch (e) {
      console.error('saveSessionChange 解析 Proxy 失败', e)
    }
  }

  // 连接/保存会话：把代理面板当前值落盘到 KV，供会话快照与连接读取
  saveProxyChange() {
    this.saveStore()
  }

  aboutToAppear(): void {
    this.init()
  }

  aboutToDisappear(): void {
    this.allDataKvStore?.off('dataChange')
  }

  // 初始化：从 KV 读取 Proxy 配置（密码解密为明文显示）
  async init() {
    this.allDataKvStore = await this.KVManager.getStore('allData')
    const proxyData = await this.allDataKvStore?.get('Proxy')
    if (proxyData) {
      try {
        const parsed = JSON.parse(proxyData as string) as ProxyType
        let password = parsed.password ?? ''
        if (password.startsWith('H1:')) {
          try {
            password = await decryptProxyPassword(password)
          } catch (e) {
            console.error('代理密码解密失败', e)
            password = ''
          }
        }
        this.proxyData = {
          type: parsed.type ?? pageDefaultData.Proxy.type,
          host: parsed.host ?? pageDefaultData.Proxy.host,
          port: parsed.port ?? pageDefaultData.Proxy.port,
          username: parsed.username ?? pageDefaultData.Proxy.username,
          password: password,
          enabled: parsed.enabled ?? pageDefaultData.Proxy.enabled
        }
      } catch (e) {
        console.error('解析 Proxy 数据失败', e)
      }
    }
  }

  // 保存当前数据（密码经 HUKS 加密后落盘）
  async saveStore() {
    if (!this.allDataKvStore) {
      return
    }
    let password = this.proxyData.password
    if (password && !password.startsWith('H1:')) {
      try {
        password = await encryptProxyPassword(password)
      } catch (e) {
        console.error('代理密码加密失败', e)
      }
    }
    // 代理未启用（开关关）或类型为 None 时，host/port/username/password 一并清空，
    // 避免 KV 残留旧端口/账号/密码导致"退出后部分字段未清空"的偏态
    const active = this.proxyData.enabled && this.proxyData.type !== 'None'
    const keep = (v: string) => active ? v : ''
    const record: ProxyType = {
      type: this.proxyData.type,
      host: keep(this.proxyData.host),
      port: keep(this.proxyData.port),
      username: keep(this.proxyData.username),
      password: active ? password : '',
      enabled: this.proxyData.enabled
    }
    await this.allDataKvStore.put('Proxy', JSON.stringify(record))
  }

  // 切换代理类型
  radioChange(value: 'None' | 'SOCKS5' | 'HTTP') {
    this.proxyData.type = value
    if (value === 'None') {
      this.proxyData.enabled = false
    }
  }

  // 未被勾选的代理类型置灰显示（选中项正常显示）
  isProxyTypeGray(t: 'None' | 'SOCKS5' | 'HTTP'): boolean {
    return this.proxyData.type !== t
  }

  build() {
    Column({ space: 20 }) {
      // 全局开关
      TitleCard({ titleString: $r('app.string.Proxy_GlobalSwitch') }) {
        Row({ space: 16 }) {
          Text($r('app.string.Proxy_Enabled')).fontSize(16)
          Blank()
          Toggle({ type: ToggleType.Switch, isOn: this.proxyData.enabled })
            .onChange((isOn: boolean) => {
              this.proxyData.enabled = isOn
              if (!isOn) {
                this.proxyData.type = 'None'
              }
            })
        }
        .width('100%')
      }

      // 代理类型
      TitleCard({ titleString: $r('app.string.Proxy_Type') }) {
        Row({ space: 32 }) {
          Radio({ value: 'None', group: 'ProxyTypeRadio' })
            .checked(this.proxyData.type === 'None')
            .onChange((checked: boolean) => {
              if (checked) { this.radioChange('None') }
            })
          Text('None').opacity(this.isProxyTypeGray('None') ? 0.4 : 1)
          Radio({ value: 'SOCKS5', group: 'ProxyTypeRadio' })
            .checked(this.proxyData.type === 'SOCKS5')
            .enabled(this.proxyData.enabled)
            .onChange((checked: boolean) => {
              if (checked) { this.radioChange('SOCKS5') }
            })
          Text('SOCKS5').opacity(this.isProxyTypeGray('SOCKS5') ? 0.4 : 1)
          Radio({ value: 'HTTP', group: 'ProxyTypeRadio' })
            .checked(this.proxyData.type === 'HTTP')
            .enabled(this.proxyData.enabled)
            .onChange((checked: boolean) => {
              if (checked) { this.radioChange('HTTP') }
            })
          Text($r('app.string.Proxy_HttpConnect')).opacity(this.isProxyTypeGray('HTTP') ? 0.4 : 1)
        }
        .width('100%')
      }

      // 代理参数
      TitleCard({ titleString: $r('app.string.Proxy_Settings') }) {
        Column({ space: 16 }) {
          Flex({ direction: FlexDirection.Row, justifyContent: FlexAlign.SpaceBetween }) {
            Column() {
              Text($r('app.string.Proxy_Address'))
                .textAlign(TextAlign.Start).width('100%').margin({ bottom: 5 })
              TextInput({ text: $$this.proxyData.host, placeholder: $r('app.string.Proxy_Address_Placeholder') })
                .inputTextStyle()
                .width('100%')
                .maxLength(100)
                .enabled(this.proxyData.enabled)
            }.width('45%')
            Column() {
              Text($r('app.string.Proxy_Port'))
                .textAlign(TextAlign.Start).width('100%').margin({ bottom: 5 })
              TextInput({ text: $$this.proxyData.port, placeholder: $r('app.string.Proxy_Port_Placeholder') })
                .inputTextStyle()
                .width('100%')
                .maxLength(10)
                .enabled(this.proxyData.enabled)
            }.width('45%')
          }.width('100%')

          Flex({ direction: FlexDirection.Row, justifyContent: FlexAlign.SpaceBetween }) {
            Column() {
              Text($r('app.string.Proxy_Username'))
                .textAlign(TextAlign.Start).width('100%').margin({ bottom: 5 })
              TextInput({ text: $$this.proxyData.username, placeholder: $r('app.string.Proxy_Username_Placeholder') })
                .inputTextStyle()
                .width('100%')
                .maxLength(100)
                .enabled(this.proxyData.enabled)
            }.width('45%')
            Column() {
              Text($r('app.string.Proxy_Password'))
                .textAlign(TextAlign.Start).width('100%').margin({ bottom: 5 })
              TextInput({ text: $$this.proxyData.password, placeholder: $r('app.string.Proxy_Password_Placeholder') })
                .inputTextStyle()
                .type(InputType.Password)
                .width('100%')
                .maxLength(100)
                .enabled(this.proxyData.enabled)
            }.width('45%')
          }.width('100%')
        }.alignItems(HorizontalAlign.Start)
      }
    }
    .alignItems(HorizontalAlign.Start)
    .padding({ left: 20, right: 20 })
    .width('100%')
  }
}
