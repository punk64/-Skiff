import { socket } from '@kit.NetworkKit'

export function probeTcp(host: string, port: number, timeoutMs: number): Promise<boolean> {
  return new Promise<boolean>((resolve: (v: boolean) => void) => {
    let tcp: socket.TCPSocket
    try {
      tcp = socket.constructTCPSocketInstance()
    } catch (e) {
      resolve(false)
      return
    }
    let done: boolean = false
    let timer: number = -1
    const finish = (ok: boolean): void => {
      if (done) {
        return
      }
      done = true
      if (timer >= 0) {
        clearTimeout(timer)
        timer = -1
      }
      try {
        tcp.close()
      } catch (e) {
      }
      resolve(ok)
    }
    timer = setTimeout(() => {
      finish(false)
    }, timeoutMs + 300)
    tcp.connect({
      address: { address: host, port: port, family: 1 },
      timeout: timeoutMs
    }).then(() => {
      finish(true)
    }).catch(() => {
      finish(false)
    })
  })
}

export async function waitPort(host: string, port: number, tries: number, gapMs: number): Promise<boolean> {
  for (let i = 0; i < tries; i++) {
    const ok: boolean = await probeTcp(host, port, 900)
    if (ok) {
      return true
    }
    await new Promise<void>((resolve: (v: void) => void) => {
      setTimeout(() => {
        resolve()
      }, gapMs)
    })
  }
  return false
}
