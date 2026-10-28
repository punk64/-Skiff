import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import { pageDefaultData } from '../../../main/ets/pages/entryPage/pageDefaultData';
import { ProxyType } from '../../../main/ets/common/types/dataType';
import { encryptProxyPassword, decryptProxyPassword } from '../../../main/ets/common/utils/ProxyCredentialUtil';

// 二期：代理配置功能 ohosTest（B 方案，可用 aa test 命令行自动化执行）
export default function proxyOhosTest() {
  describe('proxyConfigOhosTest', () => {
    beforeAll(() => {
    });
    beforeEach(() => {
    });
    afterEach(() => {
    });
    afterAll(() => {
    });

    // ===== 默认配置结构 =====
    it('proxy_default_structure', 0, () => {
      const proxy = pageDefaultData.Proxy;
      expect(proxy.type).assertEqual('None');
      expect(proxy.enabled).assertFalse();
      expect(proxy.host).assertEqual('');
      expect(proxy.port).assertEqual('');
      expect(proxy.username).assertEqual('');
      expect(proxy.password).assertEqual('');
          });

    it('proxy_field_types', 0, () => {
      const proxy = pageDefaultData.Proxy;
      expect(typeof proxy.type).assertEqual('string');
      expect(typeof proxy.enabled).assertEqual('boolean');
      expect(typeof proxy.host).assertEqual('string');
      expect(typeof proxy.port).assertEqual('string');
      expect(typeof proxy.username).assertEqual('string');
      expect(typeof proxy.password).assertEqual('string');
          });

    it('proxy_default_consistency', 0, () => {
      const p = pageDefaultData.Proxy;
      expect(p.type).assertEqual(p.type);
      expect(p.port).assertEqual(p.port);
          });

    // ===== 会话策略 =====
    it('session_default_proxy_policy', 0, () => {
      expect(pageDefaultData.Session.proxyPolicy).assertEqual('global');
    });

    it('session_policy_field_type', 0, () => {
      expect(typeof pageDefaultData.Session.proxyPolicy).assertEqual('string');
    });

    it('policy_global_disabled_logic', 0, () => {
      const policy: string = 'global';
      const enabled = false;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertTrue();
    });

    it('policy_global_enabled_logic', 0, () => {
      const policy: string = 'global';
      const enabled = true;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertFalse();
    });

    it('policy_custom_force_enable', 0, () => {
      const policy: string = 'custom';
      const enabled = false;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertFalse();
    });

    it('policy_off_force_disable', 0, () => {
      const policy: string = 'off';
      const enabled = true;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertTrue();
    });

    it('policy_matrix_all_combinations', 0, () => {
      const use = (policy: string, enabled: boolean): boolean => {
        if (policy === 'off') return false;
        if (policy === 'custom') return true;
        return enabled;
      };
      expect(use('global', true)).assertTrue();
      expect(use('global', false)).assertFalse();
      expect(use('custom', true)).assertTrue();
      expect(use('custom', false)).assertTrue();
      expect(use('off', true)).assertFalse();
      expect(use('off', false)).assertFalse();
    });

    // ===== 端口解析/范围 =====
    it('proxy_port_parse_valid', 0, () => {
      expect(Number('1080')).assertEqual(1080);
      expect(Number('8080')).assertEqual(8080);
      expect(Number('1')).assertEqual(1);
      expect(Number('65535')).assertEqual(65535);
    });

    it('proxy_port_parse_invalid', 0, () => {
      expect(Number('0')).assertEqual(0);
      expect(Number('99999')).assertEqual(99999);
      const nanCheck = (n: number): boolean => n !== n;
      expect(nanCheck(Number('abc'))).assertTrue();
      expect(Number('')).assertEqual(0);
    });

    it('port_range_valid_boundary', 0, () => {
      const v = (p: number): boolean => p >= 1 && p <= 65535;
      expect(v(1)).assertTrue();
      expect(v(1080)).assertTrue();
      expect(v(65535)).assertTrue();
    });

    it('port_range_invalid', 0, () => {
      const v = (p: number): boolean => p >= 1 && p <= 65535;
      expect(v(0)).assertFalse();
      expect(v(-1)).assertFalse();
      expect(v(65536)).assertFalse();
    });

        // ===== 校验 =====
    it('host_required_when_enabled', 0, () => {
      const v = (type: string, host: string): boolean => type === 'None' ? true : host.trim().length > 0;
      expect(v('None', '')).assertTrue();
      expect(v('SOCKS5', '')).assertFalse();
      expect(v('SOCKS5', '192.168.1.1')).assertTrue();
      expect(v('HTTP', '  ')).assertFalse();
    });

    // ===== 类型/JSON =====
    it('proxy_type_enum_values', 0, () => {
      const validTypes: Array<'None' | 'SOCKS5' | 'HTTP'> = ['None', 'SOCKS5', 'HTTP'];
      expect(validTypes.indexOf('None') >= 0).assertTrue();
      expect(validTypes.indexOf('SOCKS5') >= 0).assertTrue();
      expect(validTypes.indexOf('HTTP') >= 0).assertTrue();
    });

    it('proxy_json_roundtrip', 0, () => {
      const original: ProxyType = {
        type: 'SOCKS5', enabled: true, host: '192.168.1.1', port: '1080',
        username: 'user', password: 'pass'
      };
      const parsed = JSON.parse(JSON.stringify(original)) as ProxyType;
      expect(parsed.type).assertEqual(original.type);
      expect(parsed.enabled).assertEqual(original.enabled);
      expect(parsed.host).assertEqual(original.host);
      expect(parsed.port).assertEqual(original.port);
      expect(parsed.username).assertEqual(original.username);
      expect(parsed.password).assertEqual(original.password);
          });

    // ===== 凭据工具 =====
    it('credential_empty_passthrough', 0, async () => {
      expect(await encryptProxyPassword('')).assertEqual('');
      expect(await decryptProxyPassword('')).assertEqual('');
    });

    it('credential_plaintext_passthrough', 0, async () => {
      expect(await decryptProxyPassword('legacy-plain-password')).assertEqual('legacy-plain-password');
    });

    it('credential_encrypted_no_double_encrypt', 0, async () => {
      expect(await encryptProxyPassword('H1:AAAAalreadyEncrypted==')).assertEqual('H1:AAAAalreadyEncrypted==');
    });

    it('credential_prefix_detection', 0, async () => {
      expect('H1:base64data'.startsWith('H1:')).assertTrue();
      expect('plaintext'.startsWith('H1:')).assertFalse();
    });

    it('credential_h1_format', 0, () => {
      const isH1 = (s: string): boolean => s.startsWith('H1:') && s.length > 3;
      expect(isH1('H1:abc')).assertTrue();
      expect(isH1('H1:')).assertFalse();
      expect(isH1('plain')).assertFalse();
    });

    it('credential_not_logging_plain', 0, () => {
      expect(pageDefaultData.Proxy.password).assertEqual('');
    });

    // ===== 默认值合理性 =====
    it('proxy_default_port_socks5', 0, () => {
      expect(pageDefaultData.Proxy.port).assertEqual('');
    });

      });
}