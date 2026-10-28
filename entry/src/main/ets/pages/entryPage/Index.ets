import { pageType } from './pagesConfig/pageTypes'
import CustomMenuTree from './layout/CustomMenuTree';
import RightContent from './layout/RightContent';
import BottomButton from './layout/BottomButton'
import PageLoading from '../../components/PageLoading';
import { AbilityLifecycleCallback, UIAbility } from '@kit.AbilityKit';
import window from '@ohos.window';

export let localStorage: LocalStorage = new LocalStorage();
localStorage.setOrCreate('saveSession', '');
let localStorageDefault: LocalStorage = new LocalStorage();
localStorageDefault.setOrCreate('defaultData', '');
let localStorageLoad: LocalStorage = new LocalStorage();
localStorageLoad.setOrCreate('load', false);
let localStorageLoadText: LocalStorage = new LocalStorage();
localStorageLoadText.setOrCreate('loadText', '');

@Entry(localStorage)
@Component
struct Index {
  @State checkNodeParamData:pageType  = {
    id:0,
    title: $r('app.string.check_node_param_data'),
    name: 'Session',
  }
  @LocalStorageLink('saveSession') saveSession: string = '';
  @LocalStorageLink('defaultData') defaultData:string = ''
  @LocalStorageLink('load') load:boolean = false
  @LocalStorageLink('loadText')loadText:Resource = $r('app.string.loading')
  @Provide abilityCallback: AbilityLifecycleCallback = {
    onAbilityCreate(ability) {
      if (ability.launchWant.abilityName === 'CrossShellNext_Terminal') {
        console.log('UIAbility已创建');
      }
    },
    onAbilityDestroy(ability) {
      if (ability.launchWant.abilityName === 'CrossShellNext_Terminal') {
        console.log('UIAbility已销毁');
      }
    },
    onWindowStageCreate: (ability: UIAbility, windowStage: window.WindowStage): void => {
      throw new Error('Function not implemented.');
    },
    onWindowStageActive: (ability: UIAbility, windowStage: window.WindowStage): void => {
      throw new Error('Function not implemented.');
    },
    onWindowStageInactive: (ability: UIAbility, windowStage: window.WindowStage): void => {
      throw new Error('Function not implemented.');
    },
    onWindowStageDestroy: (ability: UIAbility, windowStage: window.WindowStage): void => {
      throw new Error('Function not implemented.');
    },
    onAbilityForeground: (ability: UIAbility): void => {
      throw new Error('Function not implemented.');
    },
    onAbilityBackground: (ability: UIAbility): void => {
      throw new Error('Function not implemented.');
    },
    onAbilityContinue: (ability: UIAbility): void => {
      throw new Error('Function not implemented.');
    }
  }
  aboutToAppear(): void {
    // 当entryAbility创建完成的时候,创建globalPorts
    AppStorage.setOrCreate('globalPorts',new Map())
  }

  build() {
    Column(){
      PageLoading({load:this.load,loadText:this.loadText}){
        Column({space:10}) {
          Flex({ direction: FlexDirection.Row }) {
            CustomMenuTree({checkNodeParam:this.checkNodeParamData})
              .width('20%')
              .backgroundColor($r('app.color.menuTree_color'))
            Flex({ direction: FlexDirection.Column }){
              RightContent({pageParams:this.checkNodeParamData}).height('90%').backgroundColor($r('app.color.rightContent_color'))
              BottomButton().height('10%').backgroundColor($r('app.color.button_color'))
            }.width('80%').height('100%')
          }
        }
        .width('100%')
        .height('100%')
      }
    }
    .width('100%')
    .height('100%')
  }
}
