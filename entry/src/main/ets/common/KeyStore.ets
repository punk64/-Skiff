import { fileIo as fs } from '@kit.CoreFileKit'
import { common } from '@kit.AbilityKit'
import { generateKeyPair } from 'libentry.so'

export interface ManagedKey {
  name: string
  alg: string
  bits: number
  path: string
  pubPath: string
  encrypted: boolean
  size: number
}

const DIR: string = 'keys_mgr'

export function keyDir(ctx: common.Context): string {
  const d: string = ctx.filesDir + '/' + DIR
  if (!fs.accessSync(d)) {
    fs.mkdirSync(d, true)
  }
  return d
}

function prefixOf(alg: string): string {
  if (alg === 'RSA') {
    return 'id_rsa'
  }
  if (alg === 'ECDSA') {
    return 'id_ecdsa'
  }
  return 'id_ed25519'
}

function algOf(name: string): string {
  if (name.indexOf('rsa') >= 0) {
    return 'RSA'
  }
  if (name.indexOf('ecdsa') >= 0) {
    return 'ECDSA'
  }
  return 'Ed25519'
}

function bitsOf(alg: string): number {
  return alg === 'RSA' ? 2048 : 256
}

function isEncrypted(path: string): boolean {
  let f: fs.File | null = null
  try {
    f = fs.openSync(path, fs.OpenMode.READ_ONLY)
    const buf: ArrayBuffer = new ArrayBuffer(64)
    const n: number = fs.readSync(f.fd, buf)
    const arr: Uint8Array = new Uint8Array(buf, 0, n)
    let s: string = ''
    for (let i = 0; i < arr.length; i++) {
      s += String.fromCharCode(arr[i])
    }
    return s.indexOf('ENCRYPTED') >= 0
  } catch (e) {
    return false
  } finally {
    if (f !== null) {
      try {
        fs.closeSync(f)
      } catch (e) {
      }
    }
  }
}

export function listKeys(ctx: common.Context): ManagedKey[] {
  const d: string = keyDir(ctx)
  const out: ManagedKey[] = []
  let names: string[] = []
  try {
    names = fs.listFileSync(d)
  } catch (e) {
    return out
  }
  names.forEach((n: string) => {
    if (n.endsWith('.pub')) {
      return
    }
    const p: string = d + '/' + n
    const alg: string = algOf(n)
    let size: number = 0
    try {
      size = fs.statSync(p).size
    } catch (e) {
    }
    out.push({
      name: n,
      alg: alg,
      bits: bitsOf(alg),
      path: p,
      pubPath: p + '.pub',
      encrypted: isEncrypted(p),
      size: size
    })
  })
  out.sort((a: ManagedKey, b: ManagedKey) => a.name < b.name ? 1 : -1)
  return out
}

export async function createKey(ctx: common.Context, alg: string, pass: string): Promise<ManagedKey> {
  const d: string = keyDir(ctx)
  const name: string = prefixOf(alg) + '_' + Date.now().toString()
  const privPath: string = d + '/' + name
  const pubPath: string = privPath + '.pub'
  const priv: fs.File = fs.openSync(privPath, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC)
  const pub: fs.File = fs.openSync(pubPath, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC)
  try {
    await generateKeyPair(alg, pass.length === 0 ? null : pass, priv.fd, pub.fd)
  } finally {
    try {
      fs.closeSync(priv)
    } catch (e) {
    }
    try {
      fs.closeSync(pub)
    } catch (e) {
    }
  }
  let size: number = 0
  try {
    size = fs.statSync(privPath).size
  } catch (e) {
  }
  return {
    name: name,
    alg: alg,
    bits: bitsOf(alg),
    path: privPath,
    pubPath: pubPath,
    encrypted: pass.length > 0,
    size: size
  }
}

export function removeKey(k: ManagedKey): void {
  const targets: string[] = [k.path, k.pubPath]
  targets.forEach((p: string) => {
    try {
      if (fs.accessSync(p)) {
        fs.unlinkSync(p)
      }
    } catch (e) {
    }
  })
}
