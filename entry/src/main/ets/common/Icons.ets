function pathOf(name: string): string {
  if (name === 'terminal') {
    return 'M4 6l5 6-5 6M12 18h8'
  }
  if (name === 'tunnel') {
    return 'M17 4l4 4-4 4M21 8H9M7 20l-4-4 4-4M3 16h12'
  }
  if (name === 'folder') {
    return 'M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'
  }
  if (name === 'plus') {
    return 'M12 5v14M5 12h14'
  }
  if (name === 'trash') {
    return 'M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2M10 11v6M14 11v6'
  }
  if (name === 'power') {
    return 'M12 3v8M6.3 6.3a8 8 0 1 0 11.4 0'
  }
  if (name === 'back') {
    return 'M14 6l-6 6 6 6'
  }
  if (name === 'close') {
    return 'M6 6l12 12M18 6L6 18'
  }
  if (name === 'refresh') {
    return 'M21 4v6h-6M3 20v-6h6M20 9a8 8 0 0 0-15-3M4 15a8 8 0 0 0 15 3'
  }
  if (name === 'swap') {
    return 'M8 3L4 7l4 4M4 7h16M16 21l4-4-4-4M20 17H4'
  }
  return 'M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z'
}

@Component
export struct Ic {
  name: string = 'gear'
  icSize: number = 20
  color: string = '#98A4B6'
  sw: number = 1.9

  build() {
    Shape() {
      Path()
        .commands(pathOf(this.name))
        .fillOpacity(0)
        .stroke(this.color)
        .strokeWidth(this.sw)
        .strokeLineCap(LineCapStyle.Round)
        .strokeLineJoin(LineJoinStyle.Round)
    }
    .viewPort({ x: 0, y: 0, width: 24, height: 24 })
    .width(this.icSize)
    .height(this.icSize)
  }
}
