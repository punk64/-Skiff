import { pageType } from '../pagesConfig/pageTypes';
import { pages, nodes } from '../pagesConfig/page';
import { NodeParam } from '@kit.ArkUI';

// 定义树节点结构
interface TreeNode {
  id: number;
  title: string | Resource;  // 接受 string 或 Resource
  icon: string | Resource;   // 修改为接受 string 或 Resource
  selectedIcon: string | Resource;  // 修改为接受 string 或 Resource
  isFolder: boolean;
  children?: TreeNode[];
  expanded?: boolean;
}

@Component
export default struct CustomMenuTree {
  @Link checkNodeParam: pageType | null;
  @State private treeData: TreeNode[] = [];
  @State private expandedNodeIds: number[] = [];

  aboutToAppear() {
    // 将你的节点数据转换为树形结构
    this.treeData = this.convertToTreeData();
  }

  // 将原始数据转换为树形结构
  private convertToTreeData(): TreeNode[] {
    // 使用 nodes 数据构建树结构
    const treeNodes: TreeNode[] = [];
    const nodeMap = new Map<number, TreeNode>();

    // 第一步：创建所有节点
    nodes.forEach(node => {
      const treeNode: TreeNode = {
        id: node.currentNodeId ?? 0, // 添加默认值 0
        title: node.primaryTitle ?? '', // 使用空值合并运算符
        icon: node.icon ?? $rawfile('FileLIcon/Directory.png'), // 使用空值合并运算符
        selectedIcon: node.selectedIcon ?? $rawfile('FileLIcon/DirectorySelected.png'), // 使用空值合并运算符
        isFolder: node.isFolder ?? false,
        expanded: false,
        children: []
      };

      nodeMap.set(node.currentNodeId ?? 0, treeNode);
    });

    // 第二步：建立父子关系
    nodes.forEach(node => {
      if (node.parentNodeId !== undefined && node.parentNodeId !== -1) {
        // 这是子节点，添加到父节点的children中
        const parentNode = nodeMap.get(node.parentNodeId);
        const currentNode = nodeMap.get(node.currentNodeId ?? 0);

        if (parentNode && currentNode) {
          if (!parentNode.children) {
            parentNode.children = [];
          }
          parentNode.children.push(currentNode);
        }
      } else {
        // 这是根节点，直接添加到结果数组
        const rootNode = nodeMap.get(node.currentNodeId ?? 0);
        if (rootNode) {
          treeNodes.push(rootNode);
        }
      }
    });
    // 调试输出
    console.log("树形结构：", JSON.stringify(treeNodes));
    return treeNodes;
  }

  // 切换节点展开/折叠状态
  private toggleNode(node: TreeNode) {
    const index = this.expandedNodeIds.indexOf(node.id);
    if (index >= 0) {
      // 节点已展开，折叠它
      this.expandedNodeIds.splice(index, 1);
    } else {
      // 节点已折叠，展开它
      this.expandedNodeIds.push(node.id);
    }

    console.log(`节点 ${node.title} 展开状态：${this.isNodeExpanded(node.id)}`);
    // 输出子节点信息
    if (node.children && node.children.length > 0) {
      console.log(`${node.title} 有 ${node.children.length} 个子节点：`);
      node.children.forEach(child => {
        console.log(`- ${child.title}`);
      });
    }
  }

  // 检查节点是否展开
  private isNodeExpanded(nodeId: number): boolean {
    return this.expandedNodeIds.indexOf(nodeId) >= 0;
  }

  // 处理节点点击
  private handleNodeClick(node: TreeNode) {
    // 查找对应的页面数据
    const selectedPage = pages.find(page => page.id === node.id);
    if (selectedPage) {
      this.checkNodeParam = selectedPage;
    }

    // 如果是文件夹，则切换展开/折叠状态
    if (node.isFolder) {
      this.toggleNode(node);
    }
  }

  // 渲染单个树节点
  @Builder
  TreeNodeItem(node: TreeNode, level: number = 0) {
    Column() {
      Row() {
        Flex({justifyContent:FlexAlign.SpaceBetween}) {
         Row(){
           // 缩进
           ForEach(this.createIndentArray(level), (item: number) => {
             Blank()
               .width(20)
           })

           // 节点图标
           Image(this.checkNodeParam?.id === node.id ? node.selectedIcon : node.icon)
             .width(16)
             .height(16)
             .margin({ right: 8 })

           // 节点标签
           Text(node.title)
             .fontColor(this.checkNodeParam?.id === node.id ? '#0A59F7' : $r('app.color.Menu_tree_name'))
         }
          // 如果是文件夹，显示展开/折叠指示器
          if (node.children && node.children.length > 0 && node.isFolder) {
            Text(this.isNodeExpanded(node.id) ? '󰃛' : '󰃙')
              .fontSize(20                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      )
              .margin({ left: 8 })
              .align(Alignment.End)
          }
        }
        .width('100%')
        .padding(10)
        .borderRadius(4)
        .backgroundColor(this.checkNodeParam?.id === node.id ? $r('app.color.menuTree_color') : 'transparent')
        .onClick(() => this.handleNodeClick(node))
      }
      .width('100%')

      // 渲染子节点
      if (this.isNodeExpanded(node.id) && node.children && node.children.length > 0) {
        Column() {
          ForEach(node.children, (childNode: TreeNode) => {
            this.TreeNodeItem(childNode, level + 1)
          })
        }
        .width('100%')
      }
    }
    .width('100%')
  }


  // 创建缩进数组
  private createIndentArray(level: number): number[] {
    const result: number[] = [];
    for (let i = 0; i < level; i++) {
      result.push(i);
    }
    return result;
  }

  build() {
    Column() {
      Scroll() {
        Column() {
          ForEach(this.treeData, (node: TreeNode) => {
            this.TreeNodeItem(node)
          })
        }
        .width('100%')
      }
      .scrollBar(BarState.Auto)
    }
    .width('100%')
    .height('100%')
    .backgroundColor($r('app.color.menuTree_color'))
    .padding(10)
  }
}