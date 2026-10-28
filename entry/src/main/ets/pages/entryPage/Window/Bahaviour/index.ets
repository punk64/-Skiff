@Component
export default struct Bahaviour {

  build() {
    Column({ space: 10 }) {
      Flex({ direction: FlexDirection.Row }) {

        Text("2")

      }
      .width('100%')
      .height('100%')
      .padding(20)
    }
  }
}
