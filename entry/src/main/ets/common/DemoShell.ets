import { L } from './I18n'

const RST: string = '\x1b[0m'
const BLD: string = '\x1b[1m'
const DIM: string = '\x1b[2m'
const RED: string = '\x1b[31m'
const GRN: string = '\x1b[32m'
const YEL: string = '\x1b[33m'
const BLU: string = '\x1b[34m'
const MAG: string = '\x1b[35m'
const CYN: string = '\x1b[36m'
const BBLU: string = '\x1b[1;34m'
const BGRN: string = '\x1b[1;32m'
const BCYN: string = '\x1b[1;36m'

const USER: string = 'ubuntu'
const HOST: string = 'web-01'

export class DemoShell {
  private buf: string = ''
  private cwd: string = '~'
  private history: string[] = []
  private histIdx: number = -1

  banner(): string {
    const lines: string[] = [
      `Welcome to Ubuntu 22.04.4 LTS (GNU/Linux 5.15.0-101-generic x86_64)`,
      ``,
      ` * Documentation:  https://help.ubuntu.com`,
      ` * Management:     https://landscape.canonical.com`,
      ` * Support:        https://ubuntu.com/pro`,
      ``,
      `  System information as of Wed Oct 28 17:35:02 CST 2026`,
      ``,
      `  System load:  0.08               Processes:             128`,
      `  Usage of /:   42.3% of 39.02GB   Users logged in:       1`,
      `  Memory usage: 23%                IPv4 address for eth0: 10.0.8.12`,
      `  Swap usage:   0%`,
      ``,
      `Last login: Wed Oct 28 15:41:07 2026 from 192.0.2.15`,
      ``,
    ]
    return lines.join('\r\n') + this.prompt()
  }

  prompt(): string {
    return `${BGRN}${USER}@${HOST}${RST}:${BBLU}${this.cwd}${RST}$ `
  }

  handle(data: string): string {
    if (data === '\x1b[A') {
      return this.historyUp()
    }
    if (data === '\x1b[B') {
      return this.historyDown()
    }
    if (data === '\x1b[C' || data === '\x1b[D' || data === '\x1b[H' || data === '\x1b[F') {
      return ''
    }
    if (data === '\x1b[5~' || data === '\x1b[6~') {
      return ''
    }
    let out: string = ''
    for (let i = 0; i < data.length; i++) {
      out += this.handleChar(data.charAt(i))
    }
    return out
  }

  private handleChar(ch: string): string {
    if (ch === '\r' || ch === '\n') {
      return this.exec()
    }
    if (ch === '\x7f' || ch === '\b') {
      if (this.buf.length === 0) {
        return ''
      }
      this.buf = this.buf.substring(0, this.buf.length - 1)
      return '\b \b'
    }
    if (ch === '\x03') {
      this.buf = ''
      this.histIdx = -1
      return `^C\r\n${this.prompt()}`
    }
    if (ch === '\x04') {
      return this.doExit()
    }
    if (ch === '\t') {
      return this.complete()
    }
    if (ch === '\x0c') {
      this.buf = ''
      return `\x1b[2J\x1b[H${this.prompt()}`
    }
    if (ch < ' ') {
      return ''
    }
    this.buf += ch
    return ch
  }

  private complete(): string {
    if (this.buf.length === 0) {
      return ''
    }
    const cmds: string[] = ['clear', 'cd', 'cat', 'date', 'df', 'echo', 'exit', 'free', 'git',
      'help', 'hostname', 'ip', 'ls', 'neofetch', 'ps', 'pwd', 'uname', 'uptime', 'whoami']
    const hit: string[] = cmds.filter((c: string) => c.startsWith(this.buf))
    if (hit.length === 1) {
      const add: string = hit[0].substring(this.buf.length)
      this.buf = hit[0]
      return add
    }
    if (hit.length > 1) {
      return `\r\n${hit.join('  ')}\r\n${this.prompt()}${this.buf}`
    }
    return ''
  }

  private historyUp(): string {
    if (this.history.length === 0) {
      return ''
    }
    if (this.histIdx < 0) {
      this.histIdx = this.history.length - 1
    } else if (this.histIdx > 0) {
      this.histIdx--
    }
    return this.replaceLine(this.history[this.histIdx])
  }

  private historyDown(): string {
    if (this.histIdx < 0) {
      return ''
    }
    if (this.histIdx < this.history.length - 1) {
      this.histIdx++
      return this.replaceLine(this.history[this.histIdx])
    }
    this.histIdx = -1
    return this.replaceLine('')
  }

  private replaceLine(next: string): string {
    let out: string = '\r' + this.prompt()
    this.buf = next
    return out + next
  }

  private doExit(): string {
    return `logout\r\nConnection to ${HOST} closed.\r\n`
  }

  private exec(): string {
    const line: string = this.buf.trim()
    this.buf = ''
    this.histIdx = -1
    if (line.length === 0) {
      return `\r\n${this.prompt()}`
    }
    this.history.push(line)
    const parts: string[] = line.split(' ')
    const cmd: string = parts[0]
    const args: string[] = parts.slice(1).filter((s: string) => s.length > 0)
    if (cmd === 'exit' || cmd === 'logout') {
      return `\r\n${this.doExit()}`
    }
    return `\r\n${this.run(cmd, args)}\r\n${this.prompt()}`
  }

  private run(cmd: string, args: string[]): string {
    if (cmd === 'help') {
      return this.helpText()
    }
    if (cmd === 'clear') {
      return '\x1b[2J\x1b[H'
    }
    if (cmd === 'pwd') {
      return this.cwd === '~' ? `/home/${USER}` : this.cwd
    }
    if (cmd === 'whoami') {
      return USER
    }
    if (cmd === 'hostname') {
      return HOST
    }
    if (cmd === 'uname') {
      return args.indexOf('-a') >= 0
        ? 'Linux web-01 5.15.0-101-generic #111-Ubuntu SMP Tue Oct 6 08:20:04 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux'
        : 'Linux'
    }
    if (cmd === 'date') {
      return 'Wed Oct 28 17:35:12 CST 2026'
    }
    if (cmd === 'uptime') {
      return ' 17:35:12 up 42 days,  6:18,  1 user,  load average: 0.08, 0.12, 0.09'
    }
    if (cmd === 'ls' || cmd === 'll') {
      return this.listDir(cmd === 'll' || args.indexOf('-l') >= 0 || args.indexOf('-la') >= 0)
    }
    if (cmd === 'cd') {
      return this.changeDir(args.length > 0 ? args[0] : '~')
    }
    if (cmd === 'cat') {
      return this.catFile(args.length > 0 ? args[0] : '')
    }
    if (cmd === 'echo') {
      return args.join(' ')
    }
    if (cmd === 'df') {
      return this.diskFree()
    }
    if (cmd === 'free') {
      return this.memory()
    }
    if (cmd === 'ps') {
      return this.processes()
    }
    if (cmd === 'ip') {
      return this.network()
    }
    if (cmd === 'git') {
      return this.gitStatus(args)
    }
    if (cmd === 'neofetch') {
      return this.neofetch()
    }
    return `bash: ${cmd}: command not found`
  }

  private helpText(): string {
    const rows: string[] = L.lang === 'en' ? [
      `${BLD}demo shell${RST} — commands below are implemented, others return command not found`,
      ``,
      `  ${CYN}ls${RST} / ${CYN}ls -l${RST}    list directory (colored)`,
      `  ${CYN}cd${RST} <dir>        change directory`,
      `  ${CYN}pwd${RST}             print working directory`,
      `  ${CYN}cat${RST} <file>      show file content`,
      `  ${CYN}df -h${RST}           disk usage`,
      `  ${CYN}free -h${RST}         memory usage`,
      `  ${CYN}ps aux${RST}          process list`,
      `  ${CYN}ip a${RST}            network interfaces`,
      `  ${CYN}git status${RST}      repository status (colored)`,
      `  ${CYN}neofetch${RST}        system information`,
      `  ${CYN}clear${RST}           clear screen`,
      `  ${CYN}exit${RST}            exit`,
    ] : [
      `${BLD}demo shell${RST} — 以下命令有实装，其余返回 command not found`,
      ``,
      `  ${CYN}ls${RST} / ${CYN}ls -l${RST}    列目录（含颜色）`,
      `  ${CYN}cd${RST} <dir>        切换目录`,
      `  ${CYN}pwd${RST}             当前路径`,
      `  ${CYN}cat${RST} <file>      查看文件`,
      `  ${CYN}df -h${RST}           磁盘占用`,
      `  ${CYN}free -h${RST}         内存占用`,
      `  ${CYN}ps aux${RST}          进程列表`,
      `  ${CYN}ip a${RST}            网卡地址`,
      `  ${CYN}git status${RST}      仓库状态（彩色）`,
      `  ${CYN}neofetch${RST}        系统信息`,
      `  ${CYN}clear${RST}           清屏`,
      `  ${CYN}exit${RST}            退出`,
    ]
    return rows.join('\r\n')
  }

  private listDir(long: boolean): string {
    if (this.cwd === '/var/log') {
      if (!long) {
        return `${BBLU}apt${RST}  ${BBLU}journal${RST}  ${BBLU}nginx${RST}  syslog  auth.log  ${RED}kern.log${RST}`
      }
      const rows: string[] = [
        'total 4824',
        `drwxr-xr-x  2 root adm     4096 Oct 28 06:25 ${BBLU}apt${RST}`,
        `drwxr-sr-x  3 root systemd 4096 Oct 27 00:00 ${BBLU}journal${RST}`,
        `drwxr-xr-x  2 root adm     4096 Oct 28 14:02 ${BBLU}nginx${RST}`,
        `-rw-r-----  1 root adm   182341 Oct 28 17:30 ${RED}syslog${RST}`,
        `-rw-r-----  1 root adm    94182 Oct 28 17:22 ${RED}auth.log${RST}`,
        '-rw-r-----  1 root adm    12048 Oct 26 09:11 kern.log',
      ]
      return rows.join('\r\n')
    }
    if (!long) {
      return `${BBLU}app${RST}  ${BBLU}backups${RST}  ${BBLU}logs${RST}  docker-compose.yml  nginx.conf  ${BGRN}deploy.sh${RST}  README.md`
    }
    const rows: string[] = [
      'total 96',
      `drwxr-xr-x  4 ubuntu ubuntu 4096 Oct 28 15:41 ${BBLU}.${RST}`,
      `drwxr-xr-x  3 root   root   4096 Oct 18 10:11 ${BBLU}..${RST}`,
      'drwxr-xr-x  6 ubuntu ubuntu 4096 Oct 28 14:22 app',
      'drwxr-xr-x  2 ubuntu ubuntu 4096 Oct 27 22:03 backups',
      'drwxr-xr-x  3 ubuntu ubuntu 4096 Oct 28 14:55 logs',
      '-rw-r--r--  1 ubuntu ubuntu 1842 Oct 26 11:20 docker-compose.yml',
      '-rw-r--r--  1 ubuntu ubuntu  612 Oct 24 18:02 nginx.conf',
      `-rwxr-xr-x  1 ubuntu ubuntu  842 Oct 27 19:26 ${BGRN}deploy.sh${RST}`,
      '-rw-r--r--  1 ubuntu ubuntu 1204 Oct 20 09:31 README.md',
    ]
    return rows.join('\r\n')
  }

  private changeDir(target: string): string {
    if (target === '~' || target === '/home/ubuntu') {
      this.cwd = '~'
      return ''
    }
    if (target === '..' || target === '-') {
      this.cwd = '~'
      return ''
    }
    if (target === '/var/log' || target === 'logs') {
      this.cwd = '/var/log'
      return ''
    }
    if (target === '/') {
      this.cwd = '/'
      return ''
    }
    return `bash: cd: ${target}: No such file or directory`
  }

  private catFile(name: string): string {
    if (name === '/etc/os-release' || name === 'os-release') {
      const rows: string[] = [
        'PRETTY_NAME="Ubuntu 22.04.4 LTS"',
        'NAME="Ubuntu"',
        'VERSION_ID="22.04"',
        'VERSION="22.04.4 LTS (Jammy Jellyfish)"',
        'VERSION_CODENAME=jammy',
        'ID=ubuntu',
        'ID_LIKE=debian',
        'HOME_URL="https://www.ubuntu.com/"',
        'UBUNTU_CODENAME=jammy',
      ]
      return rows.join('\r\n')
    }
    if (name === 'nginx.conf') {
      const rows: string[] = [
        'server {',
        '    listen 80;',
        '    server_name 192.0.2.10;',
        '',
        '    location / {',
        '        proxy_pass http://127.0.0.1:3000;',
        '        proxy_set_header Host $host;',
        '    }',
        '}',
      ]
      return rows.join('\r\n')
    }
    if (name === 'README.md') {
      return L.t('# web-01\n\n内部服务部署目录。改动前先读 deploy.sh。', '# web-01\n\nInternal service deployment directory. Read deploy.sh before changing anything.')
    }
    if (name.length === 0) {
      return 'cat: missing operand'
    }
    return `cat: ${name}: No such file or directory`
  }

  private diskFree(): string {
    const rows: string[] = [
      `${BLD}Filesystem      Size  Used Avail Use% Mounted on${RST}`,
      '/dev/vda1        39G   16G   21G  43% /',
      'tmpfs           3.9G     0  3.9G   0% /dev/shm',
      '/dev/vda15      105M  6.1M   99M   6% /boot/efi',
      'tmpfs           795M  1.1M  794M   1% /run',
    ]
    return rows.join('\r\n')
  }

  private memory(): string {
    const rows: string[] = [
      `${BLD}               total        used        free      shared  buff/cache   available${RST}`,
      'Mem:           7.8Gi       1.8Gi       4.1Gi        18Mi       1.9Gi       5.6Gi',
      'Swap:          2.0Gi          0B       2.0Gi',
    ]
    return rows.join('\r\n')
  }

  private processes(): string {
    const rows: string[] = [
      `${BLD}USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND${RST}`,
      'root         1  0.0  0.1 167636 11940 ?        Ss   Sep16   1:22 /sbin/init',
      'root       412  0.1  0.4 218740 36812 ?        Ssl  Sep16   8:41 /usr/bin/dockerd',
      'root       988  0.0  0.2  14928  9216 ?        Ss   Sep16   0:12 sshd: /usr/sbin/sshd',
      `${BGRN}ubuntu${RST}    30214  0.0  0.1  10124  5184 pts/0    Ss   17:33   0:00 -bash`,
      'ubuntu     30288  0.0  0.0   9112  3072 pts/0    R+   17:35   0:00 ps aux',
      'www-data  1820  0.3  1.2 1128472 102348 ?     Ssl  Oct20  22:18 node /opt/app/server.js',
    ]
    return rows.join('\r\n')
  }

  private network(): string {
    const rows: string[] = [
      '1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000',
      '    inet 127.0.0.1/8 scope host lo',
      '2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000',
      `    inet ${BGRN}10.0.8.12/24${RST} brd 10.0.8.255 scope global dynamic eth0`,
      '    inet6 fe80::216:3eff:fe4a:1a2b/64 scope link',
    ]
    return rows.join('\r\n')
  }

  private gitStatus(args: string[]): string {
    if (args.indexOf('status') < 0) {
      return 'usage: git status'
    }
    const rows: string[] = [
      'On branch main',
      "Your branch is ahead of 'origin/main' by 2 commits.",
      '  (use "git push" to publish your local commits)',
      '',
      'Changes not staged for commit:',
      '  (use "git add <file>..." to update what will be committed)',
      '',
      `${RED}        modified:   docker-compose.yml${RST}`,
      `${RED}        modified:   nginx.conf${RST}`,
      '',
      'Untracked files:',
      '  (use "git add <file>..." to include in what will be committed)',
      '',
      `${RED}        logs/errors.log${RST}`,
      '',
      'no changes added to commit (use "git add" and/or "git commit -a")',
    ]
    return rows.join('\r\n')
  }

  private neofetch(): string {
    const logo: string[] = [
      '   .--.      ',
      '  |o_o |     ',
      '  |:_/ |     ',
      ' //   \\ \\    ',
      '(|     | )   ',
      "/'\\_   _/`\\  ",
      '\\___)=(___/  ',
    ]
    const info: string[] = [
      `${BGRN}${USER}@${HOST}${RST}`,
      `${DIM}---------------${RST}`,
      `OS: Ubuntu 22.04.4 LTS x86_64`,
      `Kernel: 5.15.0-101-generic`,
      `Uptime: 42 days, 6 hours`,
      `Shell: bash 5.1.16`,
      `CPU: Intel Xeon (8) @ 2.50GHz`,
    ]
    const colors: string[] = [CYN, CYN, YEL, CYN, GRN, MAG, RED, BLU]
    const rows: string[] = []
    for (let i = 0; i < info.length; i++) {
      const left: string = i < logo.length ? `${CYN}${logo[i]}${RST}` : '             '
      rows.push(left + info[i])
    }
    rows.push('')
    rows.push(colors.map((c: string) => `${c}███${RST}`).join(''))
    return rows.join('\r\n')
  }
}
