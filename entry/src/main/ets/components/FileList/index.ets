import BasicDataSource from './BasicDataSource'
import { Context } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { KeyCode } from '@kit.InputKit';

type fileState  = 'hidden' | 'display';


enum fileStateEnum {
  HIDDEN = "hidden",
  DISPLAY = "display"
}

export  interface fileNodeType  {
  name:string,
  isDirectory:boolean,
}

interface ActiveState {
  active: Resource;
  unActive: Resource;
}

interface fileIconType {
  'file': ActiveState;
  'directory': ActiveState;
}

@Component
export default struct FileList{
  @Prop @Watch('dataSourceChange') readonly dataSource:fileNodeType[]// 列表数据源
  @Prop filePath:string = '' // 当前文件的路径
  @State private fileList:BasicDataSource = new BasicDataSource()
  @Link selectedFileName:string // 使用文件name作为选中标识
  @State private hoverFileName:string = '' // hover的文件name
  @Prop loading:boolean = false // 文件列表加载状态
  private fileIcon: fileIconType = {
    file: ({
      active: $rawfile('FileLIcon/FileSelected.png'),
      unActive: $rawfile('FileLIcon/File.png')
    } as ActiveState),
    directory:({
      active: $rawfile('FileLIcon/TerminalDirectorySelected.png'),
      unActive: $rawfile('FileLIcon/TerminalDirectory.png')
    } as ActiveState)
  }
/**
 * 新增文件夹
 */
  public addDirectory:(()=>void)|null = null
  /**
   * 下载文件
   */
  public downLoadFile:((name:string) => void )| null = null
  /**
   * 上传文件
   */
  public uploadFile:(() => void )| null = null
  /**
   * 删除文件
   */
  public deleteFile:((fileNode:fileNodeType) => Promise<void> )| null = null
  /**
   * 刷新文件列表
   */
  public reloadFileList:(() => void )| null = null
  /**
   * 鼠标双击
   */
  public doubleClick:((fileNode:fileNodeType)=>void) |null = null
  /**
   * 单击ListItem时触发回调
   */
  public clickListItem:((fileNode:fileNodeType) => void )| null = null
  /**
   * 重命名
   */
  public rename:(()=>void) | null = null

  // 右键菜单
  @Builder RightClickMenu(fileNode:fileNodeType){
    Menu(){
      MenuItem({ content: "刷新", labelInfo: "reload" }).onClick(()=>{
        this.reloadFileList && this.reloadFileList()
      })
      MenuItem({ content: "重命名", labelInfo: "rename" }).onClick(()=>{
        this.rename && this.rename()
      })

      MenuItem({ content: "新增文件夹", labelInfo: "add directory" }).onClick(()=>{
        this.addDirectory && this.addDirectory()
      })
      MenuItem({ content: "删除文件", labelInfo: "delete file" }).onClick(()=>{
        this.deleteFile && this.deleteFile(fileNode)
      })
      MenuItem({ content: "下载文件", labelInfo: "down file" }).onClick(()=>{
        this.downLoadFile && this.downLoadFile(fileNode.name)
      })
      MenuItem({ content: "上传文件", labelInfo: "up file" }).onClick(()=>{
        this.uploadFile && this.uploadFile()
      })
    }
  }
  // 文件列表节点
  @Builder fileNode(fileNode:fileNodeType){
    ListItem(){
      Row({space:8}){
        if (fileNode.isDirectory){
          Image(this.selectedFileName === fileNode.name?this.fileIcon.directory.active:this.fileIcon.directory.unActive).width(30)
        }else{
          Image(this.selectedFileName === fileNode.name?this.fileIcon.file.active:this.fileIcon.file.unActive).width(30)
        }
        Text(fileNode.name)
          .fontColor(this.selectedFileName === fileNode.name?Color.White:'#666666')
          .fontFamily('HarmonyOS Sans SC')
          .fontWeight(700)
          .width('calc(100% - 30vp)')
          .textOverflow({overflow:TextOverflow.Ellipsis})
          .ellipsisMode(EllipsisMode.END)
          .maxLines(1)
      }
      .padding({left:15})
    }
    // .bindContextMenu(this.RightClickMenu(fileNode),ResponseType.RightClick,{enableArrow:true,placement:Placement.Right})
    // .bindContextMenu(this.RightClickMenu(fileNode),ResponseType.LongPress,{enableArrow:true,placement:Placement.Right})
    .backgroundColor(this.selectedFileName === fileNode.name || this.hoverFileName === fileNode.name?"#252525":"#1C1C1C")
    .borderRadius(8)
    .align(Alignment.Start)
    .onHover((isHover:boolean,event:HoverEvent)=>{
      if(isHover){
        this.hoverFileName = fileNode.name
      }else{
        this.hoverFileName = ''
      }
    })
    .onMouse((event?:MouseEvent)=>{
      if(event && event.button === MouseButton.Right && this.clickListItem){
          this.selectedFileName = fileNode.name;
          this.clickListItem(fileNode)
      }
    })
    .onKeyEvent((event: KeyEvent) => {
      if ((event.keyCode === KeyCode.KEYCODE_CTRL_LEFT || event.keyCode === KeyCode.KEYCODE_CTRL_RIGHT) && event.type === KeyType.Down) {
        console.log('当前ctrl+单击选中的元素是------->',JSON.stringify(fileNode))
      }
    })
    .gesture(
      GestureGroup(GestureMode.Exclusive,
        TapGesture({ count: 2 }).onAction((event?: GestureEvent) => {
          // 双击
          console.log('双击')
          this.doubleClick && this.doubleClick(fileNode)
        }),
        TapGesture({ count: 1 }).onAction((event?: GestureEvent) => {
          // 单击
          this.clickListItem && this.clickListItem(fileNode)
        })
      )
    )
    .width('100%')
    .padding({top:10,bottom:10})
  }
  aboutToAppear(): void {
    this.dataSource.map(item=>{
      this.fileList.pushData(JSON.stringify(item))
    })
  }
  // 源数据改变时触发
  dataSourceChange(){
    console.log('dataSource改变了',JSON.stringify(this.dataSource))
    this.fileList.clearAllData()
    this.dataSource && this.dataSource.map(item=>{
      this.fileList.pushData(JSON.stringify(item))
    })
  }

  build() {
    Column(){
      if (this.loading){
        Column({space:10}){
          LoadingProgress().width(50)
          Text('加载中...').fontColor(Color.White)
        }
        .alignItems(HorizontalAlign.Center)
        .justifyContent(FlexAlign.Center)
        .width('100%')
        .height('calc(100% - 100vp)')
      }else{
        List({space:10}){
          if (this.fileList.totalCount() > 0){
            LazyForEach( this.fileList,(nodeItem:string)=>{
              this.fileNode(JSON.parse(nodeItem))
            },(itemKey:string)=>itemKey)
          }
        }
        .width('100%')
        .padding({left:20,right:20})
        .height('calc(100% - 40vp)')
        .borderRadius(0)
      }
    }
  }
}