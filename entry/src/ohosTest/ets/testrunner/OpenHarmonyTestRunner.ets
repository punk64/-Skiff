import { abilityDelegatorRegistry, TestRunner } from '@kit.TestKit';
import { UIAbility, Want } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

let abilityDelegator: abilityDelegatorRegistry.AbilityDelegator;
let abilityDelegatorArguments: abilityDelegatorRegistry.AbilityDelegatorArgs;

async function onAbilityCreateCallback(data: UIAbility) {
  hilog.info(0x0000, 'testTag', 'onAbilityCreateCallback, data: ${}', JSON.stringify(data));
}

async function addAbilityMonitorCallback(err: BusinessError) {
  hilog.info(0x0000, 'testTag', 'addAbilityMonitorCallback : %{public}s', JSON.stringify(err) ?? '');
}

export default class OpenHarmonyTestRunner implements TestRunner {
  constructor() {
  }

  onPrepare() {
    hilog.info(0x0000, 'testTag', '%{public}s', 'OpenHarmonyTestRunner OnPrepare');
  }

  async onRun() {
    hilog.info(0x0000, 'testTag', '%{public}s', 'OpenHarmonyTestRunner onRun run');
    abilityDelegatorArguments = abilityDelegatorRegistry.getArguments()
    abilityDelegator = abilityDelegatorRegistry.getAbilityDelegator()
    const bundleName = abilityDelegatorArguments.bundleName;
    let moduleName = abilityDelegatorArguments.parameters['-m'] ?? 'entry_test';
    const testAbilityName: string = 'TestAbility';
    let lMonitor: abilityDelegatorRegistry.AbilityMonitor = {
      abilityName: testAbilityName,
      onAbilityCreate: onAbilityCreateCallback,
      moduleName: moduleName
    };
    abilityDelegator.addAbilityMonitor(lMonitor, addAbilityMonitorCallback)
    const want: Want = {
      bundleName: bundleName,
      abilityName: testAbilityName,
      moduleName: moduleName
    };
    abilityDelegator.startAbility(want, (err: BusinessError, data: void) => {
      hilog.info(0x0000, 'testTag', 'startAbility : err : %{public}s', JSON.stringify(err) ?? '');
      hilog.info(0x0000, 'testTag', 'startAbility : data : %{public}s', JSON.stringify(data) ?? '');
    })
    hilog.info(0x0000, 'testTag', '%{public}s', 'OpenHarmonyTestRunner onRun end');
  }
}