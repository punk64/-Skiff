import Logging from 'ets/pages/entryPage/Session/Logging/index'
import Session from 'ets/pages/entryPage/Session/index'
import Terminal from 'ets/pages/entryPage/Terminal/index'
import Connection from 'ets/pages/entryPage/Connection/index'
import Data from 'ets/pages/entryPage/Connection/Data/index'
import SSH from 'ets/pages/entryPage/Connection/SSH/index'
import HostKeys from 'ets/pages/entryPage/Connection/SSH/HostKeys/index'
import Auth from 'ets/pages/entryPage/Connection/SSH/Auth/index'
import TTY from 'ets/pages/entryPage/Connection/SSH/TTY/index'
import X11 from 'ets/pages/entryPage/Connection/SSH/X11/index'
import Tunnels from 'ets/pages/entryPage/Connection/Tunnels/index'
import Proxy from 'ets/pages/entryPage/Connection/Proxy/index'
import Serial from 'ets/pages/entryPage/Connection/Serial/index'
import Telnet from 'ets/pages/entryPage/Connection/Telnet/index'

import Window from 'ets/pages/entryPage/Window/index'
import Appearance from 'ets/pages/entryPage/Window/Appearance'
import Bahaviour from 'ets/pages/entryPage/Window/Bahaviour'
import Colours from 'ets/pages/entryPage/Window/Colours'
import Selection from 'ets/pages/entryPage/Window/Selection'
import Translation from 'ets/pages/entryPage/Window/Translation'
import KeyGenerator from 'ets/pages/entryPage/KeyGenerator/Index'


@Component
export default struct DynamicsComponent {
  @Prop componentName: string|Resource
  @Prop componentId: number

  build() {
    Column(){
      Session().visibility(this.componentId !== 0?Visibility.None:Visibility.Visible)
      Logging().visibility(this.componentId !== 1?Visibility.None:Visibility.Visible)
      Terminal().visibility(this.componentId !== 2?Visibility.None:Visibility.Visible)
      Connection().visibility(this.componentId !== 6?Visibility.None:Visibility.Visible)
      Data().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      SSH().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      HostKeys().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      Auth().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      TTY().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      X11().visibility(this.componentId !==99?Visibility.None:Visibility.Visible)
      Tunnels().visibility(this.componentId !== 8?Visibility.None:Visibility.Visible)
      Proxy().visibility(this.componentId !== 25?Visibility.None:Visibility.Visible)
      // Serial().visibility(this.componentId !== 20?Visibility.None:Visibility.Visible)
      Telnet().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      Window().visibility(this.componentId !==18?Visibility.None:Visibility.Visible)
      Appearance().visibility(this.componentId !== 19?Visibility.None:Visibility.Visible)
      Bahaviour().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      Colours().visibility(this.componentId !== 23?Visibility.None:Visibility.Visible)
      Selection().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      Translation().visibility(this.componentId !== 99?Visibility.None:Visibility.Visible)
      KeyGenerator().visibility(this.componentId !== 24?Visibility.None:Visibility.Visible)
    }.alignItems(HorizontalAlign.Start)
  }
}
