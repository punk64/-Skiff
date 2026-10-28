import { abilityAccessCtrl, bundleManager, common, Permissions } from '@kit.AbilityKit';

class PermissionManager {

  // 检查是否授权

  checkPermissions(permissions: Permissions[]) {

    // 程序访问控制管理

    const atManager = abilityAccessCtrl.createAtManager();

    // 获取 bundle 信息

    const bundleInfo = bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION)

    // 提取 tokenID 标识

    const tokenID = bundleInfo.appInfo.accessTokenId

    // 校验应用是否被授予权限

    const authResults = permissions.map((item) => atManager.checkAccessTokenSync(tokenID, item))

    // 返回是否已授权结果

    return authResults.every(v => v === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED)

  }

  // 动态申请授权（首次弹窗申请）

  async requestPermissions(permissions: Permissions[]) {

    // 程序访问控制管理
    const atManager = abilityAccessCtrl.createAtManager();

    console.log('动态申请授权')
    // 拉起弹框请求用户授权
    const grantStatus = await atManager.requestPermissionsFromUser(getContext(this), permissions)

    console.log('拉起弹框请求用户授权',JSON.stringify(grantStatus))

    // 获取请求权限的结果

    const isAuth = grantStatus.authResults.every(item => item === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED)

    console.log('获取请求权限的结果',isAuth)

    // 返回 Promise 授权结果

    return isAuth ? Promise.resolve(true) : Promise.resolve(false)

  }

  // 打开系统设置的权限管理页（处理授权结果）

  openPermissionSettingsPage() {

    // 获取上下文

    const context = getContext(this) as common.UIAbilityContext

    console.log('打开系统设置的权限管理页')

    // 获取包信息

    const bundleInfo = bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION)

    // 打开系统设置页

    context.startAbility({

      bundleName: 'com.huawei.hmos.settings',

      abilityName: 'com.huawei.hmos.settings.MainAbility',

      uri: 'application_info_entry',

      parameters: {
        // 按照包名打开对应设置页
        pushParams: bundleInfo.name
      }

    })

  }

}

export const permissionManager = new PermissionManager()