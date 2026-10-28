import  {pageList} from './pageTypes'
import { NodeParam } from '@kit.ArkUI'

// 页面列表
const pages:pageList = [
  {
    id:0,
    title: $r('app.string.session_top'),
    name: $r('app.string.Session'),
  },
  {
    id:1,
    title:$r('app.string.log_top'),
    name:$r('app.string.Logging'),
  },
  {
    id:2,
    title:$r('app.string.Terminal_options'),
    name:$r('app.string.Terminal'),
  },
  // {
  //   id:3,
  //   title:'Options controlling the effects of keys',
  //   name:'Keyboard',
  // },
  // {
  //   id:4,
  //   title:'Options controlling the terminal bell',
  //   name:'Bell',
  // },
  // {
  //   id:5,
  //   title:'Enabling and disabling advanced terminal features',
  //   name:'Features',
  // },
  {
    id:6,
    title:$r('app.string.con_top'),
    name:$r('app.string.Connection'),
  },
  {
    id:7,
    title:'Options controlling Telnet connection',
    name:'Telnet',
  },
  {
    id:8,
    title:$r('app.string.SSHportforwarding'),
    name:$r('app.string.Tunnels'),
  },
  {
    id: 25,
    title: $r('app.string.Proxy_PageTitle'),
    name: 'Proxy',
  },
  {
    id: 18,
    title: $r('app.string.win_top'),
    name: $r('app.string.Window')
  },
  {
    id: 19,
    title: $r('app.string.appear_top'),
    name: $r('app.string.Appearance')
  },
  // {
  //   id: 20,
  //   title: $r('app.string.Serial_Title'),
  //   name: $r('app.string.Serial')
  // },
  // {
  //   id: 21,
  //   title: 'Options controlling character set translation',
  //   name: 'Translation'
  // },
  // {
  //   id: 22,
  //   title: 'Options controlling copy and paste',
  //   name: 'Selection'
  // },
  {
    id: 23,
    title: $r('app.string.colour_top'),
    name: $r('app.string.Colours')
  },
  {
    id: 24,
    title: $r('app.string.KeyGenerator'),
    name: $r('app.string.KeyGeneratortitle')
  }
]

// 导航栏列表
const nodes:NodeParam[]=[
  {
    primaryTitle: $r('app.string.Session'),
    currentNodeId:0,
    parentNodeId:-1,
    isFolder:true,
    icon:$rawfile('FileLIcon/Session.png'),
    selectedIcon:$rawfile('FileLIcon/SessionSelected.png'),
  },
  { primaryTitle:$r('app.string.Logging'),
    currentNodeId:1,
    parentNodeId:0,
    isFolder:false,
    icon:$rawfile('FileLIcon/Directory.png'),
    selectedIcon:$rawfile('FileLIcon/DirectorySelected.png'),
  },
  {
    primaryTitle:$r('app.string.Terminal'),
    currentNodeId:2,
    parentNodeId:-1,
    isFolder:true,
    icon:$rawfile('FileLIcon/Connection.png'),
    selectedIcon:$rawfile('FileLIcon/ConnectionSelected.png'),
  },
  // {
  //   primaryTitle:'Keyboard',
  //   currentNodeId:3,
  //   parentNodeId:2,
  //   isFolder:false,
  //   icon:$rawfile('FileLIcon/Directory.png'),
  //   selectedIcon:$rawfile('FileLIcon/DirectorySelected.png'),
  // },
  // {
  //   primaryTitle:'Bell',
  //   currentNodeId:4,
  //   parentNodeId:2,
  //   isFolder:false,
  //   icon:$rawfile('FileLIcon/Directory.png'),
  //   selectedIcon:$rawfile('FileLIcon/DirectorySelected.png'),
  // },
  // {
  //   primaryTitle:'Features',
  //   currentNodeId:5,
  //   parentNodeId:2,
  //   isFolder:false,
  //   icon:$rawfile('FileLIcon/Directory.png'),
  //   selectedIcon:$rawfile('FileLIcon/DirectorySelected.png'),
  // },
  {
    primaryTitle:$r('app.string.Connection'),
    currentNodeId:6,
    parentNodeId:-1,
    isFolder:true,
    icon:$rawfile('FileLIcon/Connection.png'),
    selectedIcon:$rawfile('FileLIcon/ConnectionSelected.png'),
  },
  {
    primaryTitle: $r('app.string.Proxy_MenuTitle'),
    currentNodeId: 25,
    parentNodeId: 6,
    isFolder: false,
    icon: $rawfile('FileLIcon/Connection.png'),
    selectedIcon: $rawfile('FileLIcon/ConnectionSelected.png'),
  },
  // {
  //   primaryTitle:$r('app.string.Serial'),
  //   currentNodeId:20,
  //   parentNodeId:6,
  //   isFolder:false,
  //   icon:$rawfile('FileLIcon/Connection.png'),
  //   selectedIcon:$rawfile('FileLIcon/ConnectionSelected.png'),
  // },

  // {
  //   primaryTitle:'Telnet',
  //   currentNodeId:7,
  //   parentNodeId:6,
  //   isFolder:true,
  //   icon:$rawfile('FileLIcon/Connection.png'),
  //   selectedIcon:$rawfile('FileLIcon/ConnectionSelected.png'),
  // },
  // {
  //   primaryTitle:$r('app.string.Tunnels'),
  //   currentNodeId:8,
  //   parentNodeId:6,
  //   isFolder:true,
  //   icon:$rawfile('FileLIcon/Connection.png'),
  //   selectedIcon:$rawfile('FileLIcon/ConnectionSelected.png'),
  // },
  {
    primaryTitle: $r('app.string.Window'),
    currentNodeId: 18,
    parentNodeId: -1,
    isFolder: true,
    icon:$rawfile('FileLIcon/Window.png'),
    selectedIcon:$rawfile('FileLIcon/WindowSelected.png'),
  },
  {
    primaryTitle:  $r('app.string.Appearance'),
    currentNodeId: 19,
    parentNodeId: 18,
    isFolder: false,
    icon:$rawfile('FileLIcon/Window.png'),
    selectedIcon:$rawfile('FileLIcon/WindowSelected.png'),
  },
  // {
  //   primaryTitle: 'Bahaviour',
  //   currentNodeId: 20,
  //   parentNodeId: 18,
  //   isFolder: false
  // },
  // {
  //   primaryTitle: 'Translation',
  //   currentNodeId: 21,
  //   parentNodeId: 18,
  //   isFolder: false,
  //   icon:$rawfile('FileLIcon/Directory.png'),
  //   selectedIcon:$rawfile('FileLIcon/DirectorySelected.png'),
  // },
  // {
  //   primaryTitle: 'Selection',
  //   currentNodeId: 22,
  //   parentNodeId: 18,
  //   isFolder: false,
  //   icon:$rawfile('FileLIcon/Directory.png'),
  //   selectedIcon:$rawfile('FileLIcon/DirectorySelected.png'),
  // },
  {
    primaryTitle: $r('app.string.Colours'),
    currentNodeId: 23,
    parentNodeId: 18,
    isFolder: false,
    icon:$rawfile('FileLIcon/Window.png'),
    selectedIcon:$rawfile('FileLIcon/WindowSelected.png'),
  },
  {
    primaryTitle:  $r('app.string.KeyGeneratortitle'),
    currentNodeId: 24,
    parentNodeId: -1,
    isFolder: false,
    icon:$rawfile('FileLIcon/KeyGen.png'),
    selectedIcon:$rawfile('FileLIcon/KeyGenSelected.png'),
  }
]
export {pages,nodes}
