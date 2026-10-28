import { window } from '@kit.ArkUI';
import { KvManager } from './KVManager';
import { distributedKVStore } from '@kit.ArkData';
import { WindowSizeType } from '../types/dataType';
import { bundleManager } from '@kit.AbilityKit';

class WindowSizeUtils {
  KVManager: KvManager = new KvManager({
    context: getContext(this),
    bundleName: getContext(this).resourceManager.getStringSync($r('app.string.App_bundleName').id), //您自己的App对应的包名
  })
  private allDataKvStore: distributedKVStore.SingleKVStore | undefined = undefined

  async isForbidResizingCompletely(windowStage: window.WindowStage) {

    if (!this.allDataKvStore) {
      this.allDataKvStore = await this.KVManager.getStore('allData')
    }
    try {
      switch ((JSON.parse(await this.allDataKvStore.get('WindowSize') as string) as WindowSizeType).windowResizedId) {
        case '1':
          windowStage.getMainWindowSync().setResizeByDragEnabled(true)
          windowStage.setSupportedWindowModes([bundleManager.SupportWindowMode.FULL_SCREEN,
            bundleManager.SupportWindowMode.SPLIT, bundleManager.SupportWindowMode.FLOATING])
          break;
        case '2':
          windowStage.getMainWindowSync().setResizeByDragEnabled(true)
          windowStage.setSupportedWindowModes([bundleManager.SupportWindowMode.FULL_SCREEN,
            bundleManager.SupportWindowMode.SPLIT, bundleManager.SupportWindowMode.FLOATING])
          break;
        case '3':
          windowStage.getMainWindowSync().setResizeByDragEnabled(true)
          windowStage.setSupportedWindowModes([bundleManager.SupportWindowMode.FULL_SCREEN,
            bundleManager.SupportWindowMode.SPLIT, bundleManager.SupportWindowMode.FLOATING])
          break;
        case '4':
          windowStage.getMainWindowSync().setResizeByDragEnabled(false)
          windowStage.setSupportedWindowModes([bundleManager.SupportWindowMode.SPLIT,
            bundleManager.SupportWindowMode.FLOATING])

          break;

        default:
          break;
      }
    } catch (e) {
    }

  }
}

export const windowSizeUtils = new WindowSizeUtils()