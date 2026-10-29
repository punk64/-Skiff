export interface Bi {
  zh: string
  en: string
}

export class L {
  static lang: string = 'zh'

  static t(zh: string, en: string): string {
    return L.lang === 'en' ? en : zh
  }

  static b(zh: string, en: string): Bi {
    return { zh: zh, en: en }
  }

  static s(v: string): Bi {
    return { zh: v, en: v }
  }
}
