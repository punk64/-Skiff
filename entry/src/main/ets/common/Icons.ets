const GEAR: string = 'M9.69 2.89 L9.90 0.69 A11.50 11.50 0 0 1 14.10 0.69 L14.31 2.89 A9.40 9.40 0 0 1 16.81 3.92 L18.51 2.52 A11.50 11.50 0 0 1 21.48 5.49 L20.08 7.19 A9.40 9.40 0 0 1 21.11 9.69 L23.31 9.90 A11.50 11.50 0 0 1 23.31 14.10 L21.11 14.31 A9.40 9.40 0 0 1 20.08 16.81 L21.48 18.51 A11.50 11.50 0 0 1 18.51 21.48 L16.81 20.08 A9.40 9.40 0 0 1 14.31 21.11 L14.10 23.31 A11.50 11.50 0 0 1 9.90 23.31 L9.69 21.11 A9.40 9.40 0 0 1 7.19 20.08 L5.49 21.48 A11.50 11.50 0 0 1 2.52 18.51 L3.92 16.81 A9.40 9.40 0 0 1 2.89 14.31 L0.69 14.10 A11.50 11.50 0 0 1 0.69 9.90 L2.89 9.69 A9.40 9.40 0 0 1 3.92 7.19 L2.52 5.49 A11.50 11.50 0 0 1 5.49 2.52 L7.19 3.92 A9.40 9.40 0 0 1 9.69 2.89 Z M7.60 12.00 A4.40 4.40 0 1 0 16.40 12.00 A4.40 4.40 0 1 0 7.60 12.00 Z'

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
  if (name === 'gear') {
    return GEAR
  }
  return 'M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z'
}

function argCount(c: string): number {
  if (c === 'M' || c === 'm' || c === 'L' || c === 'l' || c === 'T' || c === 't') {
    return 2
  }
  if (c === 'H' || c === 'h' || c === 'V' || c === 'v') {
    return 1
  }
  if (c === 'C' || c === 'c') {
    return 6
  }
  if (c === 'S' || c === 's' || c === 'Q' || c === 'q') {
    return 4
  }
  if (c === 'A' || c === 'a') {
    return 7
  }
  return 0
}

function isCoordArg(c: string, k: number): boolean {
  if (c === 'A' || c === 'a') {
    return k === 0 || k === 1 || k === 5 || k === 6
  }
  return true
}

function isCmd(t: string): boolean {
  return t.length === 1 && 'MmLlHhVvAaCcSsQqTtZz'.indexOf(t) >= 0
}

function scalePath(d: string, s: number): string {
  if (s === 1) {
    return d
  }
  const tokens: string[] = d.match(/[MmLlHhVvAaCcSsQqTtZz]|-?\d*\.?\d+/g) as string[]
  const out: string[] = []
  let i: number = 0
  while (i < tokens.length) {
    const t: string = tokens[i]
    if (isCmd(t)) {
      out.push(t)
      const n: number = argCount(t)
      i++
      for (let k: number = 0; k < n; k++) {
        if (i >= tokens.length) {
          break
        }
        if (isCoordArg(t, k)) {
          out.push((Number.parseFloat(tokens[i]) * s).toFixed(2))
        } else {
          out.push(tokens[i])
        }
        i++
      }
    } else {
      i++
    }
  }
  return out.join(' ')
}

@Component
export struct Ic {
  name: string = 'gear'
  icSize: number = 20
  color: ResourceColor = '#98A4B6'
  sw: number = 1.7
  filled: boolean = false

  build() {
    if (this.filled) {
      Path()
        .commands(scalePath(pathOf(this.name), vp2px(this.icSize) / 24))
        .width(this.icSize)
        .height(this.icSize)
        .fill(this.color)
    } else {
      Path()
        .commands(scalePath(pathOf(this.name), vp2px(this.icSize) / 24))
        .width(this.icSize)
        .height(this.icSize)
        .fillOpacity(0)
        .stroke(this.color)
        .strokeWidth(this.sw * vp2px(this.icSize) / 24)
        .strokeLineCap(LineCapStyle.Round)
        .strokeLineJoin(LineJoinStyle.Round)
    }
  }
}
