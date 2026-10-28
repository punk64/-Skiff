import preferences from '@ohos.data.preferences'

/**
 * 首选项工具类封装
 */
class PreferenceUtil {
  private pref: preferences.Preferences

  async loadPreference(context) {
    try {
      // 加载preferences
      this.pref = await preferences.getPreferences(context, 'test')
      console.debug(`加载Preferences[${'test'}]成功`)
    } catch (e) {
      console.error(`加载Preferences[${'test'}]失败`, JSON.stringify(e))
    }
  }

  async putPreferenceValue(key: string, value: preferences.ValueType,flush:boolean = false) {
    if (!this.pref) {
      console.error(`Preferences[${'test'}]尚未初始化！`)
      return
    }
    try {
      // 写入数据
      await this.pref.put(key, value)
      // 刷盘
      flush && await this.pref.flush()
      console.debug(`保存Preferences[${key} = ${value}]成功`)
    } catch (e) {
      console.error(`保存Preferences[${key} = ${value}]失败`, JSON.stringify(e))
    }
  }

  async getPreferenceValue(key: string, defaultValue: preferences.ValueType) {
    if (!this.pref) {
      console.error(`Preferences[${'test'}]尚未初始化！`)
      return
    }
    try {
      // 读数据
      let value = await this.pref.get(key, defaultValue)
      console.debug(`读取Preferences[${key} = ${value}]成功`)
      return value
    } catch (e) {
      console.error(`读取Preferences[${key}]失败`, JSON.stringify(e))
    }
  }
}

const preferenceUtil = new PreferenceUtil()

export default preferenceUtil as PreferenceUtil