// 校验位 下拉选择源数据
const parityOptions: SelectOption[] = [
  { value: 'NONE' },
  { value: 'ODD' },
  { value: 'EVEN'},
  { value: 'MARK'},
  { value: 'SPACE' }
]

const flowControlOptions: SelectOption[] = [
  { value: 'None' },
  { value: 'XON/XOFF' },
  { value: 'RTS/CTS' },
  { value: 'DSR/DTR' },
]

// 波特率 下拉选择源数据
const speedOptions:SelectOption[] = [
  { value: '50' },
  { value: '75' },
  { value: '110' },
  { value: '134' },
  { value: '150' },
  { value: '200' },
  { value: '300' },
  { value: '600' },
  { value: '1200' },
  { value: '1800' },
  { value: '2400' },
  { value: '4800' },
  { value: '9600' },
  { value: '19200' },
  { value: '38400' },
  { value: '57600' },
  { value: '115200' },
  { value: '230400' },
  { value: '460800' },
  { value: '500000' },
  { value: '576000' },
  { value: '921600' },
  { value: '1000000' },
  { value: '1152000' },
  { value: '1500000' },
  { value: '2000000' },
  { value: '2500000' },
  { value: '3000000' },
  { value: '3500000' },
  { value: '4000000' }
]

// 数据位宽 下拉选择源数据
const dataBitsOption:SelectOption[]=[
  {
    value:'5'
  },
  {
    value:'6'
  },
  {
    value:'7'
  },
  {
    value:'8'
  }
]

// 停止位宽 下拉数据源
const stopBitsOptions:SelectOption[]=[
  {
    value:'1',
  },
  {
    value:'2'
  }
]

// 编码列表
const encodingList:SelectOption[] = [
  {value:'utf-8'},
  {value:'gbk'}
]

export {speedOptions,stopBitsOptions,parityOptions,flowControlOptions,dataBitsOption,encodingList}