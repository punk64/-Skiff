import { allDataType, CloseWindowMode } from "../../common/types/dataType";
import { Environment, fileUri } from '@kit.CoreFileKit';

const date = new Date();

export const pageDefaultData:allDataType = {
  Session:{
    userName:'',
    hostName:'',
    port:'22',
    speed:'9600',
    password:'',
    connectionType:'SSH',
    connectionSelect:'Raw',
    keepAlive:'0',
    exit:'Only on clean exit',
    savedSessions:'',
    connectionSelected:-1,
    authType:'password',
    privateKeyFd:-1,
    closeWindowMode:CloseWindowMode.Always,
    proxyPolicy:'global'
  },
  Logging:{
    logType:0,
    logFileBaseUri:Environment.getUserDocumentDir()+ '/CrossShellNext',
    logFileName:`CrossShellNext_${date.getFullYear()}${date.getMonth()>8?(date.getMonth()+1):'0'+(date.getMonth()+1)}${date.getDate()>9?date.getDate():'0'+date.getDate()}.log`,
    existOperation:0,
    policiesArray:[],
    includeHeader:true,
    omitKnownPassword:false,
    omitSessionData:false
  },
  Serial: {
    serialLine: 'COM1',
    speed: '9600',
    dataBits: '8',
    stopBits: '1',
    parity: 'None',
    flowControl: 'XON/XOFF',
    paritySelected:0,
    sendEncoding:'utf-8',
    sendEncodingSelected:0,
    readEncoding:"utf-8",
    readEncodingSelected:0,
    connectionSelected:1,
    dataBitsSelected:3,
    speedSelected:12,
    stopBitsSelected:0
  },
  Terminal:{
    variousCheck:['1','3','5','6'],
    answerBackInput:'CrossShellNext',
    echo:'auto',
    editing:'forceOn',
    remoteControlledInput:''
  },
  Connection:{
    keepalive:0,
    tcpOptions:['1','2'],
    version:'Auto',
    SHHKeyHost:''
  },
  Telnet:{
    userName:'',
    hostName:'',
    port:'',
    ambiguity:'BSD',
    mode:'Active',
    keyboard:false,
    telnetNewLine:true
  },
  Tunnels:{
    forwardedPorts:'',
    sourcePort:'8888',
    destination:'127.0.0.1:8000',
    concatType:'local'
  },
  Proxy: {
    type: 'None',
    host: '',
    port: '',
    username: '',
    password: '',
    enabled: false
  },
  WindowSize: {
    col: 1600,
    row: 1000,
    windowResizedId:'1'
  },
  Character: {
    value:'utf-8'
  },
  Cursor:{
    cursorAppear:'block',
    cursorBlinks:true,
  },
  Appear:{
    language:"default",
    mode:-1
  },
  Color:{
    fontColor:'FFFFFF'
  },
  Select:{
  AutoCopy:false,
  CtrlIns:'no action',
  CtrlCV:'no action'
  },
  KeyGen:{
    keyType: '',
    keyFilePath: '',
    keyPassword: '',
    confirmPassword: '',
    publicKey: ''
  }
}