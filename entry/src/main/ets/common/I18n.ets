export class L {
  static lang: string = 'zh'

  static t(zh: string, en: string): string {
    return L.lang === 'en' ? en : zh
  }
}
