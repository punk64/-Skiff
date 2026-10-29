import { bundleManager } from '@kit.AbilityKit'

function readVersion(): string {
  try {
    const info: bundleManager.BundleInfo =
      bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT)
    return `${info.versionName} (${info.versionCode})`
  } catch (e) {
    return '0.3.1 (1000022)'
  }
}

export const APP_VERSION: string = readVersion()
