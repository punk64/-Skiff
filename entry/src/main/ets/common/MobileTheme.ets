export class MT {
  static dark: boolean = true

  static bg: string = '#08090C'
  static surface: string = '#14171D'
  static surface2: string = '#1C2028'
  static line: string = '#262C37'
  static text: string = '#E8EDF5'
  static text2: string = '#98A4B6'
  static text3: string = '#78849B'
  static accent: string = '#3DDC97'
  static accentFg: string = '#04140D'
  static accentSoft: string = '#1F3DDC97'
  static blue: string = '#6E9BFF'
  static blueSoft: string = '#1F6E9BFF'
  static violet: string = '#A78BFA'
  static violetSoft: string = '#1FA78BFA'
  static warn: string = '#FBBF24'
  static danger: string = '#F87171'
  static glass: string = '#FF14171D'
  static scrim: string = '#8C000000'
  static terminalBg: string = '#08090C'
  static transparent: string = '#00000000'

  static fsDisplay: number = 26
  static fsTitle: number = 16
  static fsBody: number = 14
  static fsSmall: number = 13
  static fsCaption: number = 11.5
  static fsMicro: number = 10

  static radiusCard: number = 12
  static radiusCtrl: number = 10
  static radiusChip: number = 7
  static radiusBtn: number = 10
  static pagePadding: number = 16
  static gapCard: number = 8
  static padCard: number = 14
  static bottomSpace: number = 88
  static tabbarSpace: number = 88

  static apply(dark: boolean): void {
    MT.dark = dark
    if (dark) {
      MT.bg = '#08090C'
      MT.surface = '#14171D'
      MT.surface2 = '#1C2028'
      MT.line = '#262C37'
      MT.text = '#E8EDF5'
      MT.text2 = '#98A4B6'
      MT.text3 = '#78849B'
      MT.accent = '#3DDC97'
      MT.accentFg = '#04140D'
      MT.accentSoft = '#1F3DDC97'
      MT.blue = '#E8A44B'
      MT.blueSoft = '#1FE8A44B'
      MT.violet = '#A78BFA'
      MT.violetSoft = '#1FA78BFA'
      MT.warn = '#FBBF24'
      MT.danger = '#F87171'
      MT.glass = '#FF14171D'
      MT.scrim = '#8C000000'
      MT.terminalBg = '#08090C'
    } else {
      MT.bg = '#F4F6F9'
      MT.surface = '#FFFFFF'
      MT.surface2 = '#EDF0F5'
      MT.line = '#E2E7EF'
      MT.text = '#0F141B'
      MT.text2 = '#5A6474'
      MT.text3 = '#646F80'
      MT.accent = '#0E9F6E'
      MT.accentFg = '#FFFFFF'
      MT.accentSoft = '#1A0E9F6E'
      MT.blue = '#C2711D'
      MT.blueSoft = '#1AC2711D'
      MT.violet = '#7C3AED'
      MT.violetSoft = '#1A7C3AED'
      MT.warn = '#B45309'
      MT.danger = '#DC2626'
      MT.glass = '#FFFFFFFF'
      MT.scrim = '#66000000'
      MT.terminalBg = '#08090C'
    }
  }
}
