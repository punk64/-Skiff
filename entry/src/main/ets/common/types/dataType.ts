import { fileIo, fileShare } from "@kit.CoreFileKit";

export interface dataType {
  Session:object,
  Logging:object,
}
//设置连接结束后窗口关闭行为
export enum CloseWindowMode {
  Always, //总是关闭窗口
  Never, //从不关闭窗口
  OnlyOnCleanExit //仅在正常退出时关闭窗口
}
// 各个页面的nowData接口
export interface SessionType{
  userName:string;
  hostName?: string;
  serialLine?:string;
  authType:'password'|'publickey';
  privateKeyFd:number; // 私钥文件句柄
  privateKeyName?:string; // 私钥文件名称
  privateKeyPath?:string; // 私钥文件地址
  port?: string;
  speed?:string | number | null;
  connectionType:string,
  connectionSelect:string, // 下拉值
  keepAlive:string, // 下拉值
  savedSessions:string,
  exit:string,
  connectionSelected:number, // 下拉框选中索引
  password:string,
  Telnet?:TelnetType, // 添加Telnet配置引用
  Serial?:SerialType, // 添加Serial配置引用
  closeWindowMode:CloseWindowMode,
  proxyPolicy: 'global'|'custom'|'off', // 会话代理策略（跟随全局/自定义/禁用）
}

export interface LoggingType{
  logType:0|1|2|3|4,//0:None,1:Printable output,2:All session output,
  logFileBaseUri:string,
  logFileName:string,
  existOperation:0|1,//0：Always overwrite it，1：Always append to the end of it
  policiesArray:Array<fileShare.PolicyInfo>
  includeHeader:boolean,
  omitKnownPassword:boolean,
  omitSessionData:boolean
}

//logging可选配置项
// export interface LoggingMultiSelConfig{
//
// }

export interface TerminalType{
  variousCheck:string[],
  answerBackInput:string,
  echo:string,
  editing:string,
  remoteControlledInput:string
}

export interface ConnectionType{
  keepalive: number,
  tcpOptions:string[],
  version:string,
  SHHKeyHost:string,
}

export  interface allDataType {
  Session: SessionType;
  Logging: LoggingType;
  Terminal:TerminalType;
  Connection:ConnectionType;
  Serial:SerialType;
  Telnet:TelnetType;
  Tunnels:TunnelsType;
  Proxy:ProxyType;
  WindowSize:WindowSizeType;
  Character:CharacterType;
  Cursor:CursorType;
  Color:ColorType;
  Select:SelectType;
  KeyGen:KeyGenType;
  Appear:AppearType
}

export  interface  SerialType {
  serialLine: string,
  speed: string | number  | null,
  dataBits:string | number  | null,
  stopBits:string | number  | null,
  sendEncoding:string,
  readEncoding:string,
  parity:string,
  flowControl:string,
  sendEncodingSelected:number,
  readEncodingSelected:number,
  speedSelected:number,
  dataBitsSelected: number,
  stopBitsSelected: number
  paritySelected:number,
  connectionSelected:number,
}

export interface TelnetType{
  userName:string;
  hostName?: string;
  port?:string,
  mode:string,
  ambiguity:string,
  keyboard:boolean,
  telnetNewLine:boolean
}

export interface TunnelsType{
  forwardedPorts:string,
  sourcePort:string,
  destination:string,
  concatType:string,
  selectedForward?:string // 存储选中的端口转发项，格式为JSON字符串
}

// 代理配置
export interface ProxyType{
  type:'None'|'SOCKS5'|'HTTP', // 代理类型
  host:string,                  // 代理服务器地址
  port:string,                  // 代理服务器端口
  username:string,              // 代理用户名
  password:string,              // 代理密码（存储密文，读取时解密）
  enabled:boolean               // 全局代理开关
}

export interface WindowSizeType{
  col:number;
  row:number;
  windowResizedId:string
}

export interface CharacterType{
  value:string
}

export interface CursorType{
  cursorAppear:string,
  cursorBlinks:boolean
}

export interface AppearType{
  language:string,
  mode:number
}

export interface xtermStyle{
  cursorBlinks:boolean,
  cursorAppear:string,
  foreground:string
}

export interface ColorType{
  fontColor:string
}

export interface SelectType{
  AutoCopy:boolean,
  CtrlIns:string,
  CtrlCV:string
}
export interface KeyGenType{
  keyType: string,
  keyFilePath: string,
  keyPassword: string,
  confirmPassword: string,
  publicKey: string
}