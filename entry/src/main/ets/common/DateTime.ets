const TS_RE: RegExp = /^(\d{4})-(\d{1,2})-(\d{1,2})(?:[ T](\d{1,2}):(\d{1,2})(?::(\d{1,2}))?)?$/

export function pad2(n: number): string {
  return n < 10 ? '0' + n.toString() : n.toString()
}

export function normTs(raw: string): string {
  const v: string = raw.trim().replace(/\//g, '-')
  if (v.length === 0) {
    return ''
  }
  const m: RegExpMatchArray | null = v.match(TS_RE)
  if (m === null) {
    return ''
  }
  const y: number = Number(m[1])
  const mo: number = Number(m[2])
  const d: number = Number(m[3])
  const hh: number = m[4] === undefined ? 0 : Number(m[4])
  const mi: number = m[5] === undefined ? 0 : Number(m[5])
  const ss: number = m[6] === undefined ? 0 : Number(m[6])
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || hh > 23 || mi > 59 || ss > 59) {
    return ''
  }
  const dt: Date = new Date(y, mo - 1, d, hh, mi, ss)
  if (dt.getFullYear() !== y || dt.getMonth() !== mo - 1 || dt.getDate() !== d) {
    return ''
  }
  return y.toString() + '-' + pad2(mo) + '-' + pad2(d) + ' ' + pad2(hh) + ':' + pad2(mi) + ':' + pad2(ss)
}

export function tsMillis(ts: string): number {
  const m: RegExpMatchArray | null = ts.trim().match(TS_RE)
  if (m === null) {
    return -1
  }
  const y: number = Number(m[1])
  const mo: number = Number(m[2])
  const d: number = Number(m[3])
  const hh: number = m[4] === undefined ? 0 : Number(m[4])
  const mi: number = m[5] === undefined ? 0 : Number(m[5])
  const ss: number = m[6] === undefined ? 0 : Number(m[6])
  return new Date(y, mo - 1, d, hh, mi, ss).getTime()
}

export function daysLeft(expire: string): number {
  const t: number = tsMillis(expire)
  if (t < 0) {
    return 0
  }
  const diff: number = t - Date.now()
  const d: number = Math.floor(Math.abs(diff) / 86400000)
  return diff >= 0 ? Math.max(1, d) : -Math.max(1, d)
}

export function fmtDay(ms: number): string {
  const dt: Date = new Date(ms)
  return dt.getFullYear().toString() + '-' + pad2(dt.getMonth() + 1) + '-' + pad2(dt.getDate()) + ' ' +
    pad2(dt.getHours()) + ':' + pad2(dt.getMinutes()) + ':' + pad2(dt.getSeconds())
}
