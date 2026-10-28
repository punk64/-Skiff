import { PortForwardingInfo } from 'libentry.so'

@Observed
export class ProtDialogDataSourceClass{
  dataSource:PortForwardingInfo[]
  constructor(dataSource:PortForwardingInfo[]) {
    this.dataSource = dataSource
  }
}

@CustomDialog
@Component
export default struct PortForwardingDialog {
  @ObjectLink portForwardings: ProtDialogDataSourceClass
  @State sourcePort: string = ''
  @State destination: string = ''
  @State forwardingType: 'local' | 'remote' | 'dynamic' = 'local'
  @State targetPort: string = ''
  controller?: CustomDialogController
  stopForwarding?: (port: number, isRemote: boolean) => void = () => {}
  addPortForwarding?: (sourcePort: string, destination: string, type: 'local' | 'remote' | 'dynamic') => void = () => {}

  aboutToAppear() {
    console.log('portForwardingDialog aboutToAppear', JSON.stringify(this.portForwardings))
    if (this.destination && this.destination.includes(':')) {
      const parts = this.destination.split(':');
      this.targetPort = parts[1] || '';
    }
  }

  build() {
    Column() {
      Row() {
        Text($r('app.string.PORT_FORWARDED'))
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .fontColor('#1A1A1A')
          .textAlign(TextAlign.Start)
          .layoutWeight(1)

        Button() {
          Image($rawfile('FileLIcon/port.png')).width(20).height(20)
        }
        .backgroundColor(Color.Transparent)
        .width(28)
        .height(28)
        .margin({ right: 8 })
        .onClick(() => {
          this.controller?.close()
        })
      }
      .width('100%')
      .padding({ top: 16, bottom: 8, left: 16, right: 8 })

      Divider().color('#E0E0E0').height(1)

      // 端口列表部分
      if (this.portForwardings.dataSource.length > 0) {
        Scroll() {
          Column({space: 0}) {
            ForEach(this.portForwardings.dataSource, (item: PortForwardingInfo, index) => {
              Row() {
                Text(item.isDynamic ? $r('app.string.Dynamic') : (item.isRemote ? $r('app.string.Remote') : $r('app.string.Local')))
                  .fontSize(14)
                  .fontColor('#555')
                  .width(80)

                Text(item.isDynamic ? $r('app.string.SOCKS_PROXY',item.port+'—') : `${item.port}—${item.targetHost}:${item.targetPort}`)
                  .fontSize(14)
                  .fontColor('#333')
                  .layoutWeight(1)
                  .margin({ left: 4 })

                // Button() {
                  Row(){
                    Image($rawfile('FileLIcon/PortDelete.svg'))
                      .width(16)
                      .height(16)
                  }.borderRadius(100)
                  .backgroundColor('#E4E4E4')
                  .width(32)
                  .height(32)
                  .justifyContent(FlexAlign.Center)
                  .alignItems(VerticalAlign.Center)
                  .onClick(() => {
                    if (this.stopForwarding) {
                      this.stopForwarding(item.port, item.isRemote)
                    }
                  })
                // }
                // .backgroundColor('#E4E4E4')
                // .width(28)
                // .height(28)

              }
              .width('100%')
              .justifyContent(FlexAlign.SpaceBetween)
              .alignItems(VerticalAlign.Center)
              .padding({ left: 16, right: 16, top: 12, bottom: 12 })

              if (index < this.portForwardings.dataSource.length - 1) {
                Divider().color('#F0F0F0').height(1).margin({ left: 16, right: 16 })
              }
            })
          }
          .width('100%')
        }
        .scrollable(ScrollDirection.Vertical)
        .scrollBar(BarState.Auto)
        .height(Math.min(this.portForwardings.dataSource.length * 44, 132)) // 最多显示3条，每条44高度
        .backgroundColor(Color.White)
      } else {
        Text($r('app.string.NO_FORWARDED_PORTS'))
          .fontSize(14)
          .fontColor('#999')
          .padding(16)
          .backgroundColor(Color.White)
      }

      Divider().color('#E0E0E0').height(8).backgroundColor('#F5F5F5')

      // 添加端口转发部分
      Text($r('app.string.ADD_PORT_FORWARDING'))
        .fontSize(16)
        .fontWeight(FontWeight.Medium)
        .fontColor('#1A1A1A')
        .padding({ left: 16, top: 12, bottom: 12 })
        .alignSelf(ItemAlign.Start)

      Column({ space: 16 }) {
        // Source port 输入行
        Row({ space: 8 }) {
          Text($r('app.string.Sourceport'))
            .fontSize(14)
            .fontColor('#333')
            .width(100)

          TextInput({ placeholder: '8000', text: this.sourcePort })
            .type(InputType.Number)
            .height(40)
            .layoutWeight(1)
            .backgroundColor(Color.White)
            .border({ width: 1, color: '#E0E0E0', radius: 4 })
            .padding({ left: 8, right: 8 })
            .onChange((value) => {
              this.sourcePort = value;
            })
        }
        .width('100%')
        .padding({ left: 16, right: 16 })
        .alignItems(VerticalAlign.Center)

        // Destination 输入行
        Row({ space: 8 }) {
          Text($r('app.string.Destination'))
            .fontSize(14)
            .fontColor('#333')
            .width(100)

          TextInput({ placeholder: '127.0.0.1:80', text: this.destination })
            .height(40)
            .layoutWeight(1)
            .backgroundColor(Color.White)
            .border({ width: 1, color: '#E0E0E0', radius: 4 })
            .padding({ left: 8, right: 8 })
            .onChange((value) => {
              this.destination = value;

              // 如果包含冒号，解析出目标端口
              if (value && value.includes(':')) {
                const parts = value.split(':');
                this.targetPort = parts[1] || '';
              }
            })
        }
        .width('100%')
        .padding({ left: 16, right: 16 })
        .alignItems(VerticalAlign.Center)

        // 转发类型选择和转发端口按钮
        Row() {
          // 类型选择按钮组（分段控件）
          Row() {
            Button({ type: ButtonType.Capsule }) {
              Text($r('app.string.Local')).fontSize(14).fontColor(this.forwardingType === 'local' ? Color.White : '#333')
            }
            .backgroundColor(this.forwardingType === 'local' ? '#0A59F7' : Color.White)
            .height(40)
            .width('33.3%')
            .borderRadius(0)
            .borderColor('#E0E0E0')
            .borderWidth(1)
            .onClick(() => this.forwardingType = 'local')

            Button({ type: ButtonType.Capsule }) {
              Text($r('app.string.Remote')).fontSize(14).fontColor(this.forwardingType === 'remote' ? Color.White : '#333')
            }
            .backgroundColor(this.forwardingType === 'remote' ? '#0A59F7' : Color.White)
            .height(40)
            .width('33.3%')
            .borderRadius(0)
            .borderColor('#E0E0E0')
            .borderWidth(1)
            .onClick(() => this.forwardingType = 'remote')

            Button({ type: ButtonType.Capsule }) {
              Text($r('app.string.Dynamic')).fontSize(14).fontColor(this.forwardingType === 'dynamic' ? Color.White : '#333')
            }
            .backgroundColor(this.forwardingType === 'dynamic' ? '#0A59F7' : Color.White)
            .height(40)
            .width('33.4%')
            .borderRadius(0)
            .borderColor('#E0E0E0')
            .borderWidth(1)
            .onClick(() => this.forwardingType = 'dynamic')
          }
          .layoutWeight(1)
          .borderRadius(4)
          .clip(true)
        }
        .width('100%')
        .padding({ left: 16, right: 16 })

        // 转发端口按钮
        Button() {
          Text($r('app.string.FORWARD_PORT')).fontSize(14).fontColor(Color.White)
        }
        .height(40)
        .backgroundColor('#0A59F7')
        .borderRadius(4)
        .width('100%')
        .margin({ left: 16, right: 16, top: 4, bottom: 16 })
        .onClick(() => {
          if (this.addPortForwarding) {
            this.addPortForwarding(this.sourcePort, this.destination, this.forwardingType);
          }
        })
      }
      .width('100%')
    }
    .width(480)
    .borderRadius(8)
    .backgroundColor(Color.White)
  }
}