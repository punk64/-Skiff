import data_preferences from '@ohos.data.preferences';
import { BusinessError, Callback } from '@ohos.base';
import preferences from '@ohos.data.preferences';
import { Context } from '@ohos.abilityAccessCtrl';

const TAG: string = '[PreferencesUtil]'

class PreferencesUtil {
  private context: Context | undefined = undefined
  private preferences: Map<string, data_preferences.Preferences> = new Map<string, data_preferences.Preferences>()

  // context 初始化 => 页面Context
  init(context: Context) {
    this.context = context
    console.info(TAG, '  (init) SUCCESS => 初始化成功')
  }

  // Preference 单项获取
  async getPreference(key: string, preferenceName ?: string): Promise<string> {

    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return Promise.resolve(TAG +  ` (init) ERROR => Context未获取`)
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      let value: preferences.ValueType = await preference.get(key, `DATA NOT FOUND`)
      console.info(TAG, `  (getPreference)    SUCCESS TO GET: ${preName} => DATA GET SUCESS( ${key}: ${value} )`)
      return Promise.resolve(value.toString())
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (getPreference)    FAIL TO GET: ${preName} => code: ${code}, message: ${message}`)
      return Promise.resolve(TAG +  ` (getPreference)    ERROR`)
    }
  }

  // Preference 单项添加/修改
  async putPreference(key: string , value: preferences.ValueType, preferenceName ?: string, isFlush?: boolean) {
    let flag: boolean = isFlush ? isFlush : false
    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      await preference.put(key, value)
      if (flag) {
        await preference.flush()
      }
      console.info(TAG, `  (putPreference)    SUCCESS TO PUT: ${preName} => DATA PUT SUCESS( ${key}: ${value}  flush: ${flag} )`)
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (putPreference)    FAIL TO PUT: ${preName} => code: ${code}, message: ${message}`)
    }
  }

  // Preference 单项删除
  async delPreference(key: string, isFlush?: boolean, preferenceName ?: string) {
    let flag: boolean = isFlush ? isFlush : false
    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      preference.delete(key).then(async () => {
        if (flag) {
          await preference.flush()
        }
        console.info(TAG, `  (delPreference)    SUCCESS TO DEL: ${preName} => DATA DEL SUCESS( ${key}  flush: ${flag} )`)
      })
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (delPreference)    FAIL TO DEL: ${preName} => code: ${code}, message: ${message}`)
    }
  }

  // Preference 获取全部Key
  async getAllKeys (keysCallBack: Callback<string[]>, preferenceName ?: string) {

    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      let pros = await preference.getAll()
      let keys: string[] = Object.keys(pros)
      if (keys.length == 0) {
        console.error(TAG, `  (getAllKeys)   FAIL TO GETALL: ${preName} => THERE ARE NO DATA`)
        return
      }
      keysCallBack(keys)
      console.info(TAG, `  (getAllKeys)   SUCCESS TO GETALL: ${JSON.stringify(keys)}`)
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (getAllKeys)    FAIL TO GETALL: ${preName} => code: ${code}, message: ${message}`)
    }
  }

  // Preference 以Map形式获取全部数据
  async getAllPreferences(callback: Callback<Map<string, preferences.ValueType>>, preferenceName ?: string) {
    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    let map: Map<string, preferences.ValueType> = new Map<string, preferences.ValueType>()
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      this.getAllKeys(async (keys: string[]) => {
        for (let key of keys) {
          let value: preferences.ValueType = await preference.get(key, TAG + `  (getAllPreferences]   FAIL TO GET: ${preName} => DATA NOT FOUND`)
          map.set(key, value)
        }
        callback(map)
        console.info(TAG, `  (getAllPreferences)   SUCCESS TO GETALL: ${preName} => ${JSON.stringify(map)}`)
      }, preName)
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (getAllPreferences)  FAIL TO GETALL: ${preName} => code: ${code}, message: ${message}`)
    }
  }

  // Preference 以数组形式获取全部数据
  async getArr <T> (callback: Callback<T[]>, preferenceName ?: string) {
    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    let arr: T[] = []
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      this.getAllKeys(async (keys: string[]) => {
        for (let key of keys) {
          let value: preferences.ValueType = await preference.get(key, TAG + `  (getArr]   FAIL TO GET: ${preName} => DATA NOT FOUND`)
          arr.push(JSON.parse(value.toString()) as T)
        }
        callback(arr)
        console.info(TAG, `  (getArr)   SUCCESS TO GETARR: ${preName} => ${JSON.stringify(arr)}`)
      }, preName)
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (getArr)  FAIL TO GETARR: ${preName} => code: ${code}, message: ${message}`)
    }
  }

  // Preference 删除全部数据
  async delAllPreferences (isFlush?: boolean, preferenceName ?: string) {
    let flag: boolean = isFlush ? isFlush : false
    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      this.getAllKeys(async (keys: string[]) => {
        for (let key of keys) {
          await this.delPreference(key, flag, preName)
        }
        console.info(TAG, `  (delAllPreferences)   SUCCESS TO DELALL: ${preName} => ${JSON.stringify(keys)}  flush: ${flag}`)
      }, preName)
      if (flag) {
        preference.flush()
      }
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (delAllPreferences)  FAIL TO DELALL: ${preName} => code: ${code}, message: ${message}`)
    }
  }

  async flush(preferenceName ?: string) {
    let preName: string = preferenceName ? preferenceName : 'default'
    let preference: data_preferences.Preferences
    if (!this.context) {
      console.error(TAG, `  (init) ERROR => Context未获取`)
      return
    }
    try {
      let pre = this.preferences.get(preName)
      if (pre) {
        preference = pre
      } else {
        preference = await data_preferences.getPreferences(this.context, preName)
        this.preferences.set(preName, preference)
      }
      await preference.flush()
      console.info(TAG, `  (flush)   SUCCESS TO FLUSH: ${preName} => 持久化成功`)
    } catch (err) {
      let code = (err as BusinessError).code
      let message = (err as BusinessError).message
      console.error(TAG, `  (delAllPreferences)  FAIL TO FLUSH: ${preName} => code: ${code}, message: ${message}`)
    }
  }
}

export let preferencesUtil: PreferencesUtil = new PreferencesUtil()
