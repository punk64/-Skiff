import { huks } from '@kit.UniversalKeystoreKit'
import { cryptoFramework } from '@kit.CryptoArchitectureKit'
import { buffer } from '@kit.ArkTS'
import { BusinessError } from '@kit.BasicServicesKit'

// 代理凭据 HUKS 加密存储工具
// 使用 AES-256-GCM，密钥由 HUKS 管理（不出沙箱），明文仅存于内存。
// 存储格式：H1:<base64(nonce[12] + ciphertext + tag[16])>
// 兼容旧明文：不含 H1: 前缀的原样返回（用于历史配置升级）。

const KEY_ALIAS: string = 'cross_shell_next_proxy_credential'
const GCM_NONCE_LEN: number = 12
const GCM_TAG_LEN: number = 16
const PREFIX: string = 'H1:'
const ERR_ALIAS_EXIST: number = 60106 // HUKS key alias already exists

function genKeyProps(): Array<huks.HuksParam> {
  return [
    { tag: huks.HuksTag.HUKS_TAG_ALGORITHM, value: huks.HuksKeyAlg.HUKS_ALG_AES },
    { tag: huks.HuksTag.HUKS_TAG_KEY_SIZE, value: huks.HuksKeySize.HUKS_AES_KEY_SIZE_256 },
    {
      tag: huks.HuksTag.HUKS_TAG_PURPOSE,
      value: huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_ENCRYPT | huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_DECRYPT
    },
    { tag: huks.HuksTag.HUKS_TAG_PADDING, value: huks.HuksKeyPadding.HUKS_PADDING_NONE },
    { tag: huks.HuksTag.HUKS_TAG_BLOCK_MODE, value: huks.HuksCipherMode.HUKS_MODE_GCM },
  ]
}

function opProps(purpose: number, nonce: Uint8Array): Array<huks.HuksParam> {
  const props = genKeyProps()
  props[2] = { tag: huks.HuksTag.HUKS_TAG_PURPOSE, value: purpose }
  props.push({ tag: huks.HuksTag.HUKS_TAG_NONCE, value: nonce })
  return props
}

function initOperation(alias: string, options: huks.HuksOptions): Promise<huks.HuksHandle> {
  return new Promise<huks.HuksHandle>((resolve, reject) => {
    huks.init(alias, options, (err: BusinessError, handle: huks.HuksHandle) => {
      if (err) {
        reject(err)
        return
      }
      resolve(handle)
    })
  })
}

function updateOperation(handle: number, inData: Uint8Array): Promise<huks.HuksResult> {
  return new Promise<huks.HuksResult>((resolve, reject) => {
    // 本机 SDK（API 12+）签名为 update(handle, token?: Uint8Array, options, callback)：
    // 4 参传法（token 传 undefined）是匹配该签名的正确用法
    huks.update(handle, undefined, { inData: inData }, (err: BusinessError, result: huks.HuksResult) => {
      if (err) {
        reject(err)
        return
      }
      resolve(result)
    })
  })
}

function finishOperation(handle: number, inData: Uint8Array): Promise<huks.HuksResult> {
  return new Promise<huks.HuksResult>((resolve, reject) => {
    huks.finish(handle, { inData: inData }, (err: BusinessError, result: huks.HuksResult) => {
      if (err) {
        reject(err)
        return
      }
      resolve(result)
    })
  })
}

// 确保密钥存在（已存在则忽略）
async function ensureKey(): Promise<void> {
  await new Promise<void>((resolve, reject) => {
    huks.generateKeyItem(KEY_ALIAS, { properties: genKeyProps() }, (err: BusinessError) => {
      if (!err) {
        resolve()
        return
      }
      if (err.code === ERR_ALIAS_EXIST) {
        resolve()
        return
      }
      reject(err)
    })
  })
}

// 启动预热：确保 HUKS 密钥就绪（幂等），缩短首次建连时的加解密冷路径
export async function ensureProxyHuskKey(): Promise<void> {
  await ensureKey()
}

// 将 Buffer 的有效字节转为精确视图：
// buffer.from(str) 的底层 ArrayBuffer 可能来自共享池且大于内容长度，
// 直接 new Uint8Array(buf.buffer) 会把池内垃圾字节一并带入加解密。
function bytesOf(b: buffer.Buffer): Uint8Array {
  return new Uint8Array(b.buffer, b.byteOffset, b.length)
}

/**
 * 加密代理密码。返回 H1: 前缀的密文；空字符串原样返回。
 */
export async function encryptProxyPassword(plain: string): Promise<string> {
  if (!plain || plain.startsWith(PREFIX)) {
    return plain
  }
  await ensureKey()

  const rand = cryptoFramework.createRandom()
  const nonce = (await rand.generateRandom(GCM_NONCE_LEN)).data

  const initOptions: huks.HuksOptions = {
    properties: opProps(huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_ENCRYPT, nonce)
  }
  const handle = await initOperation(KEY_ALIAS, initOptions)

  const plainBytes = bytesOf(buffer.from(plain, 'utf-8'))
  const cipherResult = await updateOperation(handle.handle, plainBytes)
  const tagResult = await finishOperation(handle.handle, new Uint8Array(0))

  const cipher = cipherResult.outData ?? new Uint8Array(0)
  const tag = tagResult.outData ?? new Uint8Array(0)

  const combined = new Uint8Array(GCM_NONCE_LEN + cipher.length + tag.length)
  combined.set(nonce, 0)
  combined.set(cipher, GCM_NONCE_LEN)
  combined.set(tag, GCM_NONCE_LEN + cipher.length)

  return PREFIX + buffer.from(combined.buffer, combined.byteOffset, combined.length).toString('base64')
}

/**
 * 解密代理密码。非 H1: 前缀（历史明文）原样返回。
 */
export async function decryptProxyPassword(encData: string): Promise<string> {
  if (!encData || !encData.startsWith(PREFIX)) {
    return encData
  }
  await ensureKey()

  const rawBuf = buffer.from(encData.substring(PREFIX.length), 'base64')
  const raw = bytesOf(rawBuf)
  if (raw.length <= GCM_NONCE_LEN + GCM_TAG_LEN) {
    return ''
  }
  const nonce = raw.slice(0, GCM_NONCE_LEN)
  const cipher = raw.slice(GCM_NONCE_LEN, raw.length - GCM_TAG_LEN)
  const tag = raw.slice(raw.length - GCM_TAG_LEN)

  const initOptions: huks.HuksOptions = {
    properties: opProps(huks.HuksKeyPurpose.HUKS_KEY_PURPOSE_DECRYPT, nonce)
  }
  const handle = await initOperation(KEY_ALIAS, initOptions)

  const cipherResult = await updateOperation(handle.handle, cipher)
  await finishOperation(handle.handle, tag)

  const plain = cipherResult.outData ?? new Uint8Array(0)
  return buffer.from(plain.buffer, plain.byteOffset, plain.length).toString('utf-8')
}
