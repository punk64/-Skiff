import { fileIo as fs } from '@kit.CoreFileKit'
import { common } from '@kit.AbilityKit'
import { util } from '@kit.ArkTS'

export interface AppLogFile {
  name: string
  path: string
  size: number
  mtime: number
}

interface AppLogLine {
  t: number
  lv: string
  tag: string
  msg: string
}

const RING_MAX: number = 500
const KEEP_FILES: number = 7

function pad2(n: number): string {
  return n < 10 ? '0' + n.toString() : n.toString()
}

function dayKey(ms: number): string {
  const d: Date = new Date(ms)
  return d.getFullYear().toString() + pad2(d.getMonth() + 1) + pad2(d.getDate())
}

function clock(ms: number): string {
  const d: Date = new Date(ms)
  return pad2(d.getHours()) + ':' + pad2(d.getMinutes()) + ':' + pad2(d.getSeconds()) + '.' +
    (d.getMilliseconds() < 100 ? (d.getMilliseconds() < 10 ? '00' : '0') : '') + d.getMilliseconds().toString()
}

export class AppLog {
  private static ring: AppLogLine[] = []
  private static dir: string = ''
  private static ready: boolean = false

  static init(ctx: common.Context): void {
    try {
      AppLog.dir = ctx.filesDir + '/logs'
      try {
        fs.mkdirSync(AppLog.dir)
      } catch (e) {
      }
      AppLog.ready = true
      AppLog.prune()
      AppLog.i('app', '---- session start ----')
    } catch (e) {
      AppLog.ready = false
    }
  }

  static dirOf(): string {
    return AppLog.dir
  }

  static i(tag: string, msg: string): void {
    AppLog.write('I', tag, msg)
  }

  static w(tag: string, msg: string): void {
    AppLog.write('W', tag, msg)
  }

  static e(tag: string, msg: string): void {
    AppLog.write('E', tag, msg)
  }

  private static write(lv: string, tag: string, msg: string): void {
    const line: AppLogLine = { t: Date.now(), lv: lv, tag: tag, msg: msg }
    AppLog.ring.push(line)
    while (AppLog.ring.length > RING_MAX) {
      AppLog.ring.shift()
    }
    AppLog.append(line)
  }

  private static append(l: AppLogLine): void {
    if (!AppLog.ready || AppLog.dir.length === 0) {
      return
    }
    const path: string = AppLog.dir + '/app-' + dayKey(l.t) + '.log'
    const text: string = clock(l.t) + ' ' + l.lv + '/' + l.tag + ' ' + l.msg + '\n'
    try {
      const f = fs.openSync(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.APPEND)
      fs.writeSync(f.fd, text)
      fs.closeSync(f.fd)
    } catch (e) {
    }
  }

  static dump(): string {
    let out: string = ''
    for (let i = 0; i < AppLog.ring.length; i++) {
      const l: AppLogLine = AppLog.ring[i]
      out += clock(l.t) + ' ' + l.lv + '/' + l.tag + ' ' + l.msg + '\n'
    }
    return out
  }

  static files(): AppLogFile[] {
    const out: AppLogFile[] = []
    if (AppLog.dir.length === 0) {
      return out
    }
    try {
      const names: string[] = fs.listFileSync(AppLog.dir)
      for (let i = 0; i < names.length; i++) {
        if (!names[i].startsWith('app-') || !names[i].endsWith('.log')) {
          continue
        }
        const p: string = AppLog.dir + '/' + names[i]
        let size: number = 0
        let mt: number = 0
        try {
          const st: fs.Stat = fs.statSync(p)
          size = st.size
          mt = st.mtime * 1000
        } catch (e) {
        }
        out.push({ name: names[i], path: p, size: size, mtime: mt })
      }
    } catch (e) {
    }
    out.sort((a: AppLogFile, b: AppLogFile) => b.mtime - a.mtime)
    return out
  }

  static read(path: string, maxBytes: number): string {
    try {
      const st: fs.Stat = fs.statSync(path)
      if (st.size <= 0) {
        return ''
      }
      let start: number = st.size > maxBytes ? st.size - maxBytes : 0
      if (start > 0) {
        start = start - 1
      }
      const len: number = st.size - start
      const f = fs.openSync(path, fs.OpenMode.READ_ONLY)
      const buf: ArrayBuffer = new ArrayBuffer(len)
      const n: number = fs.readSync(f.fd, buf, { offset: start, length: len })
      fs.closeSync(f.fd)
      const dec: util.TextDecoder = util.TextDecoder.create('utf-8', { fatal: false, ignoreBOM: true })
      let text: string = dec.decodeToString(new Uint8Array(buf, 0, n > 0 ? n : len))
      if (start > 0) {
        const nl: number = text.indexOf('\n')
        text = nl >= 0 ? text.substring(nl + 1) : ''
      }
      return text
    } catch (e) {
      return ''
    }
  }

  static clear(): number {
    const list: AppLogFile[] = AppLog.files()
    let n: number = 0
    for (let i = 0; i < list.length; i++) {
      try {
        fs.unlinkSync(list[i].path)
        n = n + 1
      } catch (e) {
      }
    }
    return n
  }

  static prune(): void {
    const list: AppLogFile[] = AppLog.files()
    if (list.length <= KEEP_FILES) {
      return
    }
    for (let i = KEEP_FILES; i < list.length; i++) {
      try {
        fs.unlinkSync(list[i].path)
      } catch (e) {
      }
    }
  }
}
