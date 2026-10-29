import { socket } from '@kit.NetworkKit'

export type ProbeState = 'DOWN' | 'LISTEN' | 'UP'

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

export function probeHttp(host: string, port: number, timeoutMs: number): Promise<ProbeState> {
  return new Promise<ProbeState>((resolve: (v: ProbeState) => void) => {
    let tcp: socket.TCPSocket
    try {
      tcp = socket.constructTCPSocketInstance()
    } catch (e) {
      resolve('DOWN')
      return
    }
    let done: boolean = false
    let connected: boolean = false
    let timer: number = -1
    let grace: number = -1
    const finish = (s: ProbeState): void => {
      if (done) {
        return
      }
      done = true
      if (timer >= 0) {
        clearTimeout(timer)
        timer = -1
      }
      if (grace >= 0) {
        clearTimeout(grace)
        grace = -1
      }
      try {
        tcp.off('message')
      } catch (e) {
      }
      try {
        tcp.close()
      } catch (e) {
      }
      resolve(s)
    }
    timer = setTimeout(() => {
      finish(connected ? 'LISTEN' : 'DOWN')
    }, timeoutMs + 300)
    try {
      tcp.on('message', (value: socket.SocketMessageInfo) => {
        if (value !== undefined && value.message !== undefined && value.message.byteLength > 0) {
          finish('UP')
        }
      })
    } catch (e) {
    }
    tcp.connect({
      address: { address: host, port: port, family: 1 },
      timeout: timeoutMs
    }).then(() => {
      connected = true
      const req: string = 'GET / HTTP/1.0\r\nHost: ' + host + ':' + port.toString() +
        '\r\nUser-Agent: Skiff-Probe\r\nConnection: close\r\n\r\n'
      tcp.send({ data: req }).catch((): void => {
      })
      grace = setTimeout(() => {
        finish('LISTEN')
      }, 1500)
    }).catch(() => {
      finish('DOWN')
    })
  })
}
