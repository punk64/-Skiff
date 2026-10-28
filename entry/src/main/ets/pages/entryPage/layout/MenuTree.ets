import { CustomMenuTree } from './CustomMenuTree';
import { pageType } from '../pagesConfig/pageTypes';

@Entry
@Component
struct EntryPage {
  @State checkNodeParam: pageType | null = null;

  build() {
    Column() {
      CustomMenuTree({ checkNodeParam: $checkNodeParam }).backgroundColor($r('app.color.menuTree_color'))
    }
    .width('100%')
    .height('100%')
  }
}